"""The graded confidence: how good the beat is right now, shown (CF bar, HOME, status) beside the unchanged gate.

The gate's value (the mean of the lines' F-measure and the agreement parts) is ~1 whenever locked: a wall, then a cliff. The
shown value is the mean of four parts that move with the beat's real quality (studies/follow-robust/results/confidence.md):
  soft   F of the tracker's beats against the published lines, a triangular kernel (1 on the line, 0 half a period off)
  jit    1 - clip(cv of the last 16 beat intervals / 0.15): how even the beats are
  tempo  the activation's comb at the follower's period over the best comb of the DBN's periods (the agreement part)
  dbn    the DBN's tempo mass near the follower's tempo (the agreement part)
Test AUC for "the output's tempo is right": 0.947 (the gate's value: 0.898). It does not gate anything.
"""
from __future__ import annotations

import numpy as np

WINDOW_S, DELAY_S = 8.0, 0.1
JIT_CV = 0.15


def soft_f(beats: list[float], lines: np.ndarray, period: float, now: float) -> float:
    lo, hi = now - WINDOW_S, now - DELAY_S
    ls = lines[(lines >= lo) & (lines <= hi)]
    b = np.asarray([x for x in beats[-64:] if lo - period <= x <= hi + period])
    if ls.size == 0 or b.size == 0:
        return 0.0
    d = np.abs(b[:, None] - ls[None, :])

    def kern(x: np.ndarray) -> np.ndarray:
        return np.clip(1.0 - x / (0.5 * period), 0.0, 1.0)

    recall = float(kern(d.min(axis=0)).mean())
    precision = float(kern(d.min(axis=1)).mean())
    return 0.0 if recall + precision == 0 else 2 * recall * precision / (recall + precision)


def jitter(beats: list[float]) -> float:
    if len(beats) < 6:
        return 0.0
    ibi = np.diff(np.asarray(beats[-17:]))
    m = float(np.median(ibi))
    if m <= 0:
        return 0.0
    return float(1.0 - min(1.0, (float(np.std(ibi)) / m) / JIT_CV))


def graded(soft: float, jit: float, tempo: float, dbn: float) -> float:
    return float(np.clip((soft + jit + tempo + dbn) / 4.0, 0.0, 1.0))
