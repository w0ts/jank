"""The firmware chain (tempo-changes/chain.py, tuned-l16-causal = jank M2j/M2k) with the four changes, each behind a flag.

  coast   (change 1, COAST, DON'T STOP) once the outputs run, a confidence under the gate no longer stops them: they keep
          the follower's tempo and phase. They stop on silence (0.5 s under -60 dBFS, as today) or when the follower
          itself stops: hold_s with no evidence at all (no fitted strong beat, no weak observation). on_jump "follow": a
          jump of the follower moves the outputs at once; "gate": the strict gate must open again after a jump.
          `floor`: the coast also ends when the gate's confidence (with `agree`: the agreement) falls under it.
          The confidence is still computed; `coasting` marks the ticks run under the gate.
  memory  (change 2, TEMPO MEMORY) the last confident tempo (outputs running, strict line confidence at or over the gate,
          the follower gliding) is remembered for mem_s. While the outputs are not confident (stopped, under the gate, or
          after a jump) the DBN gets a tempo prior peaked at it and its octaves (dbn.tempo_prior; `mode` "hold": on the
          new-beat transitions every frame, "once": one reweight of the forward vector when confidence is lost, "reseed":
          a uniform phase with the prior, "none": no DBN change). The follower's locks / jumps at a 3:4 / 4:3 / 2:3 / 3:2
          of it need related_beats consistent beats (robust.LockGate).
  ratio   (change 3, RATIO CHECK) robust.LockGate's comb check before any lock or jump.
  agree   (change 4, AGREEMENT CONFIDENCE) the gate's confidence is the mean of `parts`: "lines" (today's: strict to open,
          weak once open), "tempo" (comb.tempo_agreement), "phase" (comb.phase_agreement), "dbn" (the DBN's tempo mass
          within 4 % of the follower's tempo or an octave of it), refreshed every every_s (lines: every tick); the gate
          opens at `threshold` + HYSTERESIS and closes under `threshold`.
With every flag off the replay is chain.replay's (test_follow_robust checks it).
"""
from __future__ import annotations

import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tempo-changes"))
sys.path.insert(0, str(HERE))

import chain as C
import comb as CB
import eval.follow_bpm as FB
import live.rate as LRATE
from dbn import OnlineDbn, tempo_prior
from eval.follow_bpm import FollowBpm
from live.engine import HYSTERESIS, SILENCE_S
from live.fold import Fold
from live.follow import LineLog, line_confidence
import graded as GR
from robust import LockGate, RobustBpm, TempoMemory
from weak import STRONG_HALF, ready

FPS = 100
PROBE = None                            # a study's per-tick hook (confprobe.py): called with the replay's values
DBN_AGREE = 0.04


@dataclass(frozen=True)
class RConfig:
    name: str
    base: C.Config
    coast: dict | None = None
    memory: dict | None = None
    ratio: dict | None = None
    agree: dict | None = None
    skip: dict | None = None
    dbn: bool = False                   # run the DBN in the replay (forced by a memory prior, or an agree "dbn" part)

    @property
    def live_dbn(self) -> bool:
        return self.dbn or (self.memory is not None and self.memory.get("mode", "hold") != "none") or \
            (self.agree is not None and "dbn" in self.agree.get("parts", ()))


@dataclass
class RRun(C.Run):
    coasting: np.ndarray = None         # running with the gate's confidence under the threshold
    strict: np.ndarray = None           # the strict line confidence
    prior_on: np.ndarray = None
    shown: np.ndarray = None            # the graded confidence (graded.py) with agree {"shown": true}: display only, gates nothing
    gate_log: list = field(default_factory=list)
    refused: dict = field(default_factory=dict)


def make_estimator(cfg: RConfig, track: C.Track, gate: LockGate | None):
    base = cfg.base
    params = dict(base.weak or {})
    if cfg.coast is not None and "hold_s" in cfg.coast:
        params["hold_s"] = cfg.coast["hold_s"]
    if base.weak is None:
        raise ValueError("the robust chain sits on the weak follower (tuned-l16-causal)")
    est = RobustBpm(track=track, params=params, gate=gate, skip=cfg.skip)
    for k, v in base.est_kw.items():
        if not hasattr(est, k):
            raise KeyError(f"estimator has no {k}")
        setattr(est, k, v)
    est.sticky = base.sticky
    return est


def replay(track: C.Track, cfg: RConfig) -> RRun:
    with C.consts_applied(cfg.base.consts):
        return _replay(track, cfg)


