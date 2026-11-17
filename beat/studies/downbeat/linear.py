"""A learned linear accent rule: the same per-position accumulators as the single-feature rule, for several features at
once, and one weight per (feature, position in the bar). For a candidate "1" at position r, the score is
sum over features f and bar positions p of w[f, p] * rel_f(r + p), rel_f = the accumulator relative to its 4-position
mean. The "1" is the best of the 4 candidates. On the device: 4 x F accumulators and 4 x 4F multiply-adds per beat.
"""
from __future__ import annotations

import numpy as np
from scipy.optimize import minimize

from rules import SKIP_BEATS, Rule, position_scores


def rel_scores(F: np.ndarray, c: np.ndarray, cols: list[int], bars: int, mode: str = "window") -> np.ndarray:
    """(n, len(cols), 4): each feature's accumulator per count position, relative to its mean over the 4."""
    s = np.stack([position_scores(F[:, j].astype(float), c, Rule("x", bars, mode)) for j in cols], axis=1)
    m = s.mean(axis=2, keepdims=True)
    return (s - m) / (np.abs(m) + 1e-6)


def rotations(rel: np.ndarray) -> np.ndarray:
    """(n, 4, F*4): row r holds rel[:, f, (r + p) % 4] for every (f, p)."""
    n, nf, _ = rel.shape
    out = np.empty((n, 4, nf * 4))
    for r in range(4):
        out[:, r, :] = rel[:, :, (r + np.arange(4)) % 4].reshape(n, nf * 4)
    return out


def train(samples: list[tuple[np.ndarray, np.ndarray]], l2: float = 1e-2) -> np.ndarray:
    """samples: (X (m, 4, D), y (m,) the true rotation). Softmax regression over the 4 rotations."""
    X = np.concatenate([s[0] for s in samples])
    y = np.concatenate([s[1] for s in samples])
    D = X.shape[2]

    def loss(w):
        z = X @ w
        z = z - z.max(axis=1, keepdims=True)
        p = np.exp(z)
        p /= p.sum(axis=1, keepdims=True)
        nll = -np.log(p[np.arange(len(y)), y] + 1e-12).mean()
        g = (np.einsum("mr,mrd->d", p, X) - X[np.arange(len(y)), y].sum(0)) / len(y)
        return nll + l2 * w @ w, g + 2 * l2 * w

    return minimize(loss, np.zeros(D), jac=True, method="L-BFGS-B").x


def track_samples(F, c, rp, cols, bars, mode="window", every=2):
    rel = rel_scores(F, c, cols, bars, mode)
    ok = (c >= SKIP_BEATS) & (rp >= 0) & ~np.isnan(rel).any(axis=(1, 2))
    idx = np.flatnonzero(ok)[::every]
    return rotations(rel[idx]), ((c[idx] - rp[idx]) % 4).astype(int)


def decide_linear(F, c, cols, bars, w, mode="window", margin=0.0) -> np.ndarray:
    rel = rel_scores(F, c, cols, bars, mode)
    ready = ~np.isnan(rel).any(axis=(1, 2))
    sc = rotations(np.nan_to_num(rel)) @ w                       # (n, 4): score of "the 1 is position r"
    best = np.where(ready, np.argmax(sc, axis=1), -1)
    if margin <= 0:
        return best
    off = np.full(len(c), -1, dtype=int)
    cur = -1
    for i in np.flatnonzero(ready):
        b = best[i]
        if cur < 0 or (b != cur and sc[i, b] - sc[i, cur] > margin):
            cur = b
        off[i] = cur
    return off
