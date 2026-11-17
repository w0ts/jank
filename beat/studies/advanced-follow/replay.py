"""Replay of the live pipeline's decision layer from saved tracker beats, on a fixed clock.

Equal to live.engine.Engine for what Link gets (silence aside): the tracker's beats go into the estimator as they arrive;
on every tick (TICK_S, the runner polls status() every 10 ms) the confidence (live.confidence.grid_confidence over the
tracker's beats against the estimator's grid) and the LOW CONFIDENCE gate with its hysteresis decide whether the outputs
run. Per reference beat: the published grid at that moment extrapolated to it (signed ms), or "stopped".
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from eval.mode_bpm import ModeBpm
from live.confidence import grid_confidence
from live.engine import HYSTERESIS

TICK_S = 0.05


@dataclass
class Trace:
    t: np.ndarray            # ticks
    bpm: np.ndarray          # the estimator's BPM (NaN: none / stopped)
    running: np.ndarray      # outputs run (estimator not stopped, gate open)
    conf: np.ndarray
    gated: np.ndarray
    g0: np.ndarray           # the grid: a beat time and the period (NaN: none)
    gp: np.ndarray
    fit_n: np.ndarray
    misses: np.ndarray
    jumps: np.ndarray
    est: object              # the estimator at the end


def run(beats: np.ndarray, end: float, factory=ModeBpm, threshold: float = 0.8, tick: float = TICK_S,
        on_tick=None, conf_kind: str = "grid") -> Trace:
    """on_tick(est, now, fed) is called before each tick's status (the detector's hook: it may replace the estimator by
    returning a new one, or return None). conf_kind: 'grid' (today: live.confidence) or 'lines' (live.follow: against
    the lines published)."""
    from live.follow import LineLog, line_confidence
    lines = LineLog()
    est = factory()
    ticks = np.arange(tick, end, tick)
    n = ticks.size
    out = {k: np.full(n, np.nan) for k in ("bpm", "conf", "g0", "gp", "fit_n", "misses", "jumps")}
    running, gated_a = np.zeros(n, bool), np.zeros(n, bool)
    i, gated = 0, False
    fed: list[float] = []
    for k, now in enumerate(ticks):
        while i < beats.size and beats[i] <= now:
            est.update(float(beats[i]))
            fed.append(float(beats[i]))
            i += 1
        if on_tick is not None:
            new = on_tick(est, now, fed)
            if new is not None:
                est = new
                lines.clear()
        bpm, grid = est.bpm_at(now), est.grid()
        if conf_kind == "lines":
            lines.tick(grid if bpm is not None else None, now)
            conf = line_confidence(fed[-64:], lines, grid[1] if grid else None, now)
        else:
            conf = grid_confidence(fed[-64:], grid, now)
        if threshold > 0:
            gated = conf < min(threshold + HYSTERESIS, 1.0) if gated else conf < threshold
        out["conf"][k] = conf
        gated_a[k] = gated
        running[k] = bpm is not None and not gated
        if bpm is not None:
            out["bpm"][k] = bpm
        if grid is not None:
            out["g0"][k], out["gp"][k] = grid
        out["fit_n"][k] = est.fit.n
        out["misses"][k] = est.misses
        out["jumps"][k] = getattr(est, "jumps", 0)
    return Trace(ticks, out["bpm"], running, out["conf"], gated_a, out["g0"], out["gp"], out["fit_n"], out["misses"],
                 out["jumps"], est)


def ref_errors(tr: Trace, ref: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Per reference beat: (signed ms to the published grid's nearest line, running?, published period). The grid is
    the one of the last tick at or before the beat."""
    idx = np.searchsorted(tr.t, ref, side="right") - 1
    ok = idx >= 0
    j = np.where(ok, idx, 0)
    g0, gp, run = tr.g0[j], tr.gp[j], tr.running[j] & ok
    with np.errstate(invalid="ignore"):
        err = (g0 + np.round((ref - g0) / gp) * gp - ref) * 1000.0
    err[~ok | np.isnan(gp)] = np.nan
    return err, run, np.where(ok, gp, np.nan)