class Agreement:
    """Change 4's parts other than the lines, refreshed every every_s."""

    def __init__(self, cfg: dict, act: np.ndarray, periods: np.ndarray) -> None:
        self.parts = tuple(cfg.get("parts", ("lines", "tempo", "phase")))
        self.every = cfg.get("every_s", 0.5)
        self.span = int(round(cfg.get("span_s", 8.0) * FPS))
        self.act, self.periods = act, periods
        self.max_lag = CB.max_lag_for(float(periods.max()))
        self.next_t = 0.0
        self.vals = {"tempo": 0.0, "phase": 0.0, "dbn": 0.0}

    def value(self, now: float, conf_lines: float, grid, lines: LineLog, dbn: OnlineDbn | None) -> float:
        if now >= self.next_t:
            self.next_t = now + self.every
            self._refresh(now, grid, lines, dbn)
        vals = [conf_lines if p == "lines" else self.vals[p] for p in self.parts]
        return float(np.mean(vals))

    def _refresh(self, now: float, grid, lines: LineLog, dbn: OnlineDbn | None) -> None:
        if grid is None:
            self.vals = {k: 0.0 for k in self.vals}
            return
        period = grid[1]
        end = int(math.floor(now * FPS + 1e-9))
        if "tempo" in self.parts:
            r = CB.curve(self.act, end, self.span, self.max_lag)
            self.vals["tempo"] = CB.tempo_agreement(r, period * FPS, self.periods) if r is not None else 0.0
        if "phase" in self.parts:
            ls = np.asarray([x for x in lines.times() if now - self.span / FPS <= x <= now - 0.1])
            self.vals["phase"] = CB.phase_agreement(self.act, ls, period, end)
        if "dbn" in self.parts and dbn is not None:
            mass = dbn.tempo_mass()
            lr = np.log2(dbn.bpms / (60.0 / period))
            near = np.abs(lr - np.round(lr)) <= math.log2(1 + DBN_AGREE)
            self.vals["dbn"] = float(mass[near].sum())


