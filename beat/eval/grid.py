"""The Link metric: how well a grid published from the past lands on the next true beat.

For each true beat r (after a warm-up), take the tracker's beats known by time r - latency - lead
(latency = the tracker's own look-ahead, lead = how early the grid must be out, e.g. for MIDI clock),
fit tempo and phase to the last few of them, and extrapolate the grid to r.

Per true beat the outcome is one of:
  tight / loose  the grid is at the right tempo and within 20 / 50 ms of the beat
  octave         the grid runs at 1/2 or 2x the true tempo (wrong BPM for Link; a BPM range setting fixes it)
  triplet        the grid runs at 2/3 or 3/2 the true tempo (no range setting fixes it)
  offbeat        right tempo, half a period out
  off            right tempo, otherwise out
  lost           no grid yet
"""
from __future__ import annotations

import numpy as np

WARMUP_S = 5.0
FIT_BEATS = 6          # the grid comes from the last 6 tracked beats
TIGHT_MS = 20.0        # feels locked
LOOSE_MS = 50.0        # audibly flamming beyond this
OCTAVE_TOL = 0.10     # within 10 % of a wrong ratio of the true tempo
OCTAVES = (0.5, 2.0)                        # half / double: a BPM range setting folds these away
TRIPLETS = (2 / 3, 1.5, 0.75, 4 / 3)        # 2:3 feel (and 3:4 after folding): no range setting fixes these


def _fit(beats: np.ndarray) -> tuple[float, float] | None:
    """Period and phase over the last FIT_BEATS beats, bridging missed beats.

    The median interval gives a first period; each beat gets its grid index by rounding its distance
    from the last beat, so a missed beat becomes a gap of two indices instead of a slower tempo.
    A least-squares line through (index, time) then refines period and phase.
    """
    b = beats[-FIT_BEATS:]
    if b.size < 3:
        return None
    first = float(np.median(np.diff(b)))
    if first <= 0.2:
        return None
    idx = np.round((b - b[-1]) / first)
    if np.unique(idx).size < 3:
        return None
    period, t_last = np.polyfit(idx, b, 1)
    return (float(period), float(t_last)) if period > 0.2 else None


def _true_period(ref: np.ndarray, r: float) -> float:
    i = int(np.searchsorted(ref, r))
    window = ref[max(0, i - 4): i + 1]
    return float(np.median(np.diff(window))) if window.size > 1 else float(np.median(np.diff(ref)))


def _fold(period: float, true_period: float) -> float:
    """What a BPM range setting one octave wide around the true tempo does: halve or double the tracker's period
    until it is within a factor sqrt(2) of the truth. Doubling the period keeps the last beat as the anchor, so a
    double-tempo tracker lands on the off-beats about half the time, as a real controller would."""
    while period < true_period / np.sqrt(2):
        period *= 2
    while period >= true_period * np.sqrt(2):
        period /= 2
    return period


def _classify(ref: np.ndarray, r: float, fit, fold: bool = False) -> tuple[str, float]:
    if fit is None:
        return "lost", np.nan
    period, last = fit
    true_period = _true_period(ref, r)
    if fold:
        period = _fold(period, true_period)
    ratio = period / true_period
    # only wrong-ratio grids fail on tempo; live tempo drift within that shows up as phase error below
    if any(abs(ratio / o - 1) <= OCTAVE_TOL for o in OCTAVES):
        return "octave", np.nan
    if any(abs(ratio / o - 1) <= OCTAVE_TOL for o in TRIPLETS):
        return "triplet", np.nan
    err_ms = (last + np.round((r - last) / period) * period - r) * 1000.0
    a = abs(err_ms)
    if a <= TIGHT_MS:
        return "tight", err_ms
    if a <= LOOSE_MS:
        return "loose", err_ms
    if abs(a - period * 500.0) <= LOOSE_MS:
        return "offbeat", err_ms
    return "off", err_ms


KINDS = ("tight", "loose", "octave", "triplet", "offbeat", "off", "lost")


def grid_score(ref: np.ndarray, est: np.ndarray, latency_s: float = 0.0, lead_s: float = 0.0,
               fold: bool = False) -> dict:
    """Shares of true beats per outcome (tight and loose cumulative: loose includes tight), median |error|.
    fold=True scores as if the user had set a BPM range one octave wide around the true tempo."""
    est = np.sort(np.asarray(est, dtype=float))
    ref = np.asarray(ref, dtype=float)
    beats = ref[ref >= WARMUP_S]
    if beats.size == 0:
        return {}
    outcomes = [_classify(ref, r, _fit(est[est <= r - latency_s - lead_s]), fold) for r in beats]
    kinds = [k for k, _ in outcomes]
    locked = np.array([abs(e) for k, e in outcomes if k in ("tight", "loose")])
    share = {k: kinds.count(k) / beats.size for k in KINDS}
    share["loose"] += share["tight"]
    share["median_ms"] = float(np.median(locked)) if locked.size else float("nan")
    return share
