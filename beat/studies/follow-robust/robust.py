"""The follower with the lock rules of changes 2 and 3 (prototype on weak.py's CoastBpm = the M2j/M2k firmware chain).

  TempoMemory  the last confident tempo (the tracker's level, unfolded) and when it was: set by the replay while the
               outputs run with the strict confidence at or over the gate; forgotten after mem_s
  LockGate     asked before the follower locks (stopped -> steady) or jumps to a new grid of period P:
    memory     (change 2) P at about 3:4 / 4:3 / 2:3 / 3:2 of the remembered tempo (folded ratio within rel_tol of 0.75 or
               4/3): the lock needs related_beats consistent beats (a lock: beats fitted in a row; a jump: the last
               related_beats tracked beats on one grid of P); any other tempo locks as today
    ratio      (change 3) the activation's comb at P must beat the best of P's 3:4 / 4:3 / 2:3 / 3:2 relatives by
               `margin` over the last span_s; refused for at most give_up beats in a row (None: always)
  RobustBpm    CoastBpm asking the gate in _set_mode (the lock) and _jump; it also keeps every tracked beat (the jump's
               consistency check) and the time of the last beat (the gate's "now")
"""
from __future__ import annotations

import math
import sys
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

FPS = 100

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tempo-changes"))

import comb as CB
from dbn import fold_ratio
from eval.locked_bpm import LockedBpm
from weak import CoastBpm

RELATED = (0.75, 4.0 / 3.0)       # 3:4, 4:3 (and 2:3, 3:2 once folded by an octave)
CONSISTENT_RESID = 0.1            # of a period: a beat on the candidate grid
CONSISTENT_SHARE = 0.9            # of the last N beats on it, no gap over MAX_GAP beats
MAX_GAP = 2


@dataclass
class TempoMemory:
    mem_s: float = 120.0
    bpm: float | None = None
    t: float = -math.inf

    def remember(self, bpm: float, now: float) -> None:
        self.bpm, self.t = bpm, now

    def active(self, now: float) -> float | None:
        return self.bpm if self.bpm is not None and now - self.t <= self.mem_s else None


def related(bpm: float, mem_bpm: float, tol: float) -> bool:
    r = fold_ratio(bpm / mem_bpm)
    return any(abs(r / q - 1.0) <= tol for q in RELATED)


def consistent(beats: list[float], period: float, n: int) -> bool:
    """The last n beats lie on one grid of `period` (CONSISTENT_SHARE of them within CONSISTENT_RESID, no gap over
    MAX_GAP beats)."""
    if len(beats) < n:
        return False
    b = np.asarray(beats[-n:])
    k = np.round((b - b[-1]) / period)
    if np.any(np.diff(k) > MAX_GAP) or np.unique(k).size < CONSISTENT_SHARE * n:
        return False
    coef = np.polyfit(k, b, 1)
    if abs(coef[0] / period - 1.0) > 0.02:
        return False
    resid = np.abs(b - np.polyval(coef, k))
    return bool(np.mean(resid <= CONSISTENT_RESID * period) >= CONSISTENT_SHARE)


@dataclass
class LockGate:
    memory: TempoMemory | None = None
    related_beats: int = 32
    rel_tol: float = 0.04
    ratio: dict | None = None         # {"margin", "span_s", "give_up"}
    act: np.ndarray | None = None
    periods: np.ndarray | None = None  # the DBN's periods (frames), for the curve's length
    asked: int = 0
    refused_memory: int = 0
    refused_ratio: int = 0
    refused_run: int = 0
    log: list = field(default_factory=list)

    def allow(self, est, now: float, period: float, kind: str) -> bool:
        self.asked += 1
        ok = self._memory_ok(est, now, period, kind) and self._ratio_ok(now, period)
        self.refused_run = 0 if ok else self.refused_run + 1
        return ok

    def _memory_ok(self, est, now: float, period: float, kind: str) -> bool:
        mem = self.memory.active(now) if self.memory is not None else None
        if mem is None or not related(60.0 / period, mem, self.rel_tol):
            return True
        n = self.related_beats
        ok = est.in_a_row >= n if kind == "lock" else consistent(list(est.tracked), period, n)
        if not ok:
            self.refused_memory += 1
            self.log.append((now, f"{kind} refused: related to {mem:.1f}"))
        return ok

    def _ratio_ok(self, now: float, period: float) -> bool:
        if self.ratio is None or self.act is None:
            return True
        give_up = self.ratio.get("give_up")
        if give_up is not None and self.refused_run >= give_up:
            return True
        end = int(math.floor(now * CB.FPS + 1e-9))
        span = int(round(self.ratio.get("span_s", 10.0) * CB.FPS))
        r = CB.curve(self.act, end, span, CB.max_lag_for(float(self.periods.max())))
        if r is None:
            return True
        m = CB.ratio_margin(r, period * CB.FPS)
        ok = not (m == m) or m >= self.ratio.get("margin", 0.0)
        if not ok:
            self.refused_ratio += 1
            self.log.append((now, f"ratio refused {m:.3f}"))
        return ok


