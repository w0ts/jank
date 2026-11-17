"""The user's tracks with Traktor's beat grids as the reference (auto-detected, constant tempo).

    parse: collection.nml -> one record per track (grid marker, BPM, duration, break/drop cues)
    prep:  decode each track to mono 44.1 kHz WAV and write index.json with its reference beats

    python -m eval.traktor prep --nml collection.nml --tracks <dir with the mp3s> --out data/traktor --jobs N
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import xml.etree.ElementTree as ET
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

SR = 44100
MAX_S = 12 * 60          # longer files are mixes or albums: the grid has one tempo, they don't
BEATS_PER_BAR = 4        # Traktor's grid marker sits on a downbeat


def _cue_starts(entry, name: str) -> list[float]:
    return sorted(float(q.get("START")) / 1000.0 for q in entry.findall("CUE_V2") if q.get("NAME") == name)


def _relative_path(loc, tracks_dir_name: str = "Tracks") -> str | None:
    """Traktor's LOCATION DIR '/:Users/:mots/:Music/:Tracks/:Sub/:' + FILE -> 'Sub/<file>' under the tracks folder."""
    parts = [p for p in (loc.get("DIR") or "").split("/:") if p]
    lower = [p.lower() for p in parts]
    if tracks_dir_name.lower() not in lower:
        return None
    sub = parts[lower.index(tracks_dir_name.lower()) + 1:]
    return str(Path(*sub, loc.get("FILE"))) if sub else loc.get("FILE")


def parse(nml: Path) -> list[dict]:
    root = ET.parse(nml).getroot()
    records = []
    for e in root.find("COLLECTION").findall("ENTRY"):
        loc, tempo, info = e.find("LOCATION"), e.find("TEMPO"), e.find("INFO")
        grid = [q for q in e.findall("CUE_V2") if q.get("TYPE") == "4"]
        if loc is None or tempo is None or info is None or not grid:
            continue
        dur = float(info.get("PLAYTIME_FLOAT") or 0)
        bpm = float(tempo.get("BPM"))
        if not (60 <= dur <= MAX_S and 50 <= bpm <= 250):
            continue
        file = _relative_path(loc)
        if file is None:          # not in the tracks folder
            continue
        tag = hashlib.sha1(file.encode()).hexdigest()[:8]
        records.append({
            "name": f"tk_{tag}", "file": file, "bpm": bpm, "grid_s": float(grid[0].get("START")) / 1000.0,
            "dur": dur, "breaks": _cue_starts(e, "tk: Break"), "drops": _cue_starts(e, "tk: Drop"),
        })
    return records


def grid_beats(grid_s: float, bpm: float, dur: float) -> tuple[list[float], list[float]]:
    """Every beat of the constant grid inside [0, dur), and the downbeats (every 4th from the marker)."""
    period = 60.0 / bpm
    k0 = math.ceil(-grid_s / period)
    ks = np.arange(k0, int((dur - grid_s) / period) + 1)
    times = grid_s + ks * period
    keep = (times >= 0) & (times < dur)
    beats = np.round(times[keep], 4)
    down = beats[(ks[keep] % BEATS_PER_BAR) == 0]
    return beats.tolist(), down.tolist()


def _decode(args: tuple[dict, str, str]) -> dict:
    rec, tracks, out = args
    import soundfile as sf
    import soxr
    dst = Path(out) / f"{rec['name']}.wav"
    try:
        if not dst.exists():
            y, sr = sf.read(str(Path(tracks) / rec["file"]), dtype="float32", always_2d=True)
            y = y.mean(axis=1)
            if sr != SR:
                y = soxr.resample(y, sr, SR)
            sf.write(str(dst), y, SR, subtype="PCM_16")
        dur = sf.info(str(dst)).duration
    except Exception as exc:   # one bad file must not stop the other 850: record it and go on
        return {**rec, "error": f"{type(exc).__name__}: {exc}"}
    beats, down = grid_beats(rec["grid_s"], rec["bpm"], dur)
    return {**rec, "audio": dst.name, "dur": dur, "beats": beats, "downbeats": down}


def prep(nml: Path, tracks: Path, out: Path, jobs: int) -> None:
    out.mkdir(parents=True, exist_ok=True)
    records = parse(nml)
    with ProcessPoolExecutor(jobs) as pool:
        done = list(pool.map(_decode, [(r, str(tracks), str(out)) for r in records], chunksize=4))
    ok = [d for d in done if "error" not in d]
    bad = [d for d in done if "error" in d]
    (out / "index.json").write_text(json.dumps(ok))
    (out / "errors.json").write_text(json.dumps(bad, indent=1))
    print(f"{len(records)} tracks with a grid, {len(ok)} decoded, {len(bad)} failed (errors.json)")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["prep"])
    ap.add_argument("--nml", type=Path, required=True)
    ap.add_argument("--tracks", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=int(os.environ.get("JOBS", "2")))
    args = ap.parse_args()
    prep(args.nml, args.tracks, args.out, args.jobs)


if __name__ == "__main__":
    main()
