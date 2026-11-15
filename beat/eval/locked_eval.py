"""The growing-window BPM estimator on a tracker's saved beats from the user's tracks.

  steady   each track on its own (constant tempo): BPM error against Traktor's BPM as listening goes on
  splice   two tracks back to back (a DJ cut): how fast the BPM follows the new tempo

    .venv/bin/python -m eval.locked_eval --tracker madmom_online
"""
from __future__ import annotations

import argparse
import json

import numpy as np

from eval.data import DATA
from eval.grid import _fold
from eval.glide_bpm import GlideBpm
from eval.locked_bpm import LockedBpm
from eval.rescore import _rows

WARMUP_S = 4.0
LOCK_BPM = 0.1
LOCK_HOLD = 8                     # beats in a row within LOCK_BPM = locked
CURVE_S = (15, 30, 60, 120, 240)
SWITCH_S = 120.0                  # splice: track A until here, then track B


def bpm_trace(beats: np.ndarray, **settings) -> tuple[np.ndarray, np.ndarray]:
    """Published BPM after each tracked beat (times, bpm); NaN until the estimator has a tempo."""
    times, bpms, _ = bpm_trace_est(beats, **settings)
    return times, bpms


ESTIMATORS = {"locked": LockedBpm, "glide": GlideBpm}


def bpm_trace_est(beats: np.ndarray, kind: str = "locked", **settings) -> tuple[np.ndarray, np.ndarray, LockedBpm]:
    """bpm_trace, plus the estimator itself (its restart count). kind: 'locked' or 'glide'."""
    est = ESTIMATORS[kind](**settings)
    times, bpms = [], []
    for t in beats:
        est.update(float(t))
        times.append(t)
        bpms.append(est.bpm() or np.nan)
    return np.array(times), np.array(bpms), est


def folded_error(bpm: np.ndarray, true_bpm) -> np.ndarray:
    """|BPM error| after folding into the true tempo's octave (a BPM range setting)."""
    true = np.broadcast_to(np.asarray(true_bpm, dtype=float), bpm.shape)
    out = np.full(bpm.shape, np.nan)
    ok = ~np.isnan(bpm)
    out[ok] = [abs(60.0 / _fold(60.0 / b, 60.0 / tb) - tb) for b, tb in zip(bpm[ok], true[ok])]
    return out


def first_lock(times: np.ndarray, err: np.ndarray, start: float = 0.0) -> float:
    """Seconds from `start` until the error stays within LOCK_BPM for LOCK_HOLD beats in a row (NaN: never)."""
    run = 0
    for t, e in zip(times, err):
        if t < start:
            continue
        run = run + 1 if e <= LOCK_BPM else 0
        if run >= LOCK_HOLD:
            return float(t - start)
    return float("nan")


def steady(rows: list[dict], truth: dict, kind: str = "locked") -> dict:
    within01, within05, locks, after_lock = [], [], [], []
    curve = {s: [] for s in CURVE_S}
    for r in rows:
        times, bpms = bpm_trace(np.array(r["est_beats"]), kind=kind)
        err = folded_error(bpms, truth[r["name"]])
        keep = times >= WARMUP_S
        if not keep.any():
            continue
        e = err[keep]
        within01.append(np.mean(e <= 0.1))
        within05.append(np.mean(e <= 0.5))
        lock = first_lock(times, err)
        locks.append(lock)
        if lock == lock:
            after_lock.append(np.mean(err[times >= lock] <= LOCK_BPM))
        for s in CURVE_S:
            at = np.searchsorted(times, s)
            if at < times.size:
                curve[s].append(err[at])
    locks = np.array(locks)
    return {
        "tracks": len(within01), "within_0.1": float(np.mean(within01)), "within_0.5": float(np.mean(within05)),
        "first_lock_median_s": float(np.nanmedian(locks)), "never_locks": float(np.mean(np.isnan(locks))),
        "locked_after_first_lock": float(np.mean(after_lock)),
        "median_err_at": {f"{s}s": float(np.nanmedian(curve[s])) for s in CURVE_S},
    }


def splice(rows: list[dict], truth: dict, pairs: int, seed: int = 0, kind: str = "locked") -> dict:
    """Track A's beats until SWITCH_S, then track B's beats (from its own 60 s mark) moved to follow on."""
    rng = np.random.default_rng(seed)
    relock, err_end = [], []
    for _ in range(pairs):
        a, b = rng.choice(len(rows), 2, replace=False)
        ra, rb = rows[a], rows[b]
        ta, tb = truth[ra["name"]], truth[rb["name"]]
        if abs(ta - tb) < 1.0:
            continue
        ba = np.array(ra["est_beats"])
        bb = np.array(rb["est_beats"])
        bb = bb[bb >= 60.0] - 60.0 + SWITCH_S
        beats = np.concatenate([ba[ba < SWITCH_S], bb])
        times, bpms = bpm_trace(beats, kind=kind)
        true = np.where(times < SWITCH_S, ta, tb)
        err = folded_error(bpms, true)
        relock.append(first_lock(times, np.where(np.abs(true - tb) < 1e-9, err, np.inf), SWITCH_S))
        tail = err[times > SWITCH_S + 60]
        if tail.size:
            err_end.append(np.nanmedian(tail))
    relock = np.array(relock)
    return {"pairs": int(relock.size), "relock_median_s": float(np.nanmedian(relock)),
            "relock_p90_s": float(np.nanpercentile(relock, 90)), "never_relocks": float(np.mean(np.isnan(relock))),
            "median_err_60s_after": float(np.median(err_end))}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracker", default="madmom_online")
    ap.add_argument("--estimator", choices=["locked", "glide"], default="locked")
    ap.add_argument("--pairs", type=int, default=300)
    args = ap.parse_args()
    truth = {r["name"]: r["bpm"] for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", args.tracker) if "error" not in r and r["name"] in truth and "window" not in r]
    print(json.dumps({"tracker": args.tracker, "estimator": args.estimator, "steady": steady(rows, truth, args.estimator)}, indent=1))
    print(json.dumps({"splice": splice(rows, truth, args.pairs, kind=args.estimator)}, indent=1))


if __name__ == "__main__":
    main()
