"""The accumulator model: the first study's structure, widened.

Per beat, K features (the study's 22 and/or feats.py's 29 descriptors) are z-scored causally (feats.running_norm).
Each feature keeps one exponential accumulator per beat-count position (count mod 4) at each of S time scales
(N bars); an accumulator moves only on the beats of its position. relative = accumulator - its mean over the 4
positions. For a candidate "1" at position r, the scorer g sees relative[f, s, (r + p) mod 4] for every feature f,
scale s and bar position p (K*S*4 numbers) and returns one score; the same g for all 4 candidates, softmax over them.

    g = linear                 K*S*4 weights
    g = MLP, one hidden layer  K*S*4*H + 2H + 1 weights

The study's own rule is the linear g with its 22 features, one scale (16 bars), its |mean| scaling instead of the z-score.
On the device: K*S*4 accumulators, updated K*S at a beat; then 4 scorer passes per beat.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import torch
from scipy.signal import lfilter
from torch import nn

from feats import running_norm
from rules import SKIP_BEATS


@dataclass(frozen=True)
class AccSpec:
    feats: str = "S+D"                # "S" = the study's 22, "D" = the 29 descriptors, "S+D"
    scales: tuple[int, ...] = (4, 16, 64)
    hidden: int = 0                   # 0 = linear scorer
    epochs: int = 12
    lr: float = 2e-3
    wd: float = 1e-3
    tau: float = 64.0                 # the z-score's memory, beats
    drop: tuple[str, ...] = ()        # descriptor names left out (ablation)

    @property
    def label(self) -> str:
        g = f"MLP H={self.hidden}" if self.hidden else "linear"
        d = f" without {'/'.join(self.drop)}" if self.drop else ""
        return f"acc [{self.feats}{d}] N={'/'.join(map(str, self.scales))} bars, {g}"


def features(S: np.ndarray, D: np.ndarray, spec: AccSpec, names: tuple[str, ...]) -> np.ndarray:
    parts = []
    if "S" in spec.feats.split("+"):
        parts.append(S)
    if "D" in spec.feats.split("+"):
        keep = [i for i, n in enumerate(names) if n not in spec.drop]
        parts.append(D[:, keep])
    F = np.nan_to_num(np.concatenate(parts, axis=1).astype(np.float64))     # beats past the audio end: 0
    return running_norm(F, spec.tau) if len(F) else F


def relative(Z: np.ndarray, c: np.ndarray, scales: tuple[int, ...]) -> np.ndarray:
    """(n, K*S, 4): per position, the accumulator from the beats BEFORE beat i, minus its mean over positions; nan until
    every position has had a beat."""
    n, K = Z.shape
    out = np.full((n, K * len(scales), 4), np.nan)
    i = np.arange(n)
    for q in range(4):
        idx = np.flatnonzero(c % 4 == q)
        k = np.searchsorted(idx, i)               # how many beats of position q come before beat i
        if len(idx) == 0:
            continue
        for si, N in enumerate(scales):
            a = np.exp(-1.0 / N)
            acc = lfilter([1.0 - a], [1.0, -a], Z[idx], axis=0)        # (len(idx), K), normalised gain
            padded = np.vstack([np.full((1, K), np.nan), acc])
            out[:, si * K:(si + 1) * K, q] = padded[k]
    return out - out.mean(axis=2, keepdims=True)


def rotations(rel: np.ndarray) -> np.ndarray:
    n, m, _ = rel.shape
    out = np.empty((n, 4, m * 4), np.float32)
    for r in range(4):
        out[:, r, :] = rel[:, :, (r + np.arange(4)) % 4].reshape(n, m * 4)
    return out


class Scorer(nn.Module):
    def __init__(self, nin: int, hidden: int):
        super().__init__()
        self.g = nn.Linear(nin, 1, bias=False) if hidden == 0 else nn.Sequential(
            nn.Linear(nin, hidden), nn.ReLU(), nn.Linear(hidden, 1))

    def forward(self, x):                 # (B, 4, nin) -> (B, 4)
        return self.g(x).squeeze(-1)


def prepare(S, D, c, rp, spec: AccSpec, names) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """(X (n, 4, nin), y (n,): the true rotation, -1 where unknown or not scored, ready (n,): every position heard)."""
    rel = relative(features(S, D, spec, names), c, spec.scales)
    ready = ~np.isnan(rel).any(axis=(1, 2))
    y = np.where((c >= SKIP_BEATS) & (rp >= 0) & ready, (c - rp) % 4, -1)
    return rotations(np.nan_to_num(rel)), y, ready


def train(spec: AccSpec, data: list[tuple[np.ndarray, np.ndarray]], seed: int = 0, every: int = 2) -> Scorer:
    torch.manual_seed(seed)
    rng = np.random.default_rng(seed)
    X = np.concatenate([x[y >= 0][::every] for x, y in data])
    Y = np.concatenate([y[y >= 0][::every] for _, y in data])
    X, Y = torch.from_numpy(X), torch.from_numpy(Y.astype(np.int64))
    net = Scorer(X.shape[2], spec.hidden)
    opt = torch.optim.AdamW(net.parameters(), lr=spec.lr, weight_decay=spec.wd)
    lossf = nn.CrossEntropyLoss()
    bs = 512
    for _ in range(spec.epochs):
        perm = torch.from_numpy(rng.permutation(len(Y)))
        for s in range(0, len(Y), bs):
            b = perm[s:s + bs]
            opt.zero_grad()
            lossf(net(X[b]), Y[b]).backward()
            opt.step()
    return net.eval()


def scores(net: Scorer, X: np.ndarray) -> np.ndarray:
    with torch.no_grad():
        return net(torch.from_numpy(X)).numpy().astype(np.float64)


def decide(sc: np.ndarray, ready: np.ndarray, margin: float) -> np.ndarray:
    """The study's hysteresis (linear.decide_linear): switch only when the new best beats the current by `margin`."""
    off = np.full(len(sc), -1)
    cur = -1
    for i in np.flatnonzero(ready):
        b = int(np.argmax(sc[i]))
        if cur < 0 or (b != cur and sc[i, b] - sc[i, cur] > margin):
            cur = b
        off[i] = cur
    return off


def softmax(sc: np.ndarray) -> np.ndarray:
    z = np.exp(sc - sc.max(axis=1, keepdims=True))
    return z / z.sum(axis=1, keepdims=True)


def n_params(net: Scorer) -> int:
    return sum(p.numel() for p in net.parameters())
