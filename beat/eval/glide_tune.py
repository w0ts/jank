"""Tune glide mode's entry: steady precision (false glides) vs following ramps.

    .venv/bin/python -m eval.glide_tune --enter 0.01
"""
from __future__ import annotations

import argparse
import json

import numpy as np

from eval.data import DATA
from eval.locked_eval import bpm_trace_est, folded_error
from eval.locked_ramp import RAMP_START_S, RAMPS, warp
from eval.rescore import _rows

ENTER_CHECKS = (4, 8, 16)


def steady(rows, truth, settings) -> tuple[float, float]:
    """(share within 0.1 BPM after 4 s, share of tracks with any glide: all false on constant-tempo tracks)."""
    shares, glided = [], []
    for r in rows:
        times, bpms, est = bpm_trace_est(np.array(r["est_beats"]), kind="glide", **settings)
        err = folded_error(bpms, truth[r["name"]])
        keep = times >= 4.0
        if keep.any():
            shares.append(np.mean(err[keep] <= 0.1))
            glided.append(est.glides > 0)
    return float(np.mean(shares)), float(np.mean(glided))


def ramps(rows, truth, settings) -> float:
    """Median BPM error during the ramps, over all RAMPS."""
    during = []
    long_rows = [r for r in rows if len(r["est_beats"]) and r["est_beats"][-1] > RAMP_START_S + 150]
    for change, ramp_s in RAMPS:
        for r in long_rows[::3]:
            beats, factor = warp(np.array(r["est_beats"]), change, ramp_s)
            times, bpms, _ = bpm_trace_est(beats, kind="glide", **settings)
            err = folded_error(bpms, truth[r["name"]] * factor(times))
            end = RAMP_START_S + ramp_s / (1 + change / 2)
            mid = (times > RAMP_START_S + 5) & (times < end)
            if mid.any():
                during.append(np.nanmedian(err[mid]))
    return float(np.median(during))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--enter", type=float, default=0.006, help="quad trigger: tempo move to enter")
    ap.add_argument("--restart-count", type=int, default=0, help="restarts trigger: N same-way restarts (0 = quad)")
    ap.add_argument("--tracker", default="madmom_online")
    args = ap.parse_args()
    truth = {r["name"]: r["bpm"] for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", args.tracker) if "error" not in r and r["name"] in truth and "window" not in r]
    if args.restart_count:
        configs = [{"trigger": "restarts", "restart_count": args.restart_count, "restart_window": w} for w in (32, 64, 128)]
    else:
        configs = [{"glide_enter": args.enter, "enter_checks": c} for c in ENTER_CHECKS]
    for settings in configs:
        within, glided = steady(rows, truth, settings)
        print(json.dumps({**settings, "steady_within_0.1": round(within, 4),
                          "tracks_with_false_glide": round(glided, 3),
                          "ramp_err_bpm": round(ramps(rows, truth, settings), 3)}), flush=True)


if __name__ == "__main__":
    main()
