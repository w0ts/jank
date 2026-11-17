"""Tables from study.py's rows: per chain, over a set of tracks (a split, a reference class).

    python studies/tempo-changes/summarize.py ROWS.jsonl [--split test] [--classes changing] [--configs a b]
        [--out T.md] [--title "..."]
"""
from __future__ import annotations

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path

import numpy as np


def load(path: Path) -> list[dict]:
    rows = [json.loads(x) for x in path.read_text().splitlines() if x.strip()]
    errors = [r for r in rows if "error" in r]
    if errors:
        raise SystemExit(f"{len(errors)} error rows, first: {errors[0]['error'][-400:]}")
    return rows


def _m(x) -> float:
    x = [v for v in x if v is not None and not (isinstance(v, float) and math.isnan(v))]
    return float(np.mean(x)) if x else float("nan")


def _med(x) -> float:
    x = [v for v in x if v is not None and not (isinstance(v, float) and math.isnan(v))]
    return float(np.median(x)) if x else float("nan")


def follow_stats(rows: list[dict]) -> dict:
    """Over the real changes the chain was right just before (it had something to follow)."""
    fs = [f for r in rows for f in r["follow"] if f["right_before"] >= 0.5]
    if not fs:
        return {"changes": 0}
    d = [f["delay_beats"] for f in fs]
    got = [x for x in d if x is not None]
    return {"changes": len(fs), "delay_beats_med": _med(got), "delay_s_med": _med([f["delay_s"] for f in fs]),
            "within_8": sum(1 for x in got if x <= 8) / len(fs), "within_16": sum(1 for x in got if x <= 16) / len(fs),
            "never": sum(1 for x in d if x is None) / len(fs),
            "all_changes": sum(len(r["follow"]) for r in rows)}


def quiet_stats(rows: list[dict]) -> dict:
    qs = [q for r in rows for q in r["quiet"]]
    if not qs:
        return {"passages": 0}
    errs = [abs(q["err_ms"]) for q in qs if q["running_after"] and q["err_ms"] == q["err_ms"]]
    return {"passages": len(qs), "q_stopped": _m([q["stopped"] for q in qs]),
            "q_running_after": _m([float(q["running_after"]) for q in qs]),
            "q_err_med": _med(errs), "q_in50": sum(1 for e in errs if e <= 50) / len(qs),
            "q_err4_med": _med([q["err4_ms"] for q in qs if q["running_after"]])}


