"""Smooth tempo changes (a DJ riding the pitch): real tracked beats time-warped into a tempo ramp.

Each track's tracked beats are warped so that, from RAMP_START_S on, the tempo glides by `change` (e.g. +3 %) over
`ramp_s` seconds, then holds. The true BPM follows the same warp. The beats keep their real jitter and misses.

    .venv/bin/python -m eval.locked_ramp --tracker madmom_online
"""
from __future__ import annotations

import argparse
import json

import numpy as np

from eval.data import DATA
from eval.locked_eval import LOCK_BPM, bpm_trace_est, first_lock, folded_error
from eval.rescore import _rows

RAMP_START_S = 90.0
RAMPS = ((0.02, 30.0), (0.04, 60.0), (-0.03, 20.0), (0.06, 120.0))   # (relative change, seconds)


def warp(t: np.ndarray, change: float, ramp_s: float) -> tuple[np.ndarray, callable]:
    """New times for original times t, and the tempo factor at a new time. The tempo factor s(u) goes linearly
    from 1 to 1 + change over [RAMP_START_S, RAMP_START_S + ramp_s] of original time u; new time = integral 1/s."""
    a, d = RAMP_START_S, ramp_s

    def new_time(u):
        u = np.asarray(u, dtype=float)
        before = np.minimum(u, a)
        x = np.clip(u - a, 0, d)
        during = d / change * np.log1p(change * x / d)
        after = np.maximum(u - a - d, 0) / (1 + change)
        return before + during + after

    us = np.linspace(0, t.max() + 1, 20000)
    ts = new_time(us)
    factor = 1 + change * np.clip(us - a, 0, d) / d
    return new_time(t), lambda tn: np.interp(tn, ts, factor)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracker", default="madmom_online")
    ap.add_argument("--estimator", choices=["locked", "glide"], default="locked")
    args = ap.parse_args()
    truth = {r["name"]: r["bpm"] for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", args.tracker) if "error" not in r and r["name"] in truth and "window" not in r]
    rows = [r for r in rows if len(r["est_beats"]) and r["est_beats"][-1] > RAMP_START_S + 150]
    for change, ramp_s in RAMPS:
        during, lag_bpm, relock = [], [], []
        for r in rows:
            beats, factor = warp(np.array(r["est_beats"]), change, ramp_s)
            times, bpms, _ = bpm_trace_est(beats, kind=args.estimator)
            true = truth[r["name"]] * factor(times)
            err = folded_error(bpms, true)
            end = RAMP_START_S + ramp_s / (1 + change / 2)       # approx. new time when the ramp ends
            mid = (times > RAMP_START_S + 5) & (times < end)
            if mid.any():
                during.append(np.nanmedian(err[mid]))
            relock.append(first_lock(times, np.where(times >= end, err, np.inf), end))
        relock = np.array(relock)
        print(json.dumps({"estimator": args.estimator, "change": change, "ramp_s": ramp_s, "tracks": len(rows),
                          "median_err_during_bpm": round(float(np.median(during)), 3),
                          "relock_after_ramp_median_s": round(float(np.nanmedian(relock)), 1),
                          "never_relocks": round(float(np.mean(np.isnan(relock))), 3)}), flush=True)


if __name__ == "__main__":
    main()
