"""Can the audio itself tell the new playback rate, at once? A varispeed step moves every partial by the tempo ratio, so
the log-frequency spectrum of the last fraction of a second, matched against the remembered spectrum of the seconds
before, gives the ratio as a shift, without waiting for beats.

  spectrum  |FFT| of the tracker's own frames (2048, hop 441), resampled to BINS_PER_OCT bins an octave from F_LO to
            F_HI, log(1 + x); blocks of 0.1 s averaged
  match     the mean of the last CUR_S against the mean of the REF_S before it (GAP_S between): correlation of the
            mean-removed spectra at shifts within +-MAX_REL, the peak refined by a parabola -> ratio = 2^(shift / BPO)

Per case (a step at t0, or no step: the control), the estimate every 0.1 s from t0 - 20 s to t0 + 4 s:
  before    |ratio - 1| on steady music (what a threshold must stay above: false triggers)
  after     the error |ratio - true| at 0.1 .. 2 s after the step

    python studies/advanced-follow/ratio_probe.py --cases 40 --changes 0 0.01 0.02 0.03 0.05 0.08 --jobs 12 --out F.jsonl
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import json
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1]))

import material as M
import step_stats as S

SR, FRAME, HOP = 44100, 2048, 441
BINS_PER_OCT = 96
F_LO, F_HI = 300.0, 8000.0
MAX_REL = 0.10
CUR_S, GAP_S, REF_S = 0.5, 0.0, 4.0
BLOCK = 10


def logfreq_blocks(y: np.ndarray) -> np.ndarray:
    """(n_blocks, n_bins) mean log(1 + |FFT|) on a log-frequency axis, 0.1 s blocks (block j ends at (j + 1) * 0.1 s)."""
    n_oct = np.log2(F_HI / F_LO)
    freqs = F_LO * 2 ** (np.arange(int(n_oct * BINS_PER_OCT)) / BINS_PER_OCT)
    fft_f = np.fft.rfftfreq(FRAME, 1 / SR)
    win = np.hanning(FRAME).astype(np.float32)
    pad = np.concatenate([np.zeros(FRAME - 1, np.float32), y])
    n = len(y) // HOP
    out = np.empty((n, freqs.size), np.float32)
    for s in range(0, n, 2048):
        ks = np.arange(s, min(n, s + 2048))
        idx = ks[:, None] * HOP + np.arange(FRAME)[None, :]
        mag = np.abs(np.fft.rfft(pad[idx] * win, axis=1))
        lin = np.array([np.interp(freqs, fft_f, m) for m in mag])
        out[ks] = np.log1p(lin)
    nb = n // BLOCK
    return out[:nb * BLOCK].reshape(nb, BLOCK, -1).mean(axis=1)


def match_ratio(ref: np.ndarray, cur: np.ndarray) -> tuple[float, float]:
    """(ratio of cur against ref, peak correlation). cur at ratio r = ref shifted up by log2(r) * BPO bins."""
    max_shift = int(np.ceil(np.log2(1 + MAX_REL) * BINS_PER_OCT))
    a = ref - ref.mean()
    b = cur - cur.mean()
    n = a.size
    corr = np.empty(2 * max_shift + 1)
    for i, s in enumerate(range(-max_shift, max_shift + 1)):
        if s >= 0:
            x, z = a[:n - s], b[s:]
        else:
            x, z = a[-s:], b[:n + s]
        corr[i] = float(x @ z / (np.linalg.norm(x) * np.linalg.norm(z) + 1e-12))
    k = int(np.argmax(corr))
    frac = 0.0
    if 0 < k < corr.size - 1:
        d = corr[k - 1] - 2 * corr[k] + corr[k + 1]
        frac = 0.5 * (corr[k - 1] - corr[k + 1]) / d if d < 0 else 0.0
    return float(2 ** ((k - max_shift + frac) / BINS_PER_OCT)), float(corr[k])


def estimates(blocks: np.ndarray, t_from: float, t_to: float) -> list[tuple[float, float, float]]:
    """(time, ratio, peak) every block between t_from and t_to: the last CUR_S against the REF_S before it."""
    cur_n, gap_n, ref_n = int(CUR_S * 10), int(GAP_S * 10), int(REF_S * 10)
    out = []
    for j in range(int(t_from * 10), min(int(t_to * 10), blocks.shape[0])):
        lo = j - cur_n - gap_n - ref_n
        if lo < 0:
            continue
        ref = blocks[lo:lo + ref_n].mean(axis=0)
        cur = blocks[j - cur_n:j].mean(axis=0)
        r, peak = match_ratio(ref, cur)
        out.append(((j) / 10.0, r, peak))
    return out


EVENT_TH = 0.006        # |ratio - 1| past this on CONFIRM estimates in a row (0.1 s apart), agreeing: a rate step
CONFIRM = 2
AGREE = 0.003
REFRACTORY_S = REF_S + CUR_S     # until the reference window holds only the new rate
DETECT_LAG_S = 0.50              # an event comes this long after the step (median over 1 150 steps of 1-8 %: 0.50 s)


def events(est: list) -> list[tuple[float, float]]:
    """(time, ratio) of the rate steps in a stream of (time, ratio) estimates (causal: decided at the last estimate)."""
    out, run, last = [], [], -1e9
    for t, r in est:
        if t - last < REFRACTORY_S:
            continue
        if abs(r - 1) > EVENT_TH and (not run or (abs(r / run[-1] - 1) < AGREE)):
            run.append(r)
        else:
            run = [r] if abs(r - 1) > EVENT_TH else []
        if len(run) >= CONFIRM:
            out.append((float(t), float(np.median(run))))
            last, run = t, []
    return out


def one(case: dict) -> str:
    if not S._recs:
        S._recs.update({r["name"]: r for r in M.load_index()})
    rec = S._recs[case["track"]]
    item = M.Item(f"same/pitch_step/{rec['name']}", "same", "pitch_step", "x", (rec["name"],),
                  {"change": case["change"], "ramp_s": 0.0, "at": case["at"]})
    try:
        y, _, info = M.render(item, S._recs)
        t0 = info["t_change"]
        est = estimates(logfreq_blocks(y), t0 - 20.0, t0 + 4.0)
        before = [abs(r - 1) for t, r, _ in est if t < t0]
        true = 1 + case["change"]
        after = {f"{d:.1f}": [abs(r - true) for t, r, _ in est if abs(t - (t0 + d)) < 0.051]
                 for d in (0.1, 0.2, 0.3, 0.5, 0.7, 1.0, 1.5, 2.0, 3.0)}
        return json.dumps({**case, "before_p95": float(np.percentile(before, 95)), "before_max": float(np.max(before)),
                           "before": [round(x, 5) for x in before], "t0": t0,
                           "est": [[round(t, 2), round(r, 5), round(p, 4)] for t, r, p in est],
                           "after": {k: (round(v[0], 5) if v else None) for k, v in after.items()}})
    except Exception as exc:
        return json.dumps({**case, "error": f"{type(exc).__name__}: {exc}"})


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=40)
    ap.add_argument("--changes", nargs="+", type=float, default=[0.0, 0.02, 0.05])
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    jobs = []
    for c in args.changes:
        jobs += S.cases(args.cases, c, seed=7)
    with ProcessPoolExecutor(args.jobs) as pool, args.out.open("w") as f:
        for line in pool.map(one, jobs, chunksize=1):
            f.write(line + "\n")
            f.flush()


if __name__ == "__main__":
    main()
