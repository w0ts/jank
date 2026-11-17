"""Tune the BPM estimator's change detection: steady precision and false restarts vs how fast it follows a cut.

    .venv/bin/python -m eval.locked_tune --tracker madmom_online
"""
from __future__ import annotations

import argparse
import itertools
import json

import numpy as np

from eval.data import DATA
from eval.locked_eval import SWITCH_S, LOCK_BPM, bpm_trace_est, first_lock, folded_error
from eval.rescore import _rows

CHANGE_REL = (0.005, 0.01, 0.02, 0.04)
CHANGE_BEATS = (8, 16, 32)


def steady(rows, truth, settings) -> tuple[float, float]:
    """(share of beats within 0.1 BPM after 4 s, median restarts per track)."""
    shares, restarts = [], []
    for r in rows:
        times, bpms, est = bpm_trace_est(np.array(r["est_beats"]), **settings)
        err = folded_error(bpms, truth[r["name"]])
        keep = times >= 4.0
        if keep.any():
            shares.append(np.mean(err[keep] <= LOCK_BPM))
            restarts.append(est.restarts)
    return float(np.mean(shares)), float(np.median(restarts))


def relock(rows, truth, settings, pairs: int, seed: int = 0) -> tuple[float, float]:
    """(median seconds to lock on the new tempo after a cut, share that never re-lock)."""
    rng = np.random.default_rng(seed)
    out = []
    for _ in range(pairs):
        a, b = rng.choice(len(rows), 2, replace=False)
        ta, tb = truth[rows[a]["name"]], truth[rows[b]["name"]]
        if abs(ta - tb) < 1.0:
            continue
        ba, bb = np.array(rows[a]["est_beats"]), np.array(rows[b]["est_beats"])
        beats = np.concatenate([ba[ba < SWITCH_S], bb[bb >= 60.0] - 60.0 + SWITCH_S])
        times, bpms, _ = bpm_trace_est(beats, **settings)
        err = folded_error(bpms, np.where(times < SWITCH_S, ta, tb))
        out.append(first_lock(times, np.where(times >= SWITCH_S, err, np.inf), SWITCH_S))
    out = np.array(out)
    return float(np.nanmedian(out)), float(np.mean(np.isnan(out)))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracker", default="madmom_online")
    ap.add_argument("--pairs", type=int, default=200)
    ap.add_argument("--rel", type=float, nargs="*", default=list(CHANGE_REL))
    args = ap.parse_args()
    truth = {r["name"]: r["bpm"] for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", args.tracker) if "error" not in r and r["name"] in truth and "window" not in r]
    for rel, beats in itertools.product(args.rel, CHANGE_BEATS):
        settings = {"change_rel": rel, "change_beats": beats}
        within, restarts = steady(rows, truth, settings)
        lock_s, never = relock(rows, truth, settings, args.pairs)
        print(json.dumps({"change_rel": rel, "change_beats": beats, "within_0.1": round(within, 4),
                          "restarts_median": restarts, "relock_median_s": round(lock_s, 1),
                          "never_relocks": round(never, 3)}), flush=True)


if __name__ == "__main__":
    main()
