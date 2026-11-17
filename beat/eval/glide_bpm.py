"""LockedBpm plus a glide mode for smooth tempo changes (a DJ riding the pitch).

Alongside the growing constant-tempo fit, a quadratic fit runs over the last GLIDE_BEATS beats. When the tempo
across that window moves by more than GLIDE_ENTER for ENTER_CHECKS beats in a row, the published tempo comes from
the quadratic's slope at the latest beat (it follows the glide). When the move stays under GLIDE_EXIT for
EXIT_CHECKS beats, a fresh growing segment starts from the recent beats.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

import numpy as np

from eval.locked_bpm import LockedBpm

GLIDE_BEATS = 32
GLIDE_ENTER = 0.006     # 0.6 % tempo move across the window
GLIDE_EXIT = 0.003
ENTER_CHECKS = 4
EXIT_CHECKS = 8
OUTLIER = 0.2           # of a period, for the quadratic fit


def quad_tempo(beats: list[float]) -> tuple[float, float, float] | None:
    """(period at the latest beat, relative tempo move across the window, last beat time on the curve).

    Missed beats bridged by rounding to the median interval; one pass drops beats off the curve and refits."""
    if len(beats) < GLIDE_BEATS // 2:
        return None
    b = np.asarray(beats)
    first = float(np.median(np.diff(b)))
    if first <= 0.2:
        return None
    k = np.round((b - b[-1]) / first)
    if np.unique(k).size < GLIDE_BEATS // 2:
        return None
    coef = np.polyfit(k, b, 2)
    resid = np.abs(np.polyval(coef, k) - b)
    keep = resid <= OUTLIER * first
    if keep.sum() >= GLIDE_BEATS // 2 and not keep.all():
        coef = np.polyfit(k[keep], b[keep], 2)
    c2, c1, c0 = coef
    if c1 <= 0.2:
        return None
    move = abs(2 * c2 * k.min()) / c1
    return float(c1), float(move), float(c0)


@dataclass
class GlideBpm(LockedBpm):
    glide_enter: float = GLIDE_ENTER
    enter_checks: int = ENTER_CHECKS
    window: deque = field(default_factory=lambda: deque(maxlen=GLIDE_BEATS))
    gliding: bool = False
    enter_run: int = 0
    exit_run: int = 0
    glide_period: float | None = None
    glide_last: float | None = None
    glides: int = 0
    # trigger "quad": the window's quadratic shows a tempo move; "restarts": the constant model restarted
    # `restart_count` times in the same direction within `restart_window` beats (a ramp keeps pushing one way)
    trigger: str = "quad"
    restart_window: int = 48
    restart_count: int = 2
    beats_seen: int = 0
    restart_log: deque = field(default_factory=lambda: deque(maxlen=8))

    def _restart(self) -> None:
        before = super().period() if self.fit.line() else None
        super()._restart()
        after = super().period()
        if before and after and abs(after / before - 1) > 1e-4:
            self.restart_log.append((self.beats_seen, 1 if after < before else -1))   # +1: faster

    def _same_way_restarts(self) -> bool:
        recent = [s for n, s in self.restart_log if self.beats_seen - n <= self.restart_window]
        tail = recent[-self.restart_count:]
        return len(tail) == self.restart_count and len(set(tail)) == 1

    def update(self, t: float) -> None:
        self.beats_seen += 1
        super().update(t)
        self.window.append(t)
        q = quad_tempo(list(self.window))
        if q is None:
            return
        period, move, last = q
        if not self.gliding:
            if self.trigger == "restarts":
                if self._same_way_restarts():
                    self.gliding, self.exit_run, self.glides = True, 0, self.glides + 1
                    self.restart_log.clear()
            else:
                self.enter_run = self.enter_run + 1 if move > self.glide_enter else 0
                if self.enter_run >= self.enter_checks:
                    self.gliding, self.exit_run, self.glides = True, 0, self.glides + 1
        else:
            self.exit_run = self.exit_run + 1 if move < GLIDE_EXIT else 0
            if self.exit_run >= EXIT_CHECKS:
                self.gliding, self.enter_run = False, 0
                self._restart()
        self.glide_period, self.glide_last = period, last

    def period(self) -> float | None:
        if self.gliding and self.glide_period:
            return self.glide_period
        return super().period()

    def grid(self) -> tuple[float, float] | None:
        if self.gliding and self.glide_period and self.glide_last is not None:
            return self.glide_last, self.glide_period
        return super().grid()
