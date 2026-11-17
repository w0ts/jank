"""The numba DBN (dbn.py) against the cache's beats (madmom's online DBN, extract_live.py) on real tracks.

    python studies/follow-robust/rdbn_check.py --cache CACHE [--limit 5]
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from dbn import OnlineDbn


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--limit", type=int, default=5)
    a = ap.parse_args()
    bad = 0
    for f in sorted(a.cache.glob("*.npz"))[:a.limit]:
        z = np.load(f)
        t0 = time.perf_counter()
        d = OnlineDbn(z["act"])
        got = []
        for k in range(0, d.n_frames, 5):              # in steps, as a replay does
            got += d.advance(k + 4)
        got = np.asarray(got)
        ref = z["beats"]
        same = got.size == ref.size and np.allclose(got, ref, atol=1e-9)
        bad += not same
        print(f"{f.stem}: {ref.size} cached, {got.size} replayed, same={same}, {time.perf_counter() - t0:.2f} s "
              f"for {d.n_frames / 100:.0f} s", flush=True)
    print("ALL SAME" if bad == 0 else f"{bad} DIFFER")


if __name__ == "__main__":
    main()
