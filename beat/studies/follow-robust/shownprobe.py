"""The graded confidence on the real tracks: the gate decisions are identical with `shown` on or off, and how well `shown` predicts right.

    python studies/follow-robust/shownprobe.py --cache C --ref ref.json --sweep skip.json --base a2-pat --with pat-shown [--tracks 40]
"""
from __future__ import annotations

import argparse
import json
import sys
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
from confanalyse import auc
from confprobe import labels


def one(job: tuple) -> dict:
    path, ref_row, mmdb, cfgs = job
    track = C.Track.load(path)
    beats = np.load(Path(mmdb) / f"{ref_row['name']}.npz")["beats"]
    a, b = (R.replay(track, c) for c in cfgs)
    same = bool(np.array_equal(a.running, b.running) and np.array_equal(a.coasting, b.coasting)
                and np.array_equal(np.nan_to_num(a.out_bpm, nan=-1), np.nan_to_num(b.out_bpm, nan=-1)))
    lab = labels(track, b, ref_row, beats)
    m = lab["running"] & lab["playing"] & (lab["right"] | lab["wrong"])
    return {"name": ref_row["name"], "same": same, "shown": b.shown[m][::3], "conf": b.conf[m][::3], "right": lab["right"][m][::3],
            "all_shown": b.shown[lab["running"] & lab["playing"]]}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--ref", type=Path, required=True)
    ap.add_argument("--mmdb", type=Path, default=BEAT / "data" / "mmdb_traktor")
    ap.add_argument("--sweep", type=Path, required=True)
    ap.add_argument("--base", required=True)
    ap.add_argument("--with", dest="with_", required=True)
    ap.add_argument("--tracks", type=int, default=40)
    ap.add_argument("--jobs", type=int, default=1)
    a = ap.parse_args()
    cfgs = RC.named([a.base, a.with_], json.loads(a.sweep.read_text()))
    rng = np.random.default_rng(5)
    rows = [r for r in json.loads(a.ref.read_text()) if (a.cache / f"{r['name']}.npz").exists()]
    pick = [rows[i] for i in rng.permutation(len(rows))[:a.tracks]]
    jobs = [(str(a.cache / f"{r['name']}.npz"), r, str(a.mmdb), cfgs) for r in pick]
    with ProcessPoolExecutor(a.jobs) as pool:
        res = list(pool.map(one, jobs, chunksize=1))
    print("gate decisions identical on", sum(r["same"] for r in res), "of", len(res), "tracks")
    shown = np.concatenate([r["shown"] for r in res]); conf = np.concatenate([r["conf"] for r in res])
    right = np.concatenate([r["right"] for r in res])
    print(f"AUC right vs wrong: shown {auc(shown, right):.3f}, gate conf {auc(conf, right):.3f}  ({len(right)} ticks, {100 * right.mean():.0f} % right)")
    allv = np.concatenate([r["all_shown"] for r in res])
    h, _ = np.histogram(np.clip(allv, 0, 1), bins=[0, .2, .4, .6, .8, 1.0001])
    print("shown over running ticks, share in 0-20/20-40/40-60/60-80/80-100: " + " / ".join(f"{100 * v / h.sum():.0f}" for v in h))
    print("mean shown when right %.3f, when wrong %.3f" % (shown[right].mean(), shown[~right].mean()))


if __name__ == "__main__":
    main()
