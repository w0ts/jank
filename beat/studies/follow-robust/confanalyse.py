"""Reads confprobe.py's npz files: the confidence's histogram and speed, and how well each candidate predicts "right".

    python studies/follow-robust/confanalyse.py DIR --out results/confidence.md
Ticks counted: playing and running (the output is on), with a known reference. "right" = within 1 % of the reference,
"wrong" = off by the study's WRONG_REL (score.py); in between is left out of the AUC.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BEAT = HERE.parents[1]
CANDS = ("conf", "strict", "loose", "tempo", "phase", "dbn", "soft", "jit", "pk")
BINS = np.linspace(0, 1, 6)


def auc(score: np.ndarray, pos: np.ndarray) -> float:
    """P(score of a right tick > score of a wrong tick), ties half (rank-sum)."""
    n1, n0 = int(pos.sum()), int((~pos).sum())
    if n1 == 0 or n0 == 0:
        return float("nan")
    order = np.argsort(score, kind="mergesort")
    s = score[order]
    ranks = np.empty(score.size)
    i = 0
    while i < s.size:
        j = i
        while j + 1 < s.size and s[j + 1] == s[i]:
            j += 1
        ranks[order[i:j + 1]] = (i + j) / 2.0 + 1.0
        i = j + 1
    return float((ranks[pos].sum() - n1 * (n1 + 1) / 2.0) / (n1 * n0))


def load(d: Path, config: str, split: dict[str, str]) -> dict[str, dict[str, np.ndarray]]:
    out: dict[str, dict[str, list]] = {"dev": {}, "test": {}}
    for f in sorted(d.glob(f"*.{config}.npz")):
        name = f.name[: -len(f".{config}.npz")]
        z = np.load(f)
        keys = [k for k in z.files]
        for sp in (split.get(name, "dev"),):
            for k in keys:
                out[sp].setdefault(k, []).append(z[k])
            out[sp].setdefault("_track", []).append(np.full(z["t"].size, len(out[sp]["t"]) - 1))
    return {sp: {k: np.concatenate(v) for k, v in dd.items()} for sp, dd in out.items() if dd}


def hist_rows(x: np.ndarray) -> list[str]:
    h, _ = np.histogram(np.clip(x, 0, 1), bins=BINS)
    share = h / max(h.sum(), 1)
    return [f"{100 * v:.0f} %" for v in share]


def speed(d: dict, cand: str) -> tuple[float, float]:
    """Share of consecutive running ticks that move by more than 0.3 / the mean |step| per second (tick 0.1 s)."""
    x, run, tr = d[cand], d["running"] & d["playing"], d["_track"]
    ok = run[1:] & run[:-1] & (tr[1:] == tr[:-1])
    step = np.abs(np.diff(x))[ok]
    return float((step > 0.3).mean()) if step.size else float("nan"), float(step.mean() * 10) if step.size else float("nan")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dir", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--configs", nargs="+", default=["fw", "all-d-f60"])
    a = ap.parse_args()
    index = json.loads((BEAT / "data" / "traktor" / "index.json").read_text())
    split = {r["name"]: ("dev" if i % 2 == 0 else "test") for i, r in enumerate(index)}
    lines = ["# The confidence: how graded is it, and what predicts a right output", ""]
    for cfg in a.configs:
        data = load(a.dir, cfg, split)
        both = {k: np.concatenate([data[s][k] for s in data]) for k in data["dev"]} if "test" in data and "dev" in data else data[next(iter(data))]
        both["_track"] = np.concatenate([data[s]["_track"] + (0 if s == "dev" else 10**6) for s in data])
        m = both["running"] & both["playing"]
        lines += [f"## {cfg}: ticks with the output running ({int(m.sum())} ticks, {int((both['playing']).sum())} playing)", "",
                  "| value | 0-20 | 20-40 | 40-60 | 60-80 | 80-100 | share < 0.05 | share > 0.95 | steps > 0.3 | mean step / s |",
                  "|---|---|---|---|---|---|---|---|---|---|"]
        for c in ("conf", "strict", "loose", "tempo", "phase", "dbn", "soft", "jit", "pk"):
            x = both[c][m]
            if not np.any(x):
                continue
            big, mean = speed(both, c)
            lines.append(f"| {c} | " + " | ".join(hist_rows(x)) + f" | {100 * (x < 0.05).mean():.0f} % | "
                         f"{100 * (x > 0.95).mean():.0f} % | {100 * big:.1f} % | {mean:.3f} |")
        lines.append("")
        lines += ["AUC for right vs wrong (running, playing, known reference), per split; higher = the value tells right from wrong.", "",
                  "| value | dev AUC | test AUC |", "|---|---|---|"]
        for c in CANDS:
            row = []
            for sp in ("dev", "test"):
                d = data.get(sp)
                if d is None or not np.any(d[c]):
                    row.append("-")
                    continue
                mm = d["running"] & d["playing"] & (d["right"] | d["wrong"])
                row.append(f"{auc(d[c][mm][::3], d['right'][mm][::3]):.3f}")
            lines.append(f"| {c} | {row[0]} | {row[1]} |")
        lines.append("")
    a.out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
