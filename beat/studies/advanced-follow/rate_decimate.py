"""Stage 2: does the device's decimation of the spectral rate (live/rate.py RateProbe step: every frame, every 2nd, every
5th into the 0.1 s blocks; every: an estimate each block or each 2nd) cost detections? The same pitch-step cases as
ratio_probe.py (the user's tracks, a step at a random place), each case's frames fed to one RateProbe per variant from
t0 - 20 s to t0 + 4 s.

Per step value: the share of steps found within 3 s, the delay, the ratio's error, and false events before the step
(per hour of music).

    python studies/advanced-follow/rate_decimate.py --cases 60 --changes 0 0.02 0.05 --jobs 4 --out F.jsonl
    python studies/advanced-follow/rate_decimate.py --table F.jsonl
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

import material as M
import step_stats as S

from live.rate import RateProbe, centred_magnitudes

STEPS = ((1, 1), (2, 1), (5, 1), (2, 2))       # (frame step, estimate every n blocks)
BEFORE_S, AFTER_S, FOUND_S = 20.0, 4.0, 3.0


def one(case: dict) -> str:
    if not S._recs:
        S._recs.update({r["name"]: r for r in M.load_index()})
    rec = S._recs[case["track"]]
    item = M.Item(f"same/pitch_step/{rec['name']}", "same", "pitch_step", "x", (rec["name"],),
                  {"change": case["change"], "ramp_s": 0.0, "at": case["at"]})
    try:
        y, _, info = M.render(item, S._recs)
        t0 = info["t_change"]
        f0 = int((t0 - BEFORE_S) * 100) // 10 * 10
        f1 = int((t0 + AFTER_S) * 100)
        mag = centred_magnitudes(y, f1 - f0, first=f0)
        out = {**case, "t0": t0}
        for step, every in STEPS:
            probe = RateProbe(step, every)
            events = [e for k in range(f1 - f0) if (e := probe.frame(f0 + k, mag[k])) is not None]
            before = [r for t, r in events if t < t0]
            after = [(t, r) for t, r in events if t0 <= t < t0 + FOUND_S]
            out[f"s{step}e{every}"] = {"false": len(before), "span": t0 - (f0 / 100 + 4.5),
                               "delay": round(after[0][0] - t0, 2) if after else None,
                               "err": round(after[0][1] - (1 + case["change"]), 5) if after else None}
        return json.dumps(out)
    except Exception as exc:
        return json.dumps({**case, "error": f"{type(exc).__name__}: {exc}"})


def table(path: Path) -> None:
    rows = [json.loads(x) for x in path.read_text().splitlines()]
    rows = [r for r in rows if "error" not in r]
    print("| change | frame step / estimate every | cases | found in 3 s | delay s med | abs ratio err med / p90 | false events / h |")
    print("|---|---|---|---|---|---|---|")
    for change in sorted({abs(r["change"]) for r in rows}):
        sel = [r for r in rows if abs(r["change"]) == change]
        for step, every in STEPS:
            v = [r[f"s{step}e{every}"] for r in sel]
            hours = sum(max(x["span"], 0.0) for x in v) / 3600
            false = sum(x["false"] for x in v) / hours if hours else float("nan")
            found = [x for x in v if x["delay"] is not None]
            if change == 0:
                print(f"| none | {step} / {every} | {len(v)} | - | - | - | {false:.0f} |")
                continue
            err = np.abs([x["err"] for x in found]) * 100
            print(f"| {change:.2f} | {step} / {every} | {len(v)} | {len(found) / len(v):.2f} | "
                  f"{np.median([x['delay'] for x in found]):.2f} | {np.median(err):.2f} % / {np.percentile(err, 90):.2f} % | "
                  f"{false:.0f} |")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=60)
    ap.add_argument("--changes", nargs="+", type=float, default=[0.0, 0.02, 0.05])
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--table", type=Path)
    args = ap.parse_args()
    if args.table:
        table(args.table)
        return
    jobs = []
    for c in args.changes:
        jobs += S.cases(args.cases, c, seed=11)
    with ProcessPoolExecutor(args.jobs) as pool, args.out.open("w") as f:
        for line in pool.map(one, jobs, chunksize=1):
            f.write(line + "\n")
            f.flush()


if __name__ == "__main__":
    main()
