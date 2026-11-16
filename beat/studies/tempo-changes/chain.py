"""The live decision chain replayed from a cached track (extract_live.py), with what Link / MIDI get: the folded output.

Per 50 ms tick, as live.engine.Engine (+ live.follow's follow engine for M2i):
  silence   RMS under 1e-3 for 0.5 s (0.1 s blocks): the estimator starts over; the fold's memory stays
  beats     the live tracker's beats (online DBN, cached: no DBN reset happens in these modes) go to the estimator
  rate      (M2i, rate on) the spectral rate's estimates while the probe is active (the follower glides); the probe's
            history restarts when it goes active, so an estimate counts only once a full 4.5 s window has run since;
            the event rule (live.rate: CONFIRM in a row past EVENT_TH within AGREE, then REFRACTORY_S) is re-run here
            and each event is proposed before the next beat (FollowBpm.propose_ratio: the beat must back it)
  gate      the confidence (today: live.confidence against the grid; M2i: live.follow's line confidence; weak: see
            weak.py) under `threshold` stops the outputs, back at threshold + HYSTERESIS (or at `start`: a sticky gate,
            hard to open, slow to close)
  fold      AUTO RANGE (live.fold.Fold, no Link session: the last output tempo is the range's reference)

Config.consts sets module constants of eval.mode_bpm / eval.locked_bpm / eval.follow_bpm / live.rate for a run (a
sweep); they are process-wide, so a worker applies them before its replays and restores them after.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np

import eval.follow_bpm as FB
import eval.locked_bpm as LB
import eval.mode_bpm as MB
import live.rate as LRATE
from eval.follow_bpm import FollowBpm
from eval.mode_bpm import ModeBpm
from live.confidence import grid_confidence
from live.engine import HYSTERESIS, SILENCE_RMS, SILENCE_S
from live.fold import Fold
from live.follow import LineLog, line_confidence

TICK_S = 0.05
BLOCK_S = 0.1
FPS = 100
PROBE_WARM_S = (LRATE.REF_N + LRATE.CUR_N) * LRATE.BLOCK / 100.0
MODULES = (MB, LB, FB, LRATE)
ADAPT_BEATS = 32
ADAPT_MIN = 16


@dataclass(frozen=True)
class Config:
    name: str
    follow: bool = False            # FollowBpm (M2i) instead of ModeBpm (today)
    conf: str = "grid"              # grid (today) / lines (M2i) / weak (weak.py)
    rate: bool = False
    threshold: float = 0.8
    start: float | None = None      # the gate opens again at this confidence (None: threshold + HYSTERESIS, today)
    sticky: bool = False            # FollowBpm.sticky always on: no octave / 2:3 jump off a trusted grid, twice the misses
    est_kw: dict = field(default_factory=dict)      # estimator attributes (hold_s, change_rel, change_beats, ...)
    consts: dict = field(default_factory=dict)      # module constants, {"MODULE.NAME": value}
    weak: dict | None = None        # weak.py's parameters (None: off)
    fast_jump: float | None = None  # FollowBpm: the last LOCK_BEATS beats on one consistent grid this far (relative) from
                                    # the follower's tempo, not an octave of it: jump at once (clear evidence of a change)
    clear_lines: bool = False       # a jump of the estimator clears the published lines the confidence is judged on
    adaptive_r: bool = False        # FollowBpm: the Kalman's beat jitter R_S from the tracker's own recent innovations
                                    # (1.4826 x their median |e| over ADAPT_BEATS, at least R_S): a jittery tracker no
                                    # longer reads as a tempo manoeuvre


@dataclass
class Track:
    beats: np.ndarray
    rms: np.ndarray
    rate: np.ndarray                # (n, 3) t, ratio, peak
    act: np.ndarray
    flux: np.ndarray
    dur: float

    @classmethod
    def load(cls, path) -> "Track":
        z = np.load(path)
        import json
        meta = json.loads(str(z["meta"]))
        return cls(z["beats"], z["rms"], z["rate"].reshape(-1, 3), z["act"], z["flux"], float(meta["dur"]))


@dataclass
class Run:
    t: np.ndarray
    running: np.ndarray
    out_bpm: np.ndarray             # what Link gets (folded), NaN when stopped
    g0: np.ndarray                  # the folded output grid (a beat, the period), NaN when none
    gp: np.ndarray
    bpm: np.ndarray                 # the estimator's own tempo (NaN: none)
    conf: np.ndarray
    fold: np.ndarray
    events: list = field(default_factory=list)      # (t, what)


class consts_applied:
    """Context: Config.consts set on their modules, restored after."""

    def __init__(self, consts: dict) -> None:
        self.consts, self.saved = consts, []

    def __enter__(self):
        mods = {m.__name__.split(".")[-1]: m for m in MODULES}
        for key, v in self.consts.items():
            mod, name = key.split(".")
            m = mods[mod]
            if not hasattr(m, name):
                raise KeyError(f"{key}: no such constant")
            self.saved.append((m, name, getattr(m, name)))
            setattr(m, name, v)
        return self

    def __exit__(self, *exc):
        for m, name, v in reversed(self.saved):
            setattr(m, name, v)
        return False


class RateEvents:
    """live.rate.RateProbe's event rule re-run on cached estimates, with the probe's activity and refractory."""

    def __init__(self, est: np.ndarray) -> None:
        self.est = est
        self.i = 0
        self.active_since: float | None = None
        self.run: list[float] = []
        self.last = -math.inf

    def step(self, now: float, active: bool) -> list[tuple[float, float]]:
        if not active:
            self.active_since, self.run = None, []
        elif self.active_since is None:
            self.active_since = now
            self.last = -math.inf
        out = []
        while self.i < len(self.est) and self.est[self.i, 0] <= now:
            t, r, _ = self.est[self.i]
            self.i += 1
            if self.active_since is None or t < math.ceil(self.active_since / BLOCK_S - 1e-9) * BLOCK_S + PROBE_WARM_S:
                continue
            if t - self.last < LRATE.REFRACTORY_S:
                continue
            if abs(r - 1) > LRATE.EVENT_TH and (not self.run or abs(r / self.run[-1] - 1) < LRATE.AGREE):
                self.run.append(r)
            else:
                self.run = [r] if abs(r - 1) > LRATE.EVENT_TH else []
            if len(self.run) >= LRATE.CONFIRM:
                out.append((float(t), float(np.median(self.run))))
                self.last, self.run = t, []
        return out


def make_estimator(cfg: Config, track: Track):
    if cfg.weak is not None:
        from weak import CoastBpm
        est = CoastBpm(track=track, params=cfg.weak)
    elif cfg.follow:
        est = FollowBpm()
    else:
        est = ModeBpm()
    for k, v in cfg.est_kw.items():
        if not hasattr(est, k):
            raise KeyError(f"estimator has no {k}")
        setattr(est, k, v)
    if isinstance(est, FollowBpm):
        est.sticky = cfg.sticky
    return est


def adapt_r(est, b: float, innov: list[float], r0: float) -> float:
    """The beat jitter for the follower's next update: the recent innovations' robust deviation (at least r0). The
    innovation of b against the glide's prediction (drift left out) is recorded when within the outlier gate."""
    kf = est.kf
    if kf is None:
        innov.clear()
        return r0
    n = round((b - kf.t) / kf.p)
    if n >= 1:
        e = b - (kf.t + n * kf.p)
        if abs(e) <= est.outlier * kf.p:
            innov.append(abs(e))
            del innov[:-ADAPT_BEATS]
    if len(innov) < ADAPT_MIN:
        return r0
    return max(r0, 1.4826 * float(np.median(innov)))


