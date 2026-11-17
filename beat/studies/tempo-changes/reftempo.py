"""The reference's local tempo, from the audio (Traktor's grid has one tempo per track: wrong on tempo-changing songs).

Primary source: madmom's offline beats (the downbeat study's mmdb cache: a continuous beat sequence, no gaps). Its
log beat intervals y_i = log(t_{i+1} - t_i) are cut by dynamic programming into segments, each either flat (a constant
tempo) or a ramp (log tempo linear in the beat number), with a penalty per segment of PEN x sigma^2 x log(n) (sigma: the
intervals' own jitter, from the median absolute successive difference). Between two segments, the tempo ratio says what
happened:

  octave    the beat level doubled or halved (a half-time / double-time feel, or the tracker's metrical level): not a
            tempo change (AUTO RANGE folds it, the user 2026-11-15)
  triplet   a 3:2 / 4:3 relation (a shuffle feel or the tracker's level): reported apart, not counted as a change
  step      any other move past STEP_REL once folded into an octave: a real tempo change
  (smaller moves are not changes: the segments are merged)

A ramp segment whose tempo moves more than STEP_REL across it is a real change too (an accelerando / ritardando).
Each change gets a confidence from the second source, Beat This! final0 offline (no tempo prior, so it follows a change
on its own): 'both' when Beat This's local tempo across the change moves by the same folded ratio within AGREE_REL,
'madmom' when Beat This has too few beats there, 'disputed' when it disagrees.
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

MIN_SEG = 8             # beat intervals a segment needs (a fill or a 1-bar break cannot be a tempo section)
MIN_SECTION = 16        # a section shorter than this is a transition (the offline DBN's switch between beat levels, a
                        # fill): changes are judged between the full sections around it
MAX_RAMP = 0.15         # a ramp moving more than this is a transition artefact, not an accelerando
PEN = 6.0               # the segment penalty in sigma^2 log(n) units (synthetic tests: steady -> 1 segment)
RAMP_PEN = 4.0          # a ramp's extra parameter, same units
SIGMA_FLOOR = 0.004     # the log-interval jitter's floor (0.4 %)
STEP_REL = 0.02         # a folded move past this: a real tempo change
TRIP_TOL = 0.02        # a 3:2 / 4:3 relation within 2 %
AGREE_REL = 0.012
BT_WINDOW = 12          # Beat This intervals each side of a point for local_bpm
BT_MIN = 6
CONFIRM_S = 30.0        # Beat This's tempo over this much of each section next to a change
SNAP = 0.15             # of a period: a Beat This beat this close to the reference's level counts


@dataclass(frozen=True)
class Segment:
    i0: int             # first beat interval (index into diff(beats))
    i1: int             # one past the last
    t0: float           # time of its first beat
    t1: float           # time of its last beat
    bpm0: float         # tempo at its start and end (equal for a flat segment)
    bpm1: float
    ramp: bool

    def bpm_at(self, t: float) -> float:
        if not self.ramp or self.t1 <= self.t0:
            return self.bpm0
        f = min(max((t - self.t0) / (self.t1 - self.t0), 0.0), 1.0)
        return float(math.exp(math.log(self.bpm0) + f * (math.log(self.bpm1) - math.log(self.bpm0))))


@dataclass(frozen=True)
class Change:
    t: float            # time of the boundary (the first beat of the later segment), or a ramp's start
    t_end: float        # = t for a step; a ramp's end
    bpm_before: float
    bpm_after: float
    kind: str           # step / ramp / octave / triplet
    confidence: str     # both / madmom / disputed (a step or a ramp); '' for octave / triplet

    @property
    def folded(self) -> float:
        return fold_ratio(self.bpm_after / self.bpm_before)


def fold_ratio(r: float) -> float:
    """r times the power of 2 that brings it nearest 1."""
    return r / 2.0 ** round(math.log2(r))


def relation(r: float) -> str:
    """octave / triplet / step / same for a tempo ratio (folded moves of STEP_REL or less are no change)."""
    lr = math.log2(r)
    f = lr - round(lr)                                    # folded, in octaves
    if abs(f) <= math.log2(1 + STEP_REL):
        return "octave" if round(lr) != 0 else "same"
    if abs(abs(f) - math.log2(4 / 3)) <= math.log2(1 + TRIP_TOL):
        return "triplet"
    return "step"


def jitter(y: np.ndarray) -> float:
    d = np.diff(y)
    if d.size < 4:
        return SIGMA_FLOOR
    return max(float(np.median(np.abs(d - np.median(d)))) * 1.4826 / math.sqrt(2.0), SIGMA_FLOOR)


class _Sums:
    """Cumulative sums for O(1) flat and linear fits of y over [i, j)."""

    def __init__(self, y: np.ndarray) -> None:
        k = np.arange(y.size, dtype=float)
        z = np.zeros(1)
        self.n = np.concatenate([z, np.cumsum(np.ones_like(y))])
        self.k = np.concatenate([z, np.cumsum(k)])
        self.kk = np.concatenate([z, np.cumsum(k * k)])
        self.y = np.concatenate([z, np.cumsum(y)])
        self.yy = np.concatenate([z, np.cumsum(y * y)])
        self.ky = np.concatenate([z, np.cumsum(k * y)])

    def costs(self, i: np.ndarray, j: int) -> tuple[np.ndarray, np.ndarray]:
        """(flat RSS, linear RSS) of y[i:j] for every start in i."""
        n = self.n[j] - self.n[i]
        sk, skk = self.k[j] - self.k[i], self.kk[j] - self.kk[i]
        sy, syy, sky = self.y[j] - self.y[i], self.yy[j] - self.yy[i], self.ky[j] - self.ky[i]
        flat = syy - sy * sy / n
        vk = skk - sk * sk / n
        cky = sky - sk * sy / n
        with np.errstate(divide="ignore", invalid="ignore"):
            lin = np.where(vk > 0, flat - cky * cky / vk, flat)
        return np.maximum(flat, 0.0), np.maximum(lin, 0.0)

    def fit(self, i: int, j: int, ramp: bool) -> tuple[float, float]:
        """(y at the first interval, y at the last) of the segment's fit."""
        n = self.n[j] - self.n[i]
        sk, sy = self.k[j] - self.k[i], self.y[j] - self.y[i]
        if not ramp:
            return sy / n, sy / n
        vk = (self.kk[j] - self.kk[i]) - sk * sk / n
        slope = ((self.ky[j] - self.ky[i]) - sk * sy / n) / vk
        mid = sk / n
        return sy / n + slope * (i - mid), sy / n + slope * (j - 1 - mid)


