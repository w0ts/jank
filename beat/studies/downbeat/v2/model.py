"""The per-beat model: from the normalised descriptors of beats j-P..j+R, the log-probabilities of beat j's position in
its bar (0 = the "1" .. 3). Kinds:

    lin   4-way softmax regression on the window                               (F*(P+R+1)+1)*4 weights
    mlp   one hidden layer (ReLU) then the 4-way softmax                        window -> H -> 4
    gru   a GRU over the beats' descriptors; its output at beat j+R scores beat j  (sees the whole past)

Trained on the dev half's beats with a reference position (cross-entropy), torch on the CPU.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import torch
from torch import nn


@dataclass(frozen=True)
class Spec:
    kind: str = "mlp"
    past: int = 4             # P
    ahead: int = 1            # R
    hidden: int = 32
    epochs: int = 30
    lr: float = 3e-3
    wd: float = 1e-4
    cols: tuple[int, ...] | None = None    # descriptor columns used (None = all)

    @property
    def delay(self) -> int:
        """Beats from beat j to the first decision that may use its evidence (descriptors ready + one beat spare)."""
        return self.ahead + 3

    @property
    def label(self) -> str:
        h = f" H={self.hidden}" if self.kind != "lin" else ""
        return f"{self.kind}{h} P={self.past} R={self.ahead}"


def windows(D: np.ndarray, past: int, ahead: int) -> np.ndarray:
    """(n, (past+ahead+1)*F): rows j-past..j+ahead, zeros outside the track."""
    n, f = D.shape
    pad = np.vstack([np.zeros((past, f), D.dtype), D, np.zeros((ahead, f), D.dtype)])
    return np.concatenate([pad[o:o + n] for o in range(past + ahead + 1)], axis=1)


class GRUNet(nn.Module):
    def __init__(self, nin: int, hidden: int):
        super().__init__()
        self.gru = nn.GRU(nin, hidden, batch_first=True)
        self.out = nn.Linear(hidden, 4)

    def forward(self, x):
        h, _ = self.gru(x)
        return self.out(h)


def build(spec: Spec, nin: int) -> nn.Module:
    if spec.kind == "lin":
        return nn.Linear(nin * (spec.past + spec.ahead + 1), 4)
    if spec.kind == "mlp":
        return nn.Sequential(nn.Linear(nin * (spec.past + spec.ahead + 1), spec.hidden), nn.ReLU(),
                             nn.Linear(spec.hidden, 4))
    return GRUNet(nin, spec.hidden)


def _cols(D: np.ndarray, spec: Spec) -> np.ndarray:
    return D if spec.cols is None else D[:, list(spec.cols)]


def train(spec: Spec, tracks: list[tuple[np.ndarray, np.ndarray]], seed: int = 0) -> nn.Module:
    """tracks: (normalised descriptors (n, F), positions (n,), -1 = no label)."""
    torch.manual_seed(seed)
    rng = np.random.default_rng(seed)
    nin = _cols(tracks[0][0], spec).shape[1]
    net = build(spec, nin)
    opt = torch.optim.AdamW(net.parameters(), lr=spec.lr, weight_decay=spec.wd)
    lossf = nn.CrossEntropyLoss(ignore_index=-1)
    if spec.kind in ("lin", "mlp"):
        X = np.concatenate([windows(_cols(D, spec), spec.past, spec.ahead) for D, _ in tracks])
        y = np.concatenate([p for _, p in tracks])
        keep = y >= 0
        X, y = torch.from_numpy(X[keep].astype(np.float32)), torch.from_numpy(y[keep].astype(np.int64))
        bs = 1024
        for _ in range(spec.epochs):
            perm = torch.from_numpy(rng.permutation(len(y)))
            for s in range(0, len(y), bs):
                b = perm[s:s + bs]
                opt.zero_grad()
                lossf(net(X[b]), y[b]).backward()
                opt.step()
        return net.eval()
    # GRU: chunks of 128 beats; the target of step j+R is beat j's position
    seqs, tgts = [], []
    for D, p in tracks:
        Dc = _cols(D, spec).astype(np.float32)
        tg = np.full(len(p), -1)
        if len(p) > spec.ahead:
            tg[spec.ahead:] = p[:len(p) - spec.ahead]
        for s in range(0, len(p), 96):
            e = min(len(p), s + 128)
            if e - s < 16:
                continue
            x = np.zeros((128, Dc.shape[1]), np.float32)
            t = np.full(128, -1)
            x[:e - s], t[:e - s] = Dc[s:e], tg[s:e]
            seqs.append(x)
            tgts.append(t)
    X, Y = torch.from_numpy(np.stack(seqs)), torch.from_numpy(np.stack(tgts).astype(np.int64))
    bs = 64
    for _ in range(spec.epochs):
        perm = torch.from_numpy(rng.permutation(len(X)))
        for s in range(0, len(X), bs):
            b = perm[s:s + bs]
            opt.zero_grad()
            lossf(net(X[b]).reshape(-1, 4), Y[b].reshape(-1)).backward()
            opt.step()
    return net.eval()


def logprobs(spec: Spec, net: nn.Module, D: np.ndarray) -> np.ndarray:
    """(n, 4) log-probabilities of each beat's bar position."""
    with torch.no_grad():
        if spec.kind in ("lin", "mlp"):
            z = net(torch.from_numpy(windows(_cols(D, spec), spec.past, spec.ahead).astype(np.float32)))
            return torch.log_softmax(z, -1).numpy()
        z = net(torch.from_numpy(_cols(D, spec).astype(np.float32))[None])[0]
        lp = torch.log_softmax(z, -1).numpy()
        out = np.full((len(D), 4), np.nan)
        out[:len(D) - spec.ahead] = lp[spec.ahead:]
        return out


def n_params(net: nn.Module) -> int:
    return sum(p.numel() for p in net.parameters())