def fast_jump(est, t: float, rel: float) -> bool:
    """The follower's grid left at once when the last LOCK_BEATS beats sit on a consistent grid of their own whose tempo
    is more than rel away and not an octave (an octave is the fold's business). Returns whether it jumped."""
    if not isinstance(est, FollowBpm) or est.kf is None or not est._recent_consistent():
        return False
    sp = LB.short_period(list(est.recent)[-MB.LOCK_BEATS:])
    if sp is None:
        return False
    r = est.kf.p / sp
    lr = math.log2(r)
    if abs(r - 1) <= rel or (round(lr) != 0 and abs(lr - round(lr)) <= math.log2(1.03)):
        return False
    before = est.jumps
    est._jump(t, sp)
    return est.jumps != before


def _folded(fold: Fold, bpm, grid, last_out):
    """live.engine.Engine._folded without a Link session."""
    if fold.exp == 0 or grid is None:
        return bpm, grid
    out = None
    if last_out is not None and not fold.anchored and fold.exp < 0:
        t0, period = last_out
        out = ((grid[0] - t0) / period, period)
    return (bpm * fold.factor if bpm else bpm), fold.grid(grid[0], grid[1], out)


def replay(track: Track, cfg: Config) -> Run:
    with consts_applied(cfg.consts):
        return _replay(track, cfg)


