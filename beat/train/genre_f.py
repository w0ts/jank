"""GTZAN beat F-measure per genre for the given trackers: genre_f.py TRACKER..."""
from __future__ import annotations

import sys
from collections import defaultdict
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(Path(__file__).resolve().parent))


def main() -> None:
    from eval.data import DATASETS
    from eval.rescore import _rows
    from report import f_measure
    ref = {p.name: p.beats for p in DATASETS["gtzan"]()}
    table: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    for t in sys.argv[1:]:
        for r in _rows("gtzan", t):
            if r["name"] in ref:
                table[r["name"].split("_")[1]][t].append(f_measure(np.array(r["est_beats"]), ref[r["name"]])[0])
    print(f"{'genre':10}" + "".join(f"{t:>14}" for t in sys.argv[1:]))
    for g in sorted(table):
        print(f"{g:10}" + "".join(f"{np.mean(table[g][t]):14.3f}" for t in sys.argv[1:]))


if __name__ == "__main__":
    main()
