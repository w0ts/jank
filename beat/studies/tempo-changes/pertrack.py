"""Per track: how well does a chain hold the BPM, after its first lock (the question: for how many tracks does the follower
NOT hold the tempo the whole way?). Replays every cached track (extract_live.py) for the chains given and writes one JSON
line per track and chain.

    python studies/tempo-changes/pertrack.py --cache CACHE --ref REF.json --out ROWS.jsonl \
        --configs today tuned-l16-causal --sweep studies/tempo-changes/final.json --jobs N

Definitions (score.py's: the folded output against the audio reference folded into the output's octave):
  music         first reference beat + WARM_S .. last, reference known, input not silent
  first lock    the first tick in the music with the outputs running
  window        the music from the first lock on; every share below is of the window
  within 1 %    running, folded tempo within RIGHT_REL of the reference
  wrong         running and more than WRONG_REL (2 %) off once folded
  stop          not running (the gate shut)
  episodes      runs of a stop / of a wrong tempo of EPISODE_S (2 s) or more
ROWS.jsonl derives from madmom (the reference): git-ignored, never pushed.
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import json
import math
import sys
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BEAT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(BEAT))

import chain as C
import configs as CF
import reftempo as RT
import score as S
from study import ref_of

EPISODE_S = 2.0
EDGE_S = 40.0           # the first / last seconds of the window: "intro / outro"
SWING_BINS = 24


def episodes(mask: np.ndarray, dt: float) -> list[tuple[float, float]]:
    """(start index, seconds) of the runs of mask lasting EPISODE_S or more."""
    return [(int(a), (b - a) * dt) for a, b in S.runs_of(mask) if (b - a) * dt >= EPISODE_S]


def ratio_text(run, ref: np.ndarray, a: int, n: int) -> float:
    """The median output / reference ratio over a wrong episode, reduced to within an octave."""
    sel = slice(a, a + n)
    r = float(np.nanmedian(run.out_bpm[sel] / ref[sel]))
    return r / 2.0 ** round(math.log2(r))


def swing(flux: np.ndarray, beats: np.ndarray) -> tuple[float, float]:
    """The reference beats' mean onset profile over one period: (phase of the strongest off-beat onset in 0.3-0.85, its
    strength against the on-beat's). A phase near 0.5 is straight eighths, near 0.67 a swung / triplet feel."""
    if beats.size < 32 or flux.size == 0:
        return float("nan"), float("nan")
    f = flux[:, 0].astype(float)
    prof = np.zeros(SWING_BINS)
    cnt = np.zeros(SWING_BINS)
    for i in range(8, beats.size - 8):
        t0, t1 = beats[i], beats[i + 1]
        a, b = int(t0 * 100), int(t1 * 100)
        if b >= f.size or b - a < 8:
            continue
        ph = ((np.arange(a, b) - t0 * 100) / ((t1 - t0) * 100) * SWING_BINS).astype(int).clip(0, SWING_BINS - 1)
        np.add.at(prof, ph, f[a:b])
        np.add.at(cnt, ph, 1)
    if cnt.min() == 0:
        return float("nan"), float("nan")
    prof = prof / cnt
    on = max(prof[:2].max(), prof[-2:].max(), 1e-9)
    lo, hi = int(0.3 * SWING_BINS), int(0.85 * SWING_BINS)
    k = lo + int(np.argmax(prof[lo:hi + 1]))
    return (k + 0.5) / SWING_BINS, float(prof[k] / on)


def act_level(act: np.ndarray, beats: np.ndarray) -> float:
    """The live activation's median peak (+-3 frames) at the reference beats."""
    if act.size == 0 or beats.size == 0:
        return float("nan")
    f = np.clip(np.round(beats * 100).astype(int), 0, act.size - 1)
    return float(np.median([act[max(i - 3, 0):i + 4].max() for i in f]))


