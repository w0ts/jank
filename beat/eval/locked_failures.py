"""Where the growing-window BPM is wrong after its first lock: in breaks or not, how far off, how many restarts.

    .venv/bin/python -m eval.locked_failures --tracker madmom_online
"""
from __future__ import annotations

import argparse
import json

import numpy as np

from eval.data import DATA
from eval.locked_eval import LOCK_BPM, bpm_trace_est, first_lock, folded_error
from eval.rescore import _rows

BANDS = ((0.1, 0.5, "0.1-0.5 BPM"), (0.5, 2.0, "0.5-2 BPM"), (2.0, np.inf, ">2 BPM (wrong tempo)"))


def in_break(times: np.ndarray, breaks: list[float], drops: list[float]) -> np.ndarray:
    """True between a 'tk: Break' cue and the next 'tk: Drop' cue (or the end)."""
    mask = np.zeros(times.shape, dtype=bool)
    for b in breaks:
        after = [d for d in drops if d > b]
        end = min(after) if after else np.inf
        mask |= (times >= b) & (times < end)
    return mask


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracker", default="madmom_online")
    args = ap.parse_args()
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", args.tracker) if "error" not in r and r["name"] in index and "window" not in r]
    n_after = n_bad = n_break = n_bad_break = 0
    bands = {label: 0 for _, _, label in BANDS}
    restarts, never, worst = [], [], []
    for r in rows:
        meta = index[r["name"]]
        times, bpms, est = bpm_trace_est(np.array(r["est_beats"]))
        err = folded_error(bpms, meta["bpm"])
        restarts.append(est.restarts)
        lock = first_lock(times, err)
        if lock != lock:
            never.append((meta["file"][:60], meta["bpm"]))
            continue
        after = times >= lock
        brk = in_break(times, meta["breaks"], meta["drops"])
        bad = after & ~(err <= LOCK_BPM)
        n_after += int(after.sum())
        n_bad += int(bad.sum())
        n_break += int((after & brk).sum())
        n_bad_break += int((bad & brk).sum())
        for lo, hi, label in BANDS:
            bands[label] += int((bad & (err > lo) & (err <= hi)).sum())
        bands[BANDS[-1][2]] += int((bad & np.isnan(err)).sum())
        worst.append((float(np.mean(bad[after])), meta["file"][:60], meta["bpm"], est.restarts))
    print(f"tracks {len(rows)}, never lock {len(never)}")
    print(f"after first lock: {n_bad / n_after:.1%} of beats off by more than {LOCK_BPM} BPM")
    print(f"  breaks are {n_break / n_after:.1%} of that time and hold {n_bad_break / max(n_bad, 1):.1%} of the misses")
    for label, n in bands.items():
        print(f"  {label:22} {n / max(n_bad, 1):.1%} of the misses")
    print(f"restarts per track: median {np.median(restarts):.0f}, p90 {np.percentile(restarts, 90):.0f}, max {max(restarts)}")
    print("worst tracks (share of beats off after lock, bpm, restarts):")
    for share, f, bpm, rs in sorted(worst, reverse=True)[:8]:
        print(f"  {share:5.1%}  {bpm:7.2f}  {rs:3d}  {f}")
    print("never lock:", never[:8])


if __name__ == "__main__":
    main()
