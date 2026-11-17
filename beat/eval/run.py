"""Run trackers over a dataset, score with mir_eval, write one JSON per tracker under runs/.

    .venv/bin/python -m eval.run --trackers madmom_online beatnet_online_pf --limit 50 --jobs 12
"""
from __future__ import annotations

import os

# One thread per worker: torch/numpy otherwise start one per core in every worker, and N workers
# then load the machine N x cores (2026-11-13: 14 workers froze the user's Mac at load 220).
for _var in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_var] = "1"
# numba (librosa's beat tracker) segfaults on gpufarm's LXC container with the host CPU detected; generic works
os.environ.setdefault("NUMBA_CPU_NAME", "generic")
os.environ.setdefault("NUMBA_CACHE_DIR", str(__import__("pathlib").Path(__file__).resolve().parent.parent / ".numba-cache"))

import argparse
import json
import time
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import mir_eval
import numpy as np
import soundfile as sf

from eval.data import DATASETS, ROOT
from eval.grid import grid_score
from eval.trackers import TRACKERS

_run = None


def default_jobs() -> int:
    """Half of the cores that are free right now (cores minus 1-minute load), at least 1: the user keeps the rest."""
    free = (os.cpu_count() or 2) - os.getloadavg()[0]
    return max(1, int(free // 2))


def _init(name: str) -> None:
    global _run
    import torch
    torch.set_num_threads(1)
    _run = TRACKERS[name].make()


def _score(ref: np.ndarray, est: np.ndarray) -> dict:
    if ref.size == 0:
        return {}
    ref = mir_eval.beat.trim_beats(ref)
    est = mir_eval.beat.trim_beats(np.sort(est))
    if est.size == 0:
        return {"F": 0.0, "CMLt": 0.0, "AMLt": 0.0}
    cml = mir_eval.beat.continuity(ref, est)
    return {"F": mir_eval.beat.f_measure(ref, est), "CMLt": cml[1], "AMLt": cml[3]}


def _one(piece) -> dict:
    t0 = time.perf_counter()
    try:
        beats, downbeats = _run(piece.audio)
    except Exception:
        return {"name": piece.name, "error": traceback.format_exc(limit=2)}
    secs = time.perf_counter() - t0
    dur = sf.info(str(piece.audio)).duration
    out = {
        "name": piece.name, "rtf": secs / dur, "beat": _score(piece.beats, beats),
        "grid": grid_score(piece.beats, beats),
        "est_beats": np.round(beats, 4).tolist(), "est_downbeats": np.round(downbeats, 4).tolist(),
    }
    if downbeats.size:
        out["downbeat"] = _score(piece.downbeats, downbeats)
    return out


def _mean(rows: list[dict], key: str) -> dict:
    vals = [r[key] for r in rows if key in r and r[key]]
    if not vals:
        return {}
    return {m: round(float(np.nanmean([v[m] for v in vals])), 4) for m in vals[0]}


def _size(piece) -> int:
    """Longest first, so the long mixes start at once and the short ones fill in around them."""
    return piece.audio.stat().st_size if piece.audio.exists() else 0


def run_tracker(name: str, pieces, jobs: int, tag: str) -> dict:
    # bundle pieces only when there are many (999 short GTZAN songs); one long mix per worker otherwise
    # (2026-11-13: chunksize 4 on 10 mixes used 3 of 12 workers and hit the farm's 20 min limit)
    chunk = max(1, len(pieces) // (jobs * 8))
    with ProcessPoolExecutor(jobs, initializer=_init, initargs=(name,)) as pool:
        rows = list(pool.map(_one, sorted(pieces, key=_size, reverse=True), chunksize=chunk))
    ok = [r for r in rows if "error" not in r]
    summary = {
        "tracker": name, "causal": TRACKERS[name].causal, "pieces": len(ok),
        "errors": len(rows) - len(ok), "beat": _mean(ok, "beat"), "downbeat": _mean(ok, "downbeat"),
        "grid": _mean(ok, "grid"),
        "rtf_per_core": round(float(np.median([r["rtf"] for r in ok])), 4) if ok else None,
    }
    out = ROOT / "runs" / f"{tag}_{name}.json"
    out.parent.mkdir(exist_ok=True)
    out.write_text(json.dumps({"summary": summary, "pieces": rows}, indent=1))
    return summary


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--trackers", nargs="+", default=list(TRACKERS))
    ap.add_argument("--limit", type=int)
    ap.add_argument("--dataset", choices=list(DATASETS), default="gtzan")
    ap.add_argument("--shard", default="0/1", help="i/n: run every n-th piece starting at i (the farm kills jobs after 20 min)")
    ap.add_argument("--jobs", type=int, default=int(os.environ.get("JOBS", 0)) or default_jobs(),
                    help="default: $JOBS (the farm slot's cap), else half of the free cores right now")
    args = ap.parse_args()
    os.nice(19)   # lowest priority, inherited by the workers
    print(f"load {os.getloadavg()[0]:.1f}/{os.cpu_count()}, {args.jobs} workers x 1 thread, nice 19", flush=True)
    i, n = (int(x) for x in args.shard.split("/"))
    pieces = DATASETS[args.dataset](args.limit)[i::n]
    tag = args.dataset if n == 1 else f"{args.dataset}.{i}of{n}"
    print(f"{len(pieces)} {args.dataset} pieces (shard {args.shard})", flush=True)
    for name in args.trackers:
        print(json.dumps(run_tracker(name, pieces, args.jobs, tag)), flush=True)


if __name__ == "__main__":
    main()
