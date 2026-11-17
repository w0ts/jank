"""Bias of madmom's offline beats against GTZAN's hand annotations (est - ref, per piece median, then the median).

The Traktor labels are corrected with madmom offline beats as the reference; this is the bias to take off.

    .venv/bin/python -I train/calibrate_gtzan.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
ANN = ROOT / "data" / "annotations" / "gtzan" / "annotations" / "beats"


def nearest_diff(est: np.ndarray, ref: np.ndarray) -> np.ndarray:
    idx = np.clip(np.searchsorted(ref, est), 1, len(ref) - 1)
    near = np.where(np.abs(ref[idx] - est) < np.abs(ref[idx - 1] - est), ref[idx], ref[idx - 1])
    return est - near


def main() -> None:
    pieces = json.loads((ROOT / "runs" / "gtzan_madmom_dbn_offline.json").read_text())["pieces"]
    offs = []
    for p in pieces:
        f = ANN / (p["name"] + ".beats")
        if not f.exists():
            continue
        ref = np.array([float(l.split()[0]) for l in f.read_text().splitlines() if l.strip()])
        if "est_beats" not in p:
            continue
        est = np.array(p["est_beats"])
        if len(est) < 10 or len(ref) < 10:
            continue
        d = nearest_diff(est, ref)
        d = d[np.abs(d) < 0.07]
        if len(d) > 10:
            offs.append(np.median(d))
    offs = np.array(offs) * 1000
    out = {"pieces": len(offs), "median_ms": float(np.median(offs)), "mean_ms": float(offs.mean()),
           "p10_p25_p75_p90_ms": [float(x) for x in np.percentile(offs, [10, 25, 75, 90])]}
    print(json.dumps(out))
    (ROOT / "data" / "gtzan_madmom_bias.json").write_text(json.dumps(out))


if __name__ == "__main__":
    main()
