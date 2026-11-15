"""Replay every cached item (extract.py) through today / quick / advanced and score it (metrics.py); 'probe' = quick
plus the detector's novelty recorded once a second and never acted on (for tuning the detector's policy).

    python studies/advanced-follow/study_eval.py --cache CACHE --out FILE.jsonl --modes today quick advanced probe
        [--split dev] [--family diff] [--jobs N] [--policy '{"timbre_diff": 3.0}']
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import json
import sys
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1]))

import live_replay as LR
import metrics as MT

PROBE = LR.Policy(chroma_same=-1.0, timbre_diff=float("inf"), chroma_diff=float("inf"))


def one(job: tuple) -> list[str]:
    path, modes, policy, follow = job
    import eval.follow_bpm as FB
    for k, v in follow.items():                  # the follower's constants for this run (a settings sweep)
        setattr(FB, k, v)
    try:
        z = np.load(path)
        meta = json.loads(str(z["meta"]))
        act, L10, ref, beats = z["act"], z["L10"], z["ref"], z["beats"]
        lines = []
        for mode in modes:
            pol = PROBE if mode == "probe" else LR.Policy(**policy)
            run = LR.run(act, L10, "advanced" if mode == "probe" else mode, pol)
            row = {"mode": mode, **MT.score(run, ref, beats, meta)}
            if mode == "probe":
                step = int(round(1.0 / LR.TICK_S))
                row["sec"] = {"t": np.round(run.t[::step], 2).tolist(),
                              "timbre": np.round(run.timbre[::step], 3).tolist(),
                              "chroma": np.round(run.chroma[::step], 4).tolist(),
                              "conf": np.round(run.conf[::step], 3).tolist(),
                              "bpm": np.round(run.bpm[::step], 2).tolist()}
                row["meta"] = {k: meta[k] for k in ("t_switch", "fade_s", "t_var", "t_change", "dur") if k in meta}
            lines.append(json.dumps(row, default=float))
        return lines
    except Exception:
        return [json.dumps({"path": path, "error": traceback.format_exc(limit=4)[-800:]})]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--modes", nargs="+", default=["today", "quick", "advanced"])
    ap.add_argument("--split", choices=["dev", "test"])
    ap.add_argument("--family", choices=["real", "same", "diff"])
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--limit", type=int)
    ap.add_argument("--policy", default="{}")
    ap.add_argument("--follow", default="{}", help="eval.follow_bpm constants, e.g. {\"MANOEUVRE_SIGMA\": 3.0}")
    ap.add_argument("--kinds", nargs="+", help="only these item kinds")
    args = ap.parse_args()
    plan = json.loads((args.cache / "plan.json").read_text())
    names = [p["name"] for p in plan if (args.split is None or p["split"] == args.split)
             and (args.family is None or p["name"].split("/")[0] == args.family)
             and (args.kinds is None or (p["name"].split("/")[1] if p["name"].count("/") > 1 else "track") in args.kinds)]
    paths = [args.cache / (n.replace("/", "__").replace("+", "_") + ".npz") for n in names]
    paths = [str(p) for p in paths if p.exists()]
    if args.limit:
        paths = paths[:args.limit]
    policy, follow = json.loads(args.policy), json.loads(args.follow)
    print(f"{len(paths)} items", flush=True)
    with ProcessPoolExecutor(args.jobs) as pool, args.out.open("w") as f:
        for k, lines in enumerate(pool.map(one, [(p, tuple(args.modes), policy, follow) for p in paths], chunksize=1), 1):
            f.write("".join(x + "\n" for x in lines))
            f.flush()
            if k % 50 == 0:
                print(f"{k}/{len(paths)}", flush=True)
    print("EVAL DONE", flush=True)


if __name__ == "__main__":
    main()
