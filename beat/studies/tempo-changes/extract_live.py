"""Per whole track of the user's Traktor set: everything the tempo-change replays need, rendered once.

  act     the live tracker's activation (one LSTM net, online, 100 fps: studies/advanced-follow/extract.activation)
  beats   the live tracker's beats (online DBN 90-180 BPM on act: today's tracker)
  rms     the input's RMS per 0.1 s block (the engine's silence gate: under 1e-3 for 0.5 s)
  rate    the spectral rate's estimates (t, ratio, peak) with the probe always active (live.rate.RateProbe at the
          device's decimation); a replay keeps an estimate only when the probe had been active for its whole window
  flux    per frame: positive spectral flux of the tracker's 81 log bands, all bands and the top bands (hats), the
          weak-signal variant's evidence
  bt      Beat This! final0 offline beats (the reference's second source), unless --no-bt

    python studies/tempo-changes/extract_live.py --out CACHE --jobs N [--names a b] [--limit N] [--no-bt]
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import json
import sys
import time
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
BEAT = HERE.parents[1]
sys.path.insert(0, str(BEAT))
sys.path.insert(0, str(BEAT / "studies" / "advanced-follow"))
os.environ.setdefault("TORCH_HOME", str(BEAT / "data" / "torch"))

HOP, FRAME = 441, 2048
HI_BAND = 60            # of the 81 log bands: the top ones (about 6 kHz and up) for the hats' flux

_state: dict = {}


def _all_estimates_probe():
    """live.rate.RateProbe that matches at every estimate time (no events, so no refractory gaps): the replay re-runs
    the event logic on these, with the refractory its own events cause."""
    from live.rate import CUR_N, REF_N, RateProbe, match_ratio

    class AllEstimates(RateProbe):
        def _estimate(self, t: float):
            if len(self._blocks) < REF_N + CUR_N:
                return None
            blocks = list(self._blocks)
            r, peak = match_ratio(np.mean(blocks[:REF_N], axis=0), np.mean(blocks[REF_N:], axis=0))
            self.estimates.append((t, r, peak))
            return None

    return AllEstimates()


def rate_estimates(y: np.ndarray) -> np.ndarray:
    """(n, 3) (t, ratio, peak) of live.rate.RateProbe's match at every estimate time (frame k = the 2048 samples
    ending at k * 441, as live.rate.RateTap), always active, no refractory."""
    probe = _all_estimates_probe()
    n = len(y) // HOP
    pad = np.concatenate([np.zeros(FRAME, np.float32), y.astype(np.float32)])
    win = np.hanning(FRAME).astype(np.float32)
    for s in range(0, n, 4096):
        ks = np.arange(s, min(n, s + 4096))
        taken = ks[[probe.taken(int(k)) for k in ks]]
        idx = taken[:, None] * HOP + np.arange(FRAME)[None, :]        # pad offset FRAME: ends at k * 441
        mags = np.abs(np.fft.rfft(pad[idx] * win, axis=1)) if taken.size else np.zeros((0, FRAME // 2 + 1))
        row = dict(zip(taken.tolist(), range(taken.size)))
        for k in ks:
            j = row.get(int(k))
            probe.frame(int(k), mags[j] if j is not None else None)
    return np.asarray(probe.estimates, dtype=np.float64).reshape(-1, 3)


def flux(L: np.ndarray) -> np.ndarray:
    """(n, 2) positive flux per frame: all bands, the top bands."""
    d = np.maximum(np.diff(L, axis=0, prepend=L[:1]), 0.0)
    return np.stack([d.sum(axis=1), d[:, HI_BAND:].sum(axis=1)], axis=1)


def block_rms(y: np.ndarray, block: int = 4410) -> np.ndarray:
    n = len(y) // block
    return np.sqrt(np.mean(y[:n * block].reshape(n, block).astype(np.float64) ** 2, axis=1))


def beat_this(path: Path) -> np.ndarray:
    if "bt" not in _state:
        import torch
        torch.set_num_threads(1)
        from beat_this.inference import File2Beats
        _state["bt"] = File2Beats(checkpoint_path="final0", device="cpu", dbn=False)
    beats, _ = _state["bt"](str(path))
    return np.asarray(beats, dtype=float)


def rate_only(dst: Path, rec: dict) -> str:
    """Recompute the rate estimates of a cached track (the first pass kept the probe's refractory gaps)."""
    import material as M
    z = dict(np.load(dst))
    if "rate_all" in z:
        return f"{rec['name']} rate cached"
    z["rate"] = rate_estimates(M.load_track(rec))
    z["rate_all"] = np.array(1)
    tmp = dst.with_name(dst.stem + ".tmp.npz")
    np.savez(tmp, **z)
    tmp.rename(dst)
    return f"{rec['name']} rate ok"


def one(job: tuple) -> str:
    rec, cache, with_bt, only_rate = job
    import extract as X
    import material as M
    dst = Path(cache) / f"{rec['name']}.npz"
    if only_rate:
        try:
            return rate_only(dst, rec) if dst.exists() else f"{rec['name']} missing"
        except Exception:
            return f"{rec['name']} ERROR {traceback.format_exc(limit=4)[-600:]}"
    if dst.exists():
        return f"{rec['name']} cached"
    t0 = time.perf_counter()
    try:
        y = M.load_track(rec)
        act = X.activation(y)
        L = X.logbands(y, act.size)
        beats = X.dbn_beats(act)
        out = {"act": act.astype(np.float32), "beats": beats, "rms": block_rms(y).astype(np.float32),
               "rate": rate_estimates(y), "rate_all": np.array(1), "flux": flux(L).astype(np.float32)}
        if with_bt:
            wav = M.DATA / rec["audio"]
            out["bt"] = beat_this(wav)
        tmp = dst.with_name(dst.stem + ".tmp.npz")
        np.savez(tmp, **out, meta=json.dumps({"name": rec["name"], "dur": len(y) / M.SR}))
        tmp.rename(dst)
        return f"{rec['name']} ok {len(y) / M.SR:.0f} s in {time.perf_counter() - t0:.1f} s"
    except Exception:
        return f"{rec['name']} ERROR {traceback.format_exc(limit=4)[-600:]}"


def main() -> None:
    import material as M
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--names", nargs="+")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--no-bt", action="store_true")
    ap.add_argument("--rate-only", action="store_true", help="recompute the rate estimates of cached tracks")
    args = ap.parse_args()
    recs = M.load_index()
    if args.names:
        recs = [r for r in recs if r["name"] in set(args.names)]
    if args.limit:
        recs = recs[:args.limit]
    args.out.mkdir(parents=True, exist_ok=True)
    print(f"{len(recs)} tracks", flush=True)
    jobs = [(r, str(args.out), not args.no_bt, args.rate_only) for r in recs]
    with ProcessPoolExecutor(args.jobs) as pool:
        for k, msg in enumerate(pool.map(one, jobs, chunksize=1), 1):
            print(f"{k}/{len(recs)} {msg}", flush=True)
    print("EXTRACT DONE", flush=True)


if __name__ == "__main__":
    main()
