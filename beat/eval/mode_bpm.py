"""The BPM estimator with explicit modes, for transport control.

  steady        the growing constant-tempo fit of LockedBpm (precision improves the longer it holds)
  speeding_up   the tempo is moving: the published tempo is a quadratic fit's slope at the latest beat
  slowing_down
  stopped       not continuous: no steady beat to follow -> transport stop (Link stop / MIDI Stop); it starts
                again once a new segment holds LOCK_BEATS consistent beats

A jump (a cut to a new tempo) is an event, not a mode: a fresh steady segment starts from the beats after the
break. The mode is decided only when LockedBpm's change detector fires, by comparing two explanations of the last
CLASSIFY_BEATS beats: a jump (two constant tempos, best break point) and a ramp (one quadratic). A ramp ends when its
tempo move stays under RAMP_EXIT for EXIT_CHECKS beats. Stopped: no fitting beat for hold_s seconds (a long
breakdown, the music ended) or the grid kept missing (beats arrive but do not fit).
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

import numpy as np

from eval.glide_bpm import quad_tempo
from eval.locked_bpm import LockedBpm, short_period

CLASSIFY_BEATS = 32
MIN_SIDE = 8           # beats on each side of a jump's break point
RAMP_MIN_MOVE = 0.004  # a ramp must move the tempo at least this much across the window
RAMP_EXIT = 0.002
EXIT_CHECKS = 16
HOLD_S = 30.0          # hold the tempo through a beat-less stretch this long, then stop
LOCK_BEATS = 8         # consistent beats in a new segment before the transport runs again
STUCK_BEATS = 32       # stopped this many beats without a lock: reseed the grid
RAMP_MODES = ("speeding_up", "slowing_down")
RAMP_MARGIN = 0.6      # the ramp's squared error must be under 60 % of the best jump's


def _bridge(beats: np.ndarray) -> np.ndarray | None:
    """Beat numbers for beat times, missed beats bridged by rounding to the median interval."""
    first = float(np.median(np.diff(beats)))
    return np.round((beats - beats[-1]) / first) if first > 0.2 else None


def _rss(k: np.ndarray, t: np.ndarray, degree: int) -> float:
    coef = np.polyfit(k, t, degree)
    return float(np.sum((np.polyval(coef, k) - t) ** 2))


def signed_quad(beats: list[float]) -> tuple[float, float, float] | None:
    """(period at the latest beat, signed tempo change across the window: + speeding up / - slowing down,
    last beat time on the curve). The sign comes from the curve's shape, not from noisy beat-to-beat periods."""
    q = quad_tempo(beats)
    if q is None:
        return None
    period, move, last = q
    b = np.asarray(beats)
    k = _bridge(b)
    if k is None:
        return None
    c2 = np.polyfit(k, b, 2)[0]
    # period(k) = c1 + 2 c2 k with k <= 0: c2 > 0 means the period grew (slowing down)
    return period, (-move if c2 > 0 else move), last


def classify(beats: list[float]) -> tuple[str, int]:
    """('jump', index of the first beat after the break) or ('ramp', 0), from the last CLASSIFY_BEATS beats."""
    b = np.asarray(beats[-CLASSIFY_BEATS:])
    k = _bridge(b) if b.size >= 2 * MIN_SIDE else None
    if k is None or np.unique(k).size < 2 * MIN_SIDE:
        return "jump", max(0, b.size - MIN_SIDE)
    ramp = _rss(k, b, 2)
    best, split = np.inf, b.size - MIN_SIDE
    for s in range(MIN_SIDE, b.size - MIN_SIDE + 1):
        jump = _rss(k[:s], b[:s], 1) + _rss(k[s:], b[s:], 1)
        if jump < best:
            best, split = jump, s
    # a ramp only when the curve explains the beats clearly better than a jump (real tracked beats are noisy; a
    # steady track with a fill or a missed stretch is otherwise read as a ramp, 2026-11-13 diagnosis)
    return ("ramp", 0) if ramp <= RAMP_MARGIN * best else ("jump", split)


