"""Golden vectors for the all-d-f60 follower (jank M2m: core/beat/jb_robust.c, jb_comb.c, the DBN's tempo prior, the lock and
jump gate; tests/test_robust.c): studies/follow-robust rchain.py replaying all-d-f60 (sweep_dev3.json) on the synthetic
tracks of scripts/tempo_golden.py, with the device's causal view (weak.py `causal`), the DBN run frame by frame as the device
does (its tempo prior steered by the tempo memory).

Written to OUT (jank's tests/robust_ref):
    NAME.track.txt   the inputs (the same format as tests/tempo_ref's: dur / beats / rms / act)
    NAME.txt         one line an operation, in the replay's order:
                       t now                      a tick (every 50 ms): the activation is in up to its frame
                       x                          a silence began: the estimator, the gate, the beats and the lines start over
                                                  (the tempo memory stays)
                       d t ...                    the beats the DBN made up to this tick (its frames up to now ran first)
                       r ratio t since            a rate event proposed
                       b t                        a beat fed (CoastBpm.update, the fast jump, the lines cleared on a jump)
                       g kind ok now period       the lock / jump gate asked (kind 0 lock, 1 jump) and its answer
                       s kf mode t p d jumps misses in_a_row manoeuvres follow_beats strong weak_hits coasted fast_jumps lines
                                                  the follower's state after a beat
                       c silent                   the tick's coast, then confidence, agreement, gate, memory and prior
                       q has_bpm bpm has_grid t0 p strict loose conf gconf gated running coasting confident has_mem mem_bpm
                         mem_t prior_on v_tempo v_phase v_dbn lines coasted jumped
                                                  what that tick gave (every 5th tick, and at each change of the gate)
Numbers are written with repr: C reads the very same doubles.

    .venv/bin/python scripts/robust_golden.py --out DIR
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent.parent
STUDY = REPO / "studies" / "tempo-changes"
ROBUST = REPO / "studies" / "follow-robust"
for p in (REPO, STUDY, ROBUST, REPO / "scripts"):
    sys.path.insert(0, str(p))

import chain as C          # noqa: E402  (the study folders are not packages)
import rchain as R         # noqa: E402
import rconfigs as RC      # noqa: E402
import robust as RB        # noqa: E402
import graded as GR        # noqa: E402
import tempo_golden as TG  # noqa: E402
import live.rate as LRATE  # noqa: E402
import weak as W           # noqa: E402
from dbn import OnlineDbn, tempo_prior  # noqa: E402
from eval.follow_bpm import FollowBpm  # noqa: E402
from live.engine import HYSTERESIS, SILENCE_S  # noqa: E402
from live.follow import LineLog, line_confidence  # noqa: E402

SWEEP = ROBUST / "sweep_dev3.json"
SKIP_SWEEP = ROBUST / "skip.json"
NAME = "all-d-f60"
SKIP_NAMES = ("all-act2-c1", "a2-pat")        # ACT and PAT of the setting; PAT also with the shown (graded) confidence
SKIP_CASES = Path(__file__).resolve().parent / "skip_cases.py"
_OPS: list[str] = []
FPS = 100


@dataclass
class CausalSkipBpm(RB.RobustBpm):
    """RobustBpm as the device sees it: the activation only up to now. The study reads the whole track's, so its window of
    +-w frames around a point just before now looks up to w - 3 frames ahead (+-0.08 period: 1-2 frames at 90-130 BPM); here
    that window ends at the newest frame in (frames_in). Everything else is the study's code."""

    frames_in: int = 0

    def _values(self, t_end: float, p: float, n: int, steps: int, w: int) -> np.ndarray:
        fr = np.round((t_end - np.arange(n) * p / steps) * FPS).astype(int)
        return np.asarray([self.act[max(f - w, 0):min(f + w + 1, self.frames_in)].max() for f in fr])


def make_estimator(cfg: R.RConfig, track: C.Track, gate) -> RB.RobustBpm:
    """rchain.make_estimator, with the causal skip estimator when the config has a skip."""
    if cfg.skip is None:
        return R.make_estimator(cfg, track, gate)
    base = cfg.base
    params = dict(base.weak or {})
    if cfg.coast is not None and "hold_s" in cfg.coast:
        params["hold_s"] = cfg.coast["hold_s"]
    est = CausalSkipBpm(track=track, params=params, gate=gate, skip=cfg.skip)
    for k, v in base.est_kw.items():
        setattr(est, k, v)
    est.sticky = base.sticky
    return est


