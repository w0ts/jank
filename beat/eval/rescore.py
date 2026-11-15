"""Recompute the grid metric from saved beats (runs/gtzan_*.json), without tracking again, and print a table.

    .venv/bin/python -m eval.rescore [--lead 0.0]
"""
from __future__ import annotations

import argparse
import json

import numpy as np

from eval.data import DATASETS, ROOT
from eval.grid import grid_score
from eval.trackers import TRACKERS

GRID_COLS = ("tight", "loose", "octave", "triplet", "offbeat", "off", "median_ms")


def _mean_or_none(values: list[float]) -> float | None:
    """Pieces without annotated downbeats (or a tracker without downbeats) give nothing to average."""
    return float(np.mean(values)) if values else None


def _rows(dataset: str, name: str) -> list[dict]:
    """All saved pieces of one tracker on one dataset: the whole-run file, or every shard file."""
    whole = ROOT / "runs" / f"{dataset}_{name}.json"
    paths = [whole] if whole.exists() else sorted((ROOT / "runs").glob(f"{dataset}.*of*_{name}.json"))
    return [r for p in paths for r in json.loads(p.read_text())["pieces"]]


def ref_for(row: dict, refs: dict) -> np.ndarray:
    """The reference beats in the row's own time base: excerpt rows carry their window [t0, t1)."""
    ref = refs[row["name"]]
    if "window" not in row:
        return ref
    t0, t1 = row["window"]
    return ref[(ref >= t0) & (ref < t1)] - t0


def track_offset(ref: np.ndarray, est: np.ndarray, skip: int = 20) -> float:
    """Median signed distance (s) from each tracked beat (after the first `skip`) to its nearest reference beat."""
    est = est[skip:]
    if est.size == 0 or ref.size == 0:
        return 0.0
    idx = np.clip(np.searchsorted(ref, est), 1, ref.size - 1)
    nearest = np.where(np.abs(ref[idx] - est) < np.abs(ref[idx - 1] - est), ref[idx], ref[idx - 1])
    return float(np.median(est - nearest))


def rescore(dataset: str, name: str, refs: dict, lead_s: float, fold: bool = False, align: bool = False) -> dict | None:
    """align=True removes each track's constant offset to the reference first (Traktor grids sit ~60 ms apart
    between download batches, 2026-11-13; a tracker's own constant offset is calibrated once in the device)."""
    rows = [r for r in _rows(dataset, name) if "error" not in r and r["name"] in refs]
    if not rows:
        return None
    grids = []
    for r in rows:
        ref, est = ref_for(r, refs), np.array(r["est_beats"])
        if align:
            est = est - track_offset(ref, est)
        grids.append(grid_score(ref, est, latency_s=r.get("delay_s", 0.0), lead_s=lead_s, fold=fold))
    return {
        "tracker": name, "causal": TRACKERS[name].causal, "pieces": len(rows),
        "F": float(np.mean([r["beat"]["F"] for r in rows])),
        "downbeat_F": _mean_or_none([r["downbeat"]["F"] for r in rows if r.get("downbeat", {}).get("F") is not None]),
        **{c: float(np.nanmean([g[c] for g in grids])) for c in GRID_COLS},
        "rtf": float(np.nanmedian([r["rtf"] for r in rows])) if any(r["rtf"] == r["rtf"] for r in rows) else float("nan"),
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--lead", type=float, default=0.0, help="seconds the grid must be out before the beat")
    ap.add_argument("--dataset", choices=list(DATASETS), default="gtzan")
    ap.add_argument("--fold", action="store_true", help="score as if a BPM range setting folded half/double tempo")
    ap.add_argument("--align", action="store_true", help="remove each track's constant offset to the reference first")
    args = ap.parse_args()
    refs = {p.name: p.beats for p in DATASETS[args.dataset]()}
    print(f"{'tracker':26} causal  n    F     dbF  | tight loose octave trip  offbt  off   med ms | rtf")
    for name in TRACKERS:
        s = rescore(args.dataset, name, refs, args.lead, args.fold, args.align)
        if s is None:
            continue
        db = f"{s['downbeat_F']:.2f}" if s["downbeat_F"] is not None else "  - "
        print(f"{name:26} {'yes' if s['causal'] else 'no ':3}  {s['pieces']:4} {s['F']:.3f} {db} | "
              f"{s['tight']:.2f}  {s['loose']:.2f}  {s['octave']:.2f}   {s['triplet']:.2f}  {s['offbeat']:.2f}  {s['off']:.2f}  "
              f"{s['median_ms']:5.1f}  | {s['rtf']:.3f}")


if __name__ == "__main__":
    main()
