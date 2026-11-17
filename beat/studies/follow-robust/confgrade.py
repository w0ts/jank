"""A graded confidence: a logistic combination of the probe's per-tick values that predicts "the output is right".

    python studies/follow-robust/confgrade.py data/followrobust/conf --out results/confidence_graded.md
Fit on the dev half's running ticks (right vs wrong), judged on the test half: AUC, the calibration (the share right in each
band of the value) and the histogram of the value. Weights are written to the report for the port.
"""
from __future__ import annotations

import argparse
import itertools
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BEAT = HERE.parents[1]
sys.path.insert(0, str(HERE))

from confanalyse import auc, load

FEATURE_SETS = {
    "gate (agreement mean)": ("conf",),
    "soft": ("soft",),
    "soft+jit": ("soft", "jit"),
    "soft+jit+tempo+phase": ("soft", "jit", "tempo", "phase"),
    "soft+jit+tempo+phase+dbn": ("soft", "jit", "tempo", "phase", "dbn"),
    "all": ("conf", "strict", "loose", "tempo", "phase", "dbn", "soft", "jit", "pk"),
}


def design(d: dict, feats: tuple[str, ...], mask: np.ndarray, step: int = 3) -> tuple[np.ndarray, np.ndarray]:
    idx = np.where(mask)[0][::step]
    x = np.stack([d[f][idx] for f in feats], axis=1).astype(np.float64)
    return x, d["right"][idx].astype(np.float64)


def fit_logistic(x: np.ndarray, y: np.ndarray, iters: int = 400, lr: float = 0.5, l2: float = 1e-3) -> np.ndarray:
    """Plain gradient descent on the mean log-loss: weights [b, w1..wn] on features in 0..1 (balanced classes)."""
    xb = np.hstack([np.ones((x.shape[0], 1)), x])
    w = np.zeros(xb.shape[1])
    pos, neg = y.mean(), 1 - y.mean()
    sw = np.where(y > 0.5, 0.5 / pos, 0.5 / neg)              # equal weight to right and wrong ticks
    for _ in range(iters):
        p = 1 / (1 + np.exp(-xb @ w))
        g = xb.T @ (sw * (p - y)) / sw.sum() + l2 * w
        w -= lr * g * 4
    return w


def predict(w: np.ndarray, x: np.ndarray) -> np.ndarray:
    return 1 / (1 + np.exp(-(w[0] + x @ w[1:])))


def bands(p: np.ndarray, y: np.ndarray) -> list[str]:
    out = []
    for lo in np.arange(0, 1, 0.2):
        m = (p >= lo) & (p < lo + 0.2 + (1e-9 if lo > 0.7 else 0))
        out.append(f"{int(m.sum()):7d} ticks, {100 * y[m].mean():3.0f} % right" if m.any() else "       none")
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dir", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--config", default="all-d-f60")
    a = ap.parse_args()
    index = json.loads((BEAT / "data" / "traktor" / "index.json").read_text())
    split = {r["name"]: ("dev" if i % 2 == 0 else "test") for i, r in enumerate(index)}
    data = load(a.dir, a.config, split)

    def usable(d: dict) -> np.ndarray:
        return d["running"] & d["playing"] & (d["right"] | d["wrong"])

    lines = [f"# A graded confidence ({a.config})", "",
             "Fit on the dev half, judged on the test half: running ticks, right (within 1 %) against wrong tempo. Balanced classes in the fit.", "",
             "| value | dev AUC | test AUC | test: share of ticks in 0-20 / 20-40 / 40-60 / 60-80 / 80-100 % |", "|---|---|---|---|"]
    best = None
    for name, feats in FEATURE_SETS.items():
        xd, yd = design(data["dev"], feats, usable(data["dev"]))
        xt, yt = design(data["test"], feats, usable(data["test"]))
        if feats == ("conf",):
            pd_, pt = xd[:, 0], xt[:, 0]
            w = None
        else:
            w = fit_logistic(xd, yd)
            pd_, pt = predict(w, xd), predict(w, xt)
        h, _ = np.histogram(np.clip(pt, 0, 1), bins=np.linspace(0, 1, 6))
        lines.append(f"| {name} | {auc(pd_, yd > 0.5):.3f} | {auc(pt, yt > 0.5):.3f} | " + " / ".join(f"{100 * v / h.sum():.0f}" for v in h) + " |")
        if w is not None and name == "soft+jit+tempo+phase+dbn":
            best = (feats, w, pt, yt)
    feats, w, pt, yt = best
    lines += ["", f"## Calibration of soft+jit+tempo+phase+dbn on the test half (the share right per band of the value)", ""]
    lines += [f"- {int(lo * 100)}-{int(lo * 100 + 20)} %: {t}" for lo, t in zip(np.arange(0, 1, 0.2), bands(pt, yt))]
    lines += ["", "Weights (p = 1 / (1 + exp(-(b + sum w_i x_i)))), x in 0..1:", "", f"- b = {w[0]:.4f}"]
    lines += [f"- w_{f} = {v:.4f}" for f, v in zip(feats, w[1:])]
    a.out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
