"""A BPM estimator for continuous listening: the window grows while the tempo holds, restarts when it changes.

Fed one tracked beat time at a time (from any live beat tracker). Constant work per beat, a few numbers of state:
fit for an MCU as well as a computer.

  segment fit   least squares of beat time against beat number since the last tempo change: precision keeps
                improving the longer the tempo holds
  bridging      a beat's number is its distance to the last beat divided by the period, rounded: missed beats
                and whole breakdowns (no beats) are bridged; with no beats the tempo simply holds
  outliers      beats more than OUTLIER periods off the predicted grid are not fitted (fills, off-beats)
  change        a short fit over the last SHORT_BEATS beats runs alongside; when it disagrees with the segment by
                more than CHANGE_REL for CHANGE_BEATS beats in a row (or the grid keeps missing), the segment restarts
                from the recent beats
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

import numpy as np

SHORT_BEATS = 16
CHANGE_REL = 0.005      # 0.5 %: 0.7 BPM at 140 (tuned 2026-11-13: most precise steady AND fastest re-lock)
CHANGE_BEATS = 8
OUTLIER = 0.25          # of a period
MISS_RESTART = 12       # this many outliers in a row: the grid is wrong, restart


@dataclass
class _Fit:
    """Running least squares of t = t0 + period * k."""
    n: int = 0
    sk: float = 0.0
    st: float = 0.0
    skk: float = 0.0
    skt: float = 0.0

    def add(self, k: float, t: float) -> None:
        self.n += 1
        self.sk += k
        self.st += t
        self.skk += k * k
        self.skt += k * t

    def line(self) -> tuple[float, float] | None:
        """(t0, period), or None until 3 beats."""
        if self.n < 3:
            return None
        var = self.n * self.skk - self.sk * self.sk
        if var <= 0:
            return None
        period = (self.n * self.skt - self.sk * self.st) / var
        return (self.st - period * self.sk) / self.n, period


def short_period(beats: list[float]) -> float | None:
    """Period from a handful of recent beats, missed beats bridged (median interval, then least squares)."""
    if len(beats) < 4:
        return None
    b = np.asarray(beats)
    first = float(np.median(np.diff(b)))
    if first <= 0.2:
        return None
    k = np.round((b - b[-1]) / first)
    if np.unique(k).size < 4:
        return None
    return float(np.polyfit(k, b, 1)[0])


@dataclass
class LockedBpm:
    min_period: float = 0.2
    change_rel: float = CHANGE_REL
    change_beats: int = CHANGE_BEATS
    outlier: float = OUTLIER
    miss_restart: int = MISS_RESTART
    fit: _Fit = field(default_factory=_Fit)
    recent: deque = field(default_factory=lambda: deque(maxlen=SHORT_BEATS))
    k_last: float = 0.0
    t_last: float | None = None
    disagree: int = 0
    misses: int = 0
    restarts: int = 0

    def period(self) -> float | None:
        line = self.fit.line()
        return line[1] if line else short_period(list(self.recent))

    def bpm(self) -> float | None:
        p = self.period()
        return 60.0 / p if p and p > self.min_period else None

    def grid(self) -> tuple[float, float] | None:
        """(time of the last fitted beat on the line, period): the published grid is t_last_fit + period * n."""
        line = self.fit.line()
        if line is None or self.t_last is None:
            return None
        t0, period = line
        return t0 + period * self.k_last, period

    def _restart(self) -> None:
        """New segment from the recent beats."""
        self.fit = _Fit()
        self.restarts += 1
        p = short_period(list(self.recent))
        if p is None:
            self.t_last = None
            return
        last = self.recent[-1]
        for t in self.recent:
            self.fit.add(round((t - last) / p), t)
        self.k_last, self.t_last = 0.0, last
        self.disagree = self.misses = 0

    def update(self, t: float) -> None:
        self.recent.append(t)
        p = self.period()
        if self.t_last is None or p is None:
            if len(self.recent) >= 4:
                self._restart()
            return
        k = self.k_last + round((t - self.t_last) / p)
        if k == self.k_last:
            return                                   # a double detection of the same beat
        t0, period = self.fit.line() or (self.t_last - p * self.k_last, p)
        if abs(t - (t0 + period * k)) > self.outlier * period:
            self.misses += 1
            if self.misses >= self.miss_restart:
                self._restart()
            return
        self.misses = 0
        self.fit.add(k, t)
        self.k_last, self.t_last = k, t
        self._check_change(period)

    def _check_change(self, period: float) -> None:
        sp = short_period(list(self.recent))
        if sp is None:
            return
        self.disagree = self.disagree + 1 if abs(sp / period - 1) > self.change_rel else 0
        if self.disagree >= self.change_beats:
            self._restart()
