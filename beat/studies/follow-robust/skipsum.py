"""One line per config and case of skiptest rows: median re-lock, share re-locked within 3 beats, gated within 2 beats, wrong beats.

    python studies/follow-robust/skipsum.py skip6.jsonl skip7.jsonl
"""
import collections
import json
import sys

import numpy as np

g = collections.defaultdict(dict)
for f in sys.argv[1:]:
    for line in open(f):
        r = json.loads(line)
        if r.get("error"):
            print(r["error"][-400:])
            break
        if r.get("locked_before"):
            g[(r["config"], r["case"])][r["name"] + str(round(r["param"], 3))] = r      # a later file overrides an earlier one
for cfg in dict.fromkeys(c for c, _ in g):
    out = []
    for case in ("jump", "nudge", "other"):
        rows = list(g.get((cfg, case), {}).values())
        if not rows:
            continue
        ph = [r["phase_relock_beats"] for r in rows if r["phase_relock_beats"] is not None]
        ok = sum(1 for r in rows if r["phase_relock_beats"] is not None and r["phase_relock_beats"] <= 3)
        gt = sum(1 for r in rows if r["gate_s"] is not None and r["gate_s"] <= 2 * r["period"])
        wr = np.median([r["wrong_out_beats"] for r in rows])
        out.append(f"{case} n={len(rows)} med {np.median(ph):.1f} <=3b {ok} ({100 * ok // len(rows)}%) gate<=2b {gt} wrong {wr:.1f}")
    print(f"{cfg:18s}| " + " ; ".join(out))