def _replay(track: Track, cfg: Config) -> Run:
    est = make_estimator(cfg, track)
    lines = LineLog()
    fold = Fold()
    rate = RateEvents(track.rate) if cfg.rate and cfg.follow else None
    queue: list[tuple[float, float]] = []
    ticks = np.arange(TICK_S, track.dur, TICK_S)
    n = ticks.size
    out = {k: np.full(n, np.nan) for k in ("out_bpm", "g0", "gp", "bpm", "conf", "fold")}
    running = np.zeros(n, bool)
    events: list = []
    beats = track.beats
    i = blk = 0
    silent_s, gated, last_out, was_running = 0.0, False, None, False
    fed: list[float] = []
    innov: list[float] = []
    r0 = FB.R_S
    for k, now in enumerate(ticks):
        while blk < track.rms.size and (blk + 1) * BLOCK_S <= now:
            was = silent_s >= SILENCE_S
            silent_s = silent_s + BLOCK_S if track.rms[blk] < SILENCE_RMS else 0.0
            if silent_s >= SILENCE_S and not was:
                est, fed = make_estimator(cfg, track), []
                innov.clear()
                lines.clear()
                queue.clear()
                events.append((float(now), "silence"))
            blk += 1
        silent = silent_s >= SILENCE_S
        while i < beats.size and beats[i] <= now:
            b = float(beats[i])
            i += 1
            if silent:
                continue
            if isinstance(est, FollowBpm):
                while queue and queue[0][0] <= b:
                    t_ev, r = queue.pop(0)
                    est.propose_ratio(r, t_ev, since=t_ev - LRATE.DETECT_LAG_S)
            jumps = getattr(est, "jumps", 0)
            if cfg.adaptive_r and isinstance(est, FollowBpm):
                FB.R_S = adapt_r(est, b, innov, r0)
            est.update(b)
            FB.R_S = r0
            if cfg.fast_jump is not None:
                fast_jump(est, b, cfg.fast_jump)
            if cfg.clear_lines and getattr(est, "jumps", 0) != jumps:
                lines.clear()
            fed.append(b)
        if hasattr(est, "coast"):
            est.coast(now)
        if rate is not None:
            active = (not silent and isinstance(est, FollowBpm) and est.kf is not None
                      and est.mode_at(now) != "stopped")
            for ev in rate.step(now, active):
                queue.append(ev)
                events.append((ev[0], f"rate {ev[1]:.4f}"))
        bpm, grid = est.bpm_at(now), est.grid()
        if silent:
            conf = 0.0
        elif cfg.conf == "lines":
            lines.tick(grid if bpm is not None else None, now)
            conf = line_confidence(fed[-64:], lines, grid[1] if grid else None, now)
        elif cfg.conf == "weak":
            # hard to open, slow to close: the gate opens on the strict line confidence and, once open, is held by
            # the weak one (unobserved lines are no evidence; only strong beats off the lines are)
            lines.tick(grid if bpm is not None else None, now)
            period = grid[1] if grid else None
            loose = est.confidence(fed[-64:], lines, period, now)
            conf = line_confidence(fed[-64:], lines, period, now) if gated or not was_running else loose
        else:
            conf = grid_confidence(fed[-64:], grid, now)
        if cfg.threshold > 0:
            start = cfg.start if cfg.start is not None else min(cfg.threshold + HYSTERESIS, 1.0)
            gated = conf < start if gated else conf < cfg.threshold
        fold.step(bpm, getattr(est, "jumps", 0), None, float(now))
        run = bpm is not None and not gated and not silent
        if run:
            fbpm, fgrid = _folded(fold, bpm, grid, last_out)
            if fgrid is not None:
                last_out = fgrid
                out["g0"][k], out["gp"][k] = fgrid
            out["out_bpm"][k] = fbpm
        running[k] = run
        was_running = run
        out["bpm"][k] = bpm if bpm is not None else np.nan
        out["conf"][k] = conf
        out["fold"][k] = fold.factor
    log = getattr(est, "log", [])
    events.extend((float(t), str(e)) for t, e in log)
    return Run(ticks, running, out["out_bpm"], out["g0"], out["gp"], out["bpm"], out["conf"], out["fold"],
               sorted(events))
