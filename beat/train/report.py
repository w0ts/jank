"""Tables for trackers saved by eval_net.py: Traktor test split (BPM while running, F-measure vs the corrected labels),
GTZAN (F, CMLt, AMLt) and the mixes (eval.mix_eval's pooled numbers).

    report.py LABELS.json TRACKER [TRACKER...] [--parts traktor,gtzan,mixes] [--split test]
"""
from __future__ import annotations

import argparse
import contextlib
import io
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))


def f_measure(est: np.ndarray, ref: np.ndarray, skip_s: float = 5.0):
    from madmom.evaluation.beats import BeatEvaluation
    est, ref = est[est >= skip_s], ref[ref >= skip_s]
    if len(est) == 0 or len(ref) == 0:
        return 0.0, 0.0, 0.0
    e = BeatEvaluation(est, ref)
    return e.fmeasure, e.cmlt, e.amlt


def traktor(labels: list[dict], trackers: list[str], split: str) -> None:
    from eval.data import DATA
    from eval.mode_eval import steady
    from eval.rescore import _rows
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    lab = {r["name"]: r for r in labels if r["split"] == split}
    print(f"\nTraktor {split} split ({len(lab)} tracks): BPM while the transport runs (eval.mode_eval steady)"
          " + beat F-measure (70 ms, after 5 s) against the offset-corrected labels")
    print(f"{'tracker':22} {'tracks':>6} {'running':>8} {'within0.1':>10} {'within0.5':>10} {'wrong>0.5':>10} {'F':>6} {'CMLt':>6} {'AMLt':>6}")
    for t in trackers:
        rows = [r for r in _rows("traktor", t) if r["name"] in lab and "error" not in r]
        s = steady(rows, index)
        fs = np.array([f_measure(np.array(r["est_beats"]), np.array(lab[r["name"]]["beats"])) for r in rows])
        print(f"{t:22} {s['tracks']:6} {s['running']:8.3f} {s['within_0.1']:10.3f} {s['within_0.5']:10.3f} "
              f"{s['wrong_gt_0.5']:10.3f} {fs[:, 0].mean():6.3f} {fs[:, 1].mean():6.3f} {fs[:, 2].mean():6.3f}")


def traktor_unfiltered(labels: list[dict], trackers: list[str], split: str) -> None:
    """BPM on everything never trained on and not filtered by madmom's lock: the split plus the tracks dropped from the
    labels (R too low or drifting), which are in no training set."""
    from eval.data import DATA
    from eval.mode_eval import steady
    from eval.rescore import _rows
    index = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    kept = {r["name"] for r in labels}
    names = {r["name"] for r in labels if r["split"] == split} | (set(index) - kept)
    print(f"\nTraktor {split} split + tracks dropped from the labels, unfiltered ({len(names)} tracks): BPM while running")
    print(f"{'tracker':22} {'tracks':>6} {'running':>8} {'within0.1':>10} {'within0.5':>10} {'wrong>0.5':>10}")
    for t in trackers:
        rows = [r for r in _rows("traktor", t) if r["name"] in names and "error" not in r]
        s = steady(rows, index)
        print(f"{t:22} {s['tracks']:6} {s['running']:8.3f} {s['within_0.1']:10.3f} {s['within_0.5']:10.3f} {s['wrong_gt_0.5']:10.3f}")


def gtzan(trackers: list[str]) -> None:
    from eval.data import DATASETS
    from eval.rescore import _rows
    ref = {p.name: p.beats for p in DATASETS["gtzan"]()}
    print("\nGTZAN (hand annotations, held out): beat F-measure 70 ms after 5 s, CMLt, AMLt; online DBN 90-180")
    print(f"{'tracker':22} {'pieces':>6} {'F':>6} {'CMLt':>6} {'AMLt':>6}")
    for t in trackers:
        rows = [r for r in _rows("gtzan", t) if r["name"] in ref]
        fs = np.array([f_measure(np.array(r["est_beats"]), ref[r["name"]]) for r in rows])
        print(f"{t:22} {len(rows):6} {fs[:, 0].mean():6.3f} {fs[:, 1].mean():6.3f} {fs[:, 2].mean():6.3f}")


def mixes(trackers: list[str]) -> None:
    from eval import mix_eval
    for t in trackers:
        mix_eval.LIVE = t
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            mix_eval.main()
        lines = buf.getvalue().splitlines()
        print(f"\nMixes, tracker {t}:")
        print("\n".join(lines))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("labels")
    ap.add_argument("trackers", nargs="+")
    ap.add_argument("--parts", default="traktor,gtzan,mixes")
    ap.add_argument("--split", default="test")
    a = ap.parse_args()
    labels = json.loads(Path(a.labels).read_text())
    parts = a.parts.split(",")
    if "traktor" in parts:
        traktor(labels, a.trackers, a.split)
        traktor_unfiltered(labels, a.trackers, a.split)
    if "gtzan" in parts:
        gtzan(a.trackers)
    if "mixes" in parts:
        mixes(a.trackers)


if __name__ == "__main__":
    main()
