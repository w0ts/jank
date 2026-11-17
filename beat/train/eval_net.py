"""Run a beat net through the SAME downstream and save the beats like eval.run: runs/<dataset>.<i>of<n>_<tracker>.json.

  net -> activation (100 fps, from the prepared features, the C front end's) -> madmom's online DBN, 90-180 BPM
  (DBNBeatTrackingProcessor online, the other parameters madmom's defaults) -> est_beats.

    eval_net.py DATASET FEAT_DIR TRACKER NET SHARD NSHARDS [--names LIST.json]
NET: "madmom0" (madmom.models.BEATS_LSTM[0]) or a .pt file of ours (a state_dict, or a train.py checkpoint).
DATASET: traktor (test split only, from --names), gtzan or mixes.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT))
MIN_BPM, MAX_BPM, FPS = 90.0, 180.0, 100


def make_net(spec: str):
    """Returns f(feats (T, 162) float32) -> activation (T,) float32."""
    if spec == "madmom0":
        from madmom.ml.nn import NeuralNetwork
        from madmom.models import BEATS_LSTM
        net = NeuralNetwork.load(BEATS_LSTM[0])
        return lambda x: np.asarray(net.process(x, reset=True)).ravel()
    import torch

    from peeplstm import BeatNet
    torch.set_num_threads(1)
    s = torch.load(spec, weights_only=False)
    m = BeatNet()
    m.load_state_dict(s["model"] if "model" in s else s)
    m.eval()

    def run(x):
        with torch.no_grad():
            return m(torch.from_numpy(np.ascontiguousarray(x))[None])[0].numpy()
    return run


def main() -> None:
    ap = argparse.ArgumentParser()
    for k in ("dataset", "feat_dir", "tracker", "net"):
        ap.add_argument(k)
    ap.add_argument("shard", type=int)
    ap.add_argument("nshards", type=int)
    ap.add_argument("--names")
    a = ap.parse_args()
    from madmom.features.beats import DBNBeatTrackingProcessor
    net = make_net(a.net)
    names = json.loads(Path(a.names).read_text()) if a.names else None
    from eval.data import DATASETS
    todo = [p.name for p in DATASETS[a.dataset]() if names is None or p.name in names][a.shard::a.nshards]
    rows = []
    for name in todo:
        f = Path(a.feat_dir) / f"{name}.npy"
        if not f.exists():
            continue
        t0 = time.time()
        act = net(np.load(f)).astype(np.float32)
        dbn = DBNBeatTrackingProcessor(min_bpm=MIN_BPM, max_bpm=MAX_BPM, fps=FPS, online=True)
        beats = np.asarray(dbn.process_online(act, reset=True))
        rows.append({"name": name, "est_beats": [round(float(b), 3) for b in beats]})
        print(f"{name}: {len(act)} frames, {len(beats)} beats, {time.time() - t0:.1f} s", flush=True)
    out = ROOT / "runs" / f"{a.dataset}.{a.shard}of{a.nshards}_{a.tracker}.json"
    out.write_text(json.dumps({"summary": {}, "pieces": rows}))


if __name__ == "__main__":
    main()
