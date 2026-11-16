"""The audio reference for every track: madmom offline beats segmented into tempo sections (reftempo.py), each change
checked against Beat This! final0 (from extract_live.py's cache when there).

    python studies/tempo-changes/reference.py --mmdb data/mmdb_traktor [--cache CACHE] --out REF.json [--table T.md]

REF.json is derived from madmom output (CC BY-NC-SA): it stays in the git-ignored scratch / farm folders, never pushed.
Only the summary table goes to results/.
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import json
import sys
from collections import Counter
from dataclasses import asdict
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BEAT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(BEAT))

import reftempo as RT

GENRES = BEAT / "studies" / "downbeat" / "genres.json"
FIXES = BEAT / "studies" / "halfbeat" / "fixes.json"


def load_index() -> list[dict]:
    return json.loads((BEAT / "data" / "traktor" / "index.json").read_text())


def one(rec: dict, mmdb: Path, cache: Path | None) -> dict | None:
    f = mmdb / f"{rec['name']}.npz"
    if not f.exists():
        return None
    beats = np.load(f)["beats"]
    bt = None
    if cache is not None and (cache / f"{rec['name']}.npz").exists():
        z = np.load(cache / f"{rec['name']}.npz")
        bt = z["bt"] if "bt" in z.files else None
    segs = RT.segment(beats)
    chs = RT.changes(segs, bt)
    return {"name": rec["name"], "file": rec["file"], "traktor_bpm": rec["bpm"], "dur": rec["dur"],
            "segments": [asdict(s) for s in segs], "changes": [asdict(c) for c in chs],
            "class": RT.track_class(chs), "octaves": sum(1 for c in chs if c.kind == "octave"), "bt": bt is not None}


def kind_of(row: dict) -> str:
    """steps / ramps / steps+ramps / sections, from the real changes."""
    real = [c for c in row["changes"] if c["kind"] in ("step", "ramp") and c["confidence"] in ("both", "madmom")]
    if not real:
        return "none"
    kinds = {c["kind"] for c in real}
    if kinds == {"ramp"}:
        return "ramps"
    if kinds == {"step"}:
        return "one step" if len(real) == 1 else "sections (several steps)"
    return "steps and ramps"


def table(rows: list[dict], genres: dict) -> str:
    """The summary: counts per class and kind, per genre group, and the changing tracks."""
    cls = Counter(r["class"] for r in rows)
    lines = [f"tracks {len(rows)}: " + ", ".join(f"{k} {v}" for k, v in sorted(cls.items())), ""]
    ch = [r for r in rows if r["class"] in ("changing", "probable")]
    kinds = Counter(kind_of(r) for r in ch)
    lines.append(f"tracks with an octave (half-time / double-time) section: {sum(1 for r in rows if r['octaves'])}")
    lines.append("changing + probable tracks by kind: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())))
    conf = Counter(c["confidence"] for r in ch for c in r["changes"] if c["kind"] in ("step", "ramp"))
    lines.append("their real changes by confidence: " + ", ".join(f"{k} {v}" for k, v in sorted(conf.items())))
    octs = Counter(c["kind"] for r in rows for c in r["changes"] if c["kind"] in ("octave", "triplet"))
    lines.append("octave / triplet relations in the reference (not changes): " +
                 ", ".join(f"{k} {v}" for k, v in sorted(octs.items())))
    groups = Counter((genres.get(r["name"]) or "?") for r in rows)
    gch = Counter((genres.get(r["name"]) or "?") for r in ch)
    gconf = Counter((genres.get(r["name"]) or "?") for r in rows if r["class"] == "changing")
    goct = Counter((genres.get(r["name"]) or "?") for r in rows if r["octaves"])
    lines += ["", "| genre | tracks | changing (confirmed) | + probable | with an octave section |", "|---|---|---|---|---|"]
    for g, n in groups.most_common(15):
        lines.append(f"| {g} | {n} | {gconf.get(g, 0)} | {gch.get(g, 0) - gconf.get(g, 0)} | {goct.get(g, 0)} |")
    lines += ["", "| track | genre | class | kind | changes (BPM before -> after, at s, confidence) |",
              "|---|---|---|---|---|"]
    for r in sorted(ch, key=lambda r: (r["class"], r["file"])):
        desc = "; ".join(f"{c['kind']} {c['bpm_before']:.1f}->{c['bpm_after']:.1f} at {c['t']:.0f} s"
                         + (f"-{c['t_end']:.0f}" if c["kind"] == "ramp" else "") + f" ({c['confidence']})"
                         for c in r["changes"] if c["kind"] in ("step", "ramp"))
        lines.append(f"| {Path(r['file']).stem[:60]} | {genres.get(r['name'], '')} | {r['class']} | {kind_of(r)} | "
                     f"{desc} |")
    return "\n".join(lines) + "\n"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mmdb", type=Path, default=BEAT / "data" / "mmdb_traktor")
    ap.add_argument("--cache", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--table", type=Path)
    a = ap.parse_args()
    rows = [r for r in (one(rec, a.mmdb, a.cache) for rec in load_index()) if r is not None]
    a.out.write_text(json.dumps(rows))
    genres = json.loads(GENRES.read_text()) if GENRES.exists() else {}
    text = table(rows, genres)
    if a.table:
        a.table.write_text(text)
    print(text[:3000])


if __name__ == "__main__":
    main()
