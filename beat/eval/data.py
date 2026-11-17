"""Datasets: audio paths paired with beat and downbeat annotations (Beat This! annotation set)."""
from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
ANN = DATA / "annotations"


@dataclass(frozen=True)
class Piece:
    name: str
    audio: Path
    beats: np.ndarray       # seconds
    downbeats: np.ndarray   # seconds


def read_beats(path: Path) -> tuple[np.ndarray, np.ndarray]:
    """A .beats file: one 'time position' line per beat, position 1 = downbeat."""
    rows = [line.split() for line in path.read_text().splitlines() if line.strip()]
    times = np.array([float(r[0]) for r in rows])
    pos = np.array([int(float(r[1])) if len(r) > 1 else 0 for r in rows])
    return times, times[pos == 1]


def traktor(limit: int | None = None) -> list[Piece]:
    """The user's tracks, prepared by eval.traktor prep: Traktor's auto grids as the reference."""
    root = DATA / "traktor"
    index = json.loads((root / "index.json").read_text())
    pieces = [Piece(r["name"], root / r["audio"], np.array(r["beats"]), np.array(r["downbeats"])) for r in index]
    return pieces[:limit] if limit else pieces


def mixes(limit: int | None = None) -> list[Piece]:
    """DJ sets and live recordings, prepared by eval.mixes prep: no annotations (references come from the offline
    trackers, see eval.mix_eval)."""
    root = DATA / "mixes"
    index = json.loads((root / "index.json").read_text())
    pieces = [Piece(r["name"], root / r["audio"], np.array([]), np.array([])) for r in index]
    return pieces[:limit] if limit else pieces


DATASETS = {"gtzan": lambda limit=None: gtzan(limit), "traktor": lambda limit=None: traktor(limit),
            "mixes": lambda limit=None: mixes(limit)}


def gtzan(limit: int | None = None) -> list[Piece]:
    """GTZAN: held out from the training of Beat This! final*/small* and BeatNet model 1."""
    beats_dir = ANN / "gtzan" / "annotations" / "beats"
    audio_root = DATA / "gtzan"
    audio_by_stem = {p.stem: p for p in audio_root.rglob("*") if p.suffix in (".wav", ".au")}
    pieces = []
    for f in sorted(beats_dir.glob("*.beats")):
        # gtzan_blues_00000 -> blues.00000
        genre, num = f.stem.split("_")[1:3]
        audio = audio_by_stem.get(f"{genre}.{num}")
        if audio is None:
            continue
        beats, downbeats = read_beats(f)
        pieces.append(Piece(f.stem, audio, beats, downbeats))
    return pieces[:limit] if limit else pieces