@dataclass
class RobustBpm(CoastBpm):
    gate: LockGate | None = None
    tracked: deque = field(default_factory=lambda: deque(maxlen=64))
    now: float = 0.0
    skip: dict | None = None          # change 5, SKIP: {"k": beats the new grid needs, "phase_tol", "tempo_tol", "suspect"}
    off_run: int = 0                  # tracked beats in a row off the follower's grid by more than phase_tol of a period
    beat_suspect: bool = False        # off_run >= suspect: the replay gates the outputs
    act_run: int = 0                  # ticks the activation has put the best phase off the grid's
    skip_jumps: int = 0
    act_next: float = 0.0
    act_tempo_changed: int = 0
    tpl: object = None                # the bar pattern memory (4 x steps)
    tpl_p: float = 0.0
    tpl_n: int = 0
    tpl_next: float | None = None
    act_last: float = -1e9
    act_resyncs: int = 0
    resyncs: int = 0                  # phase-only re-syncs (the tempo kept)
    reopen: int = -1                  # after a skip jump: the beats that fitted the new grid since (-1: not waiting)

    def update(self, t: float) -> None:
        self.tracked.append(t)
        self.now = t
        if self.skip is None or self.kf is None:
            super().update(t)
            self.off_run, self.beat_suspect = 0, False
            return
        kf = self.kf
        n = round((t - kf.t) / kf.p)
        off = n >= 1 and abs(t - (kf.t + n * kf.p)) > self.skip.get("phase_tol", 0.15) * kf.p
        jumps = self.jumps
        super().update(t)
        if self.jumps != jumps or self.kf is None:
            self.off_run, self.beat_suspect = 0, False
            return
        if self.reopen >= 0 and self.outcomes:
            self.reopen = self.reopen + 1 if self.outcomes[-1] else -1
        self.off_run = self.off_run + 1 if off else 0
        self.beat_suspect = self.off_run >= self.skip.get("suspect", 2)
        if self.off_run >= 1:
            self._skip_check(t)

    @property
    def suspect(self) -> bool:
        n = self.skip.get("suspect_ticks", 2) if self.skip is not None else 10 ** 9
        return self.beat_suspect or (self.skip is not None and self.skip.get("act", False) and self.act_run >= n)

    def coast(self, now: float) -> None:
        super().coast(now)
        if self.skip is not None and self.skip.get("act") and self.kf is not None and self.mode_at(now) == "steady":
            self._act_resync(now)
        else:
            self.act_run = 0

    def _values(self, t_end: float, p: float, n: int, steps: int, w: int) -> np.ndarray:
        """The activation's peak (+-w frames) at t_end - i p / steps, i = 0 .. n-1."""
        fr = np.round((t_end - np.arange(n) * p / steps) * FPS).astype(int)
        return np.asarray([self.act[max(f - w, 0):f + w + 1].max() for f in fr])

    def _tpl_update(self, now: float, steps: int, w: int) -> None:
        """The pattern memory: the activation of the last bars folded onto 4 beats x steps phases (decay), kept while the
        grid is trusted (steady, not suspect)."""
        a, kf = self.skip, self.kf
        p = kf.p
        if self.tpl is not None and abs(p / self.tpl_p - 1.0) > 0.03:
            self.tpl, self.tpl_n, self.tpl_next = None, 0, None
        if self.suspect or self.act_run > 0:
            return
        if self.tpl_next is None or self.tpl_next < kf.t - 8 * p:
            self.tpl_next = kf.t + p
        while self.tpl_next + 4 * p + a.get("lag_s", 0.03) <= now:
            line = self.tpl_next
            self.tpl_next = line + 4 * p
            if line > kf.t + p * 400:
                break
            fr = np.round((line + np.arange(4 * steps) * p / steps) * FPS).astype(int)
            if fr.max() + w >= self.act.size:
                break
            v = np.asarray([self.act[f - w:f + w + 1].max() for f in fr]).reshape(4, steps)
            d = a.get("decay", 0.7)
            self.tpl = v if self.tpl is None else d * self.tpl + (1 - d) * v
            self.tpl_p, self.tpl_n = p, self.tpl_n + 1

    def _pattern_scores(self, vals: np.ndarray, steps: int) -> np.ndarray:
        """The correlation of the window `vals` (newest first, one sample per phase step) with the bar pattern, for each
        of the 4 x steps positions of the newest sample in the bar."""
        flat = self.tpl.reshape(-1)
        n = flat.size
        idx = (np.arange(n)[:, None] - np.arange(vals.size)[None, :]) % n
        tpl = flat[idx]
        x = vals - vals.mean()
        y = tpl - tpl.mean(axis=1, keepdims=True)
        den = np.sqrt((x ** 2).sum() * (y ** 2).sum(axis=1)) + 1e-12
        return (y @ x) / den

    def _act_resync(self, now: float) -> None:
        """Phase from the onsets, at the kept tempo: the activation's peaks at the last `m` lines of each of `steps` phases of
        the follower's period (the comb), and with `pattern` its correlation with the remembered bar pattern (this also
        finds the beat in the bar). The grid's own phase scoring under `ratio` of the best, the best one off it by more
        than phase_tol of a period and clear (the best over the best further than phase_tol away at most `clear`) for
        `confirm` ticks in a row: take the best phase (a chop went missing). `tempo_check`: the window's autocorrelation
        must still peak at the follower's period (or an octave of it), else the tempo changed: no snap."""
        a = self.skip
        kf = self.kf
        if now < self.act_next:
            return
        self.act_next = now + a.get("every_s", 0.05)
        p = kf.p
        m, steps = int(a.get("m", 2)), int(a.get("steps", 20))
        w = max(2, int(round(a.get("win", 0.08) * p * FPS)))
        if a.get("pattern"):
            self._tpl_update(now, steps, w)
        if now - self.act_last < a.get("cool", 3.0) * p:
            return
        t_end = now - a.get("lag_s", 0.03)
        end_f = int(t_end * FPS)
        if end_f - int((m + 1) * p * FPS) - w < 0 or end_f + w >= self.act.size:
            return
        shifts = np.arange(steps) * p / steps
        vals = self._values(t_end, p, m * steps, steps, w)
        scores = np.asarray([float(vals[s::steps].mean()) for s in range(steps)])    # peak at the phase's lines
        cur_i = int(round(((t_end - kf.t) % p) / (p / steps))) % steps
        floor, ratio, tol = a.get("floor", 0.2), a.get("ratio", 0.5), a.get("phase_tol", 0.15)
        if a.get("pattern") and self.tpl is not None and self.tpl_n >= a.get("tpl_bars", 2):
            ps = self._pattern_scores(vals, steps)
            ps_beat = ps.reshape(4, steps).max(axis=0)                  # the best beat-in-bar at each phase
            best = int(np.argmax(ps_beat))
            cur = float(max(ps_beat[(cur_i + d) % steps] for d in (-1, 0, 1)))
            top = float(ps_beat[best])
            ok_ev = top >= a.get("min_corr", 0.3) and scores[best] >= floor
            lowcur = cur < ratio * top
            far = np.array([min((i - best) % steps, (best - i) % steps) / steps > tol for i in range(steps)])
            second = float(ps_beat[far].max()) if far.any() else -1.0
            clear = a.get("clear", 0.0) <= 0 or second <= a["clear"] * top
        else:
            best = int(np.argmax(scores))
            cur = float(scores[[(cur_i + d) % steps for d in (-1, 0, 1)]].max())
            top = float(scores[best])
            ok_ev = top >= floor
            lowcur = cur < ratio * top
            far = np.array([min((i - best) % steps, (best - i) % steps) / steps > tol for i in range(steps)])
            second = float(scores[far].max()) if far.any() else -1.0
            clear = a.get("clear", 0.0) <= 0 or second <= a["clear"] * top
        off = min((best - cur_i) % steps, (cur_i - best) % steps) / steps
        bad = ok_ev and lowcur and off > tol and clear
        self.act_run = self.act_run + 1 if bad else 0
        if self.act_run < a.get("confirm", 3):
            return
        if a.get("tempo_check") and not self._tempo_unchanged(end_f, p, a):
            self.act_tempo_changed += 1
            return
        beat = t_end - float(shifts[best])
        x, c = kf.x.copy(), kf.c.copy()
        x[0] = beat
        c[0, :] = 0.0
        c[:, 0] = 0.0
        c[0, 0] = (8e-3) ** 2
        kf.x, kf.c = x, c
        self.resyncs += 1
        self.act_resyncs += 1
        self.act_last, self.act_run = now, 0
        self.misses = 0
        self.last_fit_time = now
        self.outcomes.clear()
        self.reopen = 0
        self.off_run, self.beat_suspect = 0, False

    def _tempo_unchanged(self, end_f: int, p: float, a: dict) -> bool:
        """The autocorrelation of the last `tc_periods` periods peaks at the follower's period or an octave of it."""
        pf = p * FPS
        r = CB.curve(self.act, end_f, int(a.get("tc_periods", 4.0) * pf), int(2.3 * pf))
        if r is None:
            return True
        lo = int(0.4 * pf)
        lag = lo + int(np.argmax(r[lo:]))
        q = lag / pf
        return any(abs(q / f - 1.0) <= 0.06 for f in (0.5, 1.0, 2.0))

    def _phase_resync(self, t: float) -> bool:
        """"The same music, a chop went missing": the last two tracked beats are one follower period apart (the tempo is
        kept) but both off its grid by the same amount: move the grid's phase onto them, keep the period."""
        kf = self.kf
        if kf is None or len(self.tracked) < 2:
            return False
        b1, b2 = self.tracked[-2], self.tracked[-1]
        kp = kf.p
        tol = self.skip.get("phase_tol", 0.15)
        if abs((b2 - b1) / kp - 1.0) > self.skip.get("tempo_tol", 0.06):
            return False
        e1 = (b1 - kf.t + kp / 2) % kp - kp / 2
        e2 = (b2 - kf.t + kp / 2) % kp - kp / 2
        if min(abs(e1), abs(e2)) <= tol * kp or abs(e1 - e2) > 0.1 * kp:
            return False
        x, c = kf.x.copy(), kf.c.copy()
        x[0] = b2
        c[0, :] = 0.0
        c[:, 0] = 0.0
        c[0, 0] = (6e-3) ** 2
        kf.x, kf.c = x, c
        self.resyncs += 1
        self.misses = 0
        self.in_a_row = max(self.in_a_row, 1)
        self.last_fit_time = t
        self.outcomes.clear()
        self.reopen = 0
        self.off_run = 0
        self.beat_suspect = False
        return True

    def _skip_check(self, t: float) -> None:
        """The last k tracked beats on a grid of their own that is not the follower's (a phase or tempo jump): jump to it now."""
        if self.skip.get("resync", False) and self._phase_resync(t):
            return
        if not self.skip.get("jump", True):
            return
        k = int(self.skip.get("k", 4))
        b = np.asarray(list(self.tracked)[-k:])
        if b.size < k or self.kf is None:
            return
        ibi = np.diff(b)
        p = float(np.median(ibi))
        if p <= 0 or np.max(np.abs(ibi / p - 1.0)) > self.skip.get("ibi_tol", 0.08):
            return
        kp = self.kf.p
        lr = math.log2(p / kp)
        octave = round(lr) != 0 and abs(lr - round(lr)) <= math.log2(1.03)
        tempo_off = abs(p / kp - 1.0) > self.skip.get("tempo_tol", 0.06) and not octave
        phase = np.abs((b - self.kf.t + kp / 2) % kp - kp / 2)
        phase_off = bool(np.median(phase) > self.skip.get("phase_tol", 0.15) * kp) and not tempo_off
        if not (tempo_off or phase_off):
            return
        keep = max(k + 1, 2)
        recent = list(self.recent)[-keep:]
        self.recent.clear()
        self.recent.extend(recent)
        before = self.jumps
        self._jump(t, p)
        if self.jumps != before:
            self.skip_jumps += 1
            self.reopen = 0

    def _set_mode(self, mode: str, now: float) -> None:
        if mode == "steady" and self.mode == "stopped" and self.gate is not None:
            g = LockedBpm.grid(self)
            if g is not None and g[1] > 0 and not self.gate.allow(self, now, g[1], "lock"):
                return
        super()._set_mode(mode, now)

    def _jump(self, t: float, new_period: float | None) -> None:
        if self.gate is not None and new_period and not self.gate.allow(self, t, new_period, "jump"):
            self.misses = 0
            self.log.append((t, "refused"))
            return
        super()._jump(t, new_period)
