"""Confidence over time on a few Traktor tracks: per-second samples, and how it behaves in breaks.

    PYTHONPATH=. .venv/bin/python scripts/confidence_trace.py [N_TRACKS] [SECONDS]"""
import json
import sys
from pathlib import Path

import numpy as np

from live.audio import load_audio, to_mono
from live.engine import Engine
from live.tracker import HOP, SAMPLE_RATE

ROOT = Path(__file__).resolve().parent.parent


def trace(path: Path, seconds: float, bpm: float):
    e = Engine(90, 180)
    mono = to_mono(load_audio(path))[:int(seconds * SAMPLE_RATE)]
    rows = []
    for i in range(0, len(mono), HOP):
        e.feed(mono[i:i + HOP])
        if i % SAMPLE_RATE < HOP:
            s = e.status()
            rows.append((i / SAMPLE_RATE, s.mode, s.bpm, s.confidence))
    return rows


def main() -> None:
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 4
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 120.0
    index = json.loads((ROOT / "data" / "traktor" / "index.json").read_text())
    picks = [index[i] for i in np.linspace(0, len(index) - 1, n).astype(int)]
    for r in picks:
        rows = trace(Path.home() / "Music" / "Tracks" / r["file"], seconds, r["bpm"])
        conf = np.array([c for t, _, _, c in rows if t >= 15])
        breaks = [b for b in r["breaks"] if b < seconds]
        print(f"{r['file'].split('/')[-1][:60]}  Traktor {r['bpm']:.2f}  breaks {breaks}")
        print(f"   after 15 s: confidence min {conf.min():.2f}  p10 {np.percentile(conf, 10):.2f}  "
              f"median {np.median(conf):.2f}")
        print("   " + " ".join(f"{int(t)}:{c:.2f}/{b or 0:.1f}" for t, _, b, c in rows[::5]))


if __name__ == "__main__":
    main()
