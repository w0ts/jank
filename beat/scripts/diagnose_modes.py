"""Where does ModeBpm spend its time on steady tracks, and how right is each mode? (a debugging aid)

    PYTHONPATH=. .venv/bin/python scripts/diagnose_modes.py [every]
Prints, over every n-th Traktor track: share of time per mode, BPM error per mode, mode switches per track by
kind (what triggered stopped: no fitting beat for hold_s, or the grid missing), ramp entries per track.
"""
from __future__ import annotations

import collections
import json
import sys

import numpy as np

from eval.data import DATA
from eval.locked_eval import folded_error
from eval.mode_eval import trace
from eval.rescore import _rows


def main() -> None:
    every = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", "madmom_online") if "error" not in r and r["name"] in index][::every]
    share, errs, switches = collections.Counter(), collections.defaultdict(list), collections.Counter()
    total = 0
    for r in rows:
        meta = index[r["name"]]
        beats = np.array(r["est_beats"])
        times, bpms, modes = trace(beats, meta["dur"])
        err = folded_error(np.array([b if b == b else np.nan for b in bpms]), meta["bpm"])
        total += len(modes)
        for m, e in zip(modes, err):
            share[m] += 1
            if e == e:
                errs[m].append(e)
        for a, b in zip(modes, modes[1:]):
            if a != b:
                switches[f"{a} -> {b}"] += 1
    print(f"{len(rows)} tracks")
    for m, n in share.most_common():
        e = np.array(errs[m])
        extra = f"  within 0.1: {np.mean(e <= 0.1):.2f}  >0.5: {np.mean(e > 0.5):.2f}" if e.size else ""
        print(f"  {m:13} {n / total:6.1%} of time{extra}")
    print("switches per track:")
    for k, n in switches.most_common(10):
        print(f"  {k:28} {n / len(rows):.2f}")


if __name__ == "__main__":
    main()