def segment(beats: np.ndarray, pen: float = PEN, ramp_pen: float = RAMP_PEN,
            min_seg: int = MIN_SEG) -> list[Segment]:
    """The optimal flat / ramp segmentation of the beats' log intervals."""
    b = np.asarray(beats, dtype=float)
    y = np.log(np.diff(b))
    n = y.size
    if n < 2 * min_seg:
        return [Segment(0, n, float(b[0]), float(b[-1]), 60.0 / math.exp(float(np.median(y))),
                        60.0 / math.exp(float(np.median(y))), False)] if n else []
    s2 = jitter(y) ** 2
    seg_pen, extra = pen * s2 * math.log(n), ramp_pen * s2 * math.log(n)
    sums = _Sums(y)
    best = np.full(n + 1, np.inf)
    best[0] = 0.0
    back = np.zeros(n + 1, dtype=int)
    kind = np.zeros(n + 1, dtype=bool)
    for j in range(min_seg, n + 1):
        i = np.arange(0, j - min_seg + 1)
        i = i[np.isfinite(best[i])]
        if i.size == 0:
            continue
        flat, lin = sums.costs(i, j)
        lin = lin + extra
        c = best[i] + np.minimum(flat, lin) + seg_pen
        k = int(np.argmin(c))
        best[j], back[j], kind[j] = c[k], i[k], lin[k] < flat[k]
    segs = []
    j = n
    while j > 0:
        i = int(back[j])
        ya, yb = sums.fit(i, j, bool(kind[j]))
        segs.append(Segment(i, j, float(b[i]), float(b[j]), 60.0 / math.exp(ya), 60.0 / math.exp(yb), bool(kind[j])))
        j = i
    return merge(segs[::-1], y)


def merge(segs: list[Segment], y: np.ndarray) -> list[Segment]:
    """Neighbours whose tempo differs by STEP_REL or less (folded, flat ones only) become one flat segment."""
    out: list[Segment] = []
    for s in segs:
        if out and not s.ramp and not out[-1].ramp and relation(s.bpm0 / out[-1].bpm1) == "same":
            p = out.pop()
            bpm = 60.0 / math.exp(float(np.mean(y[p.i0:s.i1])))
            s = Segment(p.i0, s.i1, p.t0, s.t1, bpm, bpm, False)
        out.append(s)
    return out


def local_bpm(beats: np.ndarray, t: float, side: int, n: int = BT_WINDOW) -> float | None:
    """Tempo of up to n intervals just before (side -1) or after (+1) t: least squares of the beat times on their
    numbers, missed beats bridged by the median interval (Beat This's 50 fps frames make single intervals coarse)."""
    b = np.asarray(beats, dtype=float)
    w = b[b <= t][-(n + 1):] if side < 0 else b[b >= t][:n + 1]
    if w.size < BT_MIN + 1:
        return None
    d = np.diff(w)
    m = float(np.median(d))
    if m <= 0.2:
        return None
    k = np.concatenate([[0.0], np.cumsum(np.maximum(np.round(d / m), 1.0))])
    ok = np.abs(d / np.maximum(np.round(d / m), 1.0) / m - 1) < 0.08
    if ok.sum() < BT_MIN:
        return None
    period = float(np.polyfit(k, w, 1)[0])
    return 60.0 / period if period > 0 else None


