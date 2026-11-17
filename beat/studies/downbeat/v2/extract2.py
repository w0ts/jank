"""Stage 1 of the second downbeat study: per track, cache what every later experiment needs, once.

For each track: the tracker's ONLINE beats (live.tracker.OnlineTracker, 90-180 BPM, the study's instrument), the
reference beats and bar positions, and the 81-band log spectrogram L (float16) framed as extract.py frames it (frame k
ends at sample k * 441; the device's jb_fe centres its frames, but it also labels its beats by its own frame index, so
"the frame at the beat" holds the same audio in both). Features are computed later from L (feats.py), so trying a new
feature does not re-run the tracker.

    python extract2.py --dataset traktor --jobs 14 --cache CACHE_DIR [--limit N]
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
sys.path.insert(0, str(HERE.parent))     # the first study's extract.py: framing, positions, audio loading
sys.path.insert(0, os.environ.get("BL_REPO", str(HERE.parents[2])))     # beat/ (eval.data)

SR = 44100


def one(job) -> str:
    from extract import audio_path, frames_logbands, load_audio, online_beats
    piece, file_rel, positions, out = job
    if out.exists():
        return f"{piece.name} cached"
    t0 = time.perf_counter()
    try:
        y = load_audio(audio_path(piece, file_rel))
        L = frames_logbands(y)
        ob = online_beats(y)
        tmp = out.with_suffix(".tmp.npz")
        np.savez(tmp, L=L.astype(np.float16), online=ob, ref_beats=np.asarray(piece.beats),
                 ref_pos=np.asarray(positions), dur=len(y) / SR)
        tmp.rename(out)
        return f"{piece.name} ok {len(y) / SR:.0f} s in {time.perf_counter() - t0:.0f} s"
    except Exception:
        return f"{piece.name} ERROR {traceback.format_exc(limit=3)[-400:]}"


def jobs_for(dataset: str, limit: int | None, cache: Path) -> list:
    import json

    from eval import data
    from extract import gtzan_positions, traktor_positions
    cache.mkdir(parents=True, exist_ok=True)
    if dataset == "traktor":
        pieces = data.traktor(limit)
        files = {r["name"]: r["file"] for r in json.loads((data.DATA / "traktor" / "index.json").read_text())}
        return [(p, files[p.name], traktor_positions(p.beats, p.downbeats), cache / f"{p.name}.npz") for p in pieces]
    pieces = data.gtzan(limit)
    return [(p, None, gtzan_positions(p.name), cache / f"{p.name}.npz") for p in pieces]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", choices=["traktor", "gtzan"], required=True)
    ap.add_argument("--limit", type=int)
    ap.add_argument("--jobs", type=int, required=True)
    ap.add_argument("--cache", type=Path, required=True)
    a = ap.parse_args()
    os.nice(19)
    jobs = jobs_for(a.dataset, a.limit, a.cache / a.dataset)
    print(f"load {os.getloadavg()[0]:.1f}, {len(jobs)} pieces, {a.jobs} workers x 1 thread, nice 19", flush=True)
    t0 = time.time()
    with ProcessPoolExecutor(a.jobs) as pool:
        for k, msg in enumerate(pool.map(one, jobs, chunksize=1)):
            if (k + 1) % 25 == 0 or "ERROR" in msg:
                print(f"{k + 1}/{len(jobs)} {time.time() - t0:.0f} s {msg}", flush=True)
    print(f"done {a.dataset} {time.time() - t0:.0f} s", flush=True)


if __name__ == "__main__":
    main()
