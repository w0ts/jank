"""BPM table from saved beats: per tracker and window, how often the published BPM is right, how fast it locks.

    .venv/bin/python -m eval.bpm_report --dataset traktor
"""
from __future__ import annotations

import argparse

import numpy as np

from eval.data import DATASETS
from eval.rescore import _rows, ref_for
from eval.tempo import WINDOWS_S, tempo_score
from eval.trackers import TRACKERS


def report(dataset: str, name: str, refs: dict) -> dict | None:
    rows = [r for r in _rows(dataset, name) if "error" not in r and r["name"] in refs]
    if not rows:
        return None
    scores = [tempo_score(ref_for(r, refs), np.array(r["est_beats"]), r.get("delay_s", 0.0)) for r in rows]
    out = {"n": len(rows)}
    for w in WINDOWS_S:
        k = f"{int(w)}s"
        per = [s[k] for s in scores if k in s]
        locks = np.array([p["lock_s"] for p in per])
        out[k] = {
            **{m: float(np.mean([p[m] for p in per])) for m in ("within_0.1", "within_0.5", "within_2pct")},
            "median_err": float(np.nanmedian([p["median_err"] for p in per])),
            "lock_median_s": float(np.nanmedian(locks)) if np.any(~np.isnan(locks)) else float("nan"),
            "never_locks": float(np.mean(np.isnan(locks))),
        }
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", choices=list(DATASETS), default="gtzan")
    args = ap.parse_args()
    refs = {p.name: p.beats for p in DATASETS[args.dataset]()}
    print(f"{'tracker':24} live win |  ±0.1   ±0.5   ±2%  | med err | lock med  never")
    for name in TRACKERS:
        s = report(args.dataset, name, refs)
        if s is None:
            continue
        for w in WINDOWS_S:
            k = f"{int(w)}s"
            v = s[k]
            print(f"{name:24} {'yes' if TRACKERS[name].causal else 'no ':4} {k:>3} | {v['within_0.1']:.2f}   {v['within_0.5']:.2f}   "
                  f"{v['within_2pct']:.2f} | {v['median_err']:6.2f}  | {v['lock_median_s']:6.1f}s  {v['never_locks']:.2f}")


if __name__ == "__main__":
    main()
