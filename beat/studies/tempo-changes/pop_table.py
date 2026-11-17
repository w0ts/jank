"""Per-track table for a group of tracks (by a word of the file name, e.g. the Charli XCX tracks): the reference's
sections (tempo and octave relations) and, per chain, stopped / held shares.

    python studies/tempo-changes/pop_table.py ROWS.jsonl REF.json --match "charli" --configs today tuned
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import reftempo as RT


def sections_text(ref: dict) -> str:
    secs = RT.sections([RT.Segment(**s) for s in ref["segments"]])
    return ", ".join(f"{s.bpm0:.0f}" + ("~" if s.ramp else "") for s in secs)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("rows", type=Path)
    ap.add_argument("ref", type=Path)
    ap.add_argument("--match", default="charli")
    ap.add_argument("--configs", nargs="+", required=True)
    a = ap.parse_args()
    refs = {r["name"]: r for r in json.loads(a.ref.read_text()) if a.match.lower() in r["file"].lower()}
    by: dict = {}
    for line in a.rows.read_text().splitlines():
        r = json.loads(line)
        if r.get("name") in refs and r["config"] in a.configs:
            by.setdefault(r["name"], {})[r["config"]] = r
    head = "| track | split | sections (BPM, ~ ramp) | class | " + " | ".join(f"{c}: stopped / held" for c in a.configs) + " |"
    print(head)
    print("|" + "---|" * (4 + len(a.configs)))
    for name, d in sorted(by.items(), key=lambda kv: refs[kv[0]]["file"]):
        ref = refs[name]
        any_row = next(iter(d.values()))
        cells = " | ".join(f"{d[c]['stopped']:.2f} / {d[c]['held']:.2f}" if c in d else "-" for c in a.configs)
        print(f"| {Path(ref['file']).stem.split(' - ')[-1][:40]} | {any_row['split']} | {sections_text(ref)} | "
              f"{ref['class']} | {cells} |")


if __name__ == "__main__":
    main()