def table(rows: list[dict], configs: list[str] | None = None) -> str:
    by = defaultdict(list)
    for r in rows:
        by[r["config"]].append(r)
    names = configs or list(by)
    head = ("| chain | tracks | stopped | right (1 %) | held (0.5 %) | wrong | wrong locks / track | range breaks / track "
            "| flips / track | in phase |")
    lines = [head, "|---|---|---|---|---|---|---|---|---|---|"]
    for n in names:
        rs = by.get(n, [])
        if not rs:
            continue
        lines.append(f"| {n} | {len(rs)} | {_m([r['stopped'] for r in rs]):.3f} | {_m([r['right'] for r in rs]):.3f} | "
                     f"{_m([r['held'] for r in rs]):.3f} | {_m([r['wrong'] for r in rs]):.3f} | "
                     f"{_m([r['wrong_locks'] for r in rs]):.2f} | {_m([r['range_breaks'] for r in rs]):.2f} | "
                     f"{_m([r['flips'] for r in rs]):.2f} | {_m([r['in_phase'] for r in rs]):.3f} |")
    lines += ["", "| chain | real changes followable | median delay (beats) | median delay (s) | within 8 beats | "
                  "within 16 beats | not within 30 s |", "|---|---|---|---|---|---|---|"]
    for n in names:
        rs = by.get(n, [])
        f = follow_stats(rs) if rs else {"changes": 0}
        if f["changes"]:
            lines.append(f"| {n} | {f['changes']} (of {f['all_changes']}) | {f['delay_beats_med']:.1f} | "
                         f"{f['delay_s_med']:.1f} | {f['within_8']:.2f} | {f['within_16']:.2f} | {f['never']:.2f} |")
    lines += ["", "| chain | beat-less passages | stopped in them | running at the first strong beat after | "
                  "median abs phase error there (ms) | within 50 ms | median of the first 4 strong beats (ms) | "
                  "half-time sections: right |", "|---|---|---|---|---|---|---|---|"]
    for n in names:
        rs = by.get(n, [])
        q = quiet_stats(rs) if rs else {"passages": 0}
        hr = [r["half_right"] for r in rs if r.get("half_right") is not None]
        if q["passages"]:
            lines.append(f"| {n} | {q['passages']} | {q['q_stopped']:.3f} | {q['q_running_after']:.2f} | "
                         f"{q['q_err_med']:.0f} | {q['q_in50']:.2f} | {q['q_err4_med']:.0f} | "
                         f"{_m(hr):.3f} ({len(hr)} tracks) |")
    return "\n".join(lines) + "\n"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("rows", type=Path)
    ap.add_argument("--split", choices=["dev", "test"])
    ap.add_argument("--classes", nargs="+")
    ap.add_argument("--configs", nargs="+")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--title", default="")
    ap.add_argument("--compact", action="store_true", help="one line per chain")
    a = ap.parse_args()
    rows = [r for r in load(a.rows) if (a.split is None or r["split"] == a.split)
            and (a.classes is None or r["class"] in a.classes)]
    body = compact(rows, a.configs) if a.compact else table(rows, a.configs)
    text = (f"### {a.title}\n\n" if a.title else "") + body
    if a.out:
        with a.out.open("a") as f:
            f.write(text + "\n")
    print(text)


def all_follow(rows: list[dict]) -> dict:
    """Over every real change: the share right within 16 beats of it (a ramp: of its end), whatever came before."""
    fs = [f for r in rows for f in r["follow"]]
    got = [f["delay_beats"] for f in fs if f["delay_beats"] is not None]
    return {"n": len(fs), "within_16": sum(1 for x in got if x <= 16) / len(fs) if fs else float("nan"),
            "med": _med(got)}


def compact(rows: list[dict], configs: list[str] | None = None) -> str:
    """One line per chain: steady tracks, changing tracks (confirmed + probable), beat-less passages, half-time."""
    by = defaultdict(list)
    for r in rows:
        by[r["config"]].append(r)
    lines = ["| chain | steady: held | stopped | wrong locks / track | range breaks / track | flips / track | "
             "changing: right | stopped | wrong | changes right within 16 beats | beat-less: stopped | running after | "
             "abs error after (ms) | half-time: right |", "|" + "---|" * 14]
    for n in configs or list(by):
        rs = by.get(n, [])
        st = [r for r in rs if r["class"] == "steady"]
        ch = [r for r in rs if r["class"] in ("changing", "probable")]
        if not rs:
            continue
        q = quiet_stats(rs)
        f = all_follow(ch)
        hr = [r["half_right"] for r in rs if r.get("half_right") is not None]
        lines.append(f"| {n} | {_m([r['held'] for r in st]):.3f} | {_m([r['stopped'] for r in st]):.3f} | "
                     f"{_m([r['wrong_locks'] for r in st]):.2f} | {_m([r['range_breaks'] for r in st]):.3f} | "
                     f"{_m([r['flips'] for r in st]):.2f} | {_m([r['right'] for r in ch]):.3f} | "
                     f"{_m([r['stopped'] for r in ch]):.3f} | {_m([r['wrong'] for r in ch]):.3f} | "
                     f"{f['within_16']:.2f} ({f['n']}) | {q.get('q_stopped', float('nan')):.3f} | "
                     f"{q.get('q_running_after', float('nan')):.2f} | {q.get('q_err_med', float('nan')):.0f} | "
                     f"{_m(hr):.3f} |")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    main()
