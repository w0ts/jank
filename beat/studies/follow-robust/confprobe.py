"""How graded is the confidence, and what predicts "the output is right"? (host only)

Replays configs over the cached tracks and writes, per track and config, the per-tick candidates and labels to
OUT/<name>.<config>.npz: t, running, right, wrong (known reference, playing), conf (the gate's), strict, loose,
the agreement parts (tempo, phase, dbn; zeros without `agree`) and new graded candidates:
  soft   F of tracker beats against the lines with a triangular kernel (1 at 0, 0 at half a period) instead of +-0.1 period
  jit    1 - clip(cv of the last 16 beat intervals / 0.15)
  pk     mean activation peak (+-2 frames) at the last 16 beats, over the track's running 90th percentile of the activation

    python studies/follow-robust/confprobe.py --cache C --ref ref.json --out DIR --configs fw all-d-f60 \
        --sweep studies/follow-robust/final.json [--jobs N] [--limit N]
"""
from __future__ import annotations

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
for p in (HERE, HERE.parent / "tempo-changes", HERE.parent, BEAT):
    sys.path.insert(0, str(p))

import chain as C
import graded as GR
import rchain as R
import rconfigs as RC
import reftempo as RT
import score as S
from study import ref_of

FPS = 100
WINDOW_S, DELAY_S = 8.0, 0.1
NAMES = ("conf", "strict", "loose", "tempo", "phase", "dbn", "soft", "jit", "pk")


def peak_at(beats: list[float], act: np.ndarray, ref: float) -> float:
    fr = np.round(np.asarray(beats[-16:]) * FPS).astype(int)
    fr = fr[(fr >= 2) & (fr + 2 < act.size)]
    if fr.size < 4 or ref <= 0:
        return 0.0
    return float(min(1.0, np.mean([act[f - 2:f + 3].max() for f in fr]) / ref))


def replay_probe(track: C.Track, cfg) -> tuple[R.RRun, dict]:
    n = np.arange(C.TICK_S, track.dur, C.TICK_S).size
    cand = {k: np.zeros(n, np.float32) for k in NAMES}
    act_ref = float(np.percentile(track.act[track.act > 0], 90)) if (track.act > 0).any() else 1.0

    def probe(k: int, now: float, d: dict) -> None:
        cand["conf"][k], cand["strict"][k], cand["loose"][k] = d["conf"], d["strict"], d["loose"]
        ag = d["agree"]
        if ag is not None:
            for key in ("tempo", "phase", "dbn"):
                cand[key][k] = ag.vals[key]
        per = d["period"]
        if per:
            lines = np.asarray(d["lines"].times())
            cand["soft"][k] = GR.soft_f(d["fed"], lines, per, now)
            cand["jit"][k] = GR.jitter(d["fed"])
            cand["pk"][k] = peak_at(d["fed"], d["act"], act_ref)

    R.PROBE = probe
    try:
        run = R.replay(track, cfg)
    finally:
        R.PROBE = None
    return run, cand


def labels(track: C.Track, run: R.RRun, ref_row: dict, beats: np.ndarray) -> dict:
    segs, _ = ref_of(ref_row)
    t = run.t
    ref = RT.ref_bpm(RT.sections(segs) or segs, t)
    lo, hi = float(beats[0]) + S.WARM_S, float(beats[-1])
    blk = np.minimum((t / 0.1).astype(int), max(track.rms.size - 1, 0))
    playing = (t >= lo) & (t <= hi) & (track.rms[blk] >= 1e-3)
    known = ~np.isnan(ref)
    dev = S.folded_dev(run.out_bpm, ref)
    right = run.running & playing & known & (dev <= math.log(1 + S.RIGHT_REL))
    wrong = run.running & playing & known & (dev > math.log(1 + S.WRONG_REL))
    return {"t": t.astype(np.float32), "running": run.running, "right": right, "wrong": wrong, "playing": playing}


def one(job: tuple) -> str:
    path, ref_row, mmdb, cfgs, out = job
    try:
        track = C.Track.load(path)
        beats = np.load(Path(mmdb) / f"{ref_row['name']}.npz")["beats"]
        for cfg in cfgs:
            run, cand = replay_probe(track, cfg)
            lab = labels(track, run, ref_row, beats)
            np.savez_compressed(Path(out) / f"{ref_row['name']}.{cfg.name}.npz", **lab, **cand)
        return ""
    except Exception:
        return ref_row["name"] + "\n" + traceback.format_exc(limit=6)[-1200:]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--ref", type=Path, required=True)
    ap.add_argument("--mmdb", type=Path, default=BEAT / "data" / "mmdb_traktor")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--configs", nargs="+", required=True)
    ap.add_argument("--sweep", type=Path)
    ap.add_argument("--limit", type=int)
    ap.add_argument("--jobs", type=int, default=1)
    a = ap.parse_args()
    extra = json.loads(a.sweep.read_text()) if a.sweep else {}
    cfgs = RC.named(a.configs, extra)
    a.out.mkdir(parents=True, exist_ok=True)
    jobs = [(str(a.cache / f"{r['name']}.npz"), r, str(a.mmdb), cfgs, str(a.out))
            for r in json.loads(a.ref.read_text()) if (a.cache / f"{r['name']}.npz").exists()]
    jobs.sort(key=lambda j: -j[1]["dur"])
    if a.limit:
        jobs = jobs[:a.limit]
    print(f"{len(jobs)} tracks x {len(cfgs)} configs", flush=True)
    with ProcessPoolExecutor(a.jobs) as pool:
        for k, err in enumerate(pool.map(one, jobs, chunksize=1), 1):
            if err:
                print("ERROR", err, flush=True)
            if k % 50 == 0:
                print(f"{k}/{len(jobs)}", flush=True)
    print("CONFPROBE DONE", flush=True)


if __name__ == "__main__":
    main()
