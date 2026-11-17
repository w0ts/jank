"""Link's own beats, simulated from the tracked beats the way live.engine + live.controller make them: each tracked beat
updates ModeBpm; until the next tracked beat, Link plays the estimator's grid (a beat at tb, then every p). Link's
beat count goes up by one per beat it plays, so a single off-grid tracked beat moves nothing; a grid that jumps moves
Link to its nearest beat (the count then follows the gap, rounded). While the estimator says "stopped" Link plays no
beats. The bar must line up with THESE beats: Link's bar is 4 of them.
"""
from __future__ import annotations

import numpy as np


def link_beats(tracked: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """(times, counts) of the beats Link plays, in stream seconds."""
    from eval.mode_bpm import ModeBpm
    est = ModeBpm()
    times: list[float] = []
    counts: list[int] = []
    tracked = np.asarray(tracked, float)
    for i, t in enumerate(tracked):
        est.update(float(t))
        if est.mode_at(float(t)) == "stopped":
            continue
        g = est.grid()
        if g is None:
            continue
        tb, p = g
        end = tracked[i + 1] if i + 1 < len(tracked) else t + p
        k0 = int(np.ceil((t - tb) / p - 1e-9))
        k = k0
        while tb + k * p < end:
            b = tb + k * p
            if not times:
                times.append(b)
                counts.append(0)
            elif b > times[-1] + 0.5 * p:
                counts.append(counts[-1] + max(1, int(np.floor((b - times[-1]) / p + 0.5))))
                times.append(b)
            k += 1
    return np.asarray(times), np.asarray(counts, int)
