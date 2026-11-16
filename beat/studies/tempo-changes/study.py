"""Replay and score every track (extract_live.py's cache) for each chain (configs.py), against the audio reference.

    python studies/tempo-changes/study.py --cache CACHE --ref REF.json --mmdb data/mmdb_traktor --out ROWS.jsonl
        --configs today m2i m2i+rate [--sweep SWEEP.json] [--split dev] [--classes changing] [--jobs N] [--limit N]

SWEEP.json: {"name": {"base": "m2i+rate", "consts": {"follow_bpm.MANOEUVRE_SIGMA": 2.5}}, ...}; its names may be
listed in --configs (or --all-sweep runs the bases given plus every sweep entry).
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import json
import sys
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BEAT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(BEAT))

import chain as C
import configs as CF
import reftempo as RT
import score as S


def ref_of(row: dict) -> tuple[list[RT.Segment], list[RT.Change]]:
    return [RT.Segment(**s) for s in row["segments"]], [RT.Change(**c) for c in row["changes"]]


def one(job: tuple) -> list[str]:
    path, ref_row, mmdb, cfgs = job
    try:
        track = C.Track.load(path)
        segs, chs = ref_of(ref_row)
        beats = np.load(Path(mmdb) / f"{ref_row['name']}.npz")["beats"]
        out = []
        for cfg in cfgs:
            run = C.replay(track, cfg)
            row = S.score(run, segs, chs, beats, track.act, track.rms)
            row = {k: v for k, v in row.items() if not k.startswith("_")}
            out.append(json.dumps({"name": ref_row["name"], "split": ref_row["split"], "class": ref_row["class"],
                                   "config": cfg.name, **row}, default=float))
        return out
    except Exception:
        return [json.dumps({"name": ref_row["name"], "error": traceback.format_exc(limit=5)[-1000:]})]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--ref", type=Path, required=True)
    ap.add_argument("--mmdb", type=Path, default=BEAT / "data" / "mmdb_traktor")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--configs", nargs="+", default=["today", "m2i", "m2i+rate"])
    ap.add_argument("--sweep", type=Path)
    ap.add_argument("--all-sweep", action="store_true")
    ap.add_argument("--split", choices=["dev", "test"])
    ap.add_argument("--classes", nargs="+")
    ap.add_argument("--names", nargs="+")
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--limit", type=int)
    a = ap.parse_args()
    extra = json.loads(a.sweep.read_text()) if a.sweep else {}
    names = a.configs + (list(extra) if a.all_sweep else [])
    cfgs = CF.named(names, extra)
    order = {r["name"]: i for i, r in enumerate(json.loads((BEAT / "data" / "traktor" / "index.json").read_text()))}
    refs = []
    for r in json.loads(a.ref.read_text()):
        r["split"] = "dev" if order[r["name"]] % 2 == 0 else "test"
        if (a.split and r["split"] != a.split) or (a.classes and r["class"] not in a.classes):
            continue
        if a.names and r["name"] not in a.names:
            continue
        if (a.cache / f"{r['name']}.npz").exists():
            refs.append(r)
    if a.limit:
        refs = refs[:a.limit]
    print(f"{len(refs)} tracks x {len(cfgs)} configs", flush=True)
    jobs = [(str(a.cache / f"{r['name']}.npz"), r, str(a.mmdb), cfgs) for r in refs]
    with ProcessPoolExecutor(a.jobs) as pool, a.out.open("w") as f:
        for k, lines in enumerate(pool.map(one, jobs, chunksize=1), 1):
            f.write("".join(x + "\n" for x in lines))
            f.flush()
            if k % 50 == 0:
                print(f"{k}/{len(refs)}", flush=True)
    print("STUDY DONE", flush=True)


if __name__ == "__main__":
    main()
