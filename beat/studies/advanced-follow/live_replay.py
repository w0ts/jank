"""The whole live decision chain replayed from a cached item (extract.py): activation -> online DBN -> estimator ->
confidence gate, with the silence reset, and for 'advanced' the "same music?" detector, on a 50 ms clock.

  today     ModeBpm, live.confidence (today's engine)
  quick     FollowBpm (Kalman glide) + the confidence against the published lines (live.follow)
  advanced  quick + live.samemusic: SAME (harmony continuous) makes the follower sticky; DIFFERENT (novel sound AND
            the beats not fitting the grid) resets the follower, the DBN and the memory at once (a fast re-learn)

The DBN runs inside the loop (madmom's online DBN, 90-180 BPM, as live.tracker), so a reset restarts it as the device
would.
"""
from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from eval.follow_bpm import FollowBpm
from eval.mode_bpm import ModeBpm
from live.confidence import grid_confidence
from live.engine import HYSTERESIS
from live.follow import LineLog, line_confidence
from live.samemusic import SameMusic

TICK_S = 0.05
FPS = 100
BLOCK_S = 0.1
SILENT_LOG = 1e-3        # an L10 row under this everywhere: digital silence (the engine: -60 dBFS for 0.5 s)
SILENCE_S = 0.5


@dataclass
class Policy:
    """The detector's decision on top of its novelty (tuned on the dev half)."""
    chroma_same: float = 0.08     # harmony this close to the memory: SAME (sticky)
    timbre_diff: float = 2.5      # a sound this far from the memory ...
    chroma_diff: float = 0.12     # ... or a harmony this far ...
    conf_low: float = 0.5         # ... while the beats fit the grid this badly ...
    low_s: float = 2.0            # ... for this long: DIFFERENT
    holdoff_s: float = 12.0       # no second reset this soon


@dataclass
class Run:
    t: np.ndarray
    bpm: np.ndarray
    running: np.ndarray
    conf: np.ndarray
    g0: np.ndarray
    gp: np.ndarray
    timbre: np.ndarray
    chroma: np.ndarray
    sticky: np.ndarray
    resets: list = field(default_factory=list)      # (time, why)
    beats: list = field(default_factory=list)


def _dbn():
    from madmom.features.beats import DBNBeatTrackingProcessor
    d = DBNBeatTrackingProcessor(fps=FPS, online=True, min_bpm=90.0, max_bpm=180.0)
    d.reset()
    return d


def run(act: np.ndarray, L10: np.ndarray, mode: str, policy: Policy | None = None, threshold: float = 0.8) -> Run:
    policy = policy or Policy()
    quick = mode in ("quick", "advanced")
    make = FollowBpm if quick else ModeBpm
    est = make()
    dbn = _dbn()
    lines = LineLog()
    sm = SameMusic() if mode == "advanced" else None
    end = act.size / FPS
    ticks = np.arange(TICK_S, end, TICK_S)
    n = ticks.size
    out = {k: np.full(n, np.nan) for k in ("bpm", "conf", "g0", "gp", "timbre", "chroma")}
    running, sticky_a = np.zeros(n, bool), np.zeros(n, bool)
    frame = block = 0
    fed: list[float] = []
    all_beats: list[float] = []
    silent_s, gated, low_s, last_reset, sticky = 0.0, False, 0.0, -1e9, False
    seg_bpm: list[float] = []
    resets = []
    nov = None
    for k, now in enumerate(ticks):
        while frame < act.size and frame / FPS <= now:
            for b in dbn.process_online(np.atleast_1d(act[frame]), reset=False):
                b = float(b)
                all_beats.append(b)
                if silent_s < SILENCE_S:
                    est.update(b)
                    fed.append(b)
            frame += 1
        while block < L10.shape[0] and (block + 1) * BLOCK_S <= now:
            row = L10[block].astype(np.float32)
            was = silent_s >= SILENCE_S
            silent_s = silent_s + BLOCK_S if float(row.max()) < SILENT_LOG else 0.0
            if silent_s >= SILENCE_S and not was:          # the engine: new music after silence, start over
                est, fed = make(), []
                lines.clear()
                if sm is not None:
                    sm.reset()
                    seg_bpm = []
                resets.append((float(now), "silence"))
            if sm is not None and silent_s < SILENCE_S:
                if sm.feed(row[None, :]):
                    b_now = est.bpm_at(now)
                    seg_bpm.append(b_now if b_now else np.nan)
                    seg_bpm = seg_bpm[-64:]
                    past = np.array(seg_bpm[:-6])
                    ref = np.nanmedian(past) if past.size and np.any(~np.isnan(past)) else np.nan
                    sm.rate = float(b_now / ref) if b_now and ref == ref and ref > 0 else 1.0
                    nov = sm.novelty()
            block += 1
        bpm, grid = est.bpm_at(now), est.grid()
        if quick:
            lines.tick(grid if bpm is not None else None, now)
            conf = line_confidence(fed[-64:], lines, grid[1] if grid else None, now)
        else:
            conf = grid_confidence(fed[-64:], grid, now)
        if sm is not None:
            sticky = nov is not None and nov["chroma"] < policy.chroma_same
            if isinstance(est, FollowBpm):
                est.sticky = sticky
            low_s = low_s + TICK_S if (conf < policy.conf_low or bpm is None) else 0.0
            novel = nov is not None and (nov["timbre"] > policy.timbre_diff or nov["chroma"] > policy.chroma_diff)
            if novel and low_s >= policy.low_s and now - last_reset >= policy.holdoff_s:
                est, fed, dbn = make(), [], _dbn()
                lines.clear()
                sm.reset()
                seg_bpm, nov, last_reset, low_s = [], None, float(now), 0.0
                resets.append((float(now), "different"))
                bpm, grid, conf = None, None, 0.0
            if nov is not None:
                out["timbre"][k], out["chroma"][k] = nov["timbre"], nov["chroma"]
        if threshold > 0:
            gated = conf < min(threshold + HYSTERESIS, 1.0) if gated else conf < threshold
        out["conf"][k] = conf
        sticky_a[k] = sticky
        running[k] = bpm is not None and not gated and silent_s < SILENCE_S
        if bpm is not None:
            out["bpm"][k] = bpm
        if grid is not None:
            out["g0"][k], out["gp"][k] = grid
    return Run(ticks, out["bpm"], running, out["conf"], out["g0"], out["gp"], out["timbre"], out["chroma"], sticky_a,
               resets, all_beats)
