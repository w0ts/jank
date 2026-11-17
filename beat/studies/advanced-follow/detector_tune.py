"""The "different music" decision tuned on the dev half, from study_eval.py's probe rows (novelty and confidence once a
second, never acted on). A decision is simulated causally on those traces: novel (timbre over T or chroma over C) while
the beats fit the published lines badly (confidence under K) for L seconds in a row; after a decision nothing for
HOLDOFF_S (the memory refills).

Scored as events:
  hit       a sequence (diff item) with a decision within [switch - fade / 2, switch + 20 s]; latency from the switch
  false     decisions on same music: real tracks, same items, and the sequences before their fade starts (per hour)

    .venv/bin/python studies/advanced-follow/detector_tune.py PROBE.jsonl [--split dev]
"""
from __future__ import annotations

import argparse
import itertools
import json
from pathlib import Path

import numpy as np

HOLDOFF_S = 12.0
WARMUP_S = 20.0


def decisions(sec: dict, t_diff: float, c_diff: float, k_low: float, l_s: float) -> list[float]:
    t = np.asarray(sec["t"], float)
    tim = np.asarray(sec["timbre"], float)
    chr_ = np.asarray(sec["chroma"], float)
    conf = np.asarray(sec["conf"], float)
    bpm = np.asarray(sec["bpm"], float)
    out, low, last = [], 0.0, -1e9
    for i in range(t.size):
        low = low + 1.0 if (conf[i] < k_low or np.isnan(bpm[i])) else 0.0
        novel = (tim[i] > t_diff) or (chr_[i] > c_diff)          # NaN (no memory yet) compares False
        if novel and low >= l_s and t[i] - last >= HOLDOFF_S and t[i] >= WARMUP_S:
            out.append(float(t[i]))
            last = t[i]
    return out


def evaluate(rows: list[dict], params: tuple) -> dict:
    hits, lat, false_n, same_s = [], [], 0, 0.0
    for r in rows:
        ds = decisions(r["sec"], *params)
        m = r["meta"]
        if r["family"] == "diff":
            ts, fade = m["t_switch"], m.get("fade_s", 0.0)
            lo = ts - fade / 2
            inwin = [d for d in ds if lo <= d <= ts + 20]
            hits.append(bool(inwin))
            if inwin:
                lat.append(inwin[0] - ts)
            false_n += sum(WARMUP_S <= d < lo for d in ds)
            same_s += max(lo - WARMUP_S, 0.0)
        else:
            false_n += len(ds)
            same_s += max(m["dur"] - WARMUP_S, 0.0)
    return {"hit": float(np.mean(hits)) if hits else float("nan"), "lat_med": float(np.median(lat)) if lat else None,
            "lat_p90": float(np.percentile(lat, 90)) if lat else None, "false_per_h": false_n / (same_s / 3600.0),
            "diff_items": len(hits), "same_hours": same_s / 3600.0}


def load(path: Path, split: str | None) -> list[dict]:
    rows = [json.loads(x) for x in path.read_text().splitlines() if x.strip()]
    return [r for r in rows if r.get("mode") == "probe" and "sec" in r and (split is None or r["split"] == split)]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", type=Path)
    ap.add_argument("--split", default="dev")
    ap.add_argument("--max-false", type=float, default=1.0)
    args = ap.parse_args()
    rows = load(args.path, args.split)
    grid = itertools.product((2.0, 2.5, 3.0, 4.0, 6.0, 1e9), (0.08, 0.12, 0.2, 0.3, 1e9), (0.3, 0.5, 0.7), (1.0, 2.0, 4.0))
    res = []
    for p in grid:
        if p[0] >= 1e9 and p[1] >= 1e9:
            continue
        res.append((p, evaluate(rows, p)))
    ok = [x for x in res if x[1]["false_per_h"] <= args.max_false]
    ok.sort(key=lambda x: (-x[1]["hit"], x[1]["lat_med"] or 1e9))
    print(f"{len(rows)} probe rows ({args.split}); best with <= {args.max_false} false / h:")
    for p, e in ok[:12]:
        print(f"  timbre>{p[0]:g} chroma>{p[1]:g} conf<{p[2]} for {p[3]} s: hit {e['hit']:.2f} latency med {e['lat_med']} "
              f"p90 {e['lat_p90']} false/h {e['false_per_h']:.2f}")
    print("rhythm only (no novelty condition):")
    for k, l in itertools.product((0.3, 0.5, 0.7), (1.0, 2.0, 4.0)):
        e = evaluate(rows, (-1.0, -1.0, k, l))
        print(f"  conf<{k} for {l} s: hit {e['hit']:.2f} latency med {e['lat_med']} false/h {e['false_per_h']:.2f}")


if __name__ == "__main__":
    main()
