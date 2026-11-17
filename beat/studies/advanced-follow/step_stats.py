"""The user's case statistically: a pitch step (varispeed) of +-STEP on many tracks, at random places (any beat or bar
phase, in or out of breaks), through today's tracker (live: one net, DBN 90-180) and today's decision layer (ModeBpm,
confidence gate at 0.8 = the XIAO's default), replayed (replay.py). One JSON line per case.

    .venv/bin/python studies/advanced-follow/step_stats.py --cases 300 --change 0.02 --jobs 3 --out FILE.jsonl
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import json
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1]))

import extract as E
import material as M
import replay as P
from eval.locked_failures import in_break

SPAN_S = 30.0          # judged over this long after the step
FLAM_MS = 50.0
CONV_MS = 15.0         # converged: the grid this close to the beats (the user: phase error <= 10-15 ms)
CONV_RUN = 8
OK_REL = 0.005
_recs: dict = {}


def cases(n: int, change: float, seed: int) -> list[dict]:
    rng = np.random.default_rng(seed)
    recs = [r for r in M.load_index() if r["dur"] > 150]
    out = []
    for k in range(n):
        r = recs[int(rng.integers(len(recs)))]
        sign = 1 if k % 2 == 0 else -1
        out.append({"track": r["name"], "change": sign * change, "at": float(rng.uniform(45.0, min(r["dur"] - 70.0, 150.0)))})
    return out


def features(case: dict, rec: dict, tr: P.Trace, beats: np.ndarray, ref: np.ndarray, err: np.ndarray, run: np.ndarray,
             info: dict) -> dict:
    t0 = info["t_change"]
    k0 = int(np.searchsorted(tr.t, t0)) - 1
    pre = (ref >= t0 - 10) & (ref < t0)
    base = float(np.nanmedian(err[pre & run])) if np.any(pre & run) else float("nan")
    win = (ref >= t0) & (ref < t0 + SPAN_S)
    tw = (tr.t >= t0) & (tr.t < t0 + SPAN_S)
    half = 30000.0 / rec["bpm"]                       # half a period in ms: the offset to the old phase, wrapped
    moved = np.abs((err - base + half) % (2 * half) - half)
    flam = win & run & (moved > FLAM_MS)
    ramp = info.get("ramp_s", 0.0)
    x = np.clip((tr.t[tw] - t0) / ramp, 0, 1) if ramp > 0 else np.ones(int(tw.sum()))
    true = info["bpm"] * (1 + info["change"] * x)
    folded = np.array([60.0 / P_fold(60.0 / b, 60.0 / tb) if b == b else np.nan for b, tb in zip(tr.bpm[tw], true)])
    good = np.abs(folded / true - 1) <= OK_REL
    bad_idx = np.where(~good)[0]
    follow = float(tr.t[tw][bad_idx[-1] + 1] - t0) if bad_idx.size and bad_idx[-1] + 1 < good.size else (
        0.0 if not bad_idx.size else float("nan"))
    period = 60.0 / rec["bpm"]
    prev = ref[ref <= t0]
    beat_phase = float((t0 - prev[-1]) / period) if prev.size else float("nan")
    downs = np.asarray(rec["downbeats"])
    bar_pos = int(np.round((t0 - downs[downs <= t0][-1]) / period)) % 4 if np.any(downs <= t0) else -1
    # the tracker's own regularity before the step: residual of its beats against a straight line (ms)
    bb = beats[(beats >= t0 - 10) & (beats < t0)]
    jitter = float("nan")
    if bb.size >= 6:
        kk = np.round((bb - bb[0]) / period)
        jitter = float(np.std(bb - np.polyval(np.polyfit(kk, bb, 1), kk)) * 1000)
    stopped_s = float(np.sum(tw & ~tr.running) * P.TICK_S)
    gated_s = float(np.sum(tw & tr.gated) * P.TICK_S)
    # convergence: reference beats from the move until the published grid is, for good, within CONV_MS of the beat
    # (against its phase before) at the true tempo within OK_REL, running
    wi = np.where(win)[0]
    kk = np.searchsorted(tr.t, ref[wi], side="right") - 1
    xb = np.clip((ref[wi] - t0) / ramp, 0, 1) if ramp > 0 else np.ones(wi.size)
    true_p = 60.0 / (info["bpm"] * (1 + info["change"] * xb))
    gpk = tr.gp[np.maximum(kk, 0)]
    ok_t = np.array([g == g and abs(P_fold(g, tp) / tp - 1) <= OK_REL for g, tp in zip(gpk, true_p)])
    good_b = run[wi] & ok_t & (moved[wi] <= CONV_MS)
    # the first reference beat from which CONV_RUN beats in a row are good (a later fill or glitch does not count)
    conv_beats = next((k for k in range(max(wi.size - CONV_RUN + 1, 0)) if good_b[k:k + CONV_RUN].all()), None)
    first10 = moved[wi[:10]][run[wi[:10]]]
    worst10 = float(np.max(first10)) if first10.size else None
    log = getattr(tr.est, "log", [])
    man = [t for t, ev in log if ev == "manoeuvre"]
    after = [t for t in man if t >= t0]
    return {
        **case, "bpm": rec["bpm"], "folder": rec["file"].split("/")[0],
        "stopped_s": round(stopped_s, 2), "gated_s": round(gated_s, 2), "flam_beats": int(flam.sum()),
        "worst_ms": round(float(np.nanmax(moved[win & run])), 1) if np.any(win & run) else None,
        "follow_s": round(follow, 2), "jumps": int(np.nanmax(tr.jumps[tw]) - tr.jumps[k0]) if k0 >= 0 else None,
        "conf_before": round(float(tr.conf[k0]), 3) if k0 >= 0 else None,
        "running_before": bool(tr.running[k0]) if k0 >= 0 else None,
        "min_conf_before10": round(float(np.min(tr.conf[(tr.t >= t0 - 10) & (tr.t < t0)])), 3),
        "fit_n_before": int(tr.fit_n[k0]) if k0 >= 0 else None, "misses_before": int(tr.misses[k0]) if k0 >= 0 else None,
        "beat_phase": round(beat_phase, 3), "bar_pos": bar_pos, "jitter_ms": round(jitter, 2),
        "in_break": bool(in_break(np.array([t0]), rec["breaks"], rec["drops"])[0]),
        "beats_in_span": int(np.sum((beats >= t0) & (beats < t0 + SPAN_S))), "ref_in_span": int(win.sum()),
        "conv_beats": conv_beats, "worst10_ms": round(worst10, 1) if worst10 is not None else None,
        "detect_s": round(after[0] - t0, 3) if after else None,
        "manoeuvres_span": sum(t0 <= t < t0 + SPAN_S for t in man),
        "manoeuvres_before60": sum(t0 - 60 <= t < t0 for t in man),
    }


def P_fold(period: float, true_period: float) -> float:
    from eval.grid import _fold
    return _fold(period, true_period)


def estimators() -> dict:
    """name -> (estimator, confidence kind): today, each half of the quick win alone, and both."""
    from eval.follow_bpm import FollowBpm
    from eval.mode_bpm import ModeBpm
    return {"today": (ModeBpm, "grid"), "lines_only": (ModeBpm, "lines"), "follow_only": (FollowBpm, "grid"),
            "follow": (FollowBpm, "lines"), "follow_ratio": (FollowBpm, "lines")}


def case_beats(case: dict, cache: Path | None) -> tuple[np.ndarray, np.ndarray, dict]:
    """(tracker beats, reference beats, info) of one case, cached by its parameters."""
    if not _recs:
        _recs.update({r["name"]: r for r in M.load_index()})
    rec = _recs[case["track"]]
    ramp = case.get("ramp_s", 0.0)
    key = f"{rec['name']}_{case['change']:+.4f}_{case['at']:.3f}_{ramp:.2f}.npz"
    if cache is not None and (cache / key).exists():
        z = np.load(cache / key)
        if "ratio" in z.files:
            info = json.loads(str(z["info"]))
            return z["beats"], z["ref"], {**info, "ratio": z["ratio"].tolist()}
    import ratio_probe as RP
    kind = "pitch_ramp" if ramp > 0 else "pitch_step"
    item = M.Item(f"same/{kind}/{rec['name']}", "same", kind, "x", (rec["name"],),
                  {"change": case["change"], "ramp_s": ramp, "at": case["at"]})
    y, ref, info = M.render(item, _recs)
    beats = E.dbn_beats(E.activation(y))
    info = {k: v for k, v in info.items() if k not in ("breaks", "drops")}
    t0 = info["t_change"]
    ratio = np.array([(t, r) for t, r, _ in RP.estimates(RP.logfreq_blocks(y), max(t0 - 60.0, 5.0), t0 + SPAN_S)])
    if cache is not None:
        cache.mkdir(parents=True, exist_ok=True)
        np.savez(cache / key, beats=beats, ref=ref, info=json.dumps(info), ratio=ratio)
    return beats, ref, {**info, "ratio": ratio.tolist()}


def one(job: tuple[dict, str | None, tuple[str, ...]]) -> list[str]:
    case, cache, names = job
    try:
        beats, ref, info = case_beats(case, Path(cache) if cache else None)
        rec = _recs[case["track"]]
        info = {**info, "breaks": rec["breaks"], "drops": rec["drops"]}
        out = []
        import ratio_probe as RP
        events = RP.events(info["ratio"])
        for name in names:
            factory, conf_kind = estimators()[name]
            hook = None
            if name == "follow_ratio":
                pending = list(events)

                def hook(est, now, fed, pending=pending):
                    while pending and pending[0][0] <= now:
                        t_ev, r = pending.pop(0)
                        est.propose_ratio(r, t_ev, since=t_ev - RP.DETECT_LAG_S)
                    return None
            tr = P.run(beats, info["dur"], factory, conf_kind=conf_kind, on_tick=hook)
            err, run, _ = P.ref_errors(tr, ref)
            row = features(case, rec, tr, beats, ref, err, run, info)
            t0 = info["t_change"]
            row["ratio_events_before60"] = sum(t0 - 60 <= t < t0 for t, _ in events)
            after = [(t, r) for t, r in events if t >= t0]
            row["ratio_detect_s"] = round(after[0][0] - t0, 2) if after else None
            row["ratio_err"] = round(after[0][1] - (1 + case["change"]), 5) if after else None
            out.append(json.dumps({"estimator": name, **row}))
        return out
    except Exception as exc:
        return [json.dumps({**case, "error": f"{type(exc).__name__}: {exc}"})]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=300)
    ap.add_argument("--change", type=float, default=0.02)
    ap.add_argument("--ramp", type=float, default=0.0)
    ap.add_argument("--seed", type=int, default=2)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--cache", type=Path, help="keep each case's tracker beats here (re-scoring skips the tracker)")
    ap.add_argument("--estimators", nargs="+", default=["today", "lines_only", "follow_only", "follow", "follow_ratio"])
    args = ap.parse_args()
    cs = cases(args.cases, args.change, args.seed)
    for c in cs:
        c["ramp_s"] = args.ramp
    if args.cache:
        args.cache.mkdir(parents=True, exist_ok=True)
    jobs = [(c, str(args.cache) if args.cache else None, tuple(args.estimators)) for c in cs]
    with ProcessPoolExecutor(args.jobs) as pool, args.out.open("w") as f:
        for k, lines in enumerate(pool.map(one, jobs, chunksize=1), 1):
            f.write("".join(line + "\n" for line in lines))
            f.flush()
            if k % 20 == 0:
                print(f"{k}/{len(cs)}", flush=True)


if __name__ == "__main__":
    main()
