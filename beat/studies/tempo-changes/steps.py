"""Synthetic tempo steps and ramps on steady tracks, made from the cached features (no audio render): a pitch-fader
move by `ratio` from t0 (over ramp_s seconds) is a time warp of everything after t0, so the live tracker's beats, its
activation, the spectral flux and the RMS are warped the same way, and the rate probe's estimates whose 4.5 s window
straddles t0 read the ratio. The tracker's own reaction to the pitched audio is not modelled (an estimate: its beats
follow a varispeed within a beat or two, studies/advanced-follow); the decision layer is what is compared here.

    python studies/tempo-changes/steps.py --cache CACHE --ref REF.json --out ROWS.jsonl --configs a b [--sweep S.json]
        [--split test] [--per-track 1] [--jobs N]
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

CHANGES = (0.01, 0.02, 0.03, 0.05, 0.08, -0.03)
RAMPS = ((0.05, 8.0), (0.08, 30.0))
T0 = 70.0
JUDGE_S = 30.0
STAY_S = 4.0
RIGHT_REL = 0.01
FPS = 100


def warp_fn(t0: float, ratio: float, ramp_s: float = 0.0):
    """Output time -> source time for a rate going from 1 to `ratio` at t0 (linearly over ramp_s)."""
    def src(t: np.ndarray) -> np.ndarray:
        t = np.asarray(t, dtype=float)
        d = np.maximum(t - t0, 0.0)
        if ramp_s <= 0:
            return np.where(t < t0, t, t0 + d * ratio)
        k = (ratio - 1.0) / ramp_s
        inramp = t0 + d + 0.5 * k * np.minimum(d, ramp_s) ** 2
        after = t0 + ramp_s * (1 + ratio) / 2 + np.maximum(d - ramp_s, 0) * ratio
        return np.where(t < t0, t, np.where(d <= ramp_s, inramp, after))
    return src


def invert(src, x: np.ndarray, t_max: float) -> np.ndarray:
    grid = np.arange(0.0, t_max, 0.001)
    return np.interp(x, src(grid), grid)


def warped(track: C.Track, ratio: float, ramp_s: float = 0.0, t0: float = T0) -> tuple[C.Track, callable]:
    src = warp_fn(t0, ratio, ramp_s)
    dur = float(invert(src, np.array([track.dur]), track.dur * 1.2)[0])
    beats = invert(src, track.beats, track.dur * 1.2)
    tf = np.arange(int(dur * FPS)) / FPS
    sf = src(tf) * FPS
    act = np.interp(sf, np.arange(track.act.size), track.act).astype(np.float32)
    flux = np.stack([np.interp(sf, np.arange(track.flux.shape[0]), track.flux[:, j]) for j in range(2)], 1)
    tb = np.arange(int(dur * 10)) / 10 + 0.05
    rms = np.interp(src(tb) * 10 - 0.5, np.arange(track.rms.size), track.rms).astype(np.float32)
    rate = track.rate.copy()
    if rate.size:
        rate[:, 0] = invert(src, rate[:, 0], track.dur * 1.2)
        straddle = (rate[:, 0] > t0 + 0.5) & (rate[:, 0] < t0 + 4.5 + ramp_s)
        rate[straddle, 1] = rate[straddle, 1] * ratio
    return C.Track(beats, rms, rate, act, flux.astype(np.float32), dur), src


def judge(run: C.Run, bpm0: float, ratio: float, ramp_s: float, t0: float = T0) -> dict:
    """Delay to the new tempo (right within RIGHT_REL, folded, and staying STAY_S) from the move's end (None: not within
    JUDGE_S of it); stopped and wrong shares from t0 over JUDGE_S (a ramp: until 10 s after its end); whether it was
    running right before."""
    target = bpm0 * ratio
    end = t0 + ramp_s
    stop_judging = max(t0 + JUDGE_S, end + 10.0)
    sel = (run.t >= t0) & (run.t < stop_judging)
    with np.errstate(invalid="ignore", divide="ignore"):
        lr = np.log2(run.out_bpm / target)
        dev = np.abs(lr - np.round(lr))
    right = run.running & (dev <= math.log2(1 + RIGHT_REL))
    before = (run.t >= t0 - 5) & (run.t < t0)
    tick = float(run.t[1] - run.t[0])
    n_stay = int(round(STAY_S / tick))
    start = int(np.searchsorted(run.t, end))
    delay = None
    streak = 0
    for i in range(start, int(np.searchsorted(run.t, end + JUDGE_S + STAY_S))):
        streak = streak + 1 if i < right.size and right[i] else 0
        if streak >= n_stay:
            delay = float(run.t[i - n_stay + 1] - end)
            break
    wrong = run.running & (dev > math.log2(1.02)) & ~np.isnan(dev)
    with np.errstate(invalid="ignore", divide="ignore"):
        lb = np.log2(run.out_bpm[before] / bpm0)
    ok_before = run.running[before] & (np.abs(lb - np.round(lb)) <= math.log2(1 + RIGHT_REL))
    return {"right_before": float(ok_before.mean()) if before.any() else 0.0,
            "delay_s": delay, "delay_beats": None if delay is None else delay * target / 60.0,
            "stopped": float((~run.running[sel]).mean()), "wrong": float(wrong[sel].mean())}


def one(job: tuple) -> list[str]:
    path, name, split, bpm0, cfgs = job
    out = []
    try:
        track = C.Track.load(path)
        if track.dur < T0 + JUDGE_S + 10:
            return []
        cases = [(1 + c, 0.0) for c in CHANGES] + [(1 + c, r) for c, r in RAMPS]
        for ratio, ramp_s in cases:
            tr, _ = warped(track, ratio, ramp_s)
            for cfg in cfgs:
                row = judge(C.replay(tr, cfg), bpm0, ratio, ramp_s)
                out.append(json.dumps({"name": name, "split": split, "config": cfg.name, "ratio": ratio,
                                       "ramp_s": ramp_s, **row}))
        return out
    except Exception:
        return [json.dumps({"name": name, "error": traceback.format_exc(limit=5)[-1000:]})]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--ref", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--configs", nargs="+", default=["today", "m2i", "m2i+rate"])
    ap.add_argument("--sweep", type=Path)
    ap.add_argument("--split", choices=["dev", "test"])
    ap.add_argument("--limit", type=int)
    ap.add_argument("--jobs", type=int, default=1)
    a = ap.parse_args()
    extra = json.loads(a.sweep.read_text()) if a.sweep else {}
    cfgs = CF.named(a.configs, extra)
    order = {r["name"]: i for i, r in enumerate(json.loads((BEAT / "data" / "traktor" / "index.json").read_text()))}
    jobs = []
    for r in json.loads(a.ref.read_text()):
        split = "dev" if order[r["name"]] % 2 == 0 else "test"
        secs = [s for s in r["segments"] if s["t0"] <= T0 - 10 and s["t1"] >= T0 + JUDGE_S + 10 and not s["ramp"]]
        if r["class"] != "steady" or (a.split and split != a.split) or not secs:
            continue
        jobs.append((str(a.cache / f"{r['name']}.npz"), r["name"], split, secs[0]["bpm0"], cfgs))
    if a.limit:
        jobs = jobs[:a.limit]
    print(f"{len(jobs)} tracks x {len(CHANGES) + len(RAMPS)} moves x {len(cfgs)} configs", flush=True)
    with ProcessPoolExecutor(a.jobs) as pool, a.out.open("w") as f:
        for k, lines in enumerate(pool.map(one, jobs, chunksize=1), 1):
            f.write("".join(x + "\n" for x in lines))
            f.flush()
            if k % 50 == 0:
                print(f"{k}/{len(jobs)}", flush=True)
    print("STEPS DONE", flush=True)


def table(rows: list[dict], configs: list[str] | None = None) -> str:
    """Per move: stopped share over 30 s, median delay in beats (cases right before the move), share never right."""
    from collections import defaultdict
    by = defaultdict(list)
    for r in rows:
        if "error" not in r and r["right_before"] >= 0.5:
            by[(r["ratio"], r["ramp_s"], r["config"])].append(r)
    names = configs or sorted({r["config"] for r in rows if "config" in r})
    moves = sorted({(k[0], k[1]) for k in by})
    lines = ["| move | " + " | ".join(names) + " |", "|---|" + "---|" * len(names)]
    for ratio, ramp in moves:
        cells = []
        for n in names:
            rs = by.get((ratio, ramp, n), [])
            if not rs:
                cells.append("-")
                continue
            got = [r["delay_beats"] for r in rs if r["delay_beats"] is not None]
            never = 1 - len(got) / len(rs)
            cells.append(f"{np.mean([r['stopped'] for r in rs]):.2f} / {np.median(got) if got else float('nan'):.1f} / "
                         f"{never:.2f} ({len(rs)})")
        label = f"{(ratio - 1) * 100:+.0f} %" + (f" over {ramp:.0f} s" if ramp else " step")
        lines.append(f"| {label} | " + " | ".join(cells) + " |")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    main()