class LoggedGate(RB.LockGate):
    """LockGate that writes each question and answer into the operation list."""

    def allow(self, est, now: float, period: float, kind: str) -> bool:
        ok = super().allow(est, now, period, kind)
        _OPS.append(f"g {0 if kind == 'lock' else 1} {int(ok)} {float(now)!r} {float(period)!r}")
        return ok


def extra_cases() -> dict:
    out = {}
    # a tempo at 3:4 of the one before (130 -> 97.5): the memory asks 48 consistent beats before it locks or jumps
    m = TG.Maker(21, 110.0)
    b = TG.grid([(130.0, 45.0), (97.5, 64.0)])
    b = b + m.rng.normal(0, 0.004, b.size)
    m.peaks(b, 0.8, 0.15)
    out["related"] = m.track(b)
    # a long weak passage: the activation near its floor with a faint pulse for 28 s (coast, floor, the 30 s hold)
    m = TG.Maker(22, 110.0, floor=0.02)
    true = TG.grid([(124.0, 109.0)])
    gap = (true >= 30.0) & (true < 58.0)
    m.peaks(true[~gap], 0.8, 0.1)
    m.peaks(true[gap], 0.05, 0.3)
    out["weak_long"] = m.track(true[~gap] + m.rng.normal(0, 0.003, int((~gap).sum())))
    # the track's own pulse at 3:4 of the lock: an activation with the 3:4 comb as strong as the true one
    m = TG.Maker(23, 90.0)
    true = TG.grid([(120.0, 89.0)])
    m.peaks(true, 0.7, 0.1)
    m.peaks(true[::3] + 0.5 * 60.0 / 120.0 * 0.5, 0.45, 0.1)
    out["ratio_pulse"] = m.track(true + m.rng.normal(0, 0.003, true.size))
    return out


def state_line(est, lines: LineLog, fast: int) -> str:
    return TG.state_line(est, lines, fast)