@dataclass
class ModeBpm(LockedBpm):
    hold_s: float = HOLD_S
    mode: str = "stopped"
    history: deque = field(default_factory=lambda: deque(maxlen=CLASSIFY_BEATS))
    ramp_period: float | None = None
    ramp_last: float | None = None
    exit_run: int = 0
    last_fit_time: float | None = None
    mode_log: list = field(default_factory=list)      # (time, mode) at each switch
    jumps: int = 0
    in_a_row: int = 0                                 # beats fitted one by one since the last miss or restart
    stuck: int = 0                                    # beats while stopped since the last reseed
    last_jump: tuple[int, int] | None = None          # (beat count, direction +1 faster / -1 slower)
    beats_seen: int = 0

    def __post_init__(self) -> None:
        # hold on misses: the base estimator would throw its precise long fit away after miss_restart misses;
        # here a run of misses keeps the tempo running (a fill, an erratic breakdown) unless the recent beats form
        # a new consistent grid (then it is a jump); nothing fitting for hold_s -> stopped
        self.miss_limit = self.miss_restart
        self.miss_restart = 10 ** 9

    def _recent_consistent(self) -> bool:
        """The last LOCK_BEATS beats sit on one grid of their own (a new tempo or phase), with no gaps."""
        recent = list(self.recent)[-LOCK_BEATS:]
        if len(recent) < LOCK_BEATS:
            return False
        p = short_period(recent)
        if p is None:
            return False
        b = np.asarray(recent)
        k = np.round((b - b[-1]) / p)
        if np.unique(k).size != LOCK_BEATS or k.max() - k.min() != LOCK_BEATS - 1:
            return False
        resid = b - np.polyval(np.polyfit(k, b, 1), k)
        return bool(np.max(np.abs(resid)) < 0.08 * p)

    def _set_mode(self, mode: str, now: float) -> None:
        if mode != self.mode:
            self.mode = mode
            self.mode_log.append((now, mode))

    def mode_at(self, now: float) -> str:
        """The mode at time `now`, beats or not: a beat-less stretch longer than hold_s means stopped."""
        if self.mode != "stopped" and (self.last_fit_time is None or now - self.last_fit_time > self.hold_s):
            self._set_mode("stopped", now)
        return self.mode

    def update(self, t: float) -> None:
        self.beats_seen += 1
        self.mode_at(t)
        self.history.append(t)
        if self.mode in RAMP_MODES:
            self._ramp_update(t)        # beats are judged against the ramp's curve, not the constant grid
            return
        fitted_before, misses_before, restarts_before = self.fit.n, self.misses, self.restarts
        super().update(t)
        restarted = self.restarts > restarts_before
        if self.fit.n != fitted_before:
            self.last_fit_time = t
        self.in_a_row = 0 if restarted or self.misses else self.in_a_row + (self.fit.n == fitted_before + 1)
        if self.misses >= self.miss_limit and self._recent_consistent():
            # the beats moved to a new grid: a jump (steady keeps running on it; stopped starts on it)
            self.jumps += 1
            super()._restart()
            self.in_a_row = LOCK_BEATS
            self.last_fit_time = t
        if self.mode == "stopped":
            self.stuck = 0 if restarted else self.stuck + 1
            if self.in_a_row >= LOCK_BEATS:
                self._set_mode("steady", t)
            elif self.stuck >= STUCK_BEATS:
                self.stuck = 0
                super()._restart()      # no lock after STUCK_BEATS beats: the grid is wrong, reseed

    def _ramp_update(self, t: float) -> None:
        """One beat in a ramp mode: follow the curve; leave to steady when flat, to stopped when beats stop fitting."""
        self.recent.append(t)
        if self.ramp_period and self.ramp_last is not None:
            n = round((t - self.ramp_last) / self.ramp_period)
            if n >= 1 and abs(t - (self.ramp_last + n * self.ramp_period)) > self.outlier * self.ramp_period:
                self.history.pop()               # off the curve: not fitted
                self.misses += 1
                if self.misses >= self.miss_restart:
                    self.ramp_period = None
                    self._set_mode("stopped", t)
                    super()._restart()
                return
        self.misses = 0
        self.last_fit_time = t
        q = signed_quad(list(self.history))
        if q is None:
            return
        period, change, last = q
        self.ramp_period, self.ramp_last = period, last
        reversed_ = (change > 0) != (self.mode == "speeding_up")
        if reversed_ and abs(change) >= RAMP_MIN_MOVE:
            # a ramp that turns around is not a ramp: back to a steady segment instead of flipping
            self.ramp_period = None
            self._set_mode("steady", t)
            super()._restart()
            return
        self.exit_run = self.exit_run + 1 if abs(change) < RAMP_EXIT else 0
        if self.exit_run >= EXIT_CHECKS:
            self.ramp_period = None
            self._set_mode("steady", t)
            super()._restart()

    def _check_change(self, period: float) -> None:
        if self.mode in RAMP_MODES:
            return
        sp = short_period(list(self.recent))
        if sp is None:
            return
        self.disagree = self.disagree + 1 if abs(sp / period - 1) > self.change_rel else 0
        if self.disagree >= self.change_beats:
            self.disagree = 0
            if self.mode == "stopped":
                super()._restart()      # not running yet: reseed (a wrong first grid from an intro is fixed here)
            else:
                self._classify(self.history[-1])

    def _classify(self, now: float) -> None:
        old = super().period()
        short = short_period(list(self.recent))
        direction = 1 if (short and old and short < old) else -1
        kind, split = classify(list(self.history))
        # a second change the same way soon after a jump: the first one was the start of a ramp (a steady part
        # and a ramp bend fit two straight lines as well as a curve)
        soon = self.last_jump is not None and self.beats_seen - self.last_jump[0] <= 2 * CLASSIFY_BEATS
        if soon and self.last_jump[1] == direction:
            kind = "ramp"
        if kind == "ramp":
            q = signed_quad(list(self.history))
            if q is not None and abs(q[1]) >= RAMP_MIN_MOVE and old:
                self.ramp_period, self.ramp_last, self.exit_run, self.misses = q[0], q[2], 0, 0
                self.last_jump = None
                self._set_mode("speeding_up" if q[1] > 0 else "slowing_down", now)
                return
        self.jumps += 1
        self.last_jump = (self.beats_seen, direction)
        self.recent.clear()
        self.recent.extend(list(self.history)[split:])
        super()._restart()

    def period(self) -> float | None:
        if self.mode in RAMP_MODES and self.ramp_period:
            return self.ramp_period
        return super().period()

    def bpm_at(self, now: float) -> float | None:
        """The published BPM at `now`, or None while the transport is stopped."""
        return None if self.mode_at(now) == "stopped" else self.bpm()

    def grid(self) -> tuple[float, float] | None:
        if self.mode in RAMP_MODES and self.ramp_period and self.ramp_last is not None:
            return self.ramp_last, self.ramp_period
        return super().grid()
