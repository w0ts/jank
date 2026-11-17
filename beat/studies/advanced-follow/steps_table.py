"""One table over all step_stats.py files: per change (or ramp) and estimator, the outputs-stopped rate, the flam rate,
the beats to converge (grid within 15 ms and the tempo within 0.5 % for 8 beats in a row), the worst phase over the
first 10 beats, the detection delay, and for the no-step control the false manoeuvres / ratio events per hour.

    .venv/bin/python studies/advanced-follow/steps_table.py DIR_WITH_JSONL
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

ORDER = ("today", "lines_only", "follow_only", "follow", "follow_ratio")


def pct(v, q):
    v = [x for x in v if x is not None and x == x]
    return f"{np.percentile(v, q):.1f}" if v else "-"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dir", type=Path)
    args = ap.parse_args()
    print("| move | estimator | cases | outputs stopped | flam > 50 ms | converge beats med / p90 (never) | "
          "worst 10 beats ms med / p90 | detect s med | false manoeuvres / h | false ratio events / h |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    files = sorted(args.dir.glob("step_*.jsonl"), key=lambda p: float(p.stem.split("_")[1])) + \
        sorted(args.dir.glob("ramp_*.jsonl"))
    for f in files:
        rows = [json.loads(x) for x in f.read_text().splitlines() if x.strip()]
        rows = [r for r in rows if "error" not in r and r["running_before"]]
        label = f.stem.replace("step_", "step ").replace("ramp_", "ramp ").replace("_", " over ") + (" s" if "ramp" in f.stem else "")
        for est in ORDER:
            rs = [r for r in rows if r.get("estimator") == est]
            if not rs:
                continue
            conv = [r.get("conv_beats") for r in rs]
            never = sum(c is None for c in conv)
            hours = len(rs) * 30.0 / 3600.0
            man = sum(r.get("manoeuvres_span", 0) for r in rs) / hours if "step 0.0" in label else None
            rat = sum(r.get("ratio_events_before60", 0) for r in rs) / (len(rs) * 60.0 / 3600.0)
            print(f"| {label} | {est} | {len(rs)} | {np.mean([r['stopped_s'] > 0 for r in rs]):.2f} | "
                  f"{np.mean([r['flam_beats'] > 0 for r in rs]):.2f} | {pct(conv, 50)} / {pct(conv, 90)} ({never}) | "
                  f"{pct([r.get('worst10_ms') for r in rs], 50)} / {pct([r.get('worst10_ms') for r in rs], 90)} | "
                  f"{pct([r.get('detect_s') for r in rs], 50)} | {f'{man:.1f}' if man is not None else '-'} | "
                  f"{rat:.1f} |")


if __name__ == "__main__":
    main()
