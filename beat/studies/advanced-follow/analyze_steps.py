"""What separates the lost cases from the kept ones in step_stats.py's output.

lost = the outputs stopped at least once within 30 s of the step while they ran before it (the gate or the estimator).
flam = the published grid more than 50 ms off the beats (against its phase before the step) while running.

    .venv/bin/python studies/advanced-follow/analyze_steps.py FILE.jsonl [--estimator today]
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np


def load(path: Path, estimator: str | None) -> list[dict]:
    rows = [json.loads(x) for x in path.read_text().splitlines() if x.strip()]
    rows = [r for r in rows if "error" not in r and (estimator is None or r.get("estimator", "today") == estimator)]
    return [r for r in rows if r["running_before"] and r["ref_in_span"] > 20]


def rate(rows: list[dict], key: str = "lost") -> str:
    if not rows:
        return "  -"
    v = np.mean([r[key] for r in rows])
    return f"{100 * v:5.1f} % of {len(rows):3d}"


def table(rows: list[dict], name: str, bins, get) -> list[str]:
    out = [f"  by {name}:"]
    for lo, hi, label in bins:
        sel = [r for r in rows if get(r) is not None and lo <= get(r) < hi]
        out.append(f"    {label:>14}  lost {rate(sel)}  flam {rate(sel, 'flam')}")
    return out


def report(rows: list[dict]) -> str:
    for r in rows:
        r["lost"] = r["stopped_s"] > 0
        r["flam"] = r["flam_beats"] > 0
    lines = [f"cases (running before the step): {len(rows)}",
             f"lost (outputs stopped): {rate(rows)}; flam (> 50 ms while running): {rate(rows, 'flam')}",
             f"either: {100 * np.mean([r['lost'] or r['flam'] for r in rows]):.1f} %"]
    lost = [r for r in rows if r["lost"]]
    if lost:
        lines.append(f"lost: stopped median {np.median([r['stopped_s'] for r in lost]):.2f} s, "
                     f"max {max(r['stopped_s'] for r in lost):.2f} s")
    fol = [r["follow_s"] for r in rows if r["follow_s"] == r["follow_s"]]
    lines.append(f"BPM within 0.5 % for good after: median {np.median(fol):.1f} s, p90 {np.percentile(fol, 90):.1f} s; "
                 f"never in 30 s: {sum(r['follow_s'] != r['follow_s'] for r in rows)}")
    lines.append(f"worst phase move while running: median {np.median([r['worst_ms'] for r in rows if r['worst_ms'] is not None]):.0f} ms")
    lines += table(rows, "direction", [(-1, 0, "slower"), (0, 1, "faster")], lambda r: r["change"])
    lines += table(rows, "confidence before", [(0, 0.9, "< 0.90"), (0.9, 0.97, "0.90-0.97"), (0.97, 1.01, ">= 0.97")],
                   lambda r: r["conf_before"])
    lines += table(rows, "min conf. 10 s before", [(0, 0.85, "< 0.85"), (0.85, 0.95, "0.85-0.95"), (0.95, 1.01, ">= 0.95")],
                   lambda r: r["min_conf_before10"])
    lines += table(rows, "beat phase", [(0, 0.25, "0-0.25"), (0.25, 0.5, "0.25-0.5"), (0.5, 0.75, "0.5-0.75"),
                                        (0.75, 1.01, "0.75-1")], lambda r: r["beat_phase"])
    lines += table(rows, "bar position", [(k, k + 1, f"beat {k + 1}") for k in range(4)], lambda r: r["bar_pos"])
    lines += table(rows, "in a Traktor break", [(0, 1, "no"), (1, 2, "yes")], lambda r: int(r["in_break"]))
    lines += table(rows, "tracker jitter", [(0, 3, "< 3 ms"), (3, 6, "3-6 ms"), (6, 12, "6-12 ms"), (12, 1e9, ">= 12 ms")],
                   lambda r: r["jitter_ms"] if r["jitter_ms"] == r["jitter_ms"] else None)
    lines += table(rows, "track BPM", [(0, 120, "< 120"), (120, 130, "120-130"), (130, 140, "130-140"),
                                       (140, 1e9, ">= 140")], lambda r: r["bpm"])
    lines += table(rows, "fit beats before", [(0, 32, "< 32"), (32, 128, "32-128"), (128, 1e9, ">= 128")],
                   lambda r: r["fit_n_before"])
    lines += table(rows, "jumps after", [(0, 1, "0"), (1, 2, "1"), (2, 1e9, ">= 2")], lambda r: r["jumps"])
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", type=Path)
    ap.add_argument("--estimator")
    args = ap.parse_args()
    print(report(load(args.path, args.estimator)))


if __name__ == "__main__":
    main()
