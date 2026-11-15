"""The second study's main experiments, with the accumulator model (accmodel.py) -> ../results/v2_tables.md and
../results/v2_model.json.

Protocol as the first study: Traktor split by position (even = dev: every choice and weight; odd = test), scored on
the tracker's online beats, then GTZAN. Choices on dev use 2-fold cross-fitted scores (the model that scores a dev
track never saw it); the final model is trained on the whole dev half and scored once on test.

    PYTHONPATH=..:$BL_REPO python evaluate3.py --cache ../cache --jobs 6 [--quick]
"""
from __future__ import annotations

import argparse
import json
import os
import pickle
import time
from concurrent.futures import ProcessPoolExecutor
from dataclasses import replace
from itertools import product
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import torch

import accmodel as am
from analyze import HEAD, fmt, split_pooled, summarize, traktor_group
from evalcore import LINK_HEAD, _breaks, add_agreement, link_score, link_fmt, link_summary
from feats import NAMES
from phase import Filter, decide_link
from rules import SKIP_BEATS, ref_positions, score

HERE = Path(__file__).resolve().parent
RES = HERE.parent / "results"
MARGINS = (0.0, 0.1, 0.25, 0.5, 1.0, 2.0)
ROWS: dict[str, SimpleNamespace] = {}      # filled before the pool forks
LOG: list[str] = []


def say(s: str = "") -> None:
    print(s, flush=True)
    LOG.append(s)


def load(pkl: Path, dataset: str) -> list[SimpleNamespace]:
    from eval.data import DATA
    rows = pickle.loads(pkl.read_bytes())["rows"]
    meta, genres = {}, {}
    if dataset == "traktor":
        meta = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
        genres = json.loads((HERE.parent / "genres.json").read_text())
    out = []
    for r in rows:
        if "error" in r:
            continue
        g = traktor_group(genres.get(r["name"], "")) if dataset == "traktor" else r["name"].split("_")[1]
        tr = SimpleNamespace(**r, genre=g, breaks=_breaks(meta.get(r["name"], {})), agree=np.nan, extra={},
                             key=f"{dataset}/{r['name']}")
        ROWS[tr.key] = tr
        out.append(tr)
    return out


def stream(tr, beats: str):
    if beats == "online":
        return tr.t_on, tr.c_on, tr.rp_on, tr.S_on, tr.D_on
    return tr.t_ref, tr.c_ref, tr.rp_ref, tr.S_ref, tr.D_ref


def usable(tr, beats: str) -> bool:
    return len(stream(tr, beats)[0]) >= SKIP_BEATS + 4


# ---- workers (forked: they see ROWS) ----
def _prep(job):
    key, beats, spec, every = job
    t, c, rp, S, D = stream(ROWS[key], beats)
    X, y, _ = am.prepare(S, D, c, rp, spec, NAMES)
    keep = y >= 0
    return X[keep][::every], y[keep][::every]


def _score(job):
    key, beats, spec, state, rp_override = job
    torch.set_num_threads(1)
    t, c, rp, S, D = stream(ROWS[key], beats)
    if rp_override is not None:
        rp = rp_override
    X, _, ready = am.prepare(S, D, c, rp, spec, NAMES)
    net = am.Scorer(X.shape[2], spec.hidden)
    net.load_state_dict(state)
    return key, am.scores(net.eval(), X), ready


