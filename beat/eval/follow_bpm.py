"""ModeBpm with a continuous tempo follower in its steady mode: the grid glides with the music, beat by beat.

Today's ModeBpm publishes a growing least-squares line while steady. A pitch-fader step moves the music off that line
at once, but the line only moves after the change detector has seen CHANGE_BEATS disagreeing short fits (about 12-14
beats): meanwhile the grid drifts off the beats (2 % at 135 BPM: 9 ms a beat), the confidence falls under the gate and
the outputs stop (studies/advanced-follow/REPORT.md).

Here, once ModeBpm locks, a Kalman filter on (time of the last beat, period, period drift per beat) takes over the
steady grid (Cemgil et al. 2000 track tempo with a Kalman filter the same way):
  predict   n beats ahead (missed beats bridged by rounding), process noise per beat: a little phase, a very slow
            period and drift random walk (Q_RATE, Q_DRIFT) so a steady track converges like the growing fit, and a
            ramp is followed without lag once the drift is learnt
  update    each tracked beat within OUTLIER of a period of the prediction; further off: a miss (fills, off-beats)
  manoeuvre two innovations in a row on the same side, each over MANOEUVRE_SIGMA standard deviations: the tempo moved
            (a step or a ramp); the covariance opens (phase and period) so the next beats pull the grid onto the new
            tempo in a few beats, the phase kept (no restart, no relock)
ModeBpm's own machinery stays for everything else: the first lock, a jump to a new grid (miss_limit misses and a
consistent new grid), the hold through beat-less stretches and the stop. Ramps need no ramp mode: the manoeuvre
detector re-opens the filter as often as the tempo keeps moving.

`sticky` (the "same music" detector's SAME): manoeuvres still follow, but a jump needs twice the misses and a new grid
whose tempo is an octave or a 2:3 of the held one is refused (a breakdown's half-time or triplet feel is not a new tempo).
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

import numpy as np

from eval.locked_bpm import LockedBpm, short_period
from eval.mode_bpm import LOCK_BEATS, ModeBpm

R_S = 0.006            # the tracker's beat jitter (s, one standard deviation; measured 4-6 ms on steady tracks)
Q_PHASE_S = 0.001      # phase process noise per beat (s)
Q_RATE = 0.00002       # period random walk per beat (relative): 0.002 %
Q_DRIFT = 0.00001      # drift random walk per beat (relative period change per beat)
Q_DRIFT_SEED = 0.0001
OPEN_PHASE = 1.0       # a manoeuvre opens the phase deviation to |innovation| / OPEN_PHASE
OPEN_PERIOD = 2.0      # and the period deviation to |innovation| / OPEN_PERIOD
MANOEUVRE_SIGMA = 3.0    # with MANOEUVRE_RUN 3: the dev sweep's balance (studies/advanced-follow/REPORT.md)
MANOEUVRE_RUN = 3
OPEN_DRIFT = 4.0       # a second manoeuvre the same way within RAMP_BEATS opens the drift to |innovation| / OPEN_DRIFT
RAMP_BEATS = 24
TEMPO_REL = 0.04       # the recent beats' own period this far from the follower's for CHANGE_BEATS beats: start over
TRUST_BEATS = 32
TRUST_SHARE = 0.8
RATIO_SD = 0.002        # a measured ratio is good to about 0.2 % (ratio_probe.py on the user's tracks)
RELATED = (0.5, 2.0, 2 / 3, 1.5, 0.75, 4 / 3)
RELATED_TOL = 0.04


@dataclass
class Kalman:
    """State x = (t: time of the last beat on the line, p: period, d: period change per beat, the drift: a ramp) with
    its 3x3 covariance c."""
    x: np.ndarray
    c: np.ndarray
    run: int = 0           # innovations in a row past MANOEUVRE_SIGMA on the same side (signed)
    manoeuvres: int = 0
    last_side: int = 0     # the last manoeuvre's direction, and beats since it
    since: int = 10 ** 6

    @property
    def t(self) -> float:
        return float(self.x[0])

    @property
    def p(self) -> float:
        return float(self.x[1])

    @classmethod
    def seed(cls, t: float, p: float, n: int) -> "Kalman":
        n = max(n, 4)
        return cls(np.array([t, p, 0.0]), np.diag([R_S ** 2, 12 * R_S ** 2 / n ** 3, (Q_DRIFT_SEED * p) ** 2]))

    def step(self, obs: float, outlier: float) -> str:
        """'fit', 'double' (a second detection of the beat already taken) or 'outlier'."""
        p, d = self.p, float(self.x[2])
        n = round((obs - self.t) / p)
        if n < 1:
            return "double"
        f = np.array([[1.0, n, n * (n + 1) / 2], [0.0, 1.0, n], [0.0, 0.0, 1.0]])
        c = f @ self.c @ f.T + n * np.diag([Q_PHASE_S ** 2, (Q_RATE * p) ** 2, (Q_DRIFT * p) ** 2])
        xp = f @ self.x
        pred = float(xp[0])
        e = obs - pred
        if abs(e) > outlier * p:
            return "outlier"
        s = c[0, 0] + R_S ** 2
        z = e / np.sqrt(s)
        if abs(z) > MANOEUVRE_SIGMA:
            side = 1 if z > 0 else -1
            self.run = self.run + side if self.run * side > 0 else side
        else:
            self.run = 0
        self.since += n
        if abs(self.run) >= MANOEUVRE_RUN:
            # the tempo moved: open phase and period by what the innovation says, so the next beats pull it over; the
            # drift only when the previous manoeuvre went the same way less than RAMP_BEATS ago (a ramp, not a step)
            side = 1 if self.run > 0 else -1
            ramp = side == self.last_side and self.since <= RAMP_BEATS
            c = c + np.diag([(e / OPEN_PHASE) ** 2, (e / OPEN_PERIOD) ** 2, (e / OPEN_DRIFT) ** 2 if ramp else 0.0])
            s = c[0, 0] + R_S ** 2
            self.manoeuvres += 1
            self.last_side, self.since, self.run = side, 0, 0
        k = c[:, 0] / s
        self.x = xp + k * e
        self.c = c - np.outer(k, c[0, :])
        return "fit"


def _related(a: float, b: float) -> bool:
    return any(abs((a / b) / r - 1) <= RELATED_TOL for r in RELATED)


@dataclass
class FollowBpm(ModeBpm):
    sticky: bool = False
    kf: Kalman | None = None
    follow_beats: int = 0
    log: list = field(default_factory=list)      # (time, event): seed / jump / refused
    outcomes: deque = field(default_factory=lambda: deque(maxlen=TRUST_BEATS))   # the last beats: fitted or not
    pending: tuple | None = None                  # a proposed rate change (ratio, time, since) awaiting the next beat
    last_ratio: float = 1.0                       # the last proposed ratio a beat backed (jank's status rate_ratio)

    def update(self, t: float) -> None:
        if self.kf is not None and self.mode_at(t) != "steady":
            self.kf = None                             # stopped (hold_s without a fitting beat): ModeBpm relocks
        if self.kf is None:
            super().update(t)
            if self.mode == "steady":
                self._seed(t)
            return
        self.beats_seen += 1
        self.history.append(t)
        self.recent.append(t)
        if self.pending is not None:
            self._check_pending(t)
        before = self.kf.manoeuvres
        res = self.kf.step(t, self.outlier)
        if self.kf.manoeuvres != before:
            self.log.append((t, "manoeuvre"))
        if res == "double":
            return
        self.outcomes.append(res == "fit")
        if res == "fit":
            self.misses = 0
            self.last_fit_time = t
            self.in_a_row += 1
            self.follow_beats += 1
        else:
            self.misses += 1
            self.in_a_row = 0
            limit = self.miss_limit * (2 if self.sticky else 1)
            if self.misses >= limit and self._recent_consistent():
                self._jump(t, short_period(list(self.recent)[-LOCK_BEATS:]))
                return
        # the beats' own tempo far from the follower's for change_beats beats (a lock on a 4:3 of the music, whose
        # every third beat fits, never misses enough to jump): start over from the recent beats
        sp = short_period(list(self.recent))
        if sp is None or self.kf is None:
            return
        self.disagree = self.disagree + 1 if abs(sp / self.kf.p - 1) > TEMPO_REL else 0
        if self.disagree >= self.change_beats:
            self.disagree = 0
            self._jump(t, sp)

    def propose_ratio(self, ratio: float, t: float, since: float | None = None) -> None:
        """A measured rate change held until the next beat: taken only if that beat sits closer to the grid re-timed
        by it than to the grid as it is (the spectral match alone fires on chord changes too: ratio_probe.py)."""
        self.pending = (ratio, t, since)

    def _check_pending(self, beat: float) -> None:
        ratio, t, since = self.pending
        self.pending = None
        if self.kf is None or beat <= t - 1.0:
            return
        x_old = self.kf.x.copy()
        c_old = self.kf.c.copy()
        n = max(round((beat - x_old[0]) / x_old[1]), 1)
        e_old = beat - (x_old[0] + n * x_old[1])
        if not self.apply_ratio(ratio, t, since):
            return
        x_new = self.kf.x
        e_new = beat - (x_new[0] + max(round((beat - x_new[0]) / x_new[1]), 1) * x_new[1])
        if abs(e_new) >= abs(e_old):
            self.kf.x, self.kf.c = x_old, c_old                 # the beat does not back it: undone
            self.log[-1] = (t, "ratio refused")
        else:
            self.last_ratio = ratio

    def apply_ratio(self, ratio: float, t: float, since: float | None = None) -> bool:
        """A measured playback-rate change (the spectrum moved by `ratio`: a varispeed step, studies/advanced-follow
        ratio_probe.py) taken at once: the period divided by it, the drift cleared, and the next line moved as if the
        music had run at the new rate from `since` (the step's estimated time; None: the last beat). Only while
        following a trusted grid; returns whether it was taken."""
        if self.kf is None or not self.trusted() or not 0.85 < ratio < 1.15:
            return False
        x = self.kf.x.copy()
        t_last, p_old = float(x[0]), float(x[1])
        x[1] = p_old / ratio
        x[2] = 0.0
        if since is not None and since > t_last:
            nxt = t_last + p_old                                   # the next line at the old rate
            if nxt > since:
                x[0] = since + (nxt - since) / ratio - x[1]        # re-timed from the step on
        self.kf.x = x
        c = self.kf.c.copy()
        c[1, 1] += (RATIO_SD * x[1]) ** 2
        self.kf.c = c
        self.log.append((t, "ratio"))
        return True

    def trusted(self) -> bool:
        """The held grid fitted most of the last beats: only then may SAME refuse an octave / 2:3 move."""
        return len(self.outcomes) >= TRUST_BEATS // 2 and sum(self.outcomes) >= TRUST_SHARE * len(self.outcomes)

    def _jump(self, t: float, new_period: float | None) -> None:
        if self.sticky and self.trusted() and new_period and _related(new_period, self.kf.p):
            self.log.append((t, "refused"))
            self.misses = 0
            return
        self.jumps += 1
        LockedBpm._restart(self)
        self.in_a_row = LOCK_BEATS
        self.last_fit_time = t
        self.log.append((t, "jump"))
        self._seed(t)

    def _seed(self, t: float) -> None:
        g = LockedBpm.grid(self)
        if g is None:
            return
        self.kf = Kalman.seed(g[0], g[1], self.fit.n)
        self.outcomes.clear()
        self.disagree = 0
        self.log.append((t, "seed"))

    def period(self) -> float | None:
        return self.kf.p if self.kf is not None else super().period()

    def grid(self) -> tuple[float, float] | None:
        return (self.kf.t, self.kf.p) if self.kf is not None else super().grid()
