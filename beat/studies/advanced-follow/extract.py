"""Per item of the material: render the audio once, keep what every later replay needs.

  act    the live tracker's activation per frame (one LSTM net, online, live.tracker's framing), float32, 100 fps
  beats  the live tracker's beats (online DBN 90-180 BPM on act): today's tracker, = live.tracker.OnlineTracker
  L10    the tracker's 81 log bands averaged over blocks of 10 frames (0.1 s), float16: the detector's input
  ref    the reference beats; info: switch / variation / change times and tempos (JSON)

The replays (DBN resets, estimators, the detector) run from these files without the audio.

    python studies/advanced-follow/extract.py --out CACHE --jobs N [--split test] [--family same] [--limit N]
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import json
import sys
import time
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1]))

import material as M

HOP, FRAME, BLOCK = 441, 2048, 10
N_SAME = {"dev": 25, "test": 40}
N_DIFF = {"dev": 25, "test": 40}

_recs: dict = {}


def activation(y: np.ndarray) -> np.ndarray:
    """The live tracker's activation, frame k = the 2048 samples ending at sample k * HOP (equal to OnlineTracker:
    checked on a 60 s excerpt, every beat identical)."""
    from madmom.audio.signal import Signal
    from madmom.features.beats import RNNBeatProcessor
    from madmom.models import BEATS_LSTM
    rnn = RNNBeatProcessor(online=True, nn_files=[BEATS_LSTM[0]], origin="stream", num_frames=1, fps=100)
    pad = np.concatenate([np.zeros(FRAME - 1, np.float32), y])
    n = (len(y) - 1) // HOP + 1
    out = np.empty(n, np.float32)
    for k in range(n):
        out[k] = float(np.atleast_1d(rnn.process(Signal(pad[k * HOP:k * HOP + FRAME], sample_rate=M.SR,
                                                        num_channels=1), reset=False))[0])
    return out


def dbn_beats(act: np.ndarray, lo: float = 90.0, hi: float = 180.0) -> np.ndarray:
    from madmom.features.beats import DBNBeatTrackingProcessor
    dbn = DBNBeatTrackingProcessor(fps=100, online=True, min_bpm=lo, max_bpm=hi)
    dbn.reset()
    beats: list[float] = []
    for a in act:
        beats.extend(float(b) for b in dbn.process_online(np.atleast_1d(a), reset=False))
    return np.asarray(beats)


def logbands(y: np.ndarray, n: int) -> np.ndarray:
    """(n, 81) log10(1 + filterbank(|STFT|)), the same frames as activation()."""
    from madmom.audio.filters import LogarithmicFilterbank
    bins = np.fft.rfftfreq(FRAME, 1 / M.SR)[:FRAME // 2]
    fb = np.asarray(LogarithmicFilterbank(bins, num_bands=12, fmin=30, fmax=17000, norm_filters=True,
                                          unique_filters=True), dtype=np.float32)
    pad = np.concatenate([np.zeros(FRAME - 1, np.float32), y, np.zeros(FRAME, np.float32)])
    win = np.hanning(FRAME).astype(np.float32)
    out = np.empty((n, fb.shape[1]), np.float32)
    for s in range(0, n, 2048):
        ks = np.arange(s, min(n, s + 2048))
        idx = ks[:, None] * HOP + np.arange(FRAME)[None, :]
        mag = np.abs(np.fft.rfft(pad[idx] * win, axis=1))[:, :FRAME // 2].astype(np.float32)
        out[ks] = np.log10(1.0 + mag @ fb)
    return out


def blocks(L: np.ndarray) -> np.ndarray:
    n = L.shape[0] // BLOCK
    return L[:n * BLOCK].reshape(n, BLOCK, -1).mean(axis=1)


def out_path(cache: Path, item: M.Item) -> Path:
    return cache / (item.name.replace("/", "__").replace("+", "_") + ".npz")


def one(job: tuple[M.Item, str]) -> str:
    item, cache = job
    dst = out_path(Path(cache), item)
    if dst.exists():
        return f"{item.name} cached"
    t0 = time.perf_counter()
    try:
        if not _recs:
            _recs.update({r["name"]: r for r in M.load_index()})
        y, ref, info = M.render(item, _recs)
        act = activation(y)
        L10 = blocks(logbands(y, act.size))
        beats = dbn_beats(act)
        meta = {"name": item.name, "family": item.family, "kind": item.kind, "split": item.split,
                "tracks": list(item.tracks), "params": item.params, **info}
        tmp = dst.with_name(dst.stem + ".tmp.npz")
        np.savez(tmp, act=act, beats=beats, L10=L10.astype(np.float16), ref=np.asarray(ref, dtype=float),
                 meta=json.dumps(meta))
        tmp.rename(dst)
        return f"{item.name} ok {len(y) / M.SR:.0f} s in {time.perf_counter() - t0:.1f} s"
    except Exception:
        return f"{item.name} ERROR {traceback.format_exc(limit=4)[-600:]}"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--split", choices=["dev", "test"])
    ap.add_argument("--family", choices=["real", "same", "diff"])
    ap.add_argument("--kind")
    ap.add_argument("--limit", type=int)
    args = ap.parse_args()
    items = M.plan(N_SAME, N_DIFF)
    items = [i for i in items if (args.split is None or i.split == args.split)
             and (args.family is None or i.family == args.family) and (args.kind is None or i.kind == args.kind)]
    if args.limit:
        items = items[:args.limit]
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "plan.json").write_text(json.dumps([{"name": i.name, "split": i.split} for i in items]))
    print(f"{len(items)} items", flush=True)
    with ProcessPoolExecutor(args.jobs) as pool:
        for k, msg in enumerate(pool.map(one, [(i, str(args.out)) for i in items], chunksize=1), 1):
            print(f"{k}/{len(items)} {msg}", flush=True)
    print("EXTRACT DONE", flush=True)


if __name__ == "__main__":
    main()