class Runner:
    def __init__(self, jobs: int):
        self.pool = ProcessPoolExecutor(jobs)

    def fit(self, spec, tracks, beats: str) -> am.Scorer:
        jobs = [(tr.key, b, spec, 2) for b in beats.split("+") for tr in tracks if usable(tr, b)]
        data = list(self.pool.map(_prep, jobs, chunksize=4))
        return am.train(spec, data)

    def score(self, spec, net, tracks, beats: str = "online", rp_of=None) -> dict:
        st = {k: v.detach().clone() for k, v in net.state_dict().items()}
        jobs = [(tr.key, beats, spec, st, None if rp_of is None else rp_of(tr)) for tr in tracks if usable(tr, beats)]
        return {k: (sc, rd) for k, sc, rd in self.pool.map(_score, jobs, chunksize=4)}

    def crossfit(self, spec, dev, beats: str = "online", only: set | None = None, eval_beats: str = "online") -> dict:
        a, b = dev[0::2], dev[1::2]

        def sub(x):
            return x if only is None else [t for t in x if t.name in only]
        out = self.score(spec, self.fit(spec, sub(a), beats), b, eval_beats)
        out.update(self.score(spec, self.fit(spec, sub(b), beats), a, eval_beats))
        return out


def results(tracks, scs: dict, margin: float, beats: str = "online", rp_of=None) -> list:
    res = []
    for tr in tracks:
        if tr.key not in scs:
            continue
        t, c, rp, _, _ = stream(tr, beats)
        if rp_of is not None:
            rp = rp_of(tr)
        sc, ready = scs[tr.key]
        res.append((tr, score(t, c, am.decide(sc, ready, margin), rp)))
    return res


def link_results(tracks, scs: dict, f: Filter) -> list:
    res = []
    for tr in tracks:
        if tr.key not in scs:
            continue
        t, c, rp, _, _ = stream(tr, "online")
        sc, ready = scs[tr.key]
        post = np.where(ready[:, None], am.softmax(sc), 0.25)
        off, moved = decide_link(post, f)
        res.append((tr, link_score(t, c, off, moved, rp)))
    return res


def pick_margin(dev, scs) -> tuple[float, float]:
    """The largest margin that costs at most 1 point of dev mean accuracy against the best (as the study)."""
    accs = {m: summarize(results(dev, scs, m)).get("acc_mean", 0.0) for m in MARGINS}
    best = max(accs.values())
    return max(m for m in MARGINS if accs[m] >= best - 0.01), best


def baseline(test) -> None:
    from analyze import LinearRule
    from linear import decide_linear
    w = json.loads((RES / "weights.json").read_text())
    rule = LinearRule("all 22", tuple(range(22)), w["bars"], w["mode"], tuple(w["w"]), w["margin"], "traktor dev")
    for beats in ("online", "ref"):
        res = []
        for tr in test:
            if usable(tr, beats):
                t, c, rp, S, _ = stream(tr, beats)
                off = decide_linear(S, c, list(rule.cols), rule.bars, np.asarray(rule.w), rule.mode, rule.margin)
                res.append((tr, score(t, c, off, rp)))
        say(f"| first study: {rule.label}, {beats} beats {fmt(summarize(res))}")


def grid(run: Runner, dev, quick: bool) -> list:
    specs = [am.AccSpec("S", (16,)), am.AccSpec("S", (4, 16, 64)), am.AccSpec("D", (4, 16, 64)),
             am.AccSpec("S+D", (16,)), am.AccSpec("S+D", (4, 16, 64)), am.AccSpec("S+D", (8, 32, 128)),
             am.AccSpec("S+D", (4, 16, 64), hidden=16), am.AccSpec("S+D", (4, 16, 64), hidden=32)]
    if quick:
        specs = specs[:2] + specs[4:5]
    say(f"\n### Accumulator models, dev half, online beats (2-fold cross-fitted, margin chosen on dev)\n\n{HEAD}")
    rows = []
    for spec in specs:
        t0 = time.time()
        scs = run.crossfit(spec, dev)
        m, best = pick_margin(dev, scs)
        d = summarize(results(dev, scs, m))
        rows.append((d["acc_mean"], spec, m))
        say(f"| {spec.label}, margin {m:g} {fmt(d)}  ({time.time() - t0:.0f} s)")
    rows.sort(key=lambda r: -r[0])
    return rows


