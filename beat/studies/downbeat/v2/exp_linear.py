"""The study's own linear rule (linear.py: |mean|-relative accumulators, softmax regression by L-BFGS), widened with the
new descriptors and several time scales; every variant 2-fold cross-fitted on the dev half, so the study's rule refitted
the same way is the row to beat.

    PYTHONPATH=..:$BL_REPO python exp_linear.py --cache ../cache --jobs 6 [--variants a,b,...] [--test NAME]
"""
from __future__ import annotations

import argparse
import json
import pickle
import time
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from analyze import HEAD, fmt, summarize
from feats import NAMES
from linear import rotations, train
from rules import SKIP_BEATS, Rule, position_scores, score

HERE = Path(__file__).resolve().parent
ROWS: dict[str, dict] = {}
POS_D = [i for i, n in enumerate(NAMES) if n[:2] not in ("dE",) and n != "tr"]   # positive descriptors as they are
KEY = {"online": "on", "ref": "ref", "link": "link"}


@dataclass(frozen=True)
class V:
    name: str
    feats: str                       # "S", "D", "S+D", "S+Dp" (D with dE as positive / negative parts, tr as |tr-1|)
    scales: tuple[int, ...]
    l2: float = 1e-2
    margin: float = 0.5
    delay: int = 1                   # beat j's features are used from beat j + delay on (1 = the first study's
                                     # protocol; 5 = causal: its nov_bar looks 4 beats ahead, its onsets 100 ms)


def feat_matrix(r: dict, beats: str, feats: str) -> np.ndarray:
    S, D = r[f"S_{KEY[beats]}"], r[f"D_{KEY[beats]}"]
    parts = []
    if "S" in feats.split("+"):
        parts.append(S)
    if "D" in feats.split("+"):
        parts.append(D[:, POS_D])
    if "Dp" in feats.split("+"):
        dE = D[:, [i for i, n in enumerate(NAMES) if n.startswith("dE")]]
        tr = D[:, [NAMES.index("tr")]]
        parts += [D[:, POS_D], np.maximum(dE, 0), np.maximum(-dE, 0), np.abs(tr - 1)]
    return np.nan_to_num(np.concatenate(parts, axis=1).astype(float))


def rel_multi(F: np.ndarray, c: np.ndarray, scales, delay: int = 1) -> np.ndarray:
    """The study's rel_scores (exp mode) for every column and scale: (n, K*S, 4), at beat i from beats <= i - delay."""
    out = []
    for N in scales:
        s = np.stack([position_scores(F[:, j], c, Rule("x", N, "exp")) for j in range(F.shape[1])], axis=1)
        m = s.mean(axis=2, keepdims=True)
        out.append((s - m) / (np.abs(m) + 1e-6))
    rel = np.concatenate(out, axis=1)
    if delay > 1:
        rel = np.concatenate([np.full((delay - 1, *rel.shape[1:]), np.nan), rel[:len(rel) - delay + 1]])
    return rel


def stream(r, beats):
    k = KEY[beats]
    return r[f"t_{k}"], r[f"c_{k}"], r[f"rp_{k}"]


def _prep(job):
    name, beats, v, every = job
    r = ROWS[name]
    t, c, rp = stream(r, beats)
    rel = rel_multi(feat_matrix(r, beats, v.feats), c, v.scales, v.delay)
    ok = (c >= SKIP_BEATS) & (rp >= 0) & ~np.isnan(rel).any(axis=(1, 2))
    idx = np.flatnonzero(ok)[::every]
    return rotations(rel[idx]), ((c[idx] - rp[idx]) % 4).astype(int)


def _decide(job):
    name, beats, v, w, margins = job
    r = ROWS[name]
    t, c, rp = stream(r, beats)
    rel = rel_multi(feat_matrix(r, beats, v.feats), c, v.scales, v.delay)
    ready = ~np.isnan(rel).any(axis=(1, 2))
    sc = rotations(np.nan_to_num(rel)) @ w
    out = {}
    for m in margins:
        off = np.full(len(c), -1)
        cur = -1
        for i in np.flatnonzero(ready):
            b = int(np.argmax(sc[i]))
            if cur < 0 or (b != cur and sc[i, b] - sc[i, cur] > m):
                cur = b
            off[i] = cur
        out[m] = score(t, c, off, rp)
    return name, out


