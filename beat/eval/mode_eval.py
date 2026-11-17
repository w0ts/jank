"""The mode estimator on the user's tracks: while the transport runs, is the tempo right, and how often does it run?

  steady   each track: share of time running; while running, share within 0.1 / 0.5 BPM and share WRONG (> 0.5);
           in breaks (Traktor 'tk: Break' to 'tk: Drop'): share of break time running (holding) vs stopped
  splice   two tracks back to back: seconds until running at the new tempo, wrong-while-running after the cut
  ramps    the time-warped ramps: share of the ramp in the right ramp mode, error while running

    .venv/bin/python -m eval.mode_eval --part steady
"""
from __future__ import annotations

import argparse
import json

import numpy as np

from eval.data import DATA
from eval.locked_eval import SWITCH_S, folded_error
from eval.locked_failures import in_break
from eval.locked_ramp import RAMP_START_S, RAMPS, warp
from eval.mode_bpm import RAMP_MODES, ModeBpm
from eval.rescore import _rows

STEP_S = 0.5       # the published state is sampled on this clock (the transport runs between beats too)


def trace(beats: np.ndarray, end: float, **settings) -> tuple[np.ndarray, np.ndarray, list[str]]:
    """(times, published bpm or NaN when stopped, mode) on a STEP_S clock, beats fed as they arrive."""
    est = ModeBpm(**settings)
    times = np.arange(STEP_S, end, STEP_S)
    bpms, modes, i = [], [], 0
    for now in times:
        while i < beats.size and beats[i] <= now:
            est.update(float(beats[i]))
            i += 1
        b = est.bpm_at(now)
        bpms.append(np.nan if b is None else b)
        modes.append(est.mode)
    return times, np.array(bpms), modes


def _running_stats(err: np.ndarray) -> dict:
    run = ~np.isnan(err)
    n = max(int(run.sum()), 1)
    return {"running": float(run.mean()), "within_0.1": float(np.sum(err[run] <= 0.1) / n),
            "within_0.5": float(np.sum(err[run] <= 0.5) / n), "wrong_gt_0.5": float(np.sum(err[run] > 0.5) / n)}


def steady(rows, index) -> dict:
    stats, break_run, ambient = [], [], []
    for r in rows:
        meta = index[r["name"]]
        times, bpms, _ = trace(np.array(r["est_beats"]), meta["dur"])
        err = folded_error(bpms, meta["bpm"])
        stats.append(_running_stats(err))
        brk = in_break(times, meta["breaks"], meta["drops"])
        if brk.any():
            break_run.append(float(np.mean(~np.isnan(err[brk]))))
    mean = {k: round(float(np.mean([s[k] for s in stats])), 4) for k in stats[0]}
    return {**mean, "break_time_running": round(float(np.mean(break_run)), 4), "tracks": len(stats)}


def splice(rows, index, pairs: int = 300, seed: int = 0) -> dict:
    rng = np.random.default_rng(seed)
    starts, wrong = [], []
    for _ in range(pairs):
        a, b = rng.choice(len(rows), 2, replace=False)
        ma, mb = index[rows[a]["name"]], index[rows[b]["name"]]
        if abs(ma["bpm"] - mb["bpm"]) < 1.0:
            continue
        ba, bb = np.array(rows[a]["est_beats"]), np.array(rows[b]["est_beats"])
        beats = np.concatenate([ba[ba < SWITCH_S], bb[bb >= 60.0] - 60.0 + SWITCH_S])
        times, bpms, _ = trace(beats, SWITCH_S + 120.0)
        err = folded_error(bpms, np.where(times < SWITCH_S, ma["bpm"], mb["bpm"]))
        after = times >= SWITCH_S
        ok = after & (err <= 0.1)
        starts.append(float(times[ok][0] - SWITCH_S) if ok.any() else np.nan)
        run = after & ~np.isnan(err)
        wrong.append(float(np.mean(err[run] > 0.5)) if run.any() else 0.0)
    starts = np.array(starts)
    return {"pairs": int(starts.size), "right_tempo_after_s_median": round(float(np.nanmedian(starts)), 1),
            "never": round(float(np.mean(np.isnan(starts))), 3), "wrong_while_running_after_cut": round(float(np.mean(wrong)), 4)}


def ramps(rows, index) -> list[dict]:
    out = []
    long_rows = [r for r in rows if index[r["name"]]["dur"] > RAMP_START_S + 150]
    for change, ramp_s in RAMPS:
        right_mode, errs = [], []
        want = "speeding_up" if change > 0 else "slowing_down"
        for r in long_rows:
            meta = index[r["name"]]
            beats, factor = warp(np.array(r["est_beats"]), change, ramp_s)
            times, bpms, modes = trace(beats, float(beats[-1]))
            err = folded_error(bpms, meta["bpm"] * factor(times))
            end = RAMP_START_S + ramp_s / (1 + change / 2)
            mid = (times > RAMP_START_S) & (times < end)
            if mid.any():
                right_mode.append(np.mean([m == want for m, k in zip(modes, mid) if k]))
                run = mid & ~np.isnan(err)
                if run.any():
                    errs.append(np.median(err[run]))
        out.append({"change": change, "ramp_s": ramp_s, "in_right_ramp_mode": round(float(np.mean(right_mode)), 3),
                    "median_err_while_running": round(float(np.median(errs)), 3)})
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--part", choices=["steady", "splice", "ramps"], required=True)
    ap.add_argument("--tracker", default="madmom_online")
    args = ap.parse_args()
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    rows = [r for r in _rows("traktor", args.tracker) if "error" not in r and r["name"] in index and "window" not in r]
    result = {"steady": steady, "splice": splice, "ramps": ramps}[args.part](rows, index)
    print(json.dumps({"part": args.part, "result": result}, indent=1))


if __name__ == "__main__":
    main()
