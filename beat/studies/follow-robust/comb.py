"""Comb / autocorrelation checks on the live activation (changes 3 and 4), causal: only frames up to `end` are read.

  curve      the mean-removed activation's autocorrelation over the last `span` frames, unbiased (mean over the overlap),
             divided by the window's variance: 1 at lag 0, ~0 for no relation; read at fractional lags by interpolation
  comb       the mean of the curve at m x period for m in LAGS (1, 2, 4 beats: Davies-Plumbley's comb, Boeck's ACF)
  ratio_margin  comb(P) minus the best comb of P's 3:4, 4:3, 2:3, 3:2 relatives (the metrical errors the per-track data
             shows: 0.75 / 1.33 folded); > margin: P wins
  tempo_agreement  comb(P) over the best comb of every DBN period 33-67 frames (P's own octave relatives count as P):
             1 when the activation's strongest periodicity is the grid's, lower otherwise (change 4's T)
  phase_agreement  the activation at the published lines (max within +-2 frames) over the best of 8 shifts of the same
             lines by an eighth of a period: 1 when the grid sits on the activation's pulse (change 4's PHI, PLP-like)

Device cost (operations, direct sums, no FFT): comb at one period = 3 lags x 2 (interpolation) x span multiply-adds
(1,000 frames: 6,000); ratio_margin = 5 combs = 30,000 multiply-adds per check (at a lock or jump only);
tempo_agreement over 35 periods ~210,000 per evaluation (every EVERY_S, not every frame); phase_agreement 8 shifts x the
window's lines x 5 compares (a few hundred).
"""
from __future__ import annotations

import math

import numpy as np

LAGS = (1, 2, 4)
RELATIVES = (0.75, 4.0 / 3.0, 2.0 / 3.0, 1.5)
FPS = 100


def curve(act: np.ndarray, end: int, span: int, max_lag: int) -> np.ndarray | None:
    """The normalised unbiased autocorrelation of frames end-span+1 .. end at lags 0..max_lag (None: too short / flat)."""
    lo = max(end - span + 1, 0)
    x = np.asarray(act[lo:end + 1], dtype=np.float64)
    if x.size < max(span // 2, max_lag + 16):
        return None
    x = x - x.mean()
    n = x.size
    f = np.fft.rfft(x, 2 * n)
    r = np.fft.irfft(f * np.conj(f), 2 * n)[:max_lag + 1]
    if r[0] <= 1e-12:
        return None
    r = r / (n - np.arange(max_lag + 1))
    return r / r[0]


def at(r: np.ndarray, lag: float) -> float:
    lo = int(math.floor(lag))
    if lo + 1 >= r.size or lo < 0:
        return float("nan")
    w = lag - lo
    return float((1.0 - w) * r[lo] + w * r[lo + 1])


def comb(r: np.ndarray, period: float, lags=LAGS) -> float:
    """Mean of the curve at the multiples of `period` (frames) that fit it."""
    vals = [at(r, m * period) for m in lags]
    vals = [v for v in vals if v == v]
    return float(np.mean(vals)) if vals else float("nan")


def max_lag_for(period_max: float, lags=LAGS, relatives=RELATIVES) -> int:
    return int(math.ceil(max(lags) * period_max * max(max(relatives), 2.0))) + 2


def ratio_margin(r: np.ndarray, period: float, relatives=RELATIVES) -> float:
    """comb(P) - max comb of P's 3:4 / 4:3 / 2:3 / 3:2 relatives."""
    own = comb(r, period)
    alts = [comb(r, period * q) for q in relatives]
    alts = [a for a in alts if a == a]
    if own != own or not alts:
        return float("nan")
    return own - max(alts)


def tempo_agreement(r: np.ndarray, period: float, periods: np.ndarray) -> float:
    """comb(P, or its octave in range) over the best comb of the DBN's periods, clipped to 0..1."""
    lo, hi = float(periods.min()), float(periods.max())
    own = [comb(r, p) for p in (period, 2 * period, period / 2) if lo - 0.5 <= p <= hi + 0.5]
    own = [v for v in own if v == v]
    best = [comb(r, float(p)) for p in periods]
    best = [v for v in best if v == v]
    if not own or not best or max(best) <= 0:
        return 0.0
    return float(min(max(max(own) / max(best), 0.0), 1.0))


def phase_agreement(act: np.ndarray, lines: np.ndarray, period: float, end: int, shifts: int = 8, half: int = 2) -> float:
    """Mean activation peak at the lines over the best of `shifts` equal shifts of them (0..1)."""
    if lines.size < 4 or period <= 0:
        return 0.0
    vals = []
    for s in range(shifts):
        fr = np.round((lines + s * period / shifts) * FPS).astype(int)
        fr = fr[(fr - half >= 0) & (fr + half <= end)]
        if fr.size == 0:
            vals.append(0.0)
            continue
        vals.append(float(np.mean([act[f - half:f + half + 1].max() for f in fr])))
    best = max(vals)
    return float(vals[0] / best) if best > 0 else 0.0