def logged_replay(track: C.Track, cfg: R.RConfig) -> tuple[list[str], dict]:
    """rchain._replay (all-d-f60: coast, memory "hold", ratio, agreement with the DBN part) with each operation written
    down. main() checks that its outputs are rchain.replay's."""
    base = cfg.base
    assert base.weak is not None and cfg.coast and cfg.memory and cfg.ratio and cfg.agree and cfg.live_dbn
    assert cfg.memory.get("mode", "hold") == "hold" and cfg.coast.get("on_jump") == "gate"
    out = _OPS
    out.clear()
    memory = RB.TempoMemory(mem_s=cfg.memory.get("mem_s", 120.0))
    dbn = OnlineDbn(track.act)
    periods = dbn.intervals.astype(float)

    def new_gate():
        m = cfg.memory
        return LoggedGate(memory=memory, related_beats=m.get("related_beats", 32), rel_tol=m.get("rel_tol", 0.04),
                          ratio=cfg.ratio, act=track.act, periods=periods)

    gate = new_gate()
    est = make_estimator(cfg, track, gate)
    agree = R.Agreement(cfg.agree, track.act, periods)
    lines = LineLog()
    rate = C.RateEvents(track.rate) if base.rate and base.follow else None
    queue: list = []
    ticks = np.arange(C.TICK_S, track.dur, C.TICK_S)
    n = ticks.size
    res = {k: np.full(n, np.nan) for k in ("bpm", "conf")}
    res["running"] = np.zeros(n, bool)
    beats: list[float] = []
    i = blk = fast = 0
    silent_s, gated, last_out, was_running = 0.0, False, None, False
    fed: list[float] = []
    mode = cfg.memory.get("mode", "hold")
    last_gate = None
    skip = cfg.skip is not None
    shown = bool(cfg.agree.get("shown"))
    for k, now in enumerate(ticks):
        out.append(f"t {float(now)!r}")
        while blk < track.rms.size and (blk + 1) * C.BLOCK_S <= now:
            was = silent_s >= SILENCE_S
            silent_s = silent_s + C.BLOCK_S if track.rms[blk] < C.SILENCE_RMS else 0.0
            if silent_s >= SILENCE_S and not was:
                gate = new_gate()
                est, fed = make_estimator(cfg, track, gate), []
                lines.clear()
                queue.clear()
                out.append("x")
            blk += 1
        silent = silent_s >= SILENCE_S
        if skip:
            est.frames_in = int(round(now * R.FPS)) + 1       # the newest frame the device has is round(now x 100)
        made = dbn.advance(int(round(now * R.FPS)))
        beats.extend(made)
        out.append("d" + "".join(f" {float(b)!r}" for b in made))
        jumps0 = est.jumps
        while i < len(beats) and W.ready(beats[i] + W.STRONG_HALF / W.FPS, now):
            b = float(beats[i])
            i += 1
            if silent:
                continue
            while queue and queue[0][0] <= b:
                t_ev, r = queue.pop(0)
                since = t_ev - LRATE.DETECT_LAG_S
                est.propose_ratio(r, t_ev, since=since)
                out.append(f"r {float(r)!r} {float(t_ev)!r} {float(since)!r}")
            jumps, rs = est.jumps, est.resyncs
            est.update(b)
            fast += int(C.fast_jump(est, b, base.fast_jump)) if base.fast_jump is not None else 0
            if base.clear_lines and (est.jumps != jumps or est.resyncs != rs):      # (rchain: a skip's re-sync too)
                lines.clear()
            fed.append(b)
            out.append(f"b {b!r}")
            out.append(state_line(est, lines, fast))
            if skip:
                out.append(f"k {est.off_run} {int(est.beat_suspect)} {est.reopen} {est.resyncs}")
        jumped = est.jumps != jumps0
        est.coast(now)
        out.append(f"c {int(silent)}")
        if rate is not None:
            active = not silent and est.kf is not None and est.mode_at(now) != "stopped"
            for ev in rate.step(now, active):
                queue.append(ev)
        bpm, grid = est.bpm_at(now), est.grid()
        period = grid[1] if grid else None
        if silent:
            conf = strict = loose = 0.0
        else:
            lines.tick(grid if bpm is not None else None, now)
            strict = line_confidence(fed[-64:], lines, period, now)
            loose = est.confidence(fed[-64:], lines, period, now)
            conf = strict if gated or not was_running else loose
        g_conf = conf
        if not silent:
            g_conf = agree.value(now, conf, grid, lines, dbn)
        threshold = cfg.agree.get("threshold", base.threshold)
        start = min(threshold + HYSTERESIS, 1.0)
        would = g_conf < start if gated else g_conf < threshold
        keep = (was_running and not silent and bpm is not None and not jumped
                and g_conf >= cfg.coast.get("floor", 0.0))
        gated = False if keep else would
        if skip:
            if est.reopen >= cfg.skip.get("reopen", 2) and bpm is not None:
                gated = False                       # the new grid has fitted `reopen` beats: no wait for the confidence
                if g_conf >= threshold:
                    est.reopen = -1
            if est.suspect:
                gated = True
        sh = float("nan")
        if shown and not silent:
            lines_t = np.asarray(lines.times())
            sh = GR.graded(GR.soft_f(fed, lines_t, period, now) if period else 0.0, GR.jitter(fed),
                           agree.vals["tempo"], agree.vals["dbn"])
        run = bpm is not None and not gated and not silent
        coasting = run and g_conf < threshold
        was_running = run
        confident = run and strict >= base.threshold and est.kf is not None and not jumped
        if confident:
            memory.remember(bpm, float(now))
        prior_on = False
        mem = memory.active(float(now))
        if mem is not None and not confident:
            w = tempo_prior(dbn.bpms, mem, cfg.memory.get("width", 0.03), cfg.memory.get("floor", 0.1))
            dbn.set_prior(w)
            prior_on = True
        elif mode == "hold":
            dbn.set_prior(None)
        res["bpm"][k] = bpm if bpm is not None else np.nan
        res["conf"][k] = g_conf
        res["running"][k] = run
        if k % 5 == 4 or (gated, run) != last_gate:
            t0, p = grid if grid is not None else (0.0, 0.0)
            v = agree.vals
            out.append(f"q {int(bpm is not None)} {float(bpm or 0.0)!r} {int(grid is not None)} {float(t0)!r} "
                       f"{float(p)!r} {strict!r} {loose!r} {conf!r} {g_conf!r} {int(gated)} {int(run)} {int(coasting)} "
                       f"{int(confident)} {int(memory.bpm is not None)} {float(memory.bpm or 0.0)!r} "
                       f"{float(memory.t if memory.bpm is not None else 0.0)!r} {int(prior_on)} "
                       f"{v['tempo']!r} {v['phase']!r} {v['dbn']!r} {len(lines.times())} {est.coasted} {int(jumped)}")
        if skip:                                    # the skip state after the whole tick, every tick
            tpl = float(est.tpl.sum()) if est.tpl is not None else 0.0
            out.append(f"u {est.act_run} {est.reopen} {est.resyncs} {est.act_resyncs} {est.tpl_n} {int(est.suspect)} "
                       f"{float(est.act_last)!r} {tpl!r} {int(gated)} {sh!r}")
        last_gate = (gated, run)
    return list(out), res


