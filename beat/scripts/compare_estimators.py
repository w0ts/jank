"""Plain (LockedBpm) vs mode (ModeBpm) estimator on the same tracks and the same 0.5 s clock (a debugging aid).

    PYTHONPATH=. .venv/bin/python scripts/compare_estimators.py [every]
For each: share of time running (plain: whenever it has a tempo), right (within 0.1 BPM) and wrong (> 0.5 BPM) as
shares of ALL time, both pooled over time and averaged per track, and the worst tracks for wrong-while-running.
"""
from __future__ import annotations

import json
import sys

import numpy as np

from eval.data import DATA
from eval.locked_bpm import LockedBpm
from eval.locked_eval import folded_error
from eval.mode_eval import STEP_S, trace
from eval.rescore import _rows


def plain_trace(beats: np.ndarray, end: float) -> np.ndarray:
    est, out, i = LockedBpm(), [], 0
    for now in np.arange(STEP_S, end, STEP_S):
        while i < beats.size and beats[i] <= now:
            est.update(float(beats[i]))
            i += 1
        out.append(est.bpm() or np.nan)
    return np.array(out)


def main() -> None:
    every = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", "madmom_online") if "error" not in r and r["name"] in index][::every]
    stats = {"plain": [], "mode": []}
    worst = []
    for r in rows:
        meta = index[r["name"]]
        beats = np.array(r["est_beats"])
        _, mode_bpms, _ = trace(beats, meta["dur"])
        for name, bpms in (("plain", plain_trace(beats, meta["dur"])), ("mode", mode_bpms)):
            e = folded_error(bpms, meta["bpm"])
            stats[name].append((len(e), np.sum(~np.isnan(e)), np.sum(e <= 0.1), np.sum(e > 0.5)))
        e = folded_error(mode_bpms, meta["bpm"])
        worst.append((float(np.sum(e > 0.5) / max(np.sum(~np.isnan(e)), 1)), meta["bpm"], meta["file"][:55]))
    print(f"{len(rows)} tracks; shares of ALL time (pooled | per-track mean)")
    for name, s in stats.items():
        a = np.array(s, dtype=float)
        pooled = a[:, 1:].sum(axis=0) / a[:, 0].sum()
        per = (a[:, 1:] / a[:, :1]).mean(axis=0)
        print(f"  {name:5} running {pooled[0]:.2f} | {per[0]:.2f}   right {pooled[1]:.2f} | {per[1]:.2f}   wrong {pooled[2]:.3f} | {per[2]:.3f}")
    print("mode: worst tracks (wrong share while running, bpm):")
    for w, bpm, f in sorted(worst, reverse=True)[:8]:
        print(f"  {w:5.1%}  {bpm:7.2f}  {f}")


if __name__ == "__main__":
    main()
