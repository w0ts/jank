"""A skip test: splice tracks (features, not audio) and measure how fast the follower re-locks.

Cases per steady source track (50 s before the skip, 50 s after; the DBN runs on the spliced activation, so the tracker's own
re-sync is in the numbers; the RNN's transient at a join is not, an optimistic limit):
  jump   the same track jumped forward by a non-beat-aligned offset (two offsets)
  nudge  forward by 0.3-0.45 s (a cue nudge, under a beat)
  other  another steady track at a tempo more than 8 % away (not an octave or 2:3 / 3:2)
  gap    4 s of silence, then the same track further on
Truth: the offline beats of the source (mmdb_traktor). Only tracks the chain was locked on, right in tempo and phase, 2 s before
the skip (90 % of the ticks), count. Metrics (s and beats of the true period): tempo re-lock (3 s in a row within 1 %), phase re-lock (also the
output grid within 0.1 period of a true beat), the seconds the output ran while tempo or phase was wrong (first 30 s), the gate
time (first tick not running).

    python studies/follow-robust/skiptest.py --cache C --ref ref.json --out skip.jsonl --configs fw-dbn all-d-f60 \
        --sweep skip.json [--tracks 40] [--jobs N]
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
import rchain as R
import rconfigs as RC
import score as S
from dbn import OnlineDbn

FPS, PRE_S, POST_S, GAP_S = 100, 50.0, 50.0, 4.0
STAY_S, PHASE_TOL, RIGHT = 3.0, 0.1, math.log(1.01)


def cut(tr: C.Track, a: float, b: float) -> dict:
    """The features of tr in [a, b) s."""
    ia, ib = int(a * FPS), int(b * FPS)
    ra, rb = int(a / C.BLOCK_S), int(b / C.BLOCK_S)
    fl = tr.flux
    fa, fb = int(len(fl) * a / tr.dur), int(len(fl) * b / tr.dur)
    rate = tr.rate[(tr.rate[:, 0] >= a) & (tr.rate[:, 0] < b)] if tr.rate.size else tr.rate
    return {"act": tr.act[ia:ib], "rms": tr.rms[ra:rb], "flux": fl[fa:fb], "rate": rate - np.array([a, 0, 0]),
            "len": b - a}


def silence(s: float, like: C.Track) -> dict:
    return {"act": np.zeros(int(s * FPS), like.act.dtype), "rms": np.zeros(int(s / C.BLOCK_S), like.rms.dtype),
            "flux": np.zeros((max(int(len(like.flux) * s / like.dur), 1),) + like.flux.shape[1:], like.flux.dtype),
            "rate": like.rate[:0], "len": s}


def join(parts: list[dict]) -> C.Track:
    t0, rates = 0.0, []
    for p in parts:
        if p["rate"].size:
            rates.append(p["rate"] + np.array([t0, 0, 0]))
        t0 += p["len"]
    rate = np.concatenate(rates) if rates else np.zeros((0, 3))
    return C.Track(np.zeros(0), np.concatenate([p["rms"] for p in parts]), rate,
                   np.concatenate([p["act"] for p in parts]), np.concatenate([p["flux"] for p in parts]), t0)


def truth(beats: np.ndarray, a: float, b: float, start: float) -> np.ndarray:
    return beats[(beats >= a) & (beats < b)] - a + start


def period_of(b: np.ndarray) -> float:
    return float(np.median(np.diff(b))) if b.size > 3 else float("nan")


def good_ticks(run: R.RRun, tb: np.ndarray) -> tuple[np.ndarray, np.ndarray] | None:
    """(tempo ok, tempo and phase ok) per tick against the true beats tb."""
    t = run.t
    per = period_of(tb)
    if not np.isfinite(per) or tb.size < 8:
        return None
    dev = S.folded_dev(run.out_bpm, np.full(t.size, 60.0 / per))
    tempo_ok = run.running & (dev <= RIGHT)
    g0, gp = run.g0, run.gp
    phase_ok = np.zeros(t.size, bool)
    for k in np.where(run.running & ~np.isnan(g0))[0]:
        near = tb[np.abs(tb - g0[k]) < 2 * gp[k] + per]
        if near.size:
            e = np.abs((g0[k] - near + gp[k] / 2) % gp[k] - gp[k] / 2)
            phase_ok[k] = e.min() <= PHASE_TOL * gp[k]
    return tempo_ok, tempo_ok & phase_ok


def tracker_relock(trk: C.Track, skip_t: float, tb: np.ndarray) -> float | None:
    """Seconds after the skip until 4 DBN beats in a row each lie within 0.1 period of a true beat (the tracker's own floor)."""
    per = period_of(tb)
    dbn = OnlineDbn(trk.act)
    beats: list[float] = []
    for f in range(int(skip_t * FPS), int(trk.dur * FPS), 10):
        beats.extend(dbn.advance(f))
    hit = [float(np.abs(tb - b).min()) <= PHASE_TOL * per for b in beats if b >= skip_t]
    times = [b for b in beats if b >= skip_t]
    for i in range(len(hit) - 3):
        if all(hit[i:i + 4]):
            return (times[i] - skip_t) / per
    return None


def metrics(run: R.RRun, skip_t: float, tb_pre: np.ndarray, tb: np.ndarray) -> dict | None:
    t = run.t
    per = period_of(tb)
    after, before = good_ticks(run, tb), good_ticks(run, tb_pre)
    if after is None or before is None:
        return None
    tempo_ok, good = after
    dt = float(t[1] - t[0])
    pre = (t >= skip_t - 2.0) & (t < skip_t)
    if not pre.any() or before[1][pre].mean() < 0.9:
        return {"locked_before": False}
    post = t >= skip_t

    def relock(ok: np.ndarray) -> float | None:
        need = int(round(STAY_S / dt))
        idx = np.where(post)[0]
        run_len = 0
        for i in idx:
            run_len = run_len + 1 if ok[i] else 0
            if run_len >= need:
                return float(t[i - need + 1] - skip_t)
        return None

    win = post & (t < skip_t + 30.0)
    bad = win & run.running & ~good
    ungated = np.where(post & ~run.running)[0]
    tr_, ph_ = relock(tempo_ok), relock(good)
    return {"locked_before": True, "period": per, "tempo_relock_s": tr_, "phase_relock_s": ph_,
            "tempo_relock_beats": None if tr_ is None else tr_ / per,
            "phase_relock_beats": None if ph_ is None else ph_ / per,
            "wrong_out_s": float(bad.sum() * dt), "wrong_out_beats": float(bad.sum() * dt / per),
            "gate_s": float(t[ungated[0]] - skip_t) if ungated.size else None,
            "tempo_wrong_s": float((win & run.running & ~tempo_ok).sum() * dt)}


def cases(tr: C.Track, beats: np.ndarray, other: tuple[C.Track, np.ndarray], rng: np.random.Generator) -> list[tuple]:
    """(case, track, truth beats after the skip, skip time)."""
    out = []
    per = period_of(beats)
    start = 20.0
    pre = cut(tr, start, start + PRE_S)
    for name, off in (("jump", float(rng.uniform(17, 40)) + 0.37 * per), ("jump", float(rng.uniform(41, 75)) + 0.61 * per),
                      ("nudge", float(rng.uniform(0.3, 0.45)))):
        a = start + PRE_S + off
        if a + POST_S > tr.dur - 1:
            continue
        out.append((name, join([pre, cut(tr, a, a + POST_S)]), truth(beats, a, a + POST_S, PRE_S), PRE_S, off))
    a = start + PRE_S + float(rng.uniform(10, 40))
    if a + POST_S <= tr.dur - 1:
        out.append(("gap", join([pre, silence(GAP_S, tr), cut(tr, a, a + POST_S)]),
                    truth(beats, a, a + POST_S, PRE_S + GAP_S), PRE_S, GAP_S))
    ot, ob = other
    a = 30.0
    if a + POST_S <= ot.dur - 1:
        out.append(("other", join([pre, cut(ot, a, a + POST_S)]), truth(ob, a, a + POST_S, PRE_S), PRE_S,
                    60.0 / period_of(ob) / (60.0 / per)))
    return out


def one(job: tuple) -> list[str]:
    name, path, beats_path, other, cfgs, seed = job
    try:
        rng = np.random.default_rng(seed)
        tr = C.Track.load(path)
        beats = np.load(beats_path)["beats"]
        ot = (C.Track.load(other[0]), np.load(other[1])["beats"])
        rows = []
        for case, trk, tb, skip_t, param in cases(tr, beats, ot, rng):
            tb_pre = truth(beats, 20.0, 20.0 + PRE_S, 0.0)
            for cfg in cfgs:
                m = metrics(R.replay(trk, cfg), skip_t, tb_pre, tb)
                if m is not None and m.get("locked_before"):
                    m["tracker_relock_beats"] = tracker_relock(trk, skip_t, tb)
                if m is not None:
                    rows.append(json.dumps({"name": name, "case": case, "param": param, "config": cfg.name, **m}))
        return rows
    except Exception:
        return [json.dumps({"name": name, "error": traceback.format_exc(limit=6)[-1200:]})]


def pick_other(per: float, cand: list[dict], rng: np.random.Generator) -> dict | None:
    for _ in range(200):
        c = cand[int(rng.integers(len(cand)))]
        r = (60.0 / per) / c["bpm"]
        folds = [abs(r / q - 1) for q in (1.0, 2.0, 0.5, 2 / 3, 3 / 2, 3 / 4, 4 / 3)]
        if min(folds) > 0.08:
            return c
    return None


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--ref", type=Path, required=True)
    ap.add_argument("--mmdb", type=Path, default=BEAT / "data" / "mmdb_traktor")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--configs", nargs="+", required=True)
    ap.add_argument("--sweep", type=Path)
    ap.add_argument("--tracks", type=int, default=40)
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--seed", type=int, default=7)
    a = ap.parse_args()
    cfgs = RC.named(a.configs, json.loads(a.sweep.read_text()) if a.sweep else {})
    rng = np.random.default_rng(a.seed)
    cand = []
    for r in json.loads(a.ref.read_text()):
        bp = a.mmdb / f"{r['name']}.npz"
        if r["class"] == "steady" and r["dur"] >= 200 and bp.exists() and (a.cache / f"{r['name']}.npz").exists():
            b = np.load(bp)["beats"]
            if b.size > 100 and 60 <= 60.0 / period_of(b) <= 180:
                cand.append({"name": r["name"], "bpm": 60.0 / period_of(b)})
    picks = [cand[i] for i in rng.permutation(len(cand))[:a.tracks]]
    jobs = []
    for k, c in enumerate(picks):
        o = pick_other(60.0 / c["bpm"], cand, rng)
        if o is None:
            continue
        jobs.append((c["name"], str(a.cache / f"{c['name']}.npz"), str(a.mmdb / f"{c['name']}.npz"),
                     (str(a.cache / f"{o['name']}.npz"), str(a.mmdb / f"{o['name']}.npz")), cfgs, 1000 + k))
    print(f"{len(jobs)} tracks x {len(cfgs)} configs", flush=True)
    with ProcessPoolExecutor(a.jobs) as pool, a.out.open("w") as f:
        for lines in pool.map(one, jobs, chunksize=1):
            f.write("".join(x + "\n" for x in lines))
            f.flush()
    print("SKIPTEST DONE", flush=True)


if __name__ == "__main__":
    main()
