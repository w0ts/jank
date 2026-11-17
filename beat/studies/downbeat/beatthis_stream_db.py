"""Beat This! (small0) streamed as eval/lookahead.py streams it (10 s of past, a new window every 0.2 s, L frames of
future), keeping its DOWNBEAT head too: est beats and downbeats per track -> a pickle analyze_models.py scores.

    python beatthis_stream_db.py --every 16 --lookahead 5 --jobs 16 --out results/beatthis_L5.pkl
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import pickle
import sys
import time
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent))

_L = 5


def _init(ckpt: str, lookahead: int) -> None:
    global _L
    from eval import lookahead as la
    _L = lookahead
    la._init(ckpt)


def _streams(spect) -> dict[str, np.ndarray]:
    import torch
    from eval import lookahead as la
    n = spect.shape[0]
    out = {k: np.full(n, -1000.0, dtype=np.float32) for k in ("beat", "downbeat")}
    with torch.inference_mode():
        for e in range(la.STEP, n + la.STEP, la.STEP):
            e = min(e, n)
            s = max(0, e - la.CONTEXT)
            pred = la._a2f.model(spect[s:e].unsqueeze(0))
            lo, hi = max(s, e - _L - la.STEP), e - _L
            if hi > lo:
                for k in out:
                    out[k][lo:hi] = pred[k][0].numpy()[lo - s: hi - s]
    return out


def _one(piece) -> dict:
    import soundfile as sf
    from eval import lookahead as la
    t0 = time.perf_counter()
    signal, sr = sf.read(str(piece.audio), dtype="float32")
    spect = la._a2f.signal2spect(signal, sr)
    st = _streams(spect)
    beats, downs = la._peaks(st["beat"]), la._peaks(st["downbeat"])
    # Beat This! post-processing: a downbeat is snapped to its nearest beat
    if len(beats) and len(downs):
        downs = np.unique(beats[np.abs(beats[None, :] - downs[:, None]).argmin(axis=1)])
    return {"name": piece.name, "est_beats": beats, "est_downbeats": downs, "delay_s": la.delay_s(_L),
            "secs": time.perf_counter() - t0, "dur": len(signal) / sr}


def main() -> None:
    from eval.data import traktor
    ap = argparse.ArgumentParser()
    ap.add_argument("--ckpt", default="small0")
    ap.add_argument("--every", type=int, default=16)
    ap.add_argument("--limit", type=int)
    ap.add_argument("--lookahead", type=int, default=5)
    ap.add_argument("--jobs", type=int, required=True)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    os.nice(19)
    pieces = traktor()[::a.every][:a.limit]
    print(f"load {os.getloadavg()[0]:.1f}, {len(pieces)} pieces, {a.jobs} workers, nice 19", flush=True)
    with ProcessPoolExecutor(a.jobs, initializer=_init, initargs=(a.ckpt, a.lookahead)) as pool:
        rows = list(pool.map(_one, pieces))
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_bytes(pickle.dumps(rows))
    print(f"done {len(rows)}, median rtf {np.median([r['secs'] / r['dur'] for r in rows]):.3f}", flush=True)


if __name__ == "__main__":
    main()
