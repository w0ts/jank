"""A MEASURING INSTRUMENT only: madmom's offline downbeat tracker (RNNDownBeatProcessor + DBN, 3/4 and 4/4) on every
Traktor track, to find the tracks where Traktor's "1" is probably right (Traktor and madmom agree). madmom's models are
CC BY-NC-SA 4.0: nothing shipped is fitted to this output; it only selects and scores.

    python mmdb.py --jobs 8 --cache CACHE_DIR       -> CACHE_DIR/mmdb_traktor/<name>.npz (beats, pos)
"""
from __future__ import annotations

import argparse
import os
import sys
import time
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, os.environ.get("BL_REPO", str(HERE.parents[2])))


def one(job) -> str:
    from extract import audio_path, load_audio, madmom_downbeats
    piece, file_rel, out = job
    if out.exists():
        return f"{piece.name} cached"
    t0 = time.perf_counter()
    try:
        beats, pos = madmom_downbeats(load_audio(audio_path(piece, file_rel)))
        tmp = out.with_suffix(".tmp.npz")
        np.savez(tmp, beats=beats, pos=pos)
        tmp.rename(out)
        return f"{piece.name} ok {time.perf_counter() - t0:.0f} s"
    except Exception:
        return f"{piece.name} ERROR {traceback.format_exc(limit=3)[-400:]}"


def main() -> None:
    import json

    from eval import data
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, required=True)
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--limit", type=int)
    a = ap.parse_args()
    os.nice(19)
    outdir = a.cache / "mmdb_traktor"
    outdir.mkdir(parents=True, exist_ok=True)
    files = {r["name"]: r["file"] for r in json.loads((data.DATA / "traktor" / "index.json").read_text())}
    jobs = [(p, files[p.name], outdir / f"{p.name}.npz") for p in data.traktor(a.limit)]
    print(f"load {os.getloadavg()[0]:.1f}, {len(jobs)} pieces, {a.jobs} workers", flush=True)
    t0 = time.time()
    with ProcessPoolExecutor(a.jobs) as pool:
        for k, msg in enumerate(pool.map(one, jobs, chunksize=1)):
            if (k + 1) % 25 == 0 or "ERROR" in msg:
                print(f"{k + 1}/{len(jobs)} {time.time() - t0:.0f} s {msg}", flush=True)
    print(f"done mmdb {time.time() - t0:.0f} s", flush=True)


if __name__ == "__main__":
    main()
