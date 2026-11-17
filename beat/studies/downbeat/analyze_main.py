"""The study's tables: single-feature rules, learned linear rules, baselines, splits, downbeat models.

    ../../.venv/bin/python analyze_main.py          -> results/tables.md (+ results/grid_<dataset>.md, weights.json)
"""
from __future__ import annotations

import json
import pickle
from dataclasses import replace
from itertools import product

import numpy as np

from analyze import (BARS, COMBOS, HEAD, LIN_SETS, RES, Track, fit_linear, fixed_offsets, fmt, in_break, load,
                     mmdb_traktor, model_tracks, run_json, run_rule, split_pooled, summarize)
from rules import Rule


def acc(tracks, rule, names) -> float:
    return summarize(run_rule(tracks, rule, "online", names)).get("acc_mean", 0.0)


def pick_margin(dev, rule, names, margins, say) -> object:
    """The largest hysteresis that costs at most 1 point of dev accuracy."""
    base = acc(dev, rule, names)
    chosen = rule
    for m in margins:
        r = replace(rule, margin=m)
        d = summarize(run_rule(dev, r, "online", names))
        say(f"| {r.label} {fmt(d)}")
        if d.get("acc_mean", 0) >= base - 0.01:
            chosen = r
    return chosen


def meter_line(tracks: list[Track]) -> str:
    bars = sum(t.bars for t in tracks)
    non4 = sum(t.non4 for t in tracks)
    return (f"Reference bars: {bars}, not 4 beats long: {non4} ({100 * non4 / max(bars, 1):.1f} %), in "
            f"{sum(1 for t in tracks if t.non4)} tracks; tracks with most bars not 4 beats: "
            f"{sum(1 for t in tracks if t.non4 > t.bars / 2)} (their beats are skipped where the bar is not 4)\n")


