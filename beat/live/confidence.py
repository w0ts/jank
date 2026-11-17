"""How much to trust the published tempo right now: do the tracker's recent beats agree with the estimator's grid?

Over the last WINDOW_S seconds (minus the tracker's own delay), the grid predicts beat slots. A slot is hit when a
tracker beat lies within TOL of a period from it; a tracker beat near no slot is an extra. Confidence is the F-measure
of hits (recall: missed beats lower it) against extras (precision: off-grid beats lower it), 0..1. A break with no
beats, a tempo the grid no longer fits, or a tracker that wanders all bring it down within a few seconds."""
from __future__ import annotations

import math
from bisect import bisect_left

import numpy as np

WINDOW_S = 8.0
DELAY_S = 0.1
TOL = 0.1          # of a period


def grid_confidence(beats: list[float], grid: tuple[float, float] | None, now: float,
                    window_s: float = WINDOW_S, delay_s: float = DELAY_S, tol: float = TOL) -> float:
    """`beats`: the tracker's beat times, ascending (stream s). `grid`: (time of a beat, period) or None."""
    if grid is None or grid[1] <= 0:
        return 0.0
    t0, period = grid
    lo, hi = now - window_s, now - delay_s
    if hi <= lo:
        return 0.0
    slots = t0 + period * np.arange(math.ceil((lo - t0) / period), math.floor((hi - t0) / period) + 1)
    if len(slots) == 0:
        return 0.0
    margin = tol * period
    recent = np.asarray(beats[bisect_left(beats, lo - margin):bisect_left(beats, hi + margin)], dtype=np.float64)
    if len(recent) == 0:
        return 0.0
    dist = np.abs(recent[:, None] - slots[None, :])          # beats x slots
    hits = int(np.sum(dist.min(axis=0) <= margin))
    on_grid = int(np.sum(dist.min(axis=1) <= margin))
    recall, precision = hits / len(slots), on_grid / len(recent)
    return 0.0 if recall + precision == 0 else 2 * recall * precision / (recall + precision)
