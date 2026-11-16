"""Golden vectors for the device's tuned beat follow (jank M2j: core/beat/jb_weak.c, jb_follow.c's fast jump, jank_conf.c's
weak confidence and gate, jb_rate.c's CONFIRM 3; tests/test_tempo.c): studies/tempo-changes/chain.py replaying the
proposal tuned-l16 (configs + final.json) on synthetic tracks, with the device's causal view (weak.py `causal`) and,
for the port itself, the study's own look-ahead view.

Written to OUT (jank's tests/tempo_ref):
    NAME.track.txt   the inputs, once per case:
                       dur D / beats ... / rms N ... (per 0.1 s block, x 1e-4) / act N ... (per frame, x 1e-3)
    NAME.txt         the causal run (tuned-l16-causal), NAME.nc.txt the look-ahead run (tuned-l16); one line an operation,
                     in the replay's order:
                       t now                      a tick (every 50 ms): the activation is in up to its frame
                       x                          a silence began: the estimator, the beats and the lines start over
                       r ratio t since            a rate event proposed (FollowBpm.propose_ratio)
                       b t                        a beat fed (CoastBpm.update, then the fast jump, then the lines cleared
                                                  on a jump)
                       s kf mode t p d jumps misses in_a_row manoeuvres follow_beats strong weak_hits coasted fast_jumps
                         lines                    the state after it
                       c silent                   the tick's coast, then its confidence and gate
                       q has_bpm bpm has_grid t0 p strict loose conf gated running lines coasted
                                                  what that tick gave (every 5th tick and at each change of the gate)
    rate_NAME_sSeE.txt  scripts/follow_golden.py's rate runs with live/rate.py CONFIRM 3
Numbers are written with repr: C reads the very same doubles.

    .venv/bin/python scripts/tempo_golden.py --out DIR
"""
from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent.parent
STUDY = REPO / "studies" / "tempo-changes"
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(STUDY))
sys.path.insert(0, str(REPO / "scripts"))

import chain as C          # noqa: E402  (the study folder is not a package)
import configs as CF       # noqa: E402
import follow_golden as FG  # noqa: E402
import weak as W           # noqa: E402
from eval.follow_bpm import FollowBpm  # noqa: E402

FINAL = STUDY / "final.json"
MODES = {"stopped": 0, "steady": 1, "speeding_up": 2, "slowing_down": 3}
ACT_Q = 1000.0
RMS_Q = 10000.0


# ---------------------------------------------------------------------------------------------- synthetic tracks

def grid(bpms: list[tuple[float, float]], t0: float = 0.5) -> np.ndarray:
    """Beats of consecutive sections (bpm, seconds)."""
    out, t = [], t0
    for bpm, span in bpms:
        end = t + span
        while t < end:
            out.append(t)
            t += 60.0 / bpm
    return np.asarray(out)


def ramp(b0: float, b1: float, t_from: float, span: float, until: float) -> np.ndarray:
    out, t = [], 0.5
    while t < until:
        out.append(t)
        t += 60.0 / (b0 + (b1 - b0) * min(max((t - t_from) / span, 0.0), 1.0))
    return np.asarray(out)


def frames(t: np.ndarray) -> np.ndarray:
    return np.round(np.asarray(t) * 100).astype(int)


def on_frames(beats: np.ndarray) -> np.ndarray:
    """The tracker's beats fall on frames: k / 100 as the device gives them (jb_frame.time)."""
    return np.unique(frames(beats)) / 100.0


class Maker:
    def __init__(self, seed: int, dur: float, floor: float = 0.008) -> None:
        self.rng = np.random.default_rng(seed)
        self.dur = dur
        self.n = int(dur * 100) + 1
        self.act = self.rng.uniform(0.3, 1.0, self.n) * floor
        self.rms = np.full(int(dur * 10), 0.1)

    def peaks(self, times: np.ndarray, height, spread: float = 0.0) -> None:
        h = np.broadcast_to(np.asarray(height, float), np.shape(times))
        for t, a in zip(times, h):
            f = int(round(t * 100))
            if 1 <= f < self.n - 1:
                a = a * (1.0 + spread * self.rng.uniform(-1, 1))
                self.act[f] = max(self.act[f], a)
                self.act[f - 1] = max(self.act[f - 1], 0.45 * a)
                self.act[f + 1] = max(self.act[f + 1], 0.6 * a)

    def silence(self, t0: float, t1: float) -> None:
        self.rms[int(t0 * 10):int(t1 * 10)] = 0.0002
        self.act[int(t0 * 100):int(t1 * 100)] = 0.001

    def track(self, beats: np.ndarray, rate: np.ndarray | None = None) -> C.Track:
        act = (np.round(self.act * ACT_Q) / ACT_Q).astype(np.float32)
        rms = (np.round(self.rms * RMS_Q) / RMS_Q).astype(np.float32)
        r = np.zeros((0, 3)) if rate is None else rate
        return C.Track(on_frames(beats), rms, r, act, np.zeros((self.n, 2), np.float32), self.dur)


