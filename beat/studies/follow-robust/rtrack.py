"""Per track and chain: tempo-changes/pertrack.py's row (held whole, stops, wrong episodes, ...) plus what the four
changes need, one JSON line per track and chain.

    python studies/follow-robust/rtrack.py --cache CACHE --ref REF.json --out ROWS.jsonl --sweep SWEEP.json \
        --configs fw coast30 ... [--split dev] [--jobs N]

Added to pertrack.analyse's row:
  cmlc          the longest run of consecutive reference beats with the output right (within 1 %, folded) and in phase
                (within 17.5 % of a beat after the track's median offset, as the literature's CMLc), over the reference
                beats of the music window (first beat + 10 s .. last): a CMLc with octave folding (the output's octave is
                AUTO RANGE's)
  coast_s / coast_wrong_s / coast_right_s   seconds after the first lock run with the gate's confidence under the
                threshold (coasting), and of them wrong (> 2 % off) / right (within 1 %)
  coast_ends    for each coasting run of 2 s or more: how the output stood at its end: the class of the last reference
                beat in it ("in" phase, "offbeat", "off", "wrong" tempo) and what came next ("conf": the confidence came
                back, "stop": the outputs stopped)
  wrong_locks   score.locks: starts (stopped -> running, or a jump while running) at a wrong tempo for 2 s
  follow        score.follow on the reference's real changes (delay to right and staying right 4 s)
  refused       the lock gate's refusals (memory, ratio); prior_s: seconds with the DBN's tempo prior on
Rows derive from madmom (the reference): git-ignored, never pushed.
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
sys.path.insert(0, str(HERE.parent / "tempo-changes"))
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(BEAT))

import chain as C
import pertrack as PT
import reftempo as RT
import rchain as R
import rconfigs as RC
import score as S
from study import ref_of

CML_TOL = 0.175
COAST_MIN_S = 2.0


def cmlc(cls_right: np.ndarray, rel_ms: np.ndarray, rp: np.ndarray, sel: np.ndarray) -> float:
    ok = cls_right & (np.abs(rel_ms) <= CML_TOL * rp * 1000.0)
    ok = ok[sel]
    if ok.size == 0:
        return 0.0
    best = max((b - a for a, b in S.runs_of(ok)), default=0)
    return float(best / ok.size)


def coast_ends(run: R.RRun, after: np.ndarray, cls: np.ndarray, ref_idx: np.ndarray, beats: np.ndarray,
               dt: float) -> list[list]:
    out = []
    for a, b in S.runs_of(run.coasting & after):
        if (b - a) * dt < COAST_MIN_S:
            continue
        t_end = run.t[b - 1]
        inside = np.where((beats >= run.t[a]) & (beats <= t_end))[0]
        last = str(cls[inside[-1]]) if inside.size else "none"
        nxt = "stop" if b < run.t.size and not run.running[b] else "conf"
        out.append([float(run.t[a]), float((b - a) * dt), last, nxt])
    return out


def extras(track: C.Track, run: R.RRun, ref_row: dict, beats: np.ndarray) -> dict:
    segs, chs = ref_of(ref_row)
    t = run.t
    dt = float(t[1] - t[0])
    ref = RT.ref_bpm(RT.sections(segs) or segs, t)
    lo, hi = float(beats[0]) + S.WARM_S, float(beats[-1])
    blk = np.minimum((t / 0.1).astype(int), max(track.rms.size - 1, 0))
    playing = (t >= lo) & (t <= hi) & (track.rms[blk] >= 1e-3)
    known = ~np.isnan(ref)
    dev = S.folded_dev(run.out_bpm, ref)
    right = run.running & playing & known & (dev <= math.log(1 + S.RIGHT_REL))
    wrong = run.running & playing & known & (dev > math.log(1 + S.WRONG_REL))
    ph = S.phase(run, segs, beats, right)
    rp = S._ref_period(beats)
    ref_idx = np.clip(np.searchsorted(t, beats, side="right") - 1, 0, t.size - 1)
    sel = (beats >= lo) & (beats <= hi)
    tempo_ok = np.asarray([c in ("in", "offbeat", "off") for c in ph["_cls"]], dtype=bool)
    out = {"cmlc": cmlc(tempo_ok, ph["_rel"], rp, sel) if sel.any() else 0.0}
    running = run.running & playing
    out.update(S.locks(run, dev, playing & known))
    out["follow"] = S.follow(run, right, chs)
    out["refused"] = run.refused
    out["prior_s"] = float(run.prior_on.sum() * dt)
    if running.any():
        first = int(np.argmax(running))
        after = playing & (np.arange(t.size) >= first)
        co = run.coasting & after
        out.update({"coast_s": float(co.sum() * dt), "coast_wrong_s": float((co & wrong).sum() * dt),
                    "coast_right_s": float((co & right).sum() * dt),
                    "coast_ends": coast_ends(run, after, ph["_cls"], ref_idx, beats, dt)})
    else:
        out.update({"coast_s": 0.0, "coast_wrong_s": 0.0, "coast_right_s": 0.0, "coast_ends": []})
    return out


def one(job: tuple) -> list[str]:
    path, ref_row, mmdb, cfgs, meta = job
    try:
        track = C.Track.load(path)
        beats = np.load(Path(mmdb) / f"{ref_row['name']}.npz")["beats"]
        rows = []
        for cfg in cfgs:
            run = R.replay(track, cfg)
            row = PT.analyse(track, run, ref_row, beats)
            row.update(extras(track, run, ref_row, beats))
            rows.append(json.dumps({"name": ref_row["name"], "config": cfg.name, "class": ref_row["class"],
                                    "octaves": ref_row["octaves"], "dur": ref_row["dur"], **meta, **row},
                                   default=float))
        return rows
    except Exception:
        return [json.dumps({"name": ref_row["name"], "error": traceback.format_exc(limit=6)[-1500:]})]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--ref", type=Path, required=True)
    ap.add_argument("--mmdb", type=Path, default=BEAT / "data" / "mmdb_traktor")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--configs", nargs="+")
    ap.add_argument("--sweep", type=Path)
    ap.add_argument("--all-sweep", action="store_true", help="fw and every entry of the sweep file")
    ap.add_argument("--split", choices=["dev", "test"])
    ap.add_argument("--names", nargs="+")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--jobs", type=int, default=1)
    a = ap.parse_args()
    extra = json.loads(a.sweep.read_text()) if a.sweep else {}
    names = (a.configs or []) + (["fw"] + list(extra) if a.all_sweep else [])
    cfgs = RC.named(list(dict.fromkeys(names)), extra)
    index = json.loads((BEAT / "data" / "traktor" / "index.json").read_text())
    pos = {r["name"]: i for i, r in enumerate(index)}
    recs = {r["name"]: r for r in index}
    jobs = []
    for r in json.loads(a.ref.read_text()):
        split = "dev" if pos[r["name"]] % 2 == 0 else "test"
        if (a.names and r["name"] not in a.names) or (a.split and split != a.split):
            continue
        if not (a.cache / f"{r['name']}.npz").exists():
            continue
        meta = {"split": split, "file": recs[r["name"]]["file"], "traktor_bpm": recs[r["name"]]["bpm"]}
        jobs.append((str(a.cache / f"{r['name']}.npz"), r, str(a.mmdb), cfgs, meta))
    if a.limit:
        jobs = jobs[:a.limit]
    jobs.sort(key=lambda j: -j[1]["dur"])                  # the long tracks first: the pool ends together
    print(f"{len(jobs)} tracks x {len(cfgs)} configs", flush=True)
    with ProcessPoolExecutor(a.jobs) as pool, a.out.open("w") as f:
        for k, lines in enumerate(pool.map(one, jobs, chunksize=1), 1):
            f.write("".join(x + "\n" for x in lines))
            f.flush()
            if k % 50 == 0:
                print(f"{k}/{len(jobs)}", flush=True)
    print("RTRACK DONE", flush=True)


if __name__ == "__main__":
    main()