def dataset_section(ds: str, say, foreign: object | None = None):
    tracks = load(ds)
    names = tuple(pickle.loads((RES / f"{ds}.pkl").read_bytes())["features"])
    dev, test = tracks[0::2], tracks[1::2]
    say(f"## {ds}: {len(tracks)} tracks (dev = even, {len(dev)}; test = odd, {len(test)})\n")
    say(meter_line(tracks))

    rows = []
    for f, n, m in product(list(names) + list(COMBOS), BARS, ("window", "exp")):
        r = Rule(f, n, m)
        rows.append((acc(dev, r, names), r))
    rows.sort(key=lambda x: -x[0])
    (RES / f"grid_{ds}.md").write_text(
        "| rule | dev mean time in correct phase % |\n|---|---|\n" +
        "\n".join(f"| {r.label} | {100 * a:.1f} |" for a, r in rows) + "\n")
    say(f"### Single-feature rules, dev half, online beats: best 10 of {len(rows)} (all: grid_{ds}.md)\n\n{HEAD}")
    for _, r in rows[:10]:
        say(f"| {r.label} {fmt(summarize(run_rule(dev, r, 'online', names)))}")
    best_single = rows[0][1]

    say(f"\n### Learned linear rules (weights fitted on the dev half's online beats), dev half\n\n{HEAD}")
    lin = []
    for s, n, m in product(LIN_SETS, (4, 8, 16), ("window", "exp")):
        r = fit_linear(dev, names, s, n, m, f"{ds} dev")
        d = summarize(run_rule(dev, r, "online", names))
        lin.append((d.get("acc_mean", 0), r))
        say(f"| {r.label} {fmt(d)}")
    lin.sort(key=lambda x: -x[0])
    best_lin = lin[0][1]

    say(f"\n### Hysteresis, dev half, online beats\n\n{HEAD}")
    single_h = pick_margin(dev, best_single, names, (0.02, 0.05, 0.1, 0.2), say)
    lin_h = pick_margin(dev, best_lin, names, (0.25, 0.5, 1.0, 2.0), say)

    methods = [best_single, single_h, best_lin, lin_h]
    if foreign is not None:
        methods.append(foreign)
    say(f"\n### TEST half\n\n{HEAD}")
    for beats in ("online", "ref"):
        for r in methods:
            say(f"| {r.label}, {beats} beats {fmt(summarize(run_rule(test, r, beats, names)))}")
        say(f"| Link's arbitrary bar (today; mean of the 4 offsets), {beats} beats "
            f"{fmt(summarize(fixed_offsets(test, beats, (0, 1, 2, 3))))}")
        say(f"| first tracked beat = 1, {beats} beats {fmt(summarize(fixed_offsets(test, beats, (0,))))}")

    say("\n### Splits, TEST half, pooled time in correct phase (beats scored)\n")
    for r in (lin_h, single_h):
        for beats in ("online", "ref"):
            res = run_rule(test, r, beats, names)
            g = split_pooled(res, lambda tr, t: tr.genre)
            say(f"- {r.label}, {beats} beats, by genre: " + "; ".join(
                f"{k} {100 * v[0]:.0f} % ({sum(1 for x in test if x.genre == k)} tracks)" for k, v in g.items()))
            if ds == "traktor":
                b = split_pooled(res, in_break)
                say(f"- {r.label}, {beats} beats: " + "; ".join(
                    f"{k} {100 * v[0]:.0f} % ({v[1]} beats)" for k, v in b.items()))
    say()

    say(f"### Downbeat models, ALL tracks, same scoring on their own beats\n\n{HEAD}")
    if ds == "traktor":
        say(f"| BeatNet online PF (causal; runs/ of 2026-11-13) "
            f"{fmt(summarize(model_tracks(tracks, *run_json('traktor.*of4_beatnet_online_pf.json'))))}")
        mb, md, mpos = mmdb_traktor()
        sub = [t for t in tracks if t.name in mb]
        if sub:
            say(f"| madmom RNNDownBeat + DBN offline (NOT causal), {len(sub)} tracks "
                f"{fmt(summarize(model_tracks(sub, mb, md)))}")
            for r in (lin_h, single_h):
                say(f"| {r.label} on the same {len(sub)} tracks (dev+test), online beats "
                    f"{fmt(summarize(run_rule(sub, r, 'online', names)))}")
            bars3 = sum(int(np.sum(np.diff(np.flatnonzero(mpos[n] == 1)) == 3)) for n in mpos)
            allb = sum(max(len(np.flatnonzero(mpos[n] == 1)) - 1, 0) for n in mpos)
            say(f"\nmadmom offline on these Traktor tracks: {bars3} of {allb} bars are 3 beats long.")
        bt = RES / "beatthis_L5.pkl"
        if bt.exists():
            rows_bt = pickle.loads(bt.read_bytes())
            b = {r["name"]: r["est_beats"] for r in rows_bt}
            d = {r["name"]: r["est_downbeats"] for r in rows_bt}
            sub = [t for t in tracks if t.name in b]
            say(f"| Beat This! small0 streamed, 0.36 s delay, {len(sub)} tracks "
                f"{fmt(summarize(model_tracks(sub, b, d)))}")
            for r in (lin_h, single_h):
                say(f"| {r.label} on the same {len(sub)} tracks, online beats "
                    f"{fmt(summarize(run_rule(sub, r, 'online', names)))}")
    else:
        say(f"| BeatNet online PF (causal) "
            f"{fmt(summarize(model_tracks(tracks, *run_json('gtzan_beatnet_online_pf.json'))))}")
        say(f"| madmom RNNDownBeat + DBN offline (NOT causal) "
            f"{fmt(summarize(model_tracks(tracks, *run_json('gtzan_madmom_downbeat_offline.json'))))}")
    say()
    return lin_h, names


def main() -> None:
    lines: list[str] = []

    def say(s: str = "") -> None:
        print(s, flush=True)
        lines.append(s)

    say("Columns: time in correct phase = share of scored beats (tracked beats matched to a reference beat within 70 ms, "
        "inside 4-beat bars, after the first 2 bars) whose bar position the method gets right; mean and median over "
        "tracks, pooled over all beats; tracks >= 90 %; median seconds from the first tracked beat to the first run of "
        "8 right beats; never locked; flips = changes of the chosen '1' after the first 2 bars.\n")
    lin_tk, names_tk = dataset_section("traktor", say)
    (RES / "weights.json").write_text(json.dumps({"label": lin_tk.label, "features": [names_tk[i] for i in lin_tk.cols],
                                                  "bars": lin_tk.bars, "mode": lin_tk.mode, "margin": lin_tk.margin,
                                                  "w": lin_tk.w}, indent=1))
    names_gt = tuple(pickle.loads((RES / "gtzan.pkl").read_bytes())["features"])
    foreign = replace(lin_tk, trained_on="Traktor dev") if names_gt == names_tk else None
    dataset_section("gtzan", say, foreign)
    (RES / "tables.md").write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
