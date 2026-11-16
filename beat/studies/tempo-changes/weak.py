"""The weak-signal follower (prototype, behind chain.Config.weak): through beat-less or chopped passages the grid coasts
on the Kalman's prediction and is corrected gently by weak evidence, and the outputs stop only on clear contrary
evidence (the user, 2026-11-15: "use weak signals to confirm it's still on time ... does not stop or relock unless
there's clear contrary evidence").

  strong beats  a tracked beat is strong when the live tracker's activation around it (+-3 frames) reaches
                max(STRONG_ABS_MIN, min(STRONG_ABS, STRONG_REL x LEVEL)), LEVEL the 75th percentile of the last 64 beats'
                activation (a quiet pop track has a low level; a techno breakdown is far under its level). The online
                DBN keeps ticking through a breakdown from its tempo model alone: a weak beat off the lines is no
                contrary evidence. Before the first lock every beat feeds the estimator (with `filter`, only strong
                ones: measured, on pop tracks whose backbeat is the strong one, that locks at half tempo); once the
                follower glides a weak beat is a weak observation only (update()).
  weak lines    each predicted line no strong beat fitted (the follower glides): the evidence (the activation, or the
                spectral flux of the top bands: `source`) within +-WINDOW periods of it; a peak at least FLOOR and SALIENCE
                times the median of the +-1 period around it is a weak observation, taken by the Kalman with R_WEAK
                (no manoeuvre from it), and it keeps the hold (hold_s) running
  confidence    the line confidence where an unobserved line (no strong beat within half a period, no weak hit) is not
                counted as missed: only observed lines and strong beats judge the grid; with fewer than MIN_OBS observed
                lines in the window, the last confidence holds. Contrary evidence = strong beats off the lines. It only
                keeps the outputs running: they start on the strict line confidence (chain.py: hard to lock, sticky
                once locked).

Device cost (estimate from the operations): per predicted line a max and a median over about 2 periods of the
activation the device already computes (~100 values), and per tick the confidence as today: well under the Kalman's.
"""
from __future__ import annotations

from bisect import bisect_left
from collections import deque
from dataclasses import dataclass, field

import numpy as np

from eval.follow_bpm import FollowBpm

DEFAULTS = {
    "strong_abs": 0.3, "strong_abs_min": 0.05, "strong_rel": 0.4,
    "source": "act",            # act / flux / both
    "window": 0.12, "salience": 2.5, "floor": 0.03, "flux_floor": 0.0,
    "r_weak": 0.012, "min_obs": 4, "hold_s": 30.0, "lag_s": 0.05, "filter": False,
}
LEVEL_BEATS = 64
FPS = 100


def peak_near(x: np.ndarray, t: float, half: int = 3) -> float:
    f = int(round(t * FPS))
    lo, hi = max(f - half, 0), min(f + half + 1, x.size)
    return float(x[lo:hi].max()) if hi > lo else 0.0


def weak_peak(x: np.ndarray, line: float, period: float, window: float, salience: float,
              floor: float) -> float | None:
    """The time of a salient peak of x within +-window periods of the line, or None."""
    lo, hi = int(round((line - window * period) * FPS)), int(round((line + window * period) * FPS)) + 1
    blo, bhi = int(round((line - period) * FPS)), int(round((line + period) * FPS)) + 1
    if lo < 1 or hi >= x.size or blo < 0 or bhi > x.size:
        return None
    seg = x[lo:hi]
    k = int(np.argmax(seg))
    pk = float(seg[k])
    base = float(np.median(x[blo:bhi]))
    if pk < floor or pk < salience * max(base, 1e-6):
        return None
    f = lo + k
    frac = 0.0
    if 0 < f < x.size - 1:
        a, b, c = float(x[f - 1]), float(x[f]), float(x[f + 1])
        d = a - 2 * b + c
        frac = 0.5 * (a - c) / d if d < 0 else 0.0
    return (f + frac) / FPS


