"""The x2 / /2 fold and AUTO RANGE, as the XIAO firmware does it (core/jank_fold.h, jank_fold.c: the spec).

Link gets the tracked tempo times the fold (1, 2, 4, 0.5, 0.25) and a grid of that period.

- x2, x4: every tracked beat stays a beat of the folded grid, with one (three) beats between: beat = the tracked beat,
  period / fold.
- /2, /4: every 2nd (4th) tracked beat is a beat. Which ones is chosen once, when the fold changes, against the previous
  output grid: of the m candidates t - r p, the one whose place on the output (in output beats) is nearest a multiple of
  K = max(1, round(m p / P)) output beats. It is kept from beat to beat by an anchor moved along with each tracked beat,
  so nothing adds up.
- AUTO RANGE: on a NEW tempo (a lock after no tempo, or a tempo jump of the estimator) the fold becomes the one that
  brings the new tempo closest, in octaves, to the reference, when that lands within RANGE_TOL of it; else fold 1 (a
  user's fold is kept when there is no range match). The reference is the Link session's tempo while it plays, else the
  last output tempo when it is less than RECENT_S old, else none. A user's fold wins until the next new tempo.
"""
from __future__ import annotations

import math

EXP_MIN, EXP_MAX = -2, 2
RECENT_S = 120.0       # a last output tempo counts as the reference this long
RANGE_TOL = 0.06       # an octave away within 6 %: a flip of the tracker or the next track of a mix; 128 -> 90 is not 180
FOLDS = tuple(2.0 ** e for e in range(EXP_MIN, EXP_MAX + 1))


def best_exp(bpm: float, ref: float) -> int:
    """The exp in EXP_MIN..EXP_MAX that puts bpm x 2^exp closest to ref in octaves (ties: the smaller fold)."""
    if not (bpm > 0.0 and ref > 0.0):
        return 0
    best, best_d = 0, 1e9
    for e in range(EXP_MIN, EXP_MAX + 1):
        d = abs(math.log2(bpm * 2.0 ** e / ref))
        if d < best_d - 1e-9:
            best, best_d = e, d
    return best


def range_exp(bpm: float, ref: float) -> int:
    """AUTO RANGE's choice: best_exp when bpm x 2^exp is within RANGE_TOL of ref, else 0."""
    e = best_exp(bpm, ref)
    return e if e != 0 and abs(bpm * 2.0 ** e / ref - 1.0) <= RANGE_TOL else 0


class Fold:
    def __init__(self) -> None:
        self.exp = 0
        self.manual = False         # the user chose it (the fold API / UP / DOWN) since the last new tempo
        self.anchored = False       # /2, /4: the anchor is set
        self.anchor = 0.0           # a tracked beat that is a beat of the folded grid
        self.out_bpm = 0.0          # the last tempo put out (folded), 0: none yet
        self.out_s = 0.0
        self.had_tempo = False      # the estimator gave a tempo at the last step
        self.jumps = 0

    @property
    def factor(self) -> float:
        return 2.0 ** self.exp

    @property
    def auto(self) -> bool:
        """The fold in force was set by AUTO RANGE."""
        return self.exp != 0 and not self.manual

    def _set(self, exp: int) -> None:
        exp = min(max(exp, EXP_MIN), EXP_MAX)
        if exp != self.exp:
            self.anchored = False
        self.exp = exp

    def user(self, factor: float) -> bool:
        """The user's fold. Returns True when the fold changed."""
        before = self.exp
        self._set(round(math.log2(factor)))
        self.manual = True
        return self.exp != before

    def reset(self) -> bool:
        """A resync: fold 1; the memory of the last output tempo stays. Returns True when the fold changed."""
        before = self.exp
        self._set(0)
        self.manual = False
        self.had_tempo = False
        return self.exp != before

    def step(self, bpm: float | None, jumps: int, session_bpm: float | None, now_s: float) -> tuple[bool, bool]:
        """One look at the estimator: bpm (tracked, unfolded; None: no tempo), its tempo jumps so far, the Link
        session's tempo while it plays, stream time. Applies AUTO RANGE on a new tempo and remembers the output tempo.
        Returns (new tempo, fold changed)."""
        before, new = self.exp, False
        have = bpm is not None and bpm > 0.0
        if have:
            if not self.had_tempo or jumps != self.jumps:
                new = True
                if session_bpm is not None and session_bpm > 0.0:
                    ref = session_bpm
                elif self.out_bpm > 0.0 and now_s - self.out_s < RECENT_S:
                    ref = self.out_bpm
                else:
                    ref = 0.0
                e = range_exp(bpm, ref) if ref > 0.0 else 0
                if e or not self.manual:      # a range match, or no user's fold to keep: AUTO RANGE decides
                    self._set(e)
                    self.manual = False
            self.out_bpm = bpm * self.factor
            self.out_s = now_s
        self.had_tempo = have
        self.jumps = jumps
        return new, self.exp != before

    def grid(self, t: float, p: float, out: tuple[float, float] | None) -> tuple[float, float]:
        """The folded grid from a tracked beat t and the tracked period p. out: the output's place in its own beats at
        t and its period, for the /2, /4 anchor (None: no output grid, t itself)."""
        if self.exp >= 0:
            return t, p / self.factor
        m = 2 ** -self.exp
        if not self.anchored:
            self.anchor = _anchor(t, p, m, *out) if out is not None and out[1] > 0.0 else t
            self.anchored = True
        j = round((t - self.anchor) / p)
        beat = t - (j % m) * p
        self.anchor = beat              # moved along: the tracked period's small changes never add up
        return beat, p * m


def _anchor(t: float, p: float, m: int, pos_at_t: float, out_period: float) -> float:
    """Of the m tracked beats t - r p, the one nearest a multiple of K output beats, K the folded period in them."""
    k = max(1.0, float(round(m * p / out_period)))
    best, best_d = 0, 1e9
    for r in range(m):
        pos = pos_at_t - r * p / out_period
        d = abs(pos - k * round(pos / k))
        if d < best_d - 1e-9:
            best, best_d = r, d
    return t - best * p
