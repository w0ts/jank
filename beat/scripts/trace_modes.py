"""Print ModeBpm's mode switches and jumps on a synthetic tempo ramp (a debugging aid).

    .venv/bin/python scripts/trace_modes.py 124 130 60 60 60
"""
from __future__ import annotations

import sys

from eval.mode_bpm import ModeBpm
from tests.test_glide_bpm import ramp_beats


def main() -> None:
    bpm0, bpm1, hold0, ramp, hold1 = (float(x) for x in sys.argv[1:6])
    beats, tempo = ramp_beats(bpm0, bpm1, hold0, ramp, hold1)
    est = ModeBpm()
    for t in beats:
        jumps = est.jumps
        est.update(float(t))
        if est.jumps != jumps:
            print(f"jump at {t:.1f} s (true tempo {tempo(t):.2f})")
    print("modes:", [(round(t, 1), m) for t, m in est.mode_log])
    print(f"final {est.bpm():.3f} BPM (true {tempo(beats[-1]):.2f})")


if __name__ == "__main__":
    main()