def ablation(run, dev, spec, margin) -> None:
    groups = {"chroma change (cc1, cc2)": ("cc1", "cc2"), "onsets O0-O7": tuple(f"O{g}" for g in range(8)),
              "levels E0-E7": tuple(f"E{g}" for g in range(8)),
              "level changes dE0-dE7, nov2": (*(f"dE{g}" for g in range(8)), "nov2"), "tracker timing tr": ("tr",)}
    say(f"\n### Ablation, dev half, online beats: {spec.label} with one descriptor group left out (cross-fitted)\n\n{HEAD}")
    for name, drop in groups.items():
        s = replace(spec, drop=drop)
        say(f"| without {name} {fmt(summarize(results(dev, run.crossfit(s, dev), margin)))}")


def variants(run, dev, spec, margin) -> dict:
    agree = [tr for tr in dev if tr.agree >= 0.9]
    say(f"\n### What to train on, dev half, online beats, {spec.label}, margin {margin:g} (cross-fitted)\n")
    say(f"Scored on all dev tracks, then on the {len(agree)} dev tracks where Traktor and madmom offline agree.\n\n{HEAD}")
    out = {}
    for name, beats, only in (("online beats", "online", None),
                              ("reference beats (shifted by the tracker's lag; no madmom output)", "ref", None),
                              ("online + reference beats", "online+ref", None),
                              ("online beats, agreeing tracks only", "online", {t.name for t in agree}),
                              ("reference beats, agreeing tracks only", "ref", {t.name for t in agree})):
        if only is not None and len(only) < 30:
            say(f"| {name}: too few agreeing tracks ({len(only)}) |")
            continue
        scs = run.crossfit(spec, dev, beats, only)
        d = summarize(results(dev, scs, margin))
        out[name] = d["acc_mean"]
        say(f"| {name}: all dev {fmt(d)}")
        if agree:
            say(f"| {name}: agreeing dev {fmt(summarize(results(agree, scs, margin)))}")
    return out


def self_clean(run, dev, spec, margin, beats: str) -> list:
    """Drop the dev tracks whose Traktor '1' the model (cross-fitted, on the reference beats) contradicts: right on
    < 25 % of their reference beats. Uses no madmom output."""
    scs = run.crossfit(spec, dev, beats, eval_beats="ref")
    res = results(dev, scs, margin, "ref")
    drop = {tr.name for tr, s in res if s.n and s.correct / s.n < 0.25}
    keep = [tr for tr in dev if tr.name not in drop]
    agree_dropped = [tr.agree for tr in dev if tr.name in drop and np.isfinite(tr.agree)]
    say(f"\nSelf-cleaning ({beats} beats): {len(drop)} of {len(dev)} dev tracks dropped; of those madmom measured "
        f"{len(agree_dropped)}, agreeing with Traktor (>= 90 %) on {sum(a >= 0.9 for a in agree_dropped)}, mean agreement "
        f"{100 * np.mean(agree_dropped) if agree_dropped else float('nan'):.0f} %")
    scs = run.crossfit(spec, dev, beats, {t.name for t in keep})
    say(f"\n{HEAD}\n| self-cleaned training ({beats}): all dev {fmt(summarize(results(dev, scs, margin)))}")
    agree = [tr for tr in dev if tr.agree >= 0.9]
    if agree:
        say(f"| self-cleaned training ({beats}): agreeing dev {fmt(summarize(results(agree, scs, margin)))}")
    return keep


def link_tune(dev, scs) -> Filter:
    say(f"\n### Link's bar (moves only when confident), dev half, online beats (cross-fitted)\n\n{LINK_HEAD}")
    best, best_key = None, -1e9
    for commit, switch, hold in product((0.6, 0.75, 0.9), (0.75, 0.9, 0.97), (4, 8, 16)):
        if switch < commit:
            continue
        f = Filter(commit=commit, switch=switch, hold=hold)
        d = link_summary(link_results(dev, scs, f))
        key = d["acc_mean"] - 0.02 * d["moves_per_min"] * d["wrong_moves_share"] * 10
        if key > best_key:
            best, best_key = f, key
        if hold == 8:
            say(f"| commit {commit} switch {switch} hold {hold} {link_fmt(d)}")
    say(f"\nChosen on dev (mean right minus 0.2 x wrong moves/min): commit {best.commit}, switch {best.switch}, "
        f"hold {best.hold}")
    return best


