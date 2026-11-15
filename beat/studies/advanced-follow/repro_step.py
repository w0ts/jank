"""The user's case: a steady track, the player's pitch moved by a step (varispeed). Today's live pipeline (live.engine.Engine,
90-180 BPM, LOW CONFIDENCE threshold 0.8 as the XIAO's default) logged around the step: per tracked beat the estimator's
state, and the published / gated state.

    .venv/bin/python studies/advanced-follow/repro_step.py --track tk_82fba4fd --change 0.02 [--ramp 0] [--csv out.csv]
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1]))

import material as M
from live.engine import Engine

CHUNK = 1024


def ref_phase_ms(grid, ref: np.ndarray, now: float) -> float:
    """Signed ms from the latest reference beat at or before now to the nearest line of the grid."""
    if grid is None:
        return float("nan")
    past = ref[ref <= now]
    if past.size == 0:
        return float("nan")
    r = past[-1]
    t0, p = grid
    return float((t0 + np.round((r - t0) / p) * p - r) * 1000.0)


def run(track: str, change: float, ramp: float, threshold: float, est_factory=None) -> list[dict]:
    recs = {r["name"]: r for r in M.load_index()}
    kind = "pitch_ramp" if ramp > 0 else "pitch_step"
    item = M.Item(f"same/{kind}/{track}", "same", kind, "dev", (track,), {"change": change, "ramp_s": ramp})
    y, ref, info = M.render(item, recs)
    eng = Engine(90.0, 180.0, min_confidence=threshold)
    if est_factory is not None:
        eng.estimator = est_factory()
    rows, beat_i = [], 0
    for s in range(0, len(y), CHUNK):
        found = eng.feed(y[s:s + CHUNK])
        st = eng.status()
        e = eng.estimator
        now = eng.stream_time
        for b in found:
            beat_i += 1
            true_bpm = info["bpm"] if b < info["t_change"] else info["bpm_after"] if ramp == 0 else \
                info["bpm"] * M.rate_curve(int((b + 1) * M.SR), info["t_change"], ramp, change)[int(b * M.SR)]
            rows.append({
                "beat_t": round(b, 3), "true_bpm": round(true_bpm, 2), "mode": st.mode,
                "bpm": round(st.bpm, 2) if st.bpm else "", "pub": st.published.mode,
                "conf": round(st.confidence, 2), "gated": int(st.gated),
                "phase_ms": round(ref_phase_ms(st.grid, ref, now), 1),
                "misses": e.misses, "disagree": e.disagree, "restarts": e.restarts, "jumps": e.jumps,
                "fit_n": e.fit.n, "in_a_row": getattr(e, "in_a_row", ""),
            })
    return rows, info


def summary(rows: list[dict], info: dict, span: float = 30.0) -> dict:
    """Around the change: beats published stopped, seconds until the published BPM stays within 0.5 % of the truth,
    the worst |phase move| (ms, against the phase before the change: Traktor's constant offset drops out) while
    published, over `span` s from the change."""
    t0 = info["t_change"]
    pre = [r["phase_ms"] for r in rows if t0 - 10 <= r["beat_t"] < t0 and r["pub"] != "stopped"]
    base = float(np.nanmedian(pre)) if pre else float("nan")
    after = [r for r in rows if t0 <= r["beat_t"] < t0 + span]
    stopped = sum(r["pub"] == "stopped" for r in after)
    ok_from = None
    for r in after:
        good = r["bpm"] != "" and abs(r["bpm"] / r["true_bpm"] - 1) <= 0.005
        ok_from = (ok_from if ok_from is not None else r["beat_t"]) if good else None
    worst = max((abs(r["phase_ms"] - base) for r in after if r["pub"] != "stopped" and r["phase_ms"] == r["phase_ms"]),
                default=float("nan"))
    late = [r["phase_ms"] - base for r in after if r["beat_t"] > t0 + span - 5 and r["pub"] != "stopped"]
    return {"beats_stopped": stopped, "bpm_ok_after_s": round(ok_from - t0, 1) if ok_from is not None else None,
            "worst_phase_ms": round(worst, 1), "end_phase_ms": round(float(np.median(late)), 1) if late else None,
            "jumps": after[-1]["jumps"] - (after[0]["jumps"] if after else 0) if after else 0}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--track", default="tk_82fba4fd")
    ap.add_argument("--change", type=float, default=0.02)
    ap.add_argument("--ramp", type=float, default=0.0)
    ap.add_argument("--threshold", type=float, default=0.8)
    ap.add_argument("--csv", type=Path)
    ap.add_argument("--around", type=float, default=30.0, help="print the beats this many s around the change")
    args = ap.parse_args()
    rows, info = run(args.track, args.change, args.ramp, args.threshold)
    t0 = info["t_change"]
    if args.csv:
        with args.csv.open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(rows)
    print(f"change {args.change:+.3f} at {t0:.2f} s, ramp {args.ramp} s, {info['bpm']:.2f} -> {info['bpm_after']:.2f} BPM")
    print(" ".join(rows[0]))
    for r in rows:
        if t0 - 5 <= r["beat_t"] <= t0 + args.around:
            print(" ".join(str(v) for v in r.values()))


if __name__ == "__main__":
    main()
