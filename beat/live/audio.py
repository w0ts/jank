"""Decoding audio files and finding sound devices."""
from __future__ import annotations

from pathlib import Path

import numpy as np

from live.tracker import SAMPLE_RATE


def load_audio(path: str | Path) -> np.ndarray:
    """(frames, 2) float32 at 44.1 kHz, any format libsndfile reads (wav, flac, mp3, ...)."""
    import soundfile as sf
    import soxr
    p = Path(path)
    if not p.is_file():
        raise FileNotFoundError(f"audio file not found: {p}")
    y, sr = sf.read(str(p), dtype="float32", always_2d=True)
    if sr != SAMPLE_RATE:
        y = soxr.resample(y, sr, SAMPLE_RATE)
    if y.shape[1] == 1:
        y = np.repeat(y, 2, axis=1)
    return np.ascontiguousarray(y[:, :2], dtype=np.float32)


def to_mono(stereo: np.ndarray) -> np.ndarray:
    return stereo.mean(axis=1, dtype=np.float32) if stereo.ndim == 2 else stereo


def input_devices() -> list[tuple[int, str, int]]:
    import sounddevice as sd
    return [(i, d["name"], d["max_input_channels"]) for i, d in enumerate(sd.query_devices())
            if d["max_input_channels"] > 0]


def find_input_device(name: str | None) -> int:
    """Index of the input device whose name contains `name` (case-insensitive); None -> BlackHole."""
    wanted = name or "BlackHole"
    devices = input_devices()
    for index, dev_name, _ in devices:
        if wanted.lower() in dev_name.lower():
            return index
    names = ", ".join(n for _, n, _ in devices) or "none"
    raise LookupError(f"no input device matching {wanted!r}; inputs: {names}")
