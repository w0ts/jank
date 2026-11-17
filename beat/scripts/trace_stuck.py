"""Trace ModeBpm beat by beat on Traktor tracks that never leave 'stopped' (a debugging aid).

    PYTHONPATH=. .venv/bin/python scripts/trace_stuck.py [every] [beats_to_show]
"""
from __future__ import annotations

import json
import sys

import numpy as np

from eval.data import DATA
from eval.mode_bpm import ModeBpm
from eval.rescore import _rows


def main() -> None:
    every = int(sys.argv[1]) if len(sys.argv) > 1 else 12
    show = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", "madmom_online") if "error" not in r and r["name"] in index][::every]
    for r in rows:
        est = ModeBpm()
        log = []
        for t in r["est_beats"]:
            est.update(float(t))
            p = est.period()
            log.append((round(t, 2), est.mode, est.fit.n, est.misses, est.in_a_row, round(60 / p, 2) if p else None))
        if any(m != "stopped" for _, m, *_ in log):
            continue
        meta = index[r["name"]]
        print(f"== {meta['file'][:60]}  true {meta['bpm']:.2f} BPM, {len(log)} beats, restarts {est.restarts}")
        print("   t  mode  fit.n misses in_a_row bpm")
        for row in log[:show:2]:
            print("  ", row)
        return


if __name__ == "__main__":
    main()
