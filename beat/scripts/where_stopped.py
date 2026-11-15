"""Where is ModeBpm stopped? Before the first lock, inside Traktor breaks, or elsewhere (a debugging aid).

    PYTHONPATH=. .venv/bin/python scripts/where_stopped.py [every] [hold_s]
"""
from __future__ import annotations

import json
import sys

import numpy as np

from eval.data import DATA
from eval.locked_failures import in_break
from eval.mode_eval import trace
from eval.rescore import _rows


def main() -> None:
    every = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    hold = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", "madmom_online") if "error" not in r and r["name"] in index][::every]
    total = before_lock = in_brk = elsewhere = 0
    gaps = []
    for r in rows:
        meta = index[r["name"]]
        beats = np.array(r["est_beats"])
        times, bpms, modes = trace(beats, meta["dur"], hold_s=hold)
        stopped = np.array([m == "stopped" for m in modes])
        running = ~stopped
        first = int(np.argmax(running)) if running.any() else len(modes)
        brk = in_break(times, meta["breaks"], meta["drops"])
        total += len(modes)
        before_lock += int(stopped[:first].sum())
        later = stopped.copy()
        later[:first] = False
        in_brk += int((later & brk).sum())
        elsewhere += int((later & ~brk).sum())
        gaps.extend(np.diff(beats)[np.diff(beats) > 4.0].tolist())
    print(f"{len(rows)} tracks, hold {hold:.0f} s: stopped {(before_lock + in_brk + elsewhere) / total:.1%} of time")
    print(f"  before the first lock {before_lock / total:.1%}, in breaks {in_brk / total:.1%}, elsewhere {elsewhere / total:.1%}")
    g = np.array(gaps)
    if g.size:
        print(f"  tracker gaps > 4 s: {g.size / len(rows):.1f} per track, median {np.median(g):.1f} s, p90 {np.percentile(g, 90):.1f} s")


if __name__ == "__main__":
    main()
