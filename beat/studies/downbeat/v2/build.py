"""From extract2.py's cache: per track the descriptors (feats.py) at the online beats and at the reference beats, the
beat counts, the reference bar positions, and the first study's 22 features at the online beats (to reproduce its
baseline from the same cache). One pickle per dataset.

    python build.py --dataset traktor --jobs 14 --cache CACHE_DIR --out CACHE_DIR/traktor_desc.pkl [--ref-lag 0.0]
"""
from __future__ import annotations

import argparse
import os
import pickle
import sys
import time
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, os.environ.get("BL_REPO", str(HERE.parents[2])))


def one(job) -> dict:
    from extract import beat_features, frame_curves
    from feats import describe
    from linkbeats import link_beats
    from rules import count_beats, ref_positions, valid_4
    path, ref_lag = job
    try:
        z = np.load(path)
        L = z["L"].astype(np.float32)
        t_on, t_ref, ref_pos = np.asarray(z["online"]), np.asarray(z["ref_beats"]), np.asarray(z["ref_pos"])
        valid, bars, non4 = valid_4(ref_pos)
        c_on = count_beats(t_on) if len(t_on) else np.zeros(0, int)
        rp_on = ref_positions(t_on, t_ref, ref_pos, valid)
        # matched online beats: their lag behind the reference (for shifting the reference beats like the tracker)
        lag = np.nan
        if len(t_on) and len(t_ref) > 1:
            j = np.clip(np.searchsorted(t_ref, t_on), 1, len(t_ref) - 1)
            j = np.where(np.abs(t_ref[j - 1] - t_on) <= np.abs(t_ref[j] - t_on), j - 1, j)
            dd = t_on - t_ref[j]
            ok = np.abs(dd) <= 0.07
            lag = float(np.median(dd[ok])) if ok.sum() > 8 else np.nan
        t_ref_s = t_ref + ref_lag
        cur = frame_curves(L)
        t_link, c_link = link_beats(t_on) if len(t_on) > 8 else (np.zeros(0), np.zeros(0, int))
        return {"name": path.stem, "dur": float(z["dur"]), "t_on": t_on, "c_on": c_on, "rp_on": rp_on,
                "D_on": describe(L, t_on), "t_ref": t_ref, "c_ref": np.arange(len(t_ref)),
                "rp_ref": np.where(valid, ref_pos - 1, -1), "D_ref": describe(L, t_ref_s),
                "S_ref": beat_features(t_ref_s, L, cur), "ref_pos": ref_pos,
                "valid": valid, "bars": bars, "non4": non4, "lag": lag,
                "S_on": beat_features(t_on, L, cur) if len(t_on) else np.zeros((0, 22), np.float32),
                "t_link": t_link, "c_link": c_link, "rp_link": ref_positions(t_link, t_ref, ref_pos, valid),
                "D_link": describe(L, t_link), "S_link": beat_features(t_link, L, cur) if len(t_link) else
                np.zeros((0, 22), np.float32)}
    except Exception:
        return {"name": path.stem, "error": traceback.format_exc(limit=4)}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", required=True)
    ap.add_argument("--jobs", type=int, required=True)
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--ref-lag", type=float, default=0.0, help="s added to the reference beats before describing them")
    ap.add_argument("--limit", type=int)
    a = ap.parse_args()
    os.nice(19)
    from eval import data
    order = [p.name for p in (data.traktor() if a.dataset == "traktor" else data.gtzan())]
    files = [a.cache / a.dataset / f"{n}.npz" for n in order]
    files = [f for f in files if f.exists()][:a.limit]
    print(f"load {os.getloadavg()[0]:.1f}, {len(files)} tracks, {a.jobs} workers", flush=True)
    t0 = time.time()
    with ProcessPoolExecutor(a.jobs) as pool:
        rows = list(pool.map(one, [(f, a.ref_lag) for f in files], chunksize=2))
    for r in rows:
        if "error" in r:
            print("ERROR", r["name"], r["error"][-300:], flush=True)
    a.out.write_bytes(pickle.dumps({"dataset": a.dataset, "ref_lag": a.ref_lag, "rows": rows}))
    lags = [r["lag"] for r in rows if "lag" in r and np.isfinite(r["lag"])]
    print(f"done {len(rows)} in {time.time() - t0:.0f} s; online - reference lag median {1000 * np.median(lags):.1f} ms",
          flush=True)


if __name__ == "__main__":
    main()