def _replay(track: C.Track, cfg: RConfig) -> RRun:
    base = cfg.base
    memory = TempoMemory(mem_s=cfg.memory.get("mem_s", 120.0)) if cfg.memory is not None else None
    dbn = OnlineDbn(track.act) if cfg.live_dbn else None
    periods = dbn.intervals.astype(float) if dbn is not None else np.arange(33, 68, dtype=float)

    def new_gate() -> LockGate | None:
        if memory is None and cfg.ratio is None:
            return None
        m = cfg.memory or {}
        return LockGate(memory=memory if m.get("related_beats", 32) else None, related_beats=m.get("related_beats", 32),
                        rel_tol=m.get("rel_tol", 0.04), ratio=cfg.ratio, act=track.act, periods=periods)

    gate = new_gate()
    est = make_estimator(cfg, track, gate)
    agree = Agreement(cfg.agree, track.act, periods) if cfg.agree is not None else None
    lines = LineLog()
    fold = Fold()
    rate = C.RateEvents(track.rate) if base.rate and base.follow else None
    queue: list[tuple[float, float]] = []
    ticks = np.arange(C.TICK_S, track.dur, C.TICK_S)
    n = ticks.size
    out = {k: np.full(n, np.nan) for k in ("out_bpm", "g0", "gp", "bpm", "conf", "fold", "strict", "shown")}
    running = np.zeros(n, bool)
    coasting = np.zeros(n, bool)
    prior_on = np.zeros(n, bool)
    events: list = []
    beats = list(track.beats) if dbn is None else []
    i = blk = 0
    silent_s, gated, last_out, was_running = 0.0, False, None, False
    fed: list[float] = []
    causal = bool(base.weak is not None and base.weak.get("causal"))
    mode = (cfg.memory or {}).get("mode", "hold")
    confident_before = False
    refused = {"memory": 0, "ratio": 0, "follower_stops": 0}
    had_bpm = False
    for k, now in enumerate(ticks):
        while blk < track.rms.size and (blk + 1) * C.BLOCK_S <= now:
            was = silent_s >= SILENCE_S
            silent_s = silent_s + C.BLOCK_S if track.rms[blk] < C.SILENCE_RMS else 0.0
            if silent_s >= SILENCE_S and not was:
                if gate is not None:
                    refused["memory"] += gate.refused_memory
                    refused["ratio"] += gate.refused_ratio
                gate = new_gate()
                est, fed = make_estimator(cfg, track, gate), []
                lines.clear()
                queue.clear()
                events.append((float(now), "silence"))
            blk += 1
        silent = silent_s >= SILENCE_S
        if dbn is not None:
            beats.extend(dbn.advance(int(round(now * FPS))))
        jumps0, resyncs0 = est.jumps, est.resyncs
        while i < len(beats) and (beats[i] <= now if not causal else ready(beats[i] + STRONG_HALF / FPS, now)):
            b = float(beats[i])
            i += 1
            if silent:
                continue
            while queue and queue[0][0] <= b:
                t_ev, r = queue.pop(0)
                est.propose_ratio(r, t_ev, since=t_ev - LRATE.DETECT_LAG_S)
            jumps, rs = est.jumps, est.resyncs
            est.update(b)
            if base.fast_jump is not None:
                C.fast_jump(est, b, base.fast_jump)
            if base.clear_lines and (est.jumps != jumps or est.resyncs != rs):
                lines.clear()
            fed.append(b)
        jumped = est.jumps != jumps0
        est.coast(now)
        if rate is not None:
            active = not silent and est.kf is not None and est.mode_at(now) != "stopped"
            for ev in rate.step(now, active):
                queue.append(ev)
                events.append((ev[0], f"rate {ev[1]:.4f}"))
        bpm, grid = est.bpm_at(now), est.grid()
        if had_bpm and bpm is None and not silent:
            refused["follower_stops"] += 1          # the follower itself stopped (hold_s without evidence, a miss run)
        had_bpm = bpm is not None
        period = grid[1] if grid else None
        if silent:
            conf = strict = 0.0
        else:
            lines.tick(grid if bpm is not None else None, now)
            strict = line_confidence(fed[-64:], lines, period, now)
            loose = est.confidence(fed[-64:], lines, period, now)
            conf = strict if gated or not was_running else loose
        g_conf = conf
        if agree is not None and not silent:
            g_conf = agree.value(now, conf, grid, lines, dbn)
        if agree is not None and cfg.agree.get("shown") and not silent:
            lines_t = np.asarray(lines.times())
            out["shown"][k] = GR.graded(GR.soft_f(fed, lines_t, period, now) if period else 0.0, GR.jitter(fed),
                                        agree.vals["tempo"], agree.vals["dbn"])
        if PROBE is not None and not silent:
            PROBE(k, float(now), {"conf": g_conf, "strict": strict, "loose": loose, "fed": fed, "lines": lines,
                                  "period": period, "bpm": bpm, "agree": agree, "act": track.act, "dbn": dbn})
        threshold = cfg.agree.get("threshold", base.threshold) if agree is not None else base.threshold
        if threshold > 0:
            start = min(threshold + HYSTERESIS, 1.0) if agree is not None or base.start is None else base.start
            would = g_conf < start if gated else g_conf < threshold
            keep = (cfg.coast is not None and was_running and not silent and bpm is not None
                    and not (jumped and cfg.coast.get("on_jump", "follow") == "gate")
                    and g_conf >= cfg.coast.get("floor", 0.0))
            gated = False if keep else would
        if cfg.skip is not None:
            if est.reopen >= cfg.skip.get("reopen", 2) and bpm is not None:
                gated = False                       # the new grid has fitted `reopen` beats: no wait for the confidence
                if g_conf >= threshold:
                    est.reopen = -1
            if est.suspect:
                gated = True
        fold.step(bpm, est.jumps, None, float(now))
        run = bpm is not None and not gated and not silent
        if run:
            fbpm, fgrid = C._folded(fold, bpm, grid, last_out)
            if fgrid is not None:
                last_out = fgrid
                out["g0"][k], out["gp"][k] = fgrid
            out["out_bpm"][k] = fbpm
        running[k] = run
        coasting[k] = run and g_conf < threshold
        was_running = run
        confident = run and strict >= base.threshold and est.kf is not None and not jumped
        if memory is not None and confident:
            memory.remember(bpm, float(now))
        if dbn is not None and memory is not None and mode != "none":
            mem = memory.active(float(now))
            m = cfg.memory
            if mem is not None and not confident:
                w = tempo_prior(dbn.bpms, mem, m.get("width", 0.03), m.get("floor", 0.1))
                if mode == "hold":
                    dbn.set_prior(w)
                elif confident_before or jumped:
                    (dbn.reweight if mode == "once" else dbn.reseed)(w)
                prior_on[k] = True
            elif mode == "hold":
                dbn.set_prior(None)
        confident_before = confident
        out["bpm"][k] = bpm if bpm is not None else np.nan
        out["conf"][k] = g_conf
        out["strict"][k] = strict
        out["fold"][k] = fold.factor
    if gate is not None:
        refused["memory"] += gate.refused_memory
        refused["ratio"] += gate.refused_ratio
    events.extend((float(t), str(e)) for t, e in getattr(est, "log", []))
    return RRun(ticks, running, out["out_bpm"], out["g0"], out["gp"], out["bpm"], out["conf"], out["fold"],
                sorted(events), coasting=coasting, strict=out["strict"], prior_on=prior_on, shown=out["shown"], refused=refused)