def rate_step(dur: float, t_step: float, ratio: float, rng) -> np.ndarray:
    """Estimates every 0.2 s (the device: every 2nd 0.1 s block): 1 before the step, the ratio while the 0.5 s window
    holds the new rate and the 4 s reference the old one."""
    t = np.arange(0.2, dur, 0.2)
    r = 1.0 + rng.normal(0, 0.0004, t.size)
    on = (t >= t_step + 0.5) & (t < t_step + 4.5)
    r[on] = ratio + rng.normal(0, 0.0005, int(on.sum()))
    return np.stack([t, r, np.full(t.size, 0.9)], axis=1)


def cases() -> dict[str, C.Track]:
    out = {}
    m = Maker(1, 70.0)
    b = grid([(126.0, 69.0)])
    b = b + m.rng.normal(0, 0.003, b.size)
    m.peaks(b, 0.85, 0.1)
    out["steady"] = m.track(b)

    # pop: a low activation (Charli XCX: ~0.2 at the beats), some beats weaker, a busy floor
    m = Maker(2, 80.0, floor=0.04)
    b = grid([(150.0, 79.0)])
    b = b + m.rng.normal(0, 0.006, b.size)
    m.peaks(b, np.where(np.arange(b.size) % 2 == 0, 0.22, 0.12), 0.4)
    out["pop_low"] = m.track(b)

    # a +3 % pitch move with the rate's estimates, and a +8 % one without (the fast jump's)
    m = Maker(3, 70.0)
    b = grid([(135.0, 35.0), (135.0 * 1.03, 34.0)]) + m.rng.normal(0, 0.003, 1)[0]
    m.peaks(b, 0.8, 0.1)
    out["step_p3_rate"] = m.track(b, rate_step(70.0, 35.5, 1.03, m.rng))
    m = Maker(4, 70.0)
    b = grid([(135.0, 35.0), (135.0 * 1.08, 34.0)])
    m.peaks(b, 0.8, 0.1)
    out["step_p8"] = m.track(b)

    # a section change and back (Primrose Gold: 138 -> 110 -> 138)
    m = Maker(5, 100.0)
    b = grid([(138.0, 40.0), (110.0, 35.0), (138.0, 24.0)])
    b = b + m.rng.normal(0, 0.004, b.size)
    m.peaks(b, 0.7, 0.2)
    out["section"] = m.track(b)

    # a section change into a quieter part: its beats are weak, the follower's jump rule waits for misses; the fast
    # jump takes the new grid once 16 beats agree on it (138 -> 110, and a smaller 120 -> 130)
    for name, seed, bpms in (("section_quiet", 12, [(138.0, 40.0), (110.0, 30.0)]),
                             ("step_quiet", 13, [(120.0, 40.0), (130.0, 30.0)])):
        m = Maker(seed, 72.0)
        b = grid(bpms)
        b = b + m.rng.normal(0, 0.004, b.size)
        m.peaks(b[b < 40.5], 0.8, 0.2)
        m.peaks(b[b >= 40.5], 0.12, 0.2)
        out[name] = m.track(b)

    m = Maker(6, 80.0)
    b = ramp(124.0, 130.0, 20.0, 40.0, 79.0)
    b = b + m.rng.normal(0, 0.003, b.size)
    m.peaks(b, 0.8, 0.1)
    out["ramp"] = m.track(b)

    # beat-less: no tracked beat for 16 s, small activation peaks on the true beats
    m = Maker(7, 90.0)
    true = grid([(126.0, 89.0)])
    gap = (true >= 40.0) & (true < 56.0)
    m.peaks(true[~gap], 0.8, 0.1)
    m.peaks(true[gap], 0.06, 0.3)
    out["beatless"] = m.track(true[~gap] + m.rng.normal(0, 0.003, int((~gap).sum())))

    # a breakdown: the tracker's beats wander off (its tempo model alone) on a weak activation
    m = Maker(8, 90.0)
    true = grid([(128.0, 89.0)])
    gap = (true >= 40.0) & (true < 58.0)
    m.peaks(true[~gap], 0.8, 0.1)
    m.peaks(true[gap], 0.07, 0.3)
    tracked = true + np.where(gap, m.rng.uniform(-0.1, 0.1, true.size), m.rng.normal(0, 0.003, true.size))
    out["breakdown"] = m.track(np.sort(tracked))

    # the tracker slides to the off-beat for 30 s (Sympathy is a knife), the activation weak there
    m = Maker(9, 90.0, floor=0.03)
    true = grid([(124.0, 89.0)])
    p = 60.0 / 124.0
    slid = (true >= 30.0) & (true < 60.0)
    tracked = true + np.where(slid, p / 2, 0.0) + m.rng.normal(0, 0.004, true.size)
    m.peaks(true, 0.25, 0.3)
    m.peaks(tracked[slid], 0.12, 0.3)
    out["offbeat"] = m.track(tracked)

    # silence, then the music again a little later in phase
    m = Maker(10, 70.0)
    b = np.concatenate([grid([(128.0, 34.0)]), grid([(128.0, 30.0)], t0=37.13)])
    m.peaks(b, 0.8, 0.1)
    m.silence(34.6, 36.4)
    out["silence"] = m.track(b)

    # half time: the tracker's beats every other beat for 20 s (an octave: no fast jump)
    m = Maker(11, 80.0)
    true = grid([(172.0, 79.0)])
    keep = ~((true >= 30.0) & (true < 50.0) & (np.arange(true.size) % 2 == 1))
    m.peaks(true[keep], 0.8, 0.1)
    out["halftime"] = m.track(true[keep])
    return out


