"""The bar-phase filter: which beat count (mod 4) is the "1", from per-beat evidence, beat by beat.

State: a probability per phase phi in 0..3 ("the beats whose count mod 4 is phi are the 1"). Each beat j brings the
per-beat model's log-probabilities of its bar position, lp_j (4); under phi, beat j sits at position (c_j - phi) mod 4.

    post[phi] *= exp(T * lp_j[(c_j - phi) mod 4]);  normalise;  post = (1 - eps) post + eps / 4

T < 1 because neighbouring beats' evidence is correlated; eps is the chance per beat that the bar slipped (a tracker
glitch, an odd bar, a track change): it sets how fast old evidence is forgotten. Evidence for beat j arrives `delay`
beats late (the descriptors look a little ahead), so the decision at beat i uses beats <= i - delay.

Decisions (what the "1" is at beat i, from beats before it):
- estimator: the current choice moves only when the best phase's probability beats the current one by `margin`;
- link: Link's bar is left alone until the best phase is >= `commit`; it then moves to it, and later moves again only
  when another phase has been >= `switch` for `hold` beats in a row (whole beats via the controller's offset).
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numba import njit


@dataclass(frozen=True)
class Filter:
    temp: float = 0.3
    eps: float = 0.01
    delay: int = 4
    margin: float = 0.3       # estimator hysteresis, in probability
    commit: float = 0.9       # link: first move
    switch: float = 0.95      # link: later moves
    hold: int = 8             # link: beats the new phase must stay above `switch`

    @property
    def label(self) -> str:
        return f"T={self.temp:g} eps={self.eps:g} margin={self.margin:g}"


@njit(cache=True)
def _posteriors(lp, c, temp, eps, delay, glitch):
    n = len(c)
    out = np.empty((n, 4))
    post = np.full(4, 0.25)
    for i in range(n):
        out[i] = post
        j = i - delay + 1            # the evidence that becomes ready at the end of beat i (used from beat i+1 on)
        if j >= 0 and j < n and np.isfinite(lp[j, 0]):
            s = 0.0
            for phi in range(4):
                post[phi] *= np.exp(temp * lp[j, (c[j] - phi) % 4])
                s += post[phi]
            post /= s
        e = 0.5 if glitch[i] else eps
        post = (1 - e) * post + e / 4
    return out


def posteriors(lp: np.ndarray, c: np.ndarray, f: Filter, glitch: np.ndarray | None = None) -> np.ndarray:
    """(n, 4): the phase probabilities known at beat i (before beat i's own evidence). glitch[i] = True raises the
    slip chance at that beat to 0.5 (e.g. the tracker restarted)."""
    g = np.zeros(len(c), np.bool_) if glitch is None else glitch.astype(np.bool_)
    return _posteriors(np.asarray(lp, np.float64), np.asarray(c, np.int64), f.temp, f.eps, f.delay, g)


@njit(cache=True)
def _estimator(post, margin):
    n = len(post)
    off = np.full(n, -1)
    cur = -1
    for i in range(n):
        b = np.argmax(post[i])
        if post[i, b] > 0.25 + 1e-9 and (cur < 0 or (b != cur and post[i, b] - post[i, cur] > margin)):
            cur = b
        off[i] = cur
    return off


def decide_estimator(post: np.ndarray, f: Filter) -> np.ndarray:
    return _estimator(post, f.margin)


@njit(cache=True)
def _link(post, commit, switch, hold):
    n = len(post)
    off = np.full(n, -1)
    moved = np.zeros(n, np.bool_)
    cur, cand, run = -1, -1, 0
    for i in range(n):
        b = np.argmax(post[i])
        if cur < 0:
            if post[i, b] >= commit:
                cur = b
                moved[i] = True
        elif b != cur and post[i, b] >= switch:
            run = run + 1 if b == cand else 1
            cand = b
            if run >= hold:
                cur, run = b, 0
                moved[i] = True
        else:
            cand, run = -1, 0
        off[i] = cur
    return off, moved


def decide_link(post: np.ndarray, f: Filter) -> tuple[np.ndarray, np.ndarray]:
    """off[i] = Link's '1' (-1 = Link's arbitrary bar, never moved yet); moved[i] = Link's bar moved at beat i."""
    return _link(post, f.commit, f.switch, f.hold)