def analyse(track: C.Track, run: C.Run, ref_row: dict, beats: np.ndarray) -> dict:
    segs, _ = ref_of(ref_row)
    t = run.t
    dt = float(t[1] - t[0])
    ref = RT.ref_bpm(RT.sections(segs) or segs, t)      # NaN in the transitions between sections: no reference there
    lo = float(beats[0]) + S.WARM_S
    hi = float(beats[-1])
    blk = np.minimum((t / 0.1).astype(int), max(track.rms.size - 1, 0))
    playing = (t >= lo) & (t <= hi) & (track.rms[blk] >= 1e-3)
    known = ~np.isnan(ref)
    music = playing & known                             # where the output can be judged
    dev = S.folded_dev(run.out_bpm, ref)
    running = run.running & playing
    row = {"music_s": float(playing.sum() * dt)}
    if not running.any():
        ph, strength = swing(track.flux, beats)
        return {**row, "locked": False, "first_lock_s": None, "act_med": act_level(track.act, beats),
                "swing_phase": ph, "swing_strength": strength}
    first = int(np.argmax(running))
    after = playing & (np.arange(t.size) >= first)
    win = after & (known | ~run.running)                # running in a transition: neither right nor wrong
    n = int(win.sum())
    right = running & win & (dev <= math.log(1 + S.RIGHT_REL))
    wrong = running & win & (dev > math.log(1 + S.WRONG_REL))
    stopped = after & ~run.running
    stops, wrongs = episodes(stopped, dt), episodes(wrong, dt)
    bad = win & ~right
    idx = np.where(win)[0]
    edge = (t[idx] < t[first] + EDGE_S) | (t[idx] > t[idx[-1]] - EDGE_S)
    after_stop = [a > 0 and not run.running[a - 1] for a, _ in wrongs]
    eps = [("stop", a, d, None, False) for a, d in stops] + \
          [("wrong", a, d, ratio_text(run, ref, a, int(round(d / dt))), s) for (a, d), s in zip(wrongs, after_stop)]
    eps.sort(key=lambda e: -e[2])
    cls = S.phase(run, segs, beats, right)
    on_beat = np.asarray(cls["_cls"])
    live = on_beat != "stopped"
    ph, strength = swing(track.flux, beats)
    return {**row, "locked": True, "first_lock_s": float(t[first]), "window_s": float(n * dt),
            "within1": float(right.sum() / n), "stopped": float(stopped.sum() / n), "wrong": float(wrong.sum() / n),
            "near": float((running & win & ~right & ~wrong).sum() / n),
            "stops_2s": len(stops), "wrongs_2s": len(wrongs), "wrongs_after_stop": int(sum(after_stop)),
            "stops_5s": sum(1 for _, d in stops if d >= 5), "wrongs_5s": sum(1 for _, d in wrongs if d >= 5),
            "stops_10s": sum(1 for _, d in stops if d >= 10), "wrongs_10s": sum(1 for _, d in wrongs if d >= 10),
            "longest_stop_s": max([d for _, d in stops], default=0.0),
            "longest_wrong_s": max([d for _, d in wrongs], default=0.0),
            "stop_s_in_eps": float(sum(d for _, d in stops)), "wrong_s_in_eps": float(sum(d for _, d in wrongs)),
            "bad_in_edges": float(bad[idx][edge].sum() / max(bad.sum(), 1)),
            "episodes": [[k, float(t[a]), float(d), r, s] for k, a, d, r, s in eps[:6]],
            "offbeat": float(np.mean(on_beat[live] == "offbeat")) if live.any() else None,
            "act_med": act_level(track.act, beats), "swing_phase": ph, "swing_strength": strength}


def one(job: tuple) -> list[str]:
    path, ref_row, mmdb, cfgs, meta = job
    try:
        track = C.Track.load(path)
        beats = np.load(Path(mmdb) / f"{ref_row['name']}.npz")["beats"]
        out = []
        for cfg in cfgs:
            run = C.replay(track, cfg)
            row = analyse(track, run, ref_row, beats)
            out.append(json.dumps({"name": ref_row["name"], "config": cfg.name, "class": ref_row["class"],
                                   "octaves": ref_row["octaves"], "dur": ref_row["dur"], **meta, **row},
                                  default=float))
        return out
    except Exception:
        return [json.dumps({"name": ref_row["name"], "error": traceback.format_exc(limit=5)[-1000:]})]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--ref", type=Path, required=True)
    ap.add_argument("--mmdb", type=Path, default=BEAT / "data" / "mmdb_traktor")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--configs", nargs="+", required=True)
    ap.add_argument("--sweep", type=Path)
    ap.add_argument("--names", nargs="+")
    ap.add_argument("--jobs", type=int, default=1)
    a = ap.parse_args()
    cfgs = CF.named(a.configs, json.loads(a.sweep.read_text()) if a.sweep else {})
    index = json.loads((BEAT / "data" / "traktor" / "index.json").read_text())
    pos = {r["name"]: i for i, r in enumerate(index)}
    recs = {r["name"]: r for r in index}
    jobs = []
    for r in json.loads(a.ref.read_text()):
        if (a.names and r["name"] not in a.names) or not (a.cache / f"{r['name']}.npz").exists():
            continue
        meta = {"split": "dev" if pos[r["name"]] % 2 == 0 else "test", "file": recs[r["name"]]["file"],
                "traktor_bpm": recs[r["name"]]["bpm"]}
        jobs.append((str(a.cache / f"{r['name']}.npz"), r, str(a.mmdb), cfgs, meta))
    print(f"{len(jobs)} tracks x {len(cfgs)} configs", flush=True)
    with ProcessPoolExecutor(a.jobs) as pool, a.out.open("w") as f:
        for k, lines in enumerate(pool.map(one, jobs, chunksize=1), 1):
            f.write("".join(x + "\n" for x in lines))
            f.flush()
            if k % 100 == 0:
                print(f"{k}/{len(jobs)}", flush=True)
    print("PERTRACK DONE", flush=True)


if __name__ == "__main__":
    main()