# ------------------------------------------------------------------------------------------- the logged replay

def state_line(est, lines: C.LineLog, fast: int) -> str:
    kf = est.kf
    x = kf.x if kf is not None else np.zeros(3)
    man = kf.manoeuvres if kf is not None else -1
    return (f"s {int(kf is not None)} {MODES[est.mode]} {float(x[0])!r} {float(x[1])!r} {float(x[2])!r} {est.jumps} "
            f"{est.misses} {est.in_a_row} {man} {est.follow_beats} {len(est.strong)} {len(est.weak_hits)} "
            f"{est.coasted} {fast} {len(lines.times())}")


def logged_replay(track: C.Track, cfg: C.Config) -> tuple[list[str], dict]:
    """chain._replay (conf "weak", rate on, fast jump and cleared lines, no adaptive R) with each operation written down.
    main() checks that its outputs are chain.replay's."""
    assert cfg.weak is not None and cfg.conf == "weak" and cfg.fast_jump and cfg.clear_lines and not cfg.adaptive_r
    from live.engine import HYSTERESIS, SILENCE_RMS, SILENCE_S
    import live.rate as LRATE
    est = C.make_estimator(cfg, track)
    lines = C.LineLog()
    rate = C.RateEvents(track.rate)
    queue: list = []
    ticks = np.arange(C.TICK_S, track.dur, C.TICK_S)
    out: list[str] = []
    res = {k: np.full(ticks.size, np.nan) for k in ("bpm", "conf")}
    res["running"] = np.zeros(ticks.size, bool)
    beats = track.beats
    i = blk = 0
    silent_s, gated, was_running, fast = 0.0, False, False, 0
    fed: list[float] = []
    causal = bool(cfg.weak.get("causal"))
    last_gate = None
    for k, now in enumerate(ticks):
        out.append(f"t {float(now)!r}")
        while blk < track.rms.size and (blk + 1) * C.BLOCK_S <= now:
            was = silent_s >= SILENCE_S
            silent_s = silent_s + C.BLOCK_S if track.rms[blk] < SILENCE_RMS else 0.0
            if silent_s >= SILENCE_S and not was:
                est, fed = C.make_estimator(cfg, track), []
                lines.clear()
                queue.clear()
                out.append("x")
            blk += 1
        silent = silent_s >= SILENCE_S
        while i < beats.size and (beats[i] <= now if not causal else W.ready(beats[i] + W.STRONG_HALF / W.FPS, now)):
            b = float(beats[i])
            i += 1
            if silent:
                continue
            while queue and queue[0][0] <= b:
                t_ev, r = queue.pop(0)
                since = t_ev - LRATE.DETECT_LAG_S
                est.propose_ratio(r, t_ev, since=since)
                out.append(f"r {float(r)!r} {float(t_ev)!r} {float(since)!r}")
            jumps = est.jumps
            est.update(b)
            fast += int(C.fast_jump(est, b, cfg.fast_jump))
            if est.jumps != jumps:
                lines.clear()
            fed.append(b)
            out.append(f"b {b!r}")
            out.append(state_line(est, lines, fast))
        est.coast(now)
        out.append(f"c {int(silent)}")
        active = not silent and isinstance(est, FollowBpm) and est.kf is not None and est.mode_at(now) != "stopped"
        for ev in rate.step(now, active):
            queue.append(ev)
        bpm, g = est.bpm_at(now), est.grid()
        strict = loose = 0.0
        if silent:
            conf = 0.0
        else:
            lines.tick(g if bpm is not None else None, now)
            period = g[1] if g else None
            loose = est.confidence(fed[-64:], lines, period, now)
            strict = C.line_confidence(fed[-64:], lines, period, now)
            conf = strict if gated or not was_running else loose
        start = min(cfg.threshold + HYSTERESIS, 1.0)
        gated = conf < start if gated else conf < cfg.threshold
        run = bpm is not None and not gated and not silent
        was_running = run
        res["bpm"][k] = bpm if bpm is not None else np.nan
        res["conf"][k] = conf
        res["running"][k] = run
        if k % 5 == 4 or (gated, run) != last_gate:
            t0, p = g if g is not None else (0.0, 0.0)
            out.append(f"q {int(bpm is not None)} {float(bpm or 0.0)!r} {int(g is not None)} {float(t0)!r} {float(p)!r} "
                       f"{strict!r} {loose!r} {conf!r} {int(gated)} {int(run)} {len(lines.times())} {est.coasted}")
        last_gate = (gated, run)
    return out, res


