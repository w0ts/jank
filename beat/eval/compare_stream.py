"""Streaming Beat This! (0 / 0.1 / 0.24 s look-ahead) vs madmom online on the same Traktor excerpts.

For each excerpt row of a streaming run, madmom's beats from the full track are cut to the same window, so both
trackers are scored on the same audio: the growing-window BPM (within 0.1 / 0.5 BPM after 4 s) and the folded grid
with each track's constant offset removed.

    .venv/bin/python -m eval.compare_stream
"""
from __future__ import annotations

import json

import numpy as np

from eval.data import DATA
from eval.grid import grid_score
from eval.locked_eval import bpm_trace, folded_error
from eval.rescore import _rows, track_offset

STREAMS = ("beat_this_small0_stream_L0", "beat_this_small0_stream_L5", "beat_this_small0_stream_L12")


def score(ref: np.ndarray, est: np.ndarray, true_bpm: float, delay: float) -> tuple[float, float, float, float]:
    times, bpms = bpm_trace(est)
    err = folded_error(bpms, true_bpm)
    keep = times >= 4.0
    w01 = float(np.mean(err[keep] <= 0.1)) if keep.any() else np.nan
    w05 = float(np.mean(err[keep] <= 0.5)) if keep.any() else np.nan
    g = grid_score(ref, est - track_offset(ref, est), latency_s=delay, fold=True)
    return w01, w05, g.get("tight", np.nan), g.get("loose", np.nan)


def main() -> None:
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    madmom = {r["name"]: np.array(r["est_beats"]) for r in _rows("traktor", "madmom_online") if "error" not in r}
    results = {name: [] for name in ("madmom_online (same windows)",) + STREAMS}
    for stream in STREAMS:
        for r in _rows("traktor", stream):
            meta = index.get(r["name"])
            if meta is None or r["name"] not in madmom:
                continue
            t0, t1 = r["window"]
            ref = np.array(meta["beats"])
            ref = ref[(ref >= t0) & (ref < t1)] - t0
            results[stream].append(score(ref, np.array(r["est_beats"]), meta["bpm"], r["delay_s"]))
            if stream == STREAMS[0]:
                m = madmom[r["name"]]
                results["madmom_online (same windows)"].append(score(ref, m[(m >= t0) & (m < t1)] - t0, meta["bpm"], 0.0))
    print(f"{'tracker':32} n    BPM ±0.1  ±0.5 | grid 20ms  50ms")
    for name, rows in results.items():
        a = np.nanmean(np.array(rows), axis=0)
        print(f"{name:32} {len(rows):3}  {a[0]:.2f}     {a[1]:.2f} | {a[2]:.2f}      {a[3]:.2f}")


if __name__ == "__main__":
    main()