def weak_step(kf, obs: float, r_s: float, gate: float) -> bool:
    """A Kalman update from a weak observation: the prediction as Kalman.step, measurement noise r_s, no manoeuvre.
    Returns whether it was taken (within gate periods of the prediction)."""
    from eval.follow_bpm import Q_DRIFT, Q_PHASE_S, Q_RATE
    p = kf.p
    n = round((obs - kf.t) / p)
    if n < 1:
        return False
    f = np.array([[1.0, n, n * (n + 1) / 2], [0.0, 1.0, n], [0.0, 0.0, 1.0]])
    c = f @ kf.c @ f.T + n * np.diag([Q_PHASE_S ** 2, (Q_RATE * p) ** 2, (Q_DRIFT * p) ** 2])
    xp = f @ kf.x
    e = obs - float(xp[0])
    if abs(e) > gate * p:
        return False
    s = c[0, 0] + r_s ** 2
    k = c[:, 0] / s
    kf.x = xp + k * e
    kf.c = c - np.outer(k, c[0, :])
    return True


@dataclass
class CoastBpm(FollowBpm):
    track: object = None
    params: dict = field(default_factory=dict)
    weak_hits: deque = field(default_factory=lambda: deque(maxlen=128))
    levels: deque = field(default_factory=lambda: deque(maxlen=LEVEL_BEATS))
    strong: deque = field(default_factory=lambda: deque(maxlen=256))
    seen: deque = field(default_factory=lambda: deque(maxlen=256))
    last_conf: float = 0.0
    next_line: float | None = None
    coasted: int = 0

    def __post_init__(self) -> None:
        super().__post_init__()
        self.p = {**DEFAULTS, **self.params}
        self.hold_s = self.p["hold_s"]
        if self.track is not None:
            self.act = np.asarray(self.track.act, dtype=np.float32)
            fl = np.asarray(self.track.flux, dtype=np.float32)[:, 1]
            self.flux = fl / (np.median(fl[fl > 0]) + 1e-6) if np.any(fl > 0) else fl
        else:
            self.act = self.flux = np.zeros(0, np.float32)

    def is_strong(self, t: float) -> bool:
        a = peak_near(self.act, t) if self.act.size else 1.0
        level = float(np.percentile(self.levels, 75)) if len(self.levels) >= 8 else 1.0
        self.levels.append(a)
        th = max(self.p["strong_abs_min"], min(self.p["strong_abs"], self.p["strong_rel"] * level))
        return a >= th

    def update(self, t: float) -> None:
        """A strong beat (or any beat before the first lock) goes to the follower as usual; a weak one, once the
        follower glides, is only a weak observation (taken within WINDOW periods of the prediction, no manoeuvre, no
        miss, no jump): the DBN's beats in a breakdown come from its tempo model, not from the music."""
        self.seen.append(t)
        if self.is_strong(t):
            self.strong.append(t)
            super().update(t)
            return
        if self.kf is None:
            if not self.p["filter"]:
                super().update(t)
            return
        # every tracked beat stays in the recent beats, as FollowBpm keeps them: their own tempo stays at the beat
        # level (strong backbeats alone read as half tempo) and a grid that slid off the beats is still caught by the
        # jump rule (a consistent grid of the tracked beats while the strong ones miss)
        self.recent.append(t)
        self.history.append(t)
        if weak_step(self.kf, t, self.p["r_weak"], self.p["window"]):
            self.weak_hits.append(t)
            self.last_fit_time = t
            self.outcomes.append(True)
            return
        n = round((t - self.kf.t) / self.kf.p)
        if n >= 1 and abs(t - (self.kf.t + n * self.kf.p)) > self.outlier * self.kf.p:
            # off the grid: a miss like a strong one, so a grid the tracked beats left for a consistent one of their
            # own (an off-beat slide) is jumped from; wandering beats of a breakdown are not consistent: no jump
            self.misses += 1
            if self.misses >= self.miss_limit * (2 if self.sticky else 1) and self._recent_consistent():
                from eval.locked_bpm import short_period
                from eval.mode_bpm import LOCK_BEATS
                self._jump(t, short_period(list(self.recent)[-LOCK_BEATS:]))

    def coast(self, now: float) -> None:
        """Weak observations for the predicted lines that no strong beat fitted, once their window has passed."""
        if self.kf is None or self.mode_at(now) == "stopped":
            self.next_line = None
            return
        p = self.kf.p
        if self.next_line is None or self.next_line < self.kf.t + 0.5 * p:
            self.next_line = self.kf.t + p
        w = self.p["window"]
        while self.next_line + w * p + self.p["lag_s"] <= now:
            line = self.next_line
            self.next_line = line + p
            if self.seen and abs(self.seen[-1] - line) <= w * p:
                continue                                # a tracked beat is there (the follower took it or missed it)
            obs = self._evidence(line, p)
            if obs is not None and weak_step(self.kf, obs, self.p["r_weak"], w):
                self.weak_hits.append(obs)
                self.last_fit_time = obs
                self.outcomes.append(True)
                self.coasted += 1
                self.next_line = self.kf.t + self.kf.p

    def _evidence(self, line: float, p: float) -> float | None:
        src, w, s = self.p["source"], self.p["window"], self.p["salience"]
        a = weak_peak(self.act, line, p, w, s, self.p["floor"]) if src in ("act", "both") else None
        if a is None and src in ("flux", "both"):
            a = weak_peak(self.flux, line, p, w, s, self.p["flux_floor"])
        return a

    def confidence(self, beats: list[float], log, period: float | None, now: float,
                   window_s: float = 8.0, delay_s: float = 0.1, tol: float = 0.1) -> float:
        """The line confidence judged on observed lines only (see the module's doc)."""
        if not period or period <= 0:
            self.last_conf = 0.0
            return 0.0
        lo, hi = now - window_s, now - delay_s
        lines = np.asarray([x for x in log.times() if lo <= x <= hi])
        if lines.size == 0:
            self.last_conf = 0.0
            return 0.0
        sb = list(self.strong)
        strong = np.asarray(sb[bisect_left(sb, lo - period):bisect_left(sb, hi + period)], dtype=float)
        ab = list(self.seen)
        tracked = np.asarray(ab[bisect_left(ab, lo - period):bisect_left(ab, hi + period)], dtype=float)
        wh = list(self.weak_hits)
        weak = np.asarray(wh[bisect_left(wh, lo - period):bisect_left(wh, hi + period)], dtype=float)
        margin = tol * period
        d_strong = np.abs(strong[:, None] - lines[None, :]) if strong.size else np.full((0, lines.size), np.inf)
        d_weak = np.abs(weak[:, None] - lines[None, :]) if weak.size else np.full((0, lines.size), np.inf)
        near_s = d_strong.min(axis=0) if strong.size else np.full(lines.size, np.inf)
        near_w = d_weak.min(axis=0) if weak.size else np.full(lines.size, np.inf)
        near_t = np.abs(tracked[:, None] - lines[None, :]).min(axis=0) if tracked.size else np.full(lines.size, np.inf)
        hit = (near_t <= margin) | (near_w <= margin)
        observed = hit | (near_s <= 0.5 * period)
        if int(observed.sum()) < self.p["min_obs"]:
            return self.last_conf
        in_win = strong[(strong >= lo) & (strong <= hi)]
        every = np.asarray(log.times())                 # a beat at the window's edge is judged against all the lines
        if in_win.size:
            on = int(np.sum(np.abs(in_win[:, None] - every[None, :]).min(axis=1) <= margin))
            precision = on / in_win.size
        else:
            precision = 1.0
        recall = float(hit.sum()) / float(observed.sum())
        f = 0.0 if recall + precision == 0 else 2 * recall * precision / (recall + precision)
        self.last_conf = f
        return f