def track_lines(tr: C.Track) -> list[str]:
    return [f"dur {tr.dur!r}",
            "beats " + " ".join(repr(float(b)) for b in tr.beats),
            f"rms {tr.rms.size} " + " ".join(str(int(round(float(x) * RMS_Q))) for x in tr.rms),
            f"act {tr.act.size} " + " ".join(str(int(round(float(x) * ACT_Q))) for x in tr.act)]


def main() -> None:
    import json
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    extra = json.loads(FINAL.read_text())
    cfgs = {"": CF.named(["tuned-l16-causal"], extra)[0], ".nc": CF.named(["tuned-l16"], extra)[0]}
    for name, tr in cases().items():
        (args.out / f"{name}.track.txt").write_text(f"# {name}: the inputs (scripts/tempo_golden.py)\n"
                                                    + "\n".join(track_lines(tr)) + "\n")
        for suffix, cfg in cfgs.items():
            with C.consts_applied(cfg.consts):
                ops, res = logged_replay(tr, cfg)
            ref = C.replay(tr, cfg)
            same = (np.array_equal(ref.running, res["running"]) and np.allclose(ref.conf, res["conf"], equal_nan=True)
                    and np.allclose(ref.bpm, res["bpm"], equal_nan=True))
            if not same:
                raise SystemExit(f"{name}{suffix}: the logged replay is not chain.replay")
            head = f"# {name} {cfg.name}: studies/tempo-changes/chain.py, one line an operation (scripts/tempo_golden.py)\n"
            (args.out / f"{name}{suffix}.txt").write_text(head + "\n".join(ops) + "\n")
            n = ref.running.size
            print(f"{name}{suffix}: running {ref.running.mean():.2f}, beats {tr.beats.size}, ticks {n}, "
                  f"coasts {sum(1 for o in ops if o.startswith('c'))}")
    with C.consts_applied({"rate.CONFIRM": 3}):
        for name in FG.RATE_CASES:
            for step, every in ((1, 1), (2, 1), (2, 2)):
                head = (f"# rate {name} step {step} every {every}: live/rate.py RateProbe, CONFIRM 3, on jb_fe's frames "
                        f"(scripts/tempo_golden.py)\n")
                lines = FG.rate_lines(name, step, every)
                (args.out / f"rate_{name}_s{step}e{every}.txt").write_text(head + "\n".join(lines) + "\n")
                print(f"rate_{name}_s{step}e{every}: {[x for x in lines if x[0] == 'v']}")


if __name__ == "__main__":
    main()
