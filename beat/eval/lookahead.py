"""Beat This! with a limited look-ahead: quality as a function of latency.

Windows of CONTEXT frames slide over the spectrogram in STEP-frame hops. In the window that ends at
frame e, frame e - k was predicted with exactly k frames of future. For look-ahead L, each frame takes
its logits from the window where it had between L and L + STEP frames of future, so one sweep of windows
serves every L. Peak picking adds PEAK frames of future. The grid metric charges the whole delay.

    .venv/bin/python -m eval.lookahead --ckpt small0 --every 10 --jobs 14
"""
from __future__ import annotations

import argparse
import json
import os
from concurrent.futures import ProcessPoolExecutor

import numpy as np
import torch

from eval.data import DATA, DATASETS, ROOT
from eval.grid import grid_score
from eval.run import _score, default_jobs   # importing eval.run also caps every worker at one thread

os.environ.setdefault("TORCH_HOME", str(DATA / "torch"))   # checkpoints stay in the project

FPS = 50
CONTEXT = 500                  # 10 s of past in each window
STEP = 10                      # 0.2 s between windows
PEAK = 3                       # Beat This! peak picking looks +-3 frames
LOOKAHEADS = (0, 5, 12)   # frames: 0, 0.1, 0.24 s (GTZAN 2026-11-13: no gain past 0.1 s, 0.5-2 s only cost delay)

_a2f = None


def _init(ckpt: str) -> None:
    global _a2f
    from beat_this.inference import Audio2Frames
    torch.set_num_threads(1)
    _a2f = Audio2Frames(checkpoint_path=ckpt, device="cpu")


def _streams(spect: torch.Tensor) -> dict[int, np.ndarray]:
    """Beat logits per frame, assembled for each look-ahead L from the sliding windows."""
    n = spect.shape[0]
    out = {L: np.full(n, -1000.0, dtype=np.float32) for L in LOOKAHEADS}
    with torch.inference_mode():
        for e in range(STEP, n + STEP, STEP):
            e = min(e, n)
            s = max(0, e - CONTEXT)
            logits = _a2f.model(spect[s:e].unsqueeze(0))["beat"][0].numpy()
            for L in LOOKAHEADS:
                lo, hi = max(s, e - L - STEP), e - L      # frames with L..L+STEP frames of future here
                if hi > lo:
                    out[L][lo:hi] = logits[lo - s: hi - s]
    return out


def _peaks(logits: np.ndarray) -> np.ndarray:
    """Beat This! minimal post-processing: local maxima within +-PEAK frames with logit > 0, deduplicated."""
    padded = np.pad(logits, PEAK, constant_values=-1000.0)
    windows = np.lib.stride_tricks.sliding_window_view(padded, 2 * PEAK + 1)
    frames = np.flatnonzero((logits == windows.max(axis=1)) & (logits > 0))
    keep = [f for i, f in enumerate(frames) if i == 0 or f - frames[i - 1] > 1]
    return np.array(keep, dtype=float) / FPS


def _one(piece) -> dict:
    import soundfile as sf
    signal, sr = sf.read(str(piece.audio), dtype="float32")
    t0 = 0.0
    if _excerpt_s and len(signal) / sr > _excerpt_s:
        t0 = round(len(signal) / sr / 2 - _excerpt_s / 2, 2)      # the middle of the track
        signal = signal[int(t0 * sr): int((t0 + _excerpt_s) * sr)]
    t1 = t0 + len(signal) / sr
    ref = piece.beats[(piece.beats >= t0) & (piece.beats < t1)] - t0
    spect = _a2f.signal2spect(signal, sr)
    out = {}
    for L, logits in _streams(spect).items():
        beats = _peaks(logits)
        out[L] = {
            "name": piece.name, "window": [t0, t1], "delay_s": delay_s(L), "rtf": float("nan"),
            "beat": _score(ref, beats), "grid": grid_score(ref, beats, latency_s=delay_s(L)),
            "est_beats": np.round(beats, 4).tolist(), "est_downbeats": [],
        }
    return out


def delay_s(L: int) -> float:
    """The whole decision delay of look-ahead L: L frames, up to one window step, and the peak picking."""
    return (L + STEP + PEAK) / FPS


_excerpt_s = 0.0


def _init_all(ckpt: str, excerpt_s: float) -> None:
    global _excerpt_s
    _excerpt_s = excerpt_s
    _init(ckpt)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ckpt", default="small0")
    ap.add_argument("--dataset", choices=list(DATASETS), default="gtzan")
    ap.add_argument("--every", type=int, default=10, help="every n-th piece (GTZAN: 10 = 100 pieces, 10 per genre)")
    ap.add_argument("--excerpt", type=float, default=0.0, help="seconds from the middle of each piece (0 = all)")
    ap.add_argument("--shard", default="0/1", help="i/n of the selected pieces (the farm kills jobs after 20 min)")
    ap.add_argument("--jobs", type=int, default=int(os.environ.get("JOBS", 0)) or default_jobs(),
                    help="default: $JOBS (the farm slot's cap), else half of the free cores right now")
    args = ap.parse_args()
    os.nice(19)
    i, n = (int(x) for x in args.shard.split("/"))
    pieces = DATASETS[args.dataset]()[:: args.every][i::n]
    tag = args.dataset if n == 1 else f"{args.dataset}.{i}of{n}"
    with ProcessPoolExecutor(args.jobs, initializer=_init_all, initargs=(args.ckpt, args.excerpt)) as pool:
        results = list(pool.map(_one, pieces))
    for L in LOOKAHEADS:
        rows = [r[L] for r in results]
        name = f"beat_this_{args.ckpt}_stream_L{L}"
        summary = {"tracker": name, "delay_s": delay_s(L), "pieces": len(rows),
                   "F": round(float(np.mean([r["beat"]["F"] for r in rows])), 4)}
        print(json.dumps(summary), flush=True)
        out = ROOT / "runs" / f"{tag}_{name}.json"
        out.write_text(json.dumps({"summary": summary, "pieces": rows}, indent=1))


if __name__ == "__main__":
    main()
