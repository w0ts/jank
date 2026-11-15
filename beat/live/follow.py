"""Advanced beat follow (studies/advanced-follow): the pieces that sit beside live.engine, behind a flag.

LineLog / line_confidence: the confidence judged against the grid lines the outputs actually published, as time passed
them, instead of today's grid extended backwards over the window at its current tempo. Today's measure
(live.confidence.grid_confidence) assumes one constant tempo across its 8 s window: after any tempo change, even one
the grid followed exactly, the older beats in the window miss the new grid's backward extension and the confidence
falls under the LOW CONFIDENCE gate (measured: a 2 % pitch step stops the outputs in about 96 % of cases).
"""
from __future__ import annotations

import math
from bisect import bisect_left
from collections import deque

import numpy as np

from live.confidence import DELAY_S, TOL, WINDOW_S

KEEP_S = WINDOW_S + 2.0


class LineLog:
    """The published grid's lines, recorded as `now` passes them (one per beat: a line closer than half a period to
    the last recorded one is the same line, moved a little by the follower)."""

    def __init__(self) -> None:
        self._lines: deque[float] = deque()

    def tick(self, grid: tuple[float, float] | None, now: float) -> None:
        if grid is None or grid[1] <= 0:
            return
        t0, p = grid
        line = t0 + math.floor((now - t0) / p) * p          # the latest line at or before now
        new = []
        while line > (self._lines[-1] + 0.5 * p if self._lines else now - KEEP_S) and line <= now:
            new.append(line)
            line -= p
        self._lines.extend(reversed(new))
        while self._lines and self._lines[0] < now - KEEP_S:
            self._lines.popleft()

    def times(self) -> list[float]:
        return list(self._lines)

    def clear(self) -> None:
        self._lines.clear()


def _make_follow_engine():
    """The engine class is built on first use: live.engine imports the tracker (madmom), this module stays light."""
    import live.engine as em
    from eval.follow_bpm import FollowBpm
    from eval.mode_bpm import ModeBpm

    class _FollowEngine(em.Engine):
        """live.engine.Engine with the quick win (the flag --advanced-follow): the estimator is FollowBpm (the Kalman
        glide) and the confidence is judged against the published lines. Prototype: status() points live.engine's
        grid_confidence at this engine's line confidence for the duration of the call (one engine per process, as
        live/__main__.py and live/player.py run it); stage 2 gives Engine a proper hook."""

        def __init__(self, *args, **kwargs) -> None:
            super().__init__(*args, **kwargs)
            self.estimator = FollowBpm()
            self.lines = LineLog()

        def _swap(self) -> None:
            if type(self.estimator) is ModeBpm:          # silence or a resync started over with today's estimator
                self.estimator = FollowBpm()
                self.lines.clear()

        def feed(self, samples):
            found = super().feed(samples)
            with self._lock:
                self._swap()
            return found

        def resync(self) -> None:
            super().resync()
            with self._lock:
                self._swap()

        def _line_conf(self, beats, grid, now, *args, **kwargs) -> float:
            self.lines.tick(grid, now)
            return line_confidence(beats, self.lines, grid[1] if grid else None, now)

        def status(self):
            orig = em.grid_confidence
            em.grid_confidence = self._line_conf
            try:
                return super().status()
            finally:
                em.grid_confidence = orig

    return _FollowEngine


def follow_engine(*args, **kwargs):
    """A FollowEngine (see _make_follow_engine), constructed like live.engine.Engine."""
    return _make_follow_engine()(*args, **kwargs)


def line_confidence(beats: list[float], log: LineLog, period: float | None, now: float,
                    window_s: float = WINDOW_S, delay_s: float = DELAY_S, tol: float = TOL) -> float:
    """F-measure of the tracker's beats against the published lines over the window (as grid_confidence: a line is hit
    by a beat within tol of a period, a beat near no line is an extra)."""
    if not period or period <= 0:
        return 0.0
    lo, hi = now - window_s, now - delay_s
    lines = np.asarray([x for x in log.times() if lo <= x <= hi])
    if lines.size == 0:
        return 0.0
    margin = tol * period
    recent = np.asarray(beats[bisect_left(beats, lo - margin):bisect_left(beats, hi + margin)], dtype=np.float64)
    if recent.size == 0:
        return 0.0
    dist = np.abs(recent[:, None] - lines[None, :])
    hits = int(np.sum(dist.min(axis=0) <= margin))
    on = int(np.sum(dist.min(axis=1) <= margin))
    recall, precision = hits / lines.size, on / recent.size
    return 0.0 if recall + precision == 0 else 2 * recall * precision / (recall + precision)
