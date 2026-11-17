"""Advanced beat follow (studies/advanced-follow): the pieces that sit beside live.engine, behind a flag.

LineLog / line_confidence: the confidence judged against the grid lines the outputs actually published, as time passed
them, instead of today's grid extended backwards over the window at its current tempo. The lines are recorded only while
the estimator gives a tempo (studies/advanced-follow/replay.py, jank M2i's firmware). Today's measure
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


FOLLOW_STATES = ("stopped", "lock", "glide")     # jank's status follow=: no tempo / ModeBpm's grid / the Kalman glide


def _make_follow_engine():
    """The engine class is built on first use: live.engine imports the tracker (madmom), this module stays light."""
    from collections import deque

    import live.engine as em
    from eval.follow_bpm import FollowBpm
    from eval.mode_bpm import ModeBpm
    from live.rate import DETECT_LAG_S, RateTap

    class _LiveFollow(FollowBpm):
        """FollowBpm fed the rate's events in time: an event decided at t is proposed before the first beat at or after
        t (jank: the frame's beat first, then the frame's event; the next beat judges it)."""

        def __init__(self, queue: deque) -> None:
            super().__init__()
            self.queue = queue

        def update(self, t: float) -> None:
            while self.queue and self.queue[0][0] <= t:
                t_ev, r = self.queue.popleft()
                self.propose_ratio(r, t_ev, since=t_ev - DETECT_LAG_S)
            super().update(t)

    class _FollowEngine(em.Engine):
        """live.engine.Engine with the advanced follow (the flag --advanced-follow; jank M2i): the estimator is FollowBpm
        (the Kalman glide), the confidence is judged against the published lines, and while the glide runs the spectral
        rate (live/rate.py RateTap) proposes pitch moves (`rate` False: off, as jank's `set rate off`). status() points
        live.engine's grid_confidence at this engine's line confidence for the duration of the call (one engine per
        process, as live/__main__.py and live/player.py run it)."""

        def __init__(self, *args, rate: bool = True, **kwargs) -> None:
            super().__init__(*args, **kwargs)
            self.rate_on = rate
            self.rate = RateTap()
            self._queue: deque = deque()
            self.rate_events = 0
            self.estimator = _LiveFollow(self._queue)
            self.lines = LineLog()

        def _swap(self) -> None:
            if type(self.estimator) is ModeBpm:          # silence or a resync started over with today's estimator
                self.estimator = _LiveFollow(self._queue)
                self._queue.clear()
                self.lines.clear()

        def feed(self, samples):
            with self._lock:
                est = self.estimator
                active = (self.rate_on and not self.silent and isinstance(est, FollowBpm) and est.kf is not None
                          and est.mode_at(self.tracker.time) != "stopped")
            events = self.rate.feed(samples, active)            # (numpy, outside the lock, as the tracker)
            with self._lock:
                self._queue.extend(events)
                self.rate_events += len(events)
            found = super().feed(samples)
            with self._lock:
                self._swap()
            return found

        def follow_status(self) -> dict:
            """jank's status follow= / drift_ppm= / rate_ratio= and beat's rate numbers, from this engine."""
            with self._lock:
                est, now = self.estimator, self.tracker.time
                if not isinstance(est, FollowBpm) or est.mode_at(now) == "stopped":
                    state = 0
                else:
                    state = 2 if est.kf is not None else 1
                kf = getattr(est, "kf", None)
                drift = round(float(kf.x[2]) / float(kf.x[1]) * 1e6) if kf is not None and kf.x[1] > 0 else 0
                log = getattr(est, "log", [])
                return {"follow": FOLLOW_STATES[state], "drift_ppm": drift,
                        "rate_ratio": round(getattr(est, "last_ratio", 1.0), 4), "rate": self.rate_on,
                        "rate_active": self.rate.active, "rate_events": self.rate_events,
                        "rate_taken": sum(1 for _, e in log if e == "ratio"),
                        "rate_refused": sum(1 for _, e in log if e == "ratio refused")}

        def resync(self) -> None:
            super().resync()
            with self._lock:
                self._swap()

        def _line_conf(self, beats, grid, now, *args, **kwargs) -> float:
            self.lines.tick(grid if self.estimator.bpm_at(now) is not None else None, now)
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
