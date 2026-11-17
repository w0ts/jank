"""Half-beat-off reference grids: a Traktor grid against independent beat trackers ("some Traktor tracks are half a
beat out, classic Traktor bug", the user 2026-11-15).

A reference grid is half a beat out when the music's beats sit half a period between its beats, at the right tempo,
for the whole track. Per evidence source (one tracker's beats on the track):

  beat_phases     each tracked beat's phase against the grid, in periods wrapped to [-0.5, 0.5), keeping only beats
                  after a warm-up whose interval to the previous beat is the grid's period (same tempo: a tracker at
                  double or half tempo, or one that missed a beat, says nothing about the phase there)
  axial_bias      a source's constant offset to the grids of a batch of tracks (a decoder delay of one download batch,
                  a tracker's lag): the mean of the DOUBLED phase angle, so half-beat-off tracks do not pull it
  source_verdict  'on' / 'half' (most beats nearer the grid's half point than its beat, after the bias) / 'mixed' /
                  'none' (too few beats at the grid's tempo)

combine() votes per track: the primary source (madmom offline) against the others (online trackers of other families):
  traktor_off  the primary and the others' majority say half          -> shift the grid by half a period
  madmom_off   the primary says half, the others' majority say on     -> the grid is kept
  ok           the primary says on (or has no say and the others say on, none half)
  unclear      the rest: conflicting evidence                         -> excluded from scoring
  no_evidence  nobody measured the track                              -> kept as is

Fix records ({name: {"verdict": ..., "shift": 0.5 | 0.0}}) are applied by fixed_refs(); the scorers take them with
--ref-fix (eval.rescore, the downbeat study's v2/build.py and v2/final.py).
"""
from __future__ import annotations

import json
from collections.abc import Mapping
from pathlib import Path

import numpy as np

WARMUP_S = 20.0          # online trackers settle; the grid's start before the music is skipped too
TEMPO_TOL = 0.04         # a tracked interval within 4 % of the grid's period counts as the grid's tempo
NEAR_HALF = 0.25         # |phase| beyond a quarter period: nearer the grid's half point than its beat
MIN_BEATS = 32           # tempo-matched beats a source needs to have a say
MIN_TEMPO_SHARE = 0.3    # ... and that share of its beats at the grid's tempo
MIN_SIDE_SHARE = 0.7     # a verdict needs this share of beats on one side
VERDICTS = ("traktor_off", "madmom_off", "ok", "unclear", "no_evidence")
EXCLUDED = ("unclear",)


def wrap(x: np.ndarray | float) -> np.ndarray | float:
    """Phase in periods wrapped to [-0.5, 0.5)."""
    return (np.asarray(x) + 0.5) % 1.0 - 0.5


def grid_period(ref: np.ndarray) -> float:
    return float(np.median(np.diff(ref)))


def beat_phases(ref: np.ndarray, est: np.ndarray, warmup_s: float = WARMUP_S,
                tempo_tol: float = TEMPO_TOL) -> tuple[np.ndarray, float]:
    """(phases of the tempo-matched tracked beats in periods, share of tracked beats at the grid's tempo)."""
    ref = np.asarray(ref, dtype=float)
    est = np.sort(np.asarray(est, dtype=float))
    if ref.size < 2 or est.size < 2:
        return np.zeros(0), 0.0
    period, g = grid_period(ref), float(ref[0])
    t, ioi = est[1:], np.diff(est)
    late = t >= max(warmup_s, float(ref[0]))
    same = np.abs(ioi / period - 1.0) <= tempo_tol
    keep = late & same
    share = float(keep.sum() / late.sum()) if late.any() else 0.0
    return np.asarray(wrap((t[keep] - g) / period)), share


def circ_mean(phases: np.ndarray) -> tuple[float, float]:
    """(mean phase in periods, resultant length R in [0, 1])."""
    if phases.size == 0:
        return float("nan"), 0.0
    z = np.exp(2j * np.pi * phases).mean()
    return float(wrap(np.angle(z) / (2 * np.pi))), float(abs(z))


def axial_bias(phases: np.ndarray) -> float:
    """A constant offset in (-0.25, 0.25] periods that is blind to half-period flips (mean of the doubled angle)."""
    phases = np.asarray(phases, dtype=float)
    phases = phases[np.isfinite(phases)]
    if phases.size == 0:
        return 0.0
    z = np.exp(4j * np.pi * phases).mean()
    return float(np.angle(z) / (4 * np.pi))


def half_share(phases: np.ndarray, bias: float = 0.0) -> float:
    """Share of beats nearer the grid's half point than its beat, after removing the bias."""
    if phases.size == 0:
        return float("nan")
    return float(np.mean(np.abs(wrap(phases - bias)) > NEAR_HALF))


def source_verdict(phases: np.ndarray, tempo_share: float, bias: float = 0.0) -> str:
    if phases.size < MIN_BEATS or tempo_share < MIN_TEMPO_SHARE:
        return "none"
    h = half_share(phases, bias)
    if h >= MIN_SIDE_SHARE:
        return "half"
    if h <= 1.0 - MIN_SIDE_SHARE:
        return "on"
    return "mixed"


def combine(verdicts: Mapping[str, str], primary: str = "madmom_offline") -> str:
    """One track's verdict from its sources' verdicts (see the module docstring)."""
    p = verdicts.get(primary, "none")
    others = [v for k, v in verdicts.items() if k != primary]
    n_half, n_on = others.count("half"), others.count("on")
    if p == "half":
        if n_half >= 1 and n_half >= n_on:
            return "traktor_off"
        return "madmom_off" if n_on > n_half else "unclear"
    if p == "on":
        return "unclear" if (n_half >= 2 and n_on == 0) else "ok"
    if p == "mixed":
        return "unclear" if n_half else ("ok" if n_on else "unclear")
    if n_half:
        return "unclear"
    return "ok" if n_on else "no_evidence"


def shift_grid(beats: np.ndarray, shift: float) -> np.ndarray:
    """The constant grid moved by `shift` periods, kept inside the original grid's span (one beat may be added at
    the start)."""
    beats = np.asarray(beats, dtype=float)
    if shift == 0.0 or beats.size < 2:
        return beats
    period = grid_period(beats)
    new = beats + shift * period
    first = new[0] - period
    if first >= 0.0:
        new = np.concatenate([[first], new])
    return np.round(new[new <= beats[-1]], 4)


def load_fixes(path: Path | str | None) -> dict[str, dict]:
    return json.loads(Path(path).read_text()) if path else {}


def fixed_refs(refs: Mapping[str, np.ndarray], fixes: Mapping[str, dict]) -> dict[str, np.ndarray]:
    """The references with the half-beat fixes applied: traktor_off grids shifted, unclear tracks dropped."""
    out = {}
    for name, beats in refs.items():
        fix = fixes.get(name, {})
        if fix.get("verdict") in EXCLUDED:
            continue
        out[name] = shift_grid(beats, float(fix.get("shift", 0.0)))
    return out