def section_bpm(beats: np.ndarray, t0: float, t1: float, period: float, min_beats: int = 8) -> float | str | None:
    """A second source's tempo over [t0, t1], its beats numbered on the reference's period there (a beat more than
    SNAP periods from a whole number is not at the reference's level: a double, an off-beat, a 4:3 beat). None: fewer
    than min_beats there (no say); 'other': enough beats but under half the expected ones at that level (the second
    source hears another tempo there)."""
    b = np.asarray(beats, dtype=float)
    w = b[(b >= t0) & (b <= t1)]
    if w.size < min_beats or period <= 0:
        return None
    x = (w - w[0]) / period
    k = np.round(x)
    keep = np.abs(x - k) < SNAP
    k, w = k[keep], w[keep]
    k, first = np.unique(k, return_index=True)
    w = w[first]
    if k.size < min_beats or k.size < 0.5 * (t1 - t0) / period:
        return "other"
    p = float(np.polyfit(k, w, 1)[0])
    return 60.0 / p if p > 0 else None


def confirm(bt: np.ndarray | None, before: Segment, after: Segment, ratio: float) -> str:
    """'both' / 'madmom' / 'disputed': does Beat This's tempo, over the CONFIRM_S of each section next to the change
    (at madmom's beat level), move by the same ratio?"""
    if bt is None or len(bt) == 0:
        return "madmom"
    a = section_bpm(bt, max(before.t0, before.t1 - CONFIRM_S), before.t1, 60.0 / before.bpm1)
    b = section_bpm(bt, after.t0, min(after.t1, after.t0 + CONFIRM_S), 60.0 / after.bpm0)
    return _verdict(a, b, ratio)


def _verdict(a, b, ratio: float) -> str:
    if a == "other" or b == "other":
        return "disputed"
    if a is None or b is None:
        return "madmom"
    return "both" if abs(math.log((b / a) / ratio)) <= math.log(1 + AGREE_REL) else "disputed"


def confirm_ramp(bt: np.ndarray | None, s: Segment) -> str:
    """A ramp: Beat This's tempo over its first and last quarter moves by the same ratio."""
    if bt is None or len(bt) == 0:
        return "madmom"
    q = (s.t1 - s.t0) / 4
    a = section_bpm(bt, s.t0, s.t0 + q, 60.0 / s.bpm_at(s.t0 + q / 2), min_beats=6)
    b = section_bpm(bt, s.t1 - q, s.t1, 60.0 / s.bpm_at(s.t1 - q / 2), min_beats=6)
    return _verdict(a, b, s.bpm_at(s.t1 - q / 2) / s.bpm_at(s.t0 + q / 2))


def sections(segs: list[Segment]) -> list[Segment]:
    """The segments long enough to be tempo sections (MIN_SECTION intervals; a ramp within MAX_RAMP)."""
    return [s for s in segs if s.i1 - s.i0 >= MIN_SECTION and (not s.ramp or abs(s.bpm1 / s.bpm0 - 1) <= MAX_RAMP)]


def changes(segs: list[Segment], bt: np.ndarray | None = None) -> list[Change]:
    """The changes between consecutive sections (a transition between them is skipped) and the ramps."""
    secs = sections(segs)
    out = []
    for p, s in zip(secs, secs[1:]):
        r = s.bpm0 / p.bpm1
        kind = relation(r)
        if kind == "same":
            continue
        conf = confirm(bt, p, s, r) if kind == "step" else ""
        out.append(Change(s.t0, s.t0, p.bpm1, s.bpm0, kind, conf))
    for s in secs:
        if s.ramp and abs(s.bpm1 / s.bpm0 - 1) > STEP_REL:
            out.append(Change(s.t0, s.t1, s.bpm0, s.bpm1, "ramp", confirm_ramp(bt, s)))
    return sorted(out, key=lambda c: c.t)


def ref_bpm(segs: list[Segment], t: np.ndarray) -> np.ndarray:
    """The reference tempo at times t (NaN outside the beats)."""
    out = np.full(np.shape(t), np.nan)
    for s in segs:
        sel = (t >= s.t0) & (t < s.t1)
        out[sel] = [s.bpm_at(float(x)) for x in np.asarray(t)[sel]] if s.ramp else s.bpm0
    return out


def track_class(chs: list[Change]) -> str:
    """'changing' (a real change, step or ramp, that Beat This confirms), 'probable' (madmom's only: Beat This has too
    few beats there), 'disputed' (Beat This hears no such change), or 'steady'."""
    real = [c for c in chs if c.kind in ("step", "ramp")]
    if any(c.confidence == "both" for c in real):
        return "changing"
    if any(c.confidence == "madmom" for c in real):
        return "probable"
    return "disputed" if real else "steady"
