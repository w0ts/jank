"""The PyTorch BeatNet with madmom's BEATS_LSTM[0] weights must reproduce madmom's activations (same equations).

    .venv/bin/python -I train/check_equiv.py FEATS.npy...
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parent))


def main() -> None:
    from convert import madmom_to_arrays
    from madmom.ml.nn import NeuralNetwork
    from madmom.models import BEATS_LSTM
    from peeplstm import BeatNet
    net = NeuralNetwork.load(BEATS_LSTM[0])
    arr = madmom_to_arrays(net)
    m = BeatNet().double()
    with torch.no_grad():
        for i in range(3):
            for k in "wrbp":
                getattr(m.layers[i], k).copy_(torch.from_numpy(arr[i][k].astype(np.float64)))
        m.out.weight.copy_(torch.from_numpy(arr[3]["out_w"].astype(np.float64))[None])
        m.out.bias.copy_(torch.from_numpy(arr[3]["out_b"].astype(np.float64)))
    for f in sys.argv[1:]:
        x = np.load(f)[:3000]
        ref = np.asarray(net.process(x, reset=True)).ravel()
        with torch.no_grad():
            got = m(torch.from_numpy(x.astype(np.float64))[None])[0].numpy()
        print(f"{Path(f).name}: max |torch - madmom| = {np.abs(got - ref).max():.2e} over {len(ref)} frames")


if __name__ == "__main__":
    main()
