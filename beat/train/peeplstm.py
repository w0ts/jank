"""madmom's beat network in PyTorch: 3 unidirectional LSTM layers of 25 units WITH peepholes, a 25 -> 1 sigmoid output.

The equations are madmom's LSTMLayer.activate and the C core's layer_step (core/beat/jb_lstm.c), per frame:

    ig = sigmoid(x W_i + h R_i + b_i + c_prev * p_i)
    fg = sigmoid(x W_f + h R_f + b_f + c_prev * p_f)
    c  = tanh(x W_c + h R_c + b_c) * ig + c_prev * fg
    og = sigmoid(x W_o + h R_o + b_o + c * p_o)
    h  = tanh(c) * og

Gate order input, forget, cell, output. Parameter layout as the C export: w[4][n_in][H], r[4][H][H], b[4][H],
p[3][H] (peepholes of input, forget and output gate). Initial states are zero.
"""
from __future__ import annotations

import math

import torch
from torch import nn

H = 25
N_IN = 162


@torch.jit.script
def _run_layer(xw: torch.Tensor, r: torch.Tensor, p: torch.Tensor, h: torch.Tensor, c: torch.Tensor):
    """xw: (B, T, 4, H) = x W + b for every frame; r: (H, 4H); p: (3, H). Returns (outputs (B, T, H), h, c)."""
    outs = []
    for xt in xw.unbind(1):          # (indexing xw[:, t] would make the backward O(T^2))
        g = xt + (h @ r).view(-1, 4, h.size(1))
        ig = torch.sigmoid(g[:, 0] + c * p[0])
        fg = torch.sigmoid(g[:, 1] + c * p[1])
        c = torch.tanh(g[:, 2]) * ig + c * fg
        og = torch.sigmoid(g[:, 3] + c * p[2])
        h = torch.tanh(c) * og
        outs.append(h)
    return torch.stack(outs, 1), h, c


class PeepLayer(nn.Module):
    def __init__(self, n_in: int):
        super().__init__()
        k = 1.0 / math.sqrt(H)
        self.w = nn.Parameter(torch.empty(4, n_in, H).uniform_(-k, k))
        self.r = nn.Parameter(torch.empty(4, H, H).uniform_(-k, k))
        self.b = nn.Parameter(torch.zeros(4, H))
        self.p = nn.Parameter(torch.empty(3, H).uniform_(-k, k))
        with torch.no_grad():
            self.b[1].fill_(1.0)                       # forget gate open at the start

    def forward(self, x: torch.Tensor, state=None):
        bsz = x.size(0)
        h, c = state if state is not None else (x.new_zeros(bsz, H), x.new_zeros(bsz, H))
        xw = torch.einsum("bti,gij->btgj", x, self.w) + self.b
        r = self.r.permute(1, 0, 2).reshape(H, 4 * H)   # (prev, gate*H + j)
        out, h, c = _run_layer(xw, r, self.p, h, c)
        return out, (h, c)


class BeatNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.layers = nn.ModuleList([PeepLayer(N_IN), PeepLayer(H), PeepLayer(H)])
        self.out = nn.Linear(H, 1)
        self.register_buffer("mean", torch.zeros(N_IN))
        self.register_buffer("std", torch.ones(N_IN))

    def logits(self, feats: torch.Tensor, states=None):
        """feats (B, T, 162) raw features; returns logits (B, T) and the layers' states."""
        x = (feats - self.mean) / self.std
        new = []
        for i, layer in enumerate(self.layers):
            x, s = layer(x, None if states is None else states[i])
            new.append(s)
        return self.out(x).squeeze(-1), new

    def forward(self, feats: torch.Tensor):
        return torch.sigmoid(self.logits(feats)[0])
