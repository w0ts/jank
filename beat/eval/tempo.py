"""The BPM metric: is the tempo the system would publish right, and how fast does it get there?

At each true beat r (after a warm-up), the published BPM comes from the tracker's beats in the last W seconds
(known by r - latency): a least-squares line through them, missed beats bridged, folded into the octave range
around the true tempo (a BPM range setting). The true BPM comes from the reference beats in the same window.

Reported per window W:
  within_0.1 / within_0.5   share of beats with |BPM error| <= 0.1 / 0.5 BPM (at 140: ~5 ms / ~24 ms drift per 4 bars)
  within_2pct               share within 2 % (the usual tempo-estimation tolerance, loose for Link)
  median_err                median |BPM error| over the beats that had a BPM
  lock_s                    seconds until the BPM is within 0.5 and stays there for the rest of the piece (NaN: never)
"""
from __future__ import annotations

import numpy as np

from eval.grid import _fold

WINDOWS_S = (4.0, 8.0, 16.0)
WARMUP_S = 4.0
LOCK_BPM = 0.5


def _period(beats: np.ndarray) -> float | None:
    """Least-squares beat period through beats, missed beats bridged by rounding to the median interval."""
    if beats.size < 4:
        return None
    first = float(np.median(np.diff(beats)))
    if first <= 0.2:
        return None
    idx = np.round((beats - beats[-1]) / first)
    if np.unique(idx).size < 4:
        return None
    return float(np.polyfit(idx, beats, 1)[0])


def bpm_errors(ref: np.ndarray, est: np.ndarray, window_s: float, latency_s: float = 0.0) -> tuple[np.ndarray, np.ndarray]:
    """(times, signed BPM error) at each true beat after the warm-up; NaN where no BPM could be published."""
    ref = np.asarray(ref, dtype=float)
    est = np.sort(np.asarray(est, dtype=float))
    times = ref[ref >= WARMUP_S]
    errs = []
    for r in times:
        now = r - latency_s
        true_p = _period(ref[(ref > r - window_s) & (ref <= r)])
        est_p = _period(est[(est > now - window_s) & (est <= now)])
        if true_p is None or est_p is None:
            errs.append(np.nan)
            continue
        errs.append(60.0 / _fold(est_p, true_p) - 60.0 / true_p)
    return times, np.array(errs)


def tempo_score(ref: np.ndarray, est: np.ndarray, latency_s: float = 0.0) -> dict:
    out = {}
    for w in WINDOWS_S:
        times, e = bpm_errors(ref, est, w, latency_s)
        if e.size == 0:
            continue
        a = np.abs(e)
        rel = a / (60.0 / np.median(np.diff(ref)))
        bad = ~(a <= LOCK_BPM)                       # NaN counts as not locked
        last_bad = np.flatnonzero(bad)
        if last_bad.size == 0:
            lock = float(times[0])
        elif last_bad[-1] == len(times) - 1:
            lock = float("nan")
        else:
            lock = float(times[last_bad[-1] + 1])
        k = f"{int(w)}s"
        out[k] = {
            "within_0.1": float(np.mean(a <= 0.1)), "within_0.5": float(np.mean(a <= 0.5)),
            "within_2pct": float(np.mean(rel <= 0.02)),
            "median_err": float(np.nanmedian(a)) if np.any(~np.isnan(a)) else float("nan"), "lock_s": lock,
        }
    return out
