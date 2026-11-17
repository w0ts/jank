"""How far is Traktor's "1" (its grid marker, every 4th beat from it) to be trusted? Compare it with madmom's offline
downbeat tracker on the tracks extract.py --madmom-db ran on, and score the chosen rules against both references.

    ../../.venv/bin/python ref_check.py      -> appends to results/tables.md
"""
from __future__ import annotations

import json
import pickle
from collections import Counter
from dataclasses import replace

import numpy as np

from analyze import HEAD, RES, LinearRule, fit_linear, fixed_offsets, fmt, load, run_rule, summarize
from rules import MATCH_S, Rule, ref_positions


def main() -> None:
    rows = {r["name"]: r for r in pickle.loads((RES / "traktor_mmdb.0of4.pkl").read_bytes())["rows"] if "error" not in r}
    tracks = [t for t in load("traktor") if t.name in rows]
    names = tuple(pickle.loads((RES / "traktor.pkl").read_bytes())["features"])
    out = [f"\n## Traktor's '1' against madmom offline downbeats ({len(tracks)} tracks)\n"]
    agree, shifts, three = [], Counter(), 0
    agree_by: dict[str, float] = {}
    for tr in tracks:
        mb, mp = np.asarray(rows[tr.name]["mm_beats"]), np.asarray(rows[tr.name]["mm_pos"])
        three += int(mp.max() == 3)
        j = np.clip(np.searchsorted(tr.t_ref, mb), 1, len(tr.t_ref) - 1)
        j = np.where(np.abs(tr.t_ref[j - 1] - mb) <= np.abs(tr.t_ref[j] - mb), j - 1, j)
        ok = np.abs(tr.t_ref[j] - mb) <= MATCH_S
        if ok.sum() < 16:
            continue
        d = ((mp[ok] - 1) - (tr.ref_pos[j[ok]] - 1)) % 4
        agree.append(np.mean(d == 0))
        agree_by[tr.name] = agree[-1]
        shifts[int(Counter(d.tolist()).most_common(1)[0][0])] += 1
    a = np.array(agree)
    out.append(f"Tracks compared: {len(a)}; madmom chose 3/4 for {three}. Share of matched beats where madmom's bar "
               f"position = Traktor's: mean {100 * a.mean():.0f} %, median {100 * np.median(a):.0f} %; tracks agreeing "
               f">= 90 %: {100 * np.mean(a >= 0.9):.0f} %. Most common offset (madmom - Traktor, beats) per track: "
               + ", ".join(f"{k}: {v}" for k, v in sorted(shifts.items())) + "\n")
    # the chosen rules scored against madmom's '1' instead of Traktor's, on the tracks where madmom is 4/4
    w = json.loads((RES / "weights.json").read_text())
    lin = LinearRule("all 22", tuple(names.index(f) for f in w["features"]), w["bars"], w["mode"], tuple(w["w"]),
                     w["margin"], "traktor dev")
    single = Rule("nov_low", 16, "exp", 0.2)
    four = [tr for tr in tracks if np.asarray(rows[tr.name]["mm_pos"]).max() == 4]
    # out of sample: these tracks are all in the dev half, so refit the linear weights on the test half
    all_tracks = load("traktor")
    lin_odd = replace(fit_linear(all_tracks[1::2], names, "all 22", w["bars"], w["mode"], "Traktor test half"),
                      margin=w["margin"])
    both = [tr for tr in tracks if agree_by.get(tr.name, 0.0) >= 0.9]
    out.append(f"Tracks where Traktor and madmom agree on >= 90 % of beats (the '1' is probably right): {len(both)}\n")
    out.append(HEAD)
    for r in (lin_odd, single):
        out.append(f"| {r.label}, online beats, agreeing tracks {fmt(summarize(run_rule(both, r, 'online', names)))}")
        out.append(f"| {r.label}, ref beats, agreeing tracks {fmt(summarize(run_rule(both, r, 'ref', names)))}")
    out.append(f"| first tracked beat = 1, online beats, agreeing tracks "
               f"{fmt(summarize(fixed_offsets(both, 'online', (0,))))}")
    out.append(f"| Link's arbitrary bar, online beats, agreeing tracks "
               f"{fmt(summarize(fixed_offsets(both, 'online', (0, 1, 2, 3))))}\n")
    out.append(f"{HEAD}")
    for ref_name in ("Traktor", "madmom offline"):
        if ref_name == "madmom offline":
            for tr in four:
                mb, mp = np.asarray(rows[tr.name]["mm_beats"]), np.asarray(rows[tr.name]["mm_pos"])
                tr.t_ref, tr.ref_pos = mb, mp
                tr.valid = np.ones(len(mb), bool)
                tr.c_ref = np.arange(len(mb))
                tr.rp_ref = mp - 1
                tr.rp_on = ref_positions(tr.t_on, mb, mp, tr.valid)
        for r in (lin, single):
            out.append(f"| {r.label}, online beats, reference = {ref_name}, {len(four)} tracks "
                       f"{fmt(summarize(run_rule(four, r, 'online', names)))}")
    text = "\n".join(out) + "\n"
    print(text)
    with (RES / "tables.md").open("a") as f:
        f.write(text)


if __name__ == "__main__":
    main()
