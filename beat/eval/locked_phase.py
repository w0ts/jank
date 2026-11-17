"""Phase of the growing-window grid: does the published grid (fit line, predicted forward) land on the true beats?

At each true beat r after the first BPM lock, the estimator has seen the tracked beats up to r - delay; its grid
t_last + period * n is extrapolated to r. Compared with the short 6-beat grid of eval.grid (the earlier metric).
Traktor references: each track's constant offset is removed first (grids sit ~60 ms apart between download batches).

    .venv/bin/python -m eval.locked_phase --tracker madmom_online --delay 0.0
"""
from __future__ import annotations

import argparse
import json

import numpy as np

from eval.data import DATA
from eval.grid import _fold, grid_score
from eval.locked_bpm import LockedBpm
from eval.rescore import _rows, track_offset

WARMUP_S = 20.0


def growing_grid_errors(ref: np.ndarray, est: np.ndarray, true_bpm: float, delay: float) -> np.ndarray:
    """Signed ms from each true beat (after WARMUP_S) to the nearest line of the published grid."""
    lock = LockedBpm()
    i, errs = 0, []
    true_p = 60.0 / true_bpm
    for r in ref[ref >= WARMUP_S]:
        while i < est.size and est[i] <= r - delay:
            lock.update(float(est[i]))
            i += 1
        g = lock.grid()
        if g is None:
            errs.append(np.nan)
            continue
        last, period = g
        period = _fold(period, true_p)
        errs.append((last + np.round((r - last) / period) * period - r) * 1000.0)
    return np.array(errs)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracker", default="madmom_online")
    ap.add_argument("--delay", type=float, default=0.0, help="seconds between a beat and the grid being out")
    args = ap.parse_args()
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", args.tracker) if "error" not in r and r["name"] in index and "window" not in r]
    tight, loose, medians, short_tight, short_loose = [], [], [], [], []
    for r in rows:
        meta = index[r["name"]]
        ref, est = np.array(meta["beats"]), np.array(r["est_beats"])
        est = est - track_offset(ref, est)
        e = np.abs(growing_grid_errors(ref, est, meta["bpm"], args.delay))
        if e.size == 0:
            continue
        tight.append(np.mean(e <= 20))
        loose.append(np.mean(e <= 50))
        medians.append(np.nanmedian(e) if np.any(~np.isnan(e)) else np.nan)
        s = grid_score(ref[ref >= WARMUP_S - 5], est, latency_s=args.delay, fold=True)
        short_tight.append(s["tight"])
        short_loose.append(s["loose"])
    print(json.dumps({
        "tracker": args.tracker, "delay_s": args.delay, "tracks": len(tight),
        "growing_grid": {"within_20ms": round(float(np.mean(tight)), 4), "within_50ms": round(float(np.mean(loose)), 4),
                         "median_ms": round(float(np.nanmedian(medians)), 2)},
        "short_6_beat_grid": {"within_20ms": round(float(np.mean(short_tight)), 4),
                              "within_50ms": round(float(np.mean(short_loose)), 4)},
    }, indent=1))


if __name__ == "__main__":
    main()
