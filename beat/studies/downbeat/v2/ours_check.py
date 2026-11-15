"""The final rule on the beats of OUR beat network (models/beat_ours.pt, the one jank is to ship) instead of madmom's:
our net's activation from the cached spectrogram (its 162 inputs are the 81 log bands and their positive differences,
framed as the live tracker frames them), madmom's online DBN (BSD code) 90-180 BPM, then Link's beats (linkbeats.py)
and the rule from ../results/v2_weights.json. Test half of Traktor only.

    PYTHONPATH=..:$BL_REPO python ours_check.py --cache ../cache --net PATH/beat_ours.pt --jobs 6
"""
from __future__ import annotations

import argparse
import json
import os
import pickle
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
NET = {}


def _beats(job):
    path, net_path = job
    import torch
    from madmom.features.beats import DBNBeatTrackingProcessor

    from extract import beat_features, frame_curves
    from feats import describe
    from linkbeats import link_beats
    from rules import ref_positions, valid_4
    torch.set_num_threads(1)
    if "m" not in NET:
        sys.path.insert(0, str(Path(os.environ["BL_REPO"]) / "train"))
        from peeplstm import BeatNet
        s = torch.load(net_path, weights_only=False)
        m = BeatNet()
        m.load_state_dict(s["model"] if "model" in s else s)
        NET["m"] = m.eval()
    z = np.load(path)
    L = z["L"].astype(np.float32)
    x = np.concatenate([L, np.vstack([np.zeros((1, 81), np.float32), np.maximum(np.diff(L, axis=0), 0)])], axis=1)
    with torch.no_grad():
        act = NET["m"](torch.from_numpy(x)[None])[0].numpy().astype(np.float32)
    dbn = DBNBeatTrackingProcessor(min_bpm=90.0, max_bpm=180.0, fps=100, online=True)
    t_on = np.asarray(dbn.process_online(act, reset=True))
    t_link, c_link = link_beats(t_on) if len(t_on) > 8 else (np.zeros(0), np.zeros(0, int))
    t_ref, ref_pos = z["ref_beats"], z["ref_pos"]
    valid, _, _ = valid_4(ref_pos)
    return {"name": path.stem, "t_link": t_link, "c_link": c_link,
            "rp_link": ref_positions(t_link, t_ref, ref_pos, valid), "D_link": describe(L, t_link),
            "S_link": beat_features(t_link, L, frame_curves(L)) if len(t_link) else np.zeros((0, 22), np.float32)}


def main() -> None:
    import exp_linear as xl
    from analyze import HEAD, fmt, summarize
    from final import decide
    from rules import SKIP_BEATS, score
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--net", required=True)
    ap.add_argument("--jobs", type=int, default=6)
    a = ap.parse_args()
    from eval.data import traktor
    test = [p.name for p in traktor()][1::2]
    out = a.cache / "ours_test.pkl"
    if out.exists():
        rows = pickle.loads(out.read_bytes())
    else:
        with ProcessPoolExecutor(a.jobs) as pool:
            rows = list(pool.map(_beats, [(a.cache / "traktor" / f"{n}.npz", a.net) for n in test], chunksize=2))
        out.write_bytes(pickle.dumps(rows))
    w = json.loads((HERE.parent / "results" / "v2_weights.json").read_text())
    feats = "S" if len(w["features"]) == 22 else "S+Dp"
    v = xl.V("final", feats, tuple(w["bars"]), delay=5)
    res = []
    for r in rows:
        if len(r["t_link"]) < SKIP_BEATS + 4:
            continue
        rel = xl.rel_multi(xl.feat_matrix(r, "link", feats), r["c_link"], v.scales, v.delay)
        ready = ~np.isnan(rel).any(axis=(1, 2))
        sc = xl.rotations(np.nan_to_num(rel)) @ np.asarray(w["w"])
        res.append((r, score(r["t_link"], r["c_link"], decide(sc, ready, w["margin"]), r["rp_link"])))
    line = f"| final rule, Link beats from OUR beat net (beat_ours.pt), test half {fmt(summarize(res))}"
    print(HEAD + "\n" + line)
    (HERE.parent / "results" / "v2_ours_net.md").write_text(HEAD + "\n" + line + "\n")


if __name__ == "__main__":
    main()