def mm_rp(tr):
    mb, mp = tr.extra["mm_beats"], tr.extra["mm_pos"]
    return ref_positions(tr.t_on, mb, mp, np.ones(len(mb), bool))


def test_block(run, test, spec, net, margin, label) -> None:
    for beats in ("online", "ref"):
        say(f"| {label}, {beats} beats {fmt(summarize(results(test, run.score(spec, net, test, beats), margin, beats)))}")
    agree = [tr for tr in test if tr.agree >= 0.9]
    if agree:
        scs = run.score(spec, net, agree)
        say(f"| {label}, online beats, the {len(agree)} test tracks where Traktor and madmom agree "
            f"{fmt(summarize(results(agree, scs, margin)))}")
        for beats in ("ref",):
            say(f"| {label}, {beats} beats, agreeing test tracks "
                f"{fmt(summarize(results(agree, run.score(spec, net, agree, beats), margin, beats)))}")
    mm = [tr for tr in test if "mm_pos" in tr.extra and tr.extra["mm_pos"].max() == 4]
    if mm:
        scs = run.score(spec, net, mm)
        say(f"| {label}, online beats, reference = madmom offline ({len(mm)} 4/4 test tracks) "
            f"{fmt(summarize(results(mm, scs, margin, rp_of=mm_rp)))}")


