"""Is the ESP32-S3's own ADC good enough? Degrade audio the way it would, run the live pipeline, compare with clean.

Variants (applied to 2 min from the middle of each Traktor track, mono):
  clean        the decoded track
  adc12_10b    anti-alias low-pass 16 kHz, white noise for ~10 effective bits, 12-bit quantisation
  adc12_8b     the same with noise for ~8 effective bits (a pessimistic SAR ADC next to Wi-Fi)
  lowband      a 22.05 kHz sample rate (band to ~10 kHz), upsampled back for the tracker
The live pipeline: madmom online -> ModeBpm on a 0.5 s clock; scored against Traktor's BPM (octave-folded).
A line-level feed is assumed to use most of the ADC range (peak at -3 dBFS).

    .venv/bin/python -m eval.adc_sim --every 8 --jobs 12
"""
from __future__ import annotations

import argparse
import json
import os
from concurrent.futures import ProcessPoolExecutor

import numpy as np

from eval.data import DATA
from eval.locked_eval import folded_error
from eval.mode_eval import trace

SR = 44100
EXCERPT_S = 120.0
VARIANTS = ("clean", "adc12_10b", "adc12_8b", "lowband")


def degrade(y: np.ndarray, variant: str, rng: np.random.Generator) -> np.ndarray:
    import soxr
    from scipy.signal import butter, sosfilt
    y = y / (np.max(np.abs(y)) + 1e-9) * 10 ** (-3 / 20)
    if variant == "clean":
        return y
    if variant == "lowband":
        return soxr.resample(soxr.resample(y, SR, 22050), 22050, SR).astype(np.float32)
    bits = 10 if variant == "adc12_10b" else 8
    y = sosfilt(butter(4, 16000, fs=SR, output="sos"), y)
    y = y + rng.normal(0, 2.0 ** -bits / np.sqrt(3), y.size)       # noise of `bits` effective bits (full scale +-1)
    return (np.round(y * 2047) / 2047).astype(np.float32)            # 12-bit quantisation


def _one(args: tuple[dict, str]) -> dict:
    meta, root = args
    import soundfile as sf
    from madmom.audio.signal import Signal
    from madmom.features.beats import DBNBeatTrackingProcessor, RNNBeatProcessor
    y, sr = sf.read(os.path.join(root, meta["audio"]), dtype="float32")
    t0 = max(0.0, len(y) / sr / 2 - EXCERPT_S / 2)
    y = y[int(t0 * sr): int((t0 + EXCERPT_S) * sr)]
    rng = np.random.default_rng(abs(hash(meta["name"])) % 2 ** 32)
    rnn = RNNBeatProcessor(online=True)
    out = {"name": meta["name"]}
    for v in VARIANTS:
        sig = Signal(degrade(y, v, rng), sample_rate=SR, num_channels=1)
        beats = DBNBeatTrackingProcessor(fps=100, online=True).process_online(rnn(sig), reset=True)
        _, bpms, _ = trace(np.asarray(beats), EXCERPT_S)
        err = folded_error(bpms, meta["bpm"])
        run = ~np.isnan(err)
        out[v] = {"running": float(run.mean()), "right": float(np.mean(err[run] <= 0.1)) if run.any() else 0.0,
                  "wrong": float(np.mean(err[run] > 0.5)) if run.any() else 0.0}
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--every", type=int, default=8)
    ap.add_argument("--jobs", type=int, default=int(os.environ.get("JOBS", "2")))
    args = ap.parse_args()
    os.nice(19)
    root = DATA / "traktor"
    index = json.loads((root / "index.json").read_text())[:: args.every]
    with ProcessPoolExecutor(args.jobs) as pool:
        rows = list(pool.map(_one, [(m, str(root)) for m in index]))
    print(f"{len(rows)} tracks, 2 min each; shares: running of all time, right (+-0.1 BPM) / wrong (>0.5) of running")
    for v in VARIANTS:
        a = np.array([[r[v]["running"], r[v]["right"], r[v]["wrong"]] for r in rows])
        print(f"  {v:10} running {a[:, 0].mean():.2f}  right {a[:, 1].mean():.2f}  wrong {a[:, 2].mean():.3f}")


if __name__ == "__main__":
    main()
