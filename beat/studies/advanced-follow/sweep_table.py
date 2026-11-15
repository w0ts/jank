"""The manoeuvre settings sweep (farm_sweep.sh) on the dev half: steady behaviour against pitch-move following.

    .venv/bin/python studies/advanced-follow/sweep_table.py DIR
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np


def mean(rows, key):
    v = [r.get(key) for r in rows if r.get(key) is not None and r.get(key) == r.get(key)]
    return float(np.mean(v)) if v else float("nan")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dir", type=Path)
    args = ap.parse_args()
    print("| setting | real: BPM held | real: flips / track | real: tight | real: loose | variations: BPM held | "
          "variations: flips | pitch: in phase 30 s | pitch: stopped 30 s | pitch: worst ms (median) |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for f in sorted(args.dir.glob("*.jsonl")):
        rows = [json.loads(x) for x in f.read_text().splitlines() if x.strip()]
        rows = [r for r in rows if "error" not in r]
        real = [r for r in rows if r["family"] == "real"]
        var = [r for r in rows if r["kind"] in ("drums_out", "vocal_only", "lpf")]
        pitch = [r for r in rows if r["kind"] in ("pitch_step", "pitch_ramp")]
        worst = [r["chg_worst_ms"] for r in pitch if r.get("chg_worst_ms") is not None]
        print(f"| {f.stem} | {mean(real, 'held_all'):.3f} | {mean(real, 'flips'):.2f} | "
              f"{np.mean([r['harness']['tight'] for r in real]):.3f} | {np.mean([r['harness']['loose'] for r in real]):.3f} | "
              f"{mean(var, 'var_held'):.3f} | {mean(var, 'var_flips'):.2f} | {mean(pitch, 'chg_in'):.3f} | "
              f"{mean(pitch, 'chg_stopped'):.3f} | {np.median(worst):.1f} |")


if __name__ == "__main__":
    main()
