"""Corrected beat labels for the user's tracks, and the train / validation / test split.

Traktor's grids are constant-tempo and sit at a constant per-track offset to the audio. Per track, the offset is
measured against madmom's OFFLINE beats (RNNBeatProcessor + DBN, see prep_features.py), whose own bias is calibrated
on GTZAN's hand annotations (calibrate_gtzan.py: est - ref = BIAS_S). With P the beat period (Traktor's BPM folded
into 90..180 by doubling or halving) and g the grid's first beat:

    phase of madmom beat t = (t - g) mod P           (circular mean over the beats -> d, resultant length R)
    corrected beat k       = g + d - BIAS_S + k P    (d - BIAS_S: the audio's true beat position)

Tracks are dropped when madmom's beats do not agree with one constant offset (R < MIN_R: no madmom lock, or the
grid is not constant) or the offset differs between the first and the second half of the track (> MAX_DRIFT_S).

    .venv/bin/python -I train/make_labels.py FEAT_DIR OUT.json
"""
from __future__ import annotations

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
MIN_R = 0.6
MAX_DRIFT_S = 0.025
SPLIT_FRACTIONS = {"test": 0.15, "val": 0.10}


def fold_period(bpm: float) -> float:
    while bpm > 180:
        bpm /= 2
    while bpm < 90:
        bpm *= 2
    return 60.0 / bpm


def circ_offset(beats: np.ndarray, g: float, period: float) -> tuple[float, float]:
    """(offset d in (-P/2, P/2], resultant length R) of beats' phase against the grid."""
    z = np.exp(2j * np.pi * ((beats - g) % period) / period)
    m = z.mean()
    d = np.angle(m) / (2 * np.pi) * period
    return float(d), float(abs(m))


def norm_title(file: str) -> str:
    stem = Path(file).stem.lower()
    return re.sub(r"[^a-z0-9]+", "", stem)


def split_groups(records: list[dict], seed: int = 7) -> dict[str, str]:
    """Group = folder, merged with every folder that holds the same title (near-duplicates stay together).
    Whole groups go to test / val / train."""
    parent: dict[str, str] = {}

    def find(x: str) -> str:
        while parent.setdefault(x, x) != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    seen: dict[str, str] = {}
    for r in records:
        folder = r["folder"]
        find(folder)
        t = norm_title(r["file"])
        if t in seen:
            parent[find(folder)] = find(seen[t])
        else:
            seen[t] = folder
    groups: dict[str, list[dict]] = defaultdict(list)
    for r in records:
        groups[find(r["folder"])].append(r)
    order = sorted(groups, key=lambda k: hash_key(k, seed))
    total = len(records)
    want = {s: f * total for s, f in SPLIT_FRACTIONS.items()}
    got = {"test": 0, "val": 0}
    out = {}
    for g in order:
        n = len(groups[g])
        split = "train"
        for s in ("test", "val"):
            if got[s] < want[s] and got[s] + n <= want[s] * 1.4:
                split = s
                got[s] += n
                break
        for r in groups[g]:
            out[r["name"]] = split
    return out


def hash_key(s: str, seed: int) -> int:
    import hashlib
    return int(hashlib.sha1(f"{seed}:{s}".encode()).hexdigest()[:8], 16)


def main() -> None:
    feat_dir, out = Path(sys.argv[1]), Path(sys.argv[2])
    bias = json.loads((ROOT / "data" / "gtzan_madmom_bias.json").read_text())["median_ms"] / 1000.0
    index = json.loads((ROOT / "data" / "traktor" / "index.json").read_text())
    kept, dropped = [], defaultdict(int)
    for r in index:
        fj = feat_dir / f"{r['name']}.offline.json"
        if not (fj.exists() and (feat_dir / f"{r['name']}.npy").exists()):
            dropped["missing"] += 1
            continue
        mb = np.array(json.loads(fj.read_text())["beats"])
        period = fold_period(r["bpm"])
        g = r["grid_s"]
        if len(mb) < 30:
            dropped["no madmom beats"] += 1
            continue
        d, R = circ_offset(mb, g, period)
        half = r["dur"] / 2
        d1, _ = circ_offset(mb[mb < half], g, period) if (mb < half).sum() > 10 else (d, 0)
        d2, _ = circ_offset(mb[mb >= half], g, period) if (mb >= half).sum() > 10 else (d, 0)
        drift = abs((d1 - d2 + period / 2) % period - period / 2)
        if R < MIN_R:
            dropped["low R"] += 1
            continue
        if drift > MAX_DRIFT_S:
            dropped["drift"] += 1
            continue
        start = g + d - bias
        n0 = int(np.ceil(-start / period))
        beats = start + period * np.arange(n0, int(np.floor((r["dur"] - start) / period)) + 1)
        beats = beats[(beats >= 0) & (beats < r["dur"])]
        kept.append({"name": r["name"], "file": r["file"], "folder": r["file"].split("/")[0], "bpm": r["bpm"],
                     "period": period, "offset_ms": round(d * 1000, 1), "R": round(R, 3),
                     "drift_ms": round(drift * 1000, 1), "dur": r["dur"], "beats": [round(float(b), 4) for b in beats]})
    split = split_groups(kept)
    for r in kept:
        r["split"] = split[r["name"]]
    out.write_text(json.dumps(kept))
    print("kept", len(kept), "dropped", dict(dropped))
    for s in ("train", "val", "test"):
        print(s, sum(r["split"] == s for r in kept), "tracks", round(sum(r["dur"] for r in kept if r["split"] == s) / 3600, 1), "h")
    by = defaultdict(list)
    for r in kept:
        by[r["folder"]].append(r["offset_ms"])
    for f, v in sorted(by.items()):
        print(f"{f:50} n={len(v):3} offset median {np.median(v):7.1f} ms  p10-p90 {np.percentile(v, 10):6.1f}..{np.percentile(v, 90):6.1f}  {split[[r['name'] for r in kept if r['folder'] == f][0]]}")


if __name__ == "__main__":
    main()
