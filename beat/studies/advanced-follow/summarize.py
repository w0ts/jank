"""Tables from study_eval.py's rows: today -> quick -> advanced, per family and kind (one split).

Real tracks whose reference grid is half a beat out (today's harness 'offbeat' share over 0.5: the Traktor bug that
eval/ref_phase.py flags; its verdict file had not landed) are left out of the harness table.

    .venv/bin/python studies/advanced-follow/summarize.py EVAL.jsonl [--split test]
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

import numpy as np

MODES = ("today", "quick", "advanced")


def _m(vals) -> str:
    v = [x for x in vals if x is not None and x == x]
    return f"{np.mean(v):.3f}" if v else "-"


def _med(vals, q=50) -> str:
    v = [x for x in vals if x is not None and x == x]
    return f"{np.percentile(v, q):.1f}" if v else "-"


def table(title: str, header: list[str], rows: list[list[str]]) -> str:
    out = [f"\n### {title}\n", "| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
    out += ["| " + " | ".join(r) + " |" for r in rows]
    return "\n".join(out)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", type=Path)
    ap.add_argument("--split", default="test")
    args = ap.parse_args()
    rows = [json.loads(x) for x in args.path.read_text().splitlines() if x.strip()]
    errors = [r for r in rows if "error" in r]
    rows = [r for r in rows if "error" not in r and r["split"] == args.split and r["mode"] in MODES]
    by = defaultdict(list)
    for r in rows:
        by[(r["family"], r["kind"], r["mode"])].append(r)
    suspect = {r["name"] for r in rows if r["mode"] == "today" and r["family"] == "real" and r["harness"]["offbeat"] > 0.5}
    out = [f"split {args.split}; rows {len(rows)}; errors {len(errors)}; real tracks with a half-beat reference left "
           f"out of the harness: {len(suspect)}"]
    # real tracks
    hdr = ["mode", "tracks", "BPM held", "flips / track", "stopped", "break: stopped", "break: in phase",
           "break flips", "tight", "loose", "octave", "triplet", "offbeat", "off", "lost"]
    t = []
    for m in MODES:
        rs = by[("real", "track", m)]
        h = [r["harness"] for r in rs if r["name"] not in suspect]
        t.append([m, str(len(rs)), _m([r["held_all"] for r in rs]), _m([r["flips"] for r in rs]),
                  _m([r["stopped_share"] for r in rs]), _m([r.get("break_stopped") for r in rs]),
                  _m([r.get("break_in") for r in rs]), _m([r.get("break_flips") for r in rs])]
                 + [_m([x[k] for x in h]) for k in ("tight", "loose", "octave", "triplet", "offbeat", "off", "lost")])
    out.append(table("Real tracks (whole tracks; break = Traktor's Break to Drop cues)", hdr, t))
    # same, variations
    hdr = ["kind", "mode", "items", "BPM held (window+10 s)", "stopped", "in phase", "flips (window +-20 s)",
           "false 'different' resets"]
    t = []
    for kind in ("lpf", "hpf", "drums_out", "gain_dip", "vocal_only"):
        for m in MODES:
            rs = by[("same", kind, m)]
            t.append([kind, m, str(len(rs)), _m([r.get("var_held") for r in rs]), _m([r.get("var_stopped") for r in rs]),
                      _m([r.get("var_in") for r in rs]), _m([r.get("var_flips") for r in rs]),
                      str(sum(len(r["different_resets"]) for r in rs))])
    out.append(table("Same music, varied (the variation's window)", hdr, t))
    # same, pitch
    hdr = ["kind", "|change|", "mode", "items", "BPM held 30 s", "stopped", "in phase", "worst ms (median)"]
    t = []
    for kind in ("pitch_step", "pitch_ramp", "stretch_step"):
        changes = sorted({abs(r["change"]) for m in MODES for r in by[("same", kind, m)]})
        for c in changes:
            for m in MODES:
                rs = [r for r in by[("same", kind, m)] if abs(r["change"]) == c]
                t.append([kind, f"{100 * c:g} %", m, str(len(rs)), _m([r.get("chg_held") for r in rs]),
                          _m([r.get("chg_stopped") for r in rs]), _m([r.get("chg_in") for r in rs]),
                          _med([r.get("chg_worst_ms") for r in rs])])
    out.append(table("Pitch moves (30 s from the move)", hdr, t))
    # different
    hdr = ["kind", "mode", "items", "re-lock median s", "p90 s", "never", "wrong BPM s in 60 s", "BPM held 60 s",
           "held before", "detected (advanced)"]
    t = []
    for kind in ("cut_same", "fade_same", "cut_shift", "fade_shift", "half", "source"):
        for m in MODES:
            rs = by[("diff", kind, m)]
            rl = [r["relock_s"] for r in rs]
            never = sum(x is None for x in rl)
            det = "-"
            if m == "advanced":
                hits = [any(r["t_switch"] - r["fade_s"] / 2 <= d <= r["t_switch"] + 20
                            for d in r["different_resets"]) for r in rs if "t_switch" in r]
                det = f"{np.mean(hits):.2f}" if hits else "-"
            t.append([kind, m, str(len(rs)), _med(rl), _med(rl, 90), str(never), _m([r["wrong_bpm_s_60"] for r in rs]),
                      _m([r["held_after60"] for r in rs]), _m([r["held_before"] for r in rs]), det])
    out.append(table("Different music (from the cut / the fade's middle)", hdr, t))
    # false resets on same music
    same_rows = [r for r in rows if r["mode"] == "advanced" and r["family"] in ("real", "same")]
    hours = sum(max(r.get("dur", 0.0), 0.0) for r in same_rows) / 3600.0
    n = sum(len(r["different_resets"]) for r in same_rows)
    out.append(f"\nfalse 'different' resets on same music (advanced): {n} in {hours:.1f} h -> "
               f"{n / hours if hours else float('nan'):.2f} per hour")
    print("\n".join(out))


if __name__ == "__main__":
    main()