def comb_units() -> list[str]:
    """The comb pieces on two activations (a pulse at 0.5 s with a 3:4 shadow, and noise): lines `a N x1000...`, then per
    check `m end period` (ratio_margin, 10 s), `t end period` (tempo_agreement, 8 s), `p end period` (phase_agreement
    over the lines of a grid), each followed by the value (nan: written nan)."""
    import comb as CB
    out = []
    rng = np.random.default_rng(5)
    for case in range(2):
        n = 2200
        act = rng.uniform(0.0, 0.05, n)
        if case == 0:
            for f in range(40, n - 2, 50):
                act[f] += 0.8
            for f in range(40, n - 2, 38):
                act[f] += 0.35
        act = (np.round(act * 1000) / 1000).astype(np.float32)
        out.append(f"a {n} " + " ".join(str(int(round(float(x) * 1000))) for x in act))
        periods = np.arange(33, 68, dtype=float)
        ml = CB.max_lag_for(67.0)
        for end in (500, 553, 554, 900, 1500, 2199):
            for per in (50.0, 38.0, 37.5, 66.7, 33.0, 140.0):
                r = CB.curve(act, end, 1000, ml)
                m = CB.ratio_margin(r, per) if r is not None else float("nan")
                out.append(f"m {end} {per!r} {m!r}")
                r = CB.curve(act, end, 800, ml)
                t = CB.tempo_agreement(r, per, periods) if r is not None else 0.0
                out.append(f"t {end} {per!r} {t!r}")
                ls = np.arange(end / 100.0 - 7.9, end / 100.0 - 0.1, per / 100.0)
                out.append(f"p {end} {per!r} {CB.phase_agreement(act, ls, per / 100.0, end)!r}")
    # the tempo prior of one memory (bpm 128, width .04, floor .1) on the DBN's tempi
    bpms = 6000.0 / np.arange(33, 68)
    w = tempo_prior(bpms, 128.0, 0.04, 0.1)
    out.append("w 128.0 0.04 0.1 " + " ".join(repr(float(x)) for x in w))
    return out


def skip_cases() -> dict:
    """The four synthetic skips of scripts/skip_cases.py (jump, nudge, gap, other), the activation quantised as the files are."""
    import importlib.util
    spec = importlib.util.spec_from_file_location("skip_cases", SKIP_CASES)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    out = {}
    for name, (tr, _pre, _after, _t) in mod.cases().items():
        tr.act = (np.round(tr.act * TG.ACT_Q) / TG.ACT_Q).astype(np.float32)
        tr.rms = (np.round(tr.rms * TG.RMS_Q) / TG.RMS_Q).astype(np.float32)
        out["skip_" + name] = tr
    return out


def causal_cfg(cfg: R.RConfig) -> R.RConfig:
    """cfg on the device's view (weak.py `causal`: the firmware chain is tuned-l16-causal)."""
    base = RC.CF.named(["tuned-l16-causal"], json.loads(RC.FINAL.read_text()))[0]
    return R.RConfig(name=cfg.name, base=base, coast=cfg.coast, memory=cfg.memory, ratio=cfg.ratio, agree=cfg.agree,
                     skip=cfg.skip, dbn=cfg.dbn)


