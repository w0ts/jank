"""Training inputs and the offline reference beats, resumable.

For each WAV (the user's tracks, GTZAN, the mixes): madmom's online feature chain (the same as the C front end,
162 inputs at 100 fps) -> FEAT_DIR/<name>.npy (float32), and optionally madmom's offline beats (RNNBeatProcessor
offline + DBN, as eval.trackers does) -> FEAT_DIR/<name>.offline.json. Done files are skipped, so a killed
job just resumes.

    .venv/bin/python -I train/prep_features.py DATASET FEAT_DIR SHARD NSHARDS [--offline]
DATASET is traktor, gtzan or mixes. Shard i takes every NSHARDS-th piece starting at i.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))


def feature_chain():
    from madmom.audio.signal import FramedSignalProcessor
    from madmom.audio.spectrogram import (FilteredSpectrogramProcessor, LogarithmicSpectrogramProcessor,
                                          SpectrogramDifferenceProcessor)
    from madmom.audio.stft import ShortTimeFourierTransformProcessor
    from madmom.processors import SequentialProcessor
    return SequentialProcessor((
        FramedSignalProcessor(frame_size=2048), ShortTimeFourierTransformProcessor(),
        FilteredSpectrogramProcessor(num_bands=12, fmin=30, fmax=17000, norm_filters=True),
        LogarithmicSpectrogramProcessor(mul=1, add=1),
        SpectrogramDifferenceProcessor(diff_ratio=0.5, positive_diffs=True, stack_diffs=np.hstack)))


def read_mono44k(path: Path) -> np.ndarray:
    import soundfile as sf
    import soxr
    y, sr = sf.read(str(path), dtype="float32", always_2d=True)
    y = y.mean(axis=1)
    return soxr.resample(y, sr, 44100) if sr != 44100 else y


def pieces(dataset: str) -> list[tuple[str, Path]]:
    from eval.data import DATASETS
    return [(p.name, p.audio) for p in DATASETS[dataset]()]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dataset")
    ap.add_argument("feat_dir")
    ap.add_argument("shard", type=int)
    ap.add_argument("nshards", type=int)
    ap.add_argument("--offline", action="store_true")
    ap.add_argument("--speed", type=float, default=1.0, help="play the audio this much faster (tempo and pitch)")
    ap.add_argument("--names", help="JSON list of piece names to do")
    a = ap.parse_args()
    out = Path(a.feat_dir)
    out.mkdir(parents=True, exist_ok=True)
    from madmom.audio.signal import Signal
    chain = feature_chain()
    rnn = dbn = None
    if a.offline:
        from madmom.features.beats import DBNBeatTrackingProcessor, RNNBeatProcessor
        rnn = RNNBeatProcessor(online=False)
        dbn = DBNBeatTrackingProcessor(fps=100)
    names = set(json.loads(Path(a.names).read_text())) if a.names else None
    todo = [p for p in pieces(a.dataset) if names is None or p[0] in names][a.shard::a.nshards]
    tag = "" if a.speed == 1.0 else f"@{a.speed:g}"
    for name, audio in todo:
        fnpy, fjs = out / f"{name}{tag}.npy", out / f"{name}.offline.json"
        need_feat = not fnpy.exists()
        need_off = a.offline and not fjs.exists()
        if not (need_feat or need_off):
            continue
        t0 = time.time()
        try:
            y = read_mono44k(audio)
            if a.speed != 1.0:
                import soxr
                y = soxr.resample(y, 44100 * a.speed, 44100).astype(np.float32)
            sig = Signal(y, sample_rate=44100, num_channels=1)
            if need_feat:
                feats = np.asarray(chain(sig), dtype=np.float32)
                np.save(str(fnpy) + ".tmp.npy", feats)
                Path(str(fnpy) + ".tmp.npy").rename(fnpy)
            if need_off:
                beats = np.asarray(dbn(rnn(sig)))
                fjs.write_text(json.dumps({"beats": [round(float(b), 3) for b in beats]}))
            print(f"{name}: {len(y) / 44100:.0f} s in {time.time() - t0:.1f} s", flush=True)
        except Exception as e:
            print(f"{name}: FAILED {e!r}", flush=True)


if __name__ == "__main__":
    main()
