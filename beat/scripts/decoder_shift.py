"""Does the MP3 decoder shift the audio? Compare soundfile (libsndfile/mpg123) with ffmpeg on one file.

    .venv/bin/python scripts/decoder_shift.py <file.mp3> [...]
Prints the shift of soundfile's decode against ffmpeg's in samples and ms (positive: soundfile is later),
and the encoder tag, which tells how the file was made.
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 44100
SECONDS = 20
MAX_LAG = 8820   # +-200 ms


def ffmpeg_decode(path: str) -> np.ndarray:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "x.wav"
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-t", str(SECONDS), "-ac", "1", "-ar", str(SR), str(out)],
                       check=True)
        y, _ = sf.read(str(out), dtype="float32")
    return y


def shift_samples(a: np.ndarray, b: np.ndarray) -> int:
    """Lag of a against b by cross-correlation over seconds 1-6."""
    seg_a = a[SR: SR * 6]
    seg_b = b[SR - MAX_LAG: SR * 6 + MAX_LAG]
    c = np.correlate(seg_b, seg_a, "valid")
    return int(MAX_LAG - np.argmax(c))


def encoder_tag(path: str) -> str:
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format_tags=encoder:stream=sample_rate",
                          "-of", "compact=p=0:nk=1", path], capture_output=True, text=True).stdout
    return " ".join(out.split())


def main() -> None:
    for path in sys.argv[1:]:
        a, sr = sf.read(path, dtype="float32", always_2d=True, frames=SR * SECONDS)
        lag = shift_samples(a.mean(axis=1), ffmpeg_decode(path))
        print(f"{lag:+6d} samples = {lag / SR * 1000:+6.1f} ms  sr={sr}  [{encoder_tag(path)}]  {Path(path).parent.name}/{Path(path).name[:40]}")


if __name__ == "__main__":
    main()
