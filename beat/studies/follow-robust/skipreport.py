"""Tables of skiptest.py's rows: per case and config, how many skips counted and how the follower recovered.

    python studies/follow-robust/skipreport.py skip.jsonl [skip2.jsonl ...] --out results/skip.md
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

import numpy as np


def med(v: list) -> str:
    return "-" if not v else f"{np.median(v):.1f}"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("rows", type=Path, nargs="+")
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    g: dict[tuple, list] = defaultdict(list)
    for p in a.rows:
        for line in p.read_text().splitlines():
            r = json.loads(line)
            if r.get("locked_before"):
                g[(r["case"], r["config"])].append(r)
    out = ["# Skip test", "",
           "Skips counted only where the chain was locked, right in tempo and phase, before the skip. Beats = of the true period.", "",
           "| case | config | skips | phase re-locked within 50 s | median phase re-lock (beats) | 90th pct (beats) | median wrong beats sent (30 s) | skips gated | median gate (s) | never gated | tracker alone: median re-sync (beats) |",
           "|---|---|---|---|---|---|---|---|---|---|---|"]
    for (case, cfg), rows in sorted(g.items()):
        ph = [r["phase_relock_beats"] for r in rows if r["phase_relock_beats"] is not None]
        wr = [r["wrong_out_beats"] for r in rows]
        gs = [r["gate_s"] for r in rows if r["gate_s"] is not None]
        tk = [r["tracker_relock_beats"] for r in rows if r.get("tracker_relock_beats") is not None]
        p90 = f"{np.percentile(ph, 90):.0f}" if ph else "-"
        out.append(f"| {case} | {cfg} | {len(rows)} | {len(ph)} | {med(ph)} | {p90} | {med(wr)} | {len(gs)} | {med(gs)} | {len(rows) - len(gs)} | {med(tk)} ({len(tk)}/{len(rows)}) |")
    a.out.write_text("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
