"""Weights between madmom's net, our PyTorch BeatNet and the C export. Layout: w[gate][in][25], r[gate][prev][25],
b[gate][25], p[3][25]; gates input, forget, cell, output (madmom's order, the C core's)."""
from __future__ import annotations

import numpy as np

GATES = ("input_gate", "forget_gate", "cell", "output_gate")


def madmom_to_arrays(net) -> list[dict]:
    """madmom NeuralNetwork (3 LSTM layers + feed-forward) -> per-layer arrays plus the output layer."""
    lstm, ff = net.layers[:3], net.layers[3]
    layers = []
    for layer in lstm:
        g = [getattr(layer, n) for n in GATES]
        layers.append({"w": np.stack([x.weights for x in g]), "r": np.stack([x.recurrent_weights for x in g]),
                       "b": np.stack([x.bias for x in g]),
                       "p": np.stack([g[0].peephole_weights, g[1].peephole_weights, g[3].peephole_weights])})
    layers.append({"out_w": np.asarray(ff.weights).reshape(-1), "out_b": np.asarray(ff.bias).reshape(-1)})
    return layers


def torch_to_arrays(model, fold_norm: bool = True) -> list[dict]:
    """BeatNet -> the same arrays. The input normalisation (x - mean) / std is folded into layer 0 (exact, the gates
    are linear in x): W' = W / std, b' = b - sum_i mean_i / std_i W_i."""
    sd = {k: v.detach().cpu().numpy().astype(np.float64) for k, v in model.state_dict().items()}
    layers = []
    for i in range(3):
        w, r, b, p = (sd[f"layers.{i}.{n}"] for n in "wrbp")
        if i == 0 and fold_norm:
            mean, std = sd["mean"], sd["std"]
            b = b - np.einsum("i,gij->gj", mean / std, w)
            w = w / std[None, :, None]
        layers.append({k: v.astype(np.float32) for k, v in (("w", w), ("r", r), ("b", b), ("p", p))})
    layers.append({"out_w": sd["out.weight"].reshape(-1).astype(np.float32),
                   "out_b": sd["out.bias"].reshape(-1).astype(np.float32)})
    return layers