def write_skip(out_dir: Path, only: list[str] | None) -> None:
    """tests/skip_ref: per track NAME.track.txt and one NAME.<act|pat>.txt per setting (ACT = all-act2-c1, PAT = a2-pat with the
    shown confidence), and the host's own replay (the study's, whole activation) compared with the causal one."""
    sweep = json.loads(SWEEP.read_text()) | json.loads(SKIP_SWEEP.read_text())
    sweep["pat-shown"] = {"base": "a2-pat", "agree": {"shown": True}}
    settings = {"act": "all-act2-c1", "pat": "pat-shown"}
    out_dir.mkdir(parents=True, exist_ok=True)
    cases = {**TG.cases(), **extra_cases(), **skip_cases()}
    for name, tr in cases.items():
        if only and name not in only:
            continue
        (out_dir / f"{name}.track.txt").write_text(f"# {name}: the inputs (scripts/robust_golden.py --skip-out)\n"
                                                   + "\n".join(TG.track_lines(tr)) + "\n")
        for tag, cname in settings.items():
            cfg = causal_cfg(RC.named([cname], sweep)[0])
            with C.consts_applied(cfg.base.consts):
                ops, res = logged_replay(tr, cfg)
            ref = R.replay(tr, cfg)                       # the study's replay: reads the activation a few frames past now
            n_diff = int((ref.running != res["running"]).sum())
            head = f"# {name} {cname}: rchain.py (causal skip view), one line an operation (scripts/robust_golden.py --skip-out)\n"
            (out_dir / f"{name}.{tag}.txt").write_text(head + "\n".join(ops) + "\n")
            resyncs = sum(1 for o in ops if o.startswith("u ")) and max(int(o.split()[4]) for o in ops if o.startswith("u "))
            print(f"{name}.{tag}: running {res['running'].mean():.2f}, act resyncs {resyncs}, ticks {res['running'].size}; "
                  f"vs the study's whole-activation replay: {n_diff} ticks differ in running")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--skip-out", type=Path, help="write the skip / graded vectors (jank tests/skip_ref) instead")
    ap.add_argument("--only", nargs="*", default=None)
    args = ap.parse_args()
    if args.skip_out:
        write_skip(args.skip_out, args.only)
        return
    if not args.out:
        ap.error("--out or --skip-out")
    args.out.mkdir(parents=True, exist_ok=True)
    cfg = RC.named([NAME], json.loads(SWEEP.read_text()))[0]
    cfg_c = causal_cfg(cfg)
    (args.out / "comb_units.txt").write_text("# the comb pieces (scripts/robust_golden.py)\n" + "\n".join(comb_units()) + "\n")
    cases = {**TG.cases(), **extra_cases()}
    for name, tr in cases.items():
        if args.only and name not in args.only:
            continue
        (args.out / f"{name}.track.txt").write_text(f"# {name}: the inputs (scripts/robust_golden.py)\n"
                                                    + "\n".join(TG.track_lines(tr)) + "\n")
        with C.consts_applied(cfg_c.base.consts):
            ops, res = logged_replay(tr, cfg_c)
        # the logged replay is rchain's: the same decisions (rchain reads the whole cached activation but the causal
        # view; same ticks, same beats)
        ref = R.replay(tr, cfg_c)
        same = (np.array_equal(ref.running, res["running"]) and np.allclose(ref.conf, res["conf"], equal_nan=True)
                and np.allclose(ref.bpm, res["bpm"], equal_nan=True))
        if not same:
            raise SystemExit(f"{name}: the logged replay is not rchain.replay")
        head = f"# {name} {cfg.name}: studies/follow-robust/rchain.py, one line an operation (scripts/robust_golden.py)\n"
        (args.out / f"{name}.txt").write_text(head + "\n".join(ops) + "\n")
        asks = [o for o in ops if o.startswith("g ")]
        print(f"{name}: running {ref.running.mean():.2f}, coasting {ref.coasting.mean():.2f}, gate asks {len(asks)} "
              f"(refused {sum(1 for o in asks if o.split()[2] == '0')}), prior on {ref.prior_on.mean():.2f}, "
              f"ticks {ref.running.size}")


if __name__ == "__main__":
    main()