def export(spec, net, margin, f: Filter, trained_on: str) -> None:
    sd = {k: v.detach().numpy().astype(float).round(7).tolist() for k, v in net.state_dict().items()}
    feats = (["study22:" + n for n in ("e150", "e100", "e150_mean", "flux150", "flux_all", "nov_all", "nov_low",
                                       "nov_all_pos", "flux_g0", "flux_g1", "flux_g2", "flux_g3", "flux_g4", "flux_g5",
                                       "mean_g0", "mean_g1", "mean_g2", "mean_g3", "mean_g4", "mean_g5", "nov_bar_all",
                                       "nov_bar_low")] if "S" in spec.feats else []) + \
        ([n for n in NAMES if n not in spec.drop] if "D" in spec.feats else [])
    (RES / "v2_model.json").write_text(json.dumps({
        "about": "downbeat accumulator model (beat/studies/downbeat/v2/accmodel.py); see REPORT2.md",
        "trained_on": trained_on, "spec": {**spec.__dict__}, "features": feats, "margin": margin,
        "link": {"commit": f.commit, "switch": f.switch, "hold": f.hold}, "params": am.n_params(net),
        "state_dict": sd}, indent=0))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--ship", default="", help="training variant for the final model: online | ref | ref-clean | "
                                                "online-clean (default: the best on dev)")
    a = ap.parse_args()
    torch.set_num_threads(a.jobs)
    t0 = time.time()
    tk = load(a.cache / "traktor_desc.pkl", "traktor")
    gz = load(a.cache / "gtzan_desc.pkl", "gtzan")
    add_agreement(tk, a.cache / "mmdb_traktor")
    run = Runner(a.jobs)
    dev, test = tk[0::2], tk[1::2]
    if a.quick:
        dev, test = dev[:100], test[:100]
    say(f"## Traktor: {len(tk)} tracks (dev = even {len(dev)}, test = odd {len(test)}); madmom offline measured on "
        f"{sum(np.isfinite(t.agree) for t in tk)} (dev {sum(np.isfinite(t.agree) for t in dev)}), Traktor and madmom "
        f"agree (>= 90 %) on {sum(t.agree >= 0.9 for t in tk)} (dev {sum(t.agree >= 0.9 for t in dev)}, test "
        f"{sum(t.agree >= 0.9 for t in test)})\n\n{HEAD}")
    baseline(test)

    rows = grid(run, dev, a.quick)
    _, spec, margin = rows[0]
    say(f"\nBest on dev: {spec.label}, margin {margin:g}")
    ablation(run, dev, spec, margin)
    var = variants(run, dev, spec, margin)
    keep_ref = self_clean(run, dev, spec, margin, "ref")
    keep_on = self_clean(run, dev, spec, margin, "online")
    choices = {"online": ("online", dev), "ref": ("ref", dev), "ref-clean": ("ref", keep_ref),
               "online-clean": ("online", keep_on)}
    ship = a.ship or "ref-clean"
    beats, part = choices[ship]
    say(f"\nShipped variant: {ship} ({beats} beats, {len(part)} dev tracks). Dev scores of the variants: "
        f"{json.dumps({k: round(v, 3) for k, v in var.items()})}")

    say(f"\n## TEST half (odd), {len(test)} tracks; trained on the dev half\n\n{HEAD}")
    baseline(test)
    finals = {}
    for name, (b, p) in choices.items():
        net = run.fit(spec, p, b)
        finals[name] = net
        test_block(run, test, spec, net, margin, f"{spec.label}, margin {margin:g} [trained: {name}, {len(p)} tracks]")
    net = finals[ship]

    say(f"\n### Splits, TEST half, online beats [trained: {ship}]\n")
    res = results(test, run.score(spec, net, test), margin)
    g = split_pooled(res, lambda tr, t: tr.genre)
    say("- by genre (pooled): " + "; ".join(f"{k} {100 * v[0]:.0f} % ({sum(1 for x in test if x.genre == k)} tracks)"
                                           for k, v in g.items()))
    b = split_pooled(res, lambda tr, t: "in a break" if any(x <= t < e for x, e in tr.breaks) else "not in a break")
    say("- breaks (pooled): " + "; ".join(f"{k} {100 * v[0]:.0f} % ({v[1]} beats)" for k, v in b.items()))

    lf = link_tune(dev, run.crossfit(spec, dev, beats, {t.name for t in part}))
    say(f"\n### Link's bar, TEST half, online beats [trained: {ship}]\n\n{LINK_HEAD}")
    say("| today: Link's arbitrary bar | - | 25.0 | 25.0 | 0 | - | 100 | 0 | - |")
    say(f"| {spec.label} commit {lf.commit} switch {lf.switch} hold {lf.hold} "
        f"{link_fmt(link_summary(link_results(test, run.score(spec, net, test), lf)))}")
    export(spec, net, margin, lf, f"Traktor dev half, variant {ship}")

    say(f"\n## GTZAN (human downbeats, 30 s clips, {len(gz)} tracks; dev = even, test = odd)\n\n{HEAD}")
    gdev, gtest = gz[0::2], gz[1::2]
    say(f"| Traktor-trained [{ship}], GTZAN odd half {fmt(summarize(results(gtest, run.score(spec, net, gtest), margin)))}")
    gs = run.crossfit(spec, gdev)
    gm, _ = pick_margin(gdev, gs)
    gnet = run.fit(spec, gdev, "online")
    say(f"| GTZAN-trained (even half, online beats), margin {gm:g}, GTZAN odd half "
        f"{fmt(summarize(results(gtest, run.score(spec, gnet, gtest), gm)))}")
    res = results(gtest, run.score(spec, net, gtest), margin)
    say("- Traktor-trained, by genre (pooled): " + "; ".join(
        f"{k} {100 * v[0]:.0f}" for k, v in split_pooled(res, lambda tr, t: tr.genre).items()))
    say(f"\nScorer: {am.n_params(net)} parameters. Run time {time.time() - t0:.0f} s, {os.cpu_count()} CPUs seen.")
    (RES / ("v2_tables_quick.md" if a.quick else "v2_tables.md")).write_text("\n".join(LOG) + "\n")


if __name__ == "__main__":
    main()
