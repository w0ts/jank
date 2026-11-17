"""Tables of rtrack.py's rows: per chain the per-track categories (pertrack_report's "held whole" etc.), the medians, a
CMLc-like score, the coasting numbers, the re-locks and the real changes' delays.

    python studies/follow-robust/report.py ROWS.jsonl [ROWS2.jsonl ...] --out results/x.md --title "..." \
        [--split dev|test|all] [--configs fw a b] [--groups a b] [--worst NAME]

--groups: the chains also split by reference class and genre. --worst NAME: the tracks NAME changed most against fw
(names of the user's files: private).
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BEAT = HERE.parents[1]
sys.path.insert(0, str(HERE.parent / "tempo-changes"))

import pertrack_report as PR

CATS = PR.CATS
NOT_HELD = ("wrong tempo", "never locked", "long stops", "short stops")


def load(paths: list[Path]) -> dict[str, dict[str, dict]]:
    genres = json.loads((BEAT / "studies" / "downbeat" / "genres.json").read_text())
    by: dict[str, dict[str, dict]] = {}
    for p in paths:
        for line in p.read_text().splitlines():
            r = json.loads(line)
            if "error" in r:
                raise SystemExit(f"error row: {r['name']}: {r['error']}")
            r["genre"] = PR.genre_group(genres.get(r["name"], ""))
            by.setdefault(r["config"], {})[r["name"]] = r
    return by


def med(rows: list[dict], k: str) -> float:
    v = [r[k] for r in rows if r.get("locked") and r.get(k) is not None]
    return float(np.median(v)) if v else float("nan")


def coast_cells(rows: list[dict]) -> list[str]:
    co = sum(r.get("coast_s", 0.0) for r in rows)
    cw = sum(r.get("coast_wrong_s", 0.0) for r in rows)
    cr = sum(r.get("coast_right_s", 0.0) for r in rows)
    ends = Counter(e[2] for r in rows for e in r.get("coast_ends", []))
    n_end = sum(ends.values())
    end_txt = (f"{ends.get('in', 0)}/{ends.get('offbeat', 0)}/{ends.get('off', 0)}/{ends.get('wrong', 0)}"
               if n_end else "-")
    return [f"{co / 60:.0f}", f"{100 * cr / co:.0f} %" if co else "-", f"{100 * cw / co:.0f} %" if co else "-",
            f"{cw / 60:.1f}", end_txt]


def follow_cells(rows: list[dict]) -> list[str]:
    fs = [f for r in rows if r["class"] == "changing" for f in r.get("follow", []) if f.get("conf") == "both"]
    got = [f["delay_s"] for f in fs if f.get("delay_s") is not None]
    return [f"{len(got)}/{len(fs)}", f"{np.median(got):.1f}" if got else "-"]


HEAD = ("| chain | tracks | " + " | ".join(CATS) + " | median within 1 % | median stopped | median wrong | mean CMLc | "
        "median CMLc | wrong eps >= 2 s / track | of them after a stop | wrong eps >= 10 s | wrong locks / track | "
        "coasting (min) | coast right | coast wrong | coast wrong (min) | coast ends in/offbeat/off/wrong | "
        "real changes followed | median delay (s) |")


def row_of(name: str, rows: list[dict]) -> str:
    n = len(rows)
    c = Counter(PR.category(r) for r in rows)
    eps = [e for r in rows if r.get("locked") for e in r["episodes"] if e[0] == "wrong"]
    cm = [r.get("cmlc", 0.0) for r in rows]
    cells = [name, str(n)] + [PR.pct(c.get(k, 0), n) for k in CATS] + [
        f"{med(rows, 'within1'):.3f}", f"{med(rows, 'stopped'):.3f}", f"{med(rows, 'wrong'):.3f}",
        f"{np.mean(cm):.3f}", f"{np.median(cm):.3f}",
        f"{np.mean([r.get('wrongs_2s', 0) for r in rows]):.2f}",
        f"{sum(r.get('wrongs_after_stop', 0) for r in rows)}",
        f"{sum(1 for e in eps if e[2] >= 10)}",
        f"{np.mean([r.get('wrong_locks', 0) for r in rows]):.2f}"] + coast_cells(rows) + follow_cells(rows)
    return "| " + " | ".join(cells) + " |"


def table(by: dict, configs: list[str], sel) -> list[str]:
    L = [HEAD, "|---|" + "---|" * (HEAD.count("|") - 2)]
    for c in configs:
        L.append(row_of(c, [r for r in by[c].values() if sel(r)]))
    return L


def moved(by: dict, name: str, base: str, sel) -> str:
    """Tracks whose category got better / worse against base (held whole > short stops > long stops > wrong tempo,
    never locked)."""
    rank = {"held whole": 0, "short stops": 1, "long stops": 2, "wrong tempo": 3, "never locked": 3}
    up = down = 0
    for k, r in by[name].items():
        if not sel(r) or k not in by[base]:
            continue
        a, b = rank[PR.category(by[base][k])], rank[PR.category(r)]
        up += b < a
        down += b > a
    return f"{up} better, {down} worse"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("rows", type=Path, nargs="+")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--title", default="Robust follow")
    ap.add_argument("--split", choices=["dev", "test", "all"], default="all")
    ap.add_argument("--configs", nargs="+")
    ap.add_argument("--groups", nargs="+", default=[])
    ap.add_argument("--worst", nargs="+", default=[])
    ap.add_argument("--note", default="")
    a = ap.parse_args()
    by = load(a.rows)
    configs = a.configs or sorted(by, key=lambda c: (c != "fw", c))
    halves = {"dev": {"dev half": lambda r: r["split"] == "dev"},
              "test": {"test half": lambda r: r["split"] == "test"},
              "all": {"test half": lambda r: r["split"] == "test", "all tracks": lambda r: True}}[a.split]
    L = [f"# {a.title}", ""] + ([a.note, ""] if a.note else [])
    for hname, sel in halves.items():
        L += [f"## {hname}", ""] + table(by, configs, sel) + [""]
        if "fw" in by:
            L += ["Tracks whose category moved against fw (held whole > short stops > long stops > wrong tempo / never "
                  "locked):", ""]
            L += [f"- {c}: {moved(by, c, 'fw', sel)}" for c in configs if c != "fw"] + [""]
    for c in a.groups:
        rows = list(by[c].values())
        for hname, sel in halves.items():
            rs = [r for r in rows if sel(r)]
            L += [f"### {hname}, {c}: by reference class", ""] + PR.count_table(rs, PR.cls_group, "class") + [""]
            L += [f"### {hname}, {c}: by genre", ""] + PR.count_table(rs, lambda r: r["genre"], "genre") + [""]
    for c in a.worst:
        L += [f"## PRIVATE (the user's file names): the tracks {c} changed most against fw", "",
              "| track | class | genre | fw | " + c + " | within 1 % fw -> " + c + " |", "|---|---|---|---|---|---|"]
        diffs = []
        for k, r in by[c].items():
            o = by["fw"].get(k)
            if o is None:
                continue
            d = (r.get("within1", 0.0) if r.get("locked") else 0.0) - (o.get("within1", 0.0) if o.get("locked") else 0.0)
            diffs.append((d, r, o))
        diffs.sort(key=lambda x: x[0])
        for d, r, o in diffs[:12] + diffs[-12:]:
            L.append(f"| {PR.title_of(r)[:60]} | {r['class']} | {r['genre']} | {PR.category(o)} | {PR.category(r)} | "
                     f"{o.get('within1', 0):.2f} -> {r.get('within1', 0):.2f} |")
        L.append("")
    a.out.write_text("\n".join(L) + "\n")
    print(f"wrote {a.out}")


if __name__ == "__main__":
    main()