class Lab:
    def __init__(self, jobs):
        self.pool = ProcessPoolExecutor(jobs)

    def fit(self, v, names, beats="online"):
        data = list(self.pool.map(_prep, [(n, b, v, 2) for b in beats.split("+") for n in names
                                          if len(stream(ROWS[n], b)[0]) >= SKIP_BEATS + 4], chunksize=4))
        return train([d for d in data if len(d[1])], l2=v.l2)

    def run(self, v, w, names, beats="online", margins=(0.0, 0.5, 1.0, 1.5, 2.0)):
        jobs = [(n, beats, v, w, margins) for n in names if len(stream(ROWS[n], beats)[0]) >= SKIP_BEATS + 4]
        res = dict(self.pool.map(_decide, jobs, chunksize=4))
        return {m: [(ROWS[n], res[n][m]) for n in res] for m in margins}

    def crossfit(self, v, dev, beats="online", only=None, train_beats=None):
        a, b = dev[0::2], dev[1::2]
        sub = (lambda x: x) if only is None else (lambda x: [n for n in x if n in only])
        tb = train_beats or beats
        ra = self.run(v, self.fit(v, sub(a), tb), b, beats)
        rb = self.run(v, self.fit(v, sub(b), tb), a, beats)
        return {m: ra[m] + rb[m] for m in ra}


VARIANTS = {
    "a": V("study rule refit: S, N=16", "S", (16,)),
    "b": V("S, N=8/32/128", "S", (8, 32, 128)),
    "c": V("S+D, N=16", "S+D", (16,)),
    "d": V("S+Dp, N=16", "S+Dp", (16,)),
    "e": V("S+Dp, N=8/32/128", "S+Dp", (8, 32, 128)),
    "f": V("S+Dp, N=16/64", "S+Dp", (16, 64)),
    "g": V("S+Dp, N=16, l2 0.1", "S+Dp", (16,), 0.1),
    "h": V("S+Dp, N=16/64, l2 0.1", "S+Dp", (16, 64), 0.1),
    "i": V("S, N=8", "S", (8,)),
    "j": V("S, N=24", "S", (24,)),
    "k": V("S, N=32", "S", (32,)),
    "l": V("S+Dp, N=24", "S+Dp", (24,)),
    "m": V("S, N=16, l2 0.001", "S", (16,), 1e-3),
    "n": V("S+Dp, N=16, l2 0.1", "S+Dp", (16,), 0.1),
    "o": V("Dp, N=16", "Dp", (16,)),
    "a5": V("S, N=16, causal (delay 5)", "S", (16,), delay=5),
    "d5": V("S+Dp, N=16, causal (delay 5)", "S+Dp", (16,), delay=5),
    "i5": V("S, N=8, causal (delay 5)", "S", (8,), delay=5),
}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--variants", default=",".join(VARIANTS))
    ap.add_argument("--beats", default="online", choices=list(KEY), help="the beats scored")
    ap.add_argument("--train-beats", default="", help="the beats trained on, e.g. online+ref (default: --beats)")
    ap.add_argument("--out", type=Path, default=HERE.parent / "results" / "v2_linear.md")
    a = ap.parse_args()
    rows = pickle.loads((a.cache / "traktor_desc.pkl").read_bytes())["rows"]
    for r in rows:
        if "error" not in r:
            ROWS[r["name"]] = r
    names = [r["name"] for r in rows if "error" not in r]
    dev = names[0::2]
    lab = Lab(a.jobs)
    lines = [f"Dev half ({len(dev)} tracks), {a.beats} beats, 2-fold cross-fitted (each half scored by weights fitted on "
             f"the other).\n", HEAD]
    for k in a.variants.split(","):
        v = VARIANTS[k]
        t0 = time.time()
        res = lab.crossfit(v, dev, a.beats, train_beats=a.train_beats or None)
        for m, rr in res.items():
            lines.append(f"| {k}: {v.name}, margin {m:g} {fmt(summarize(rr))}")
            print(lines[-1], f"({time.time() - t0:.0f} s)", flush=True)
    a.out.write_text("\n".join(lines) + "\n")
    print(json.dumps({"done": True}))


if __name__ == "__main__":
    main()
