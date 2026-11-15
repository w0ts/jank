"""The second downbeat study's experiments -> results/v2_tables.md, results/v2_model.json.

Protocol as the first study: Traktor split by position (even = dev: every choice and every weight; odd = test), the
tracker's online beats; then GTZAN. Filter settings are chosen on dev with 2-fold cross-fitted predictions (the model
that scores a dev track never saw it); the final model is then trained on the whole dev half and scored on test.

    PYTHONPATH=..:$BL_REPO python evaluate2.py --cache ../cache --threads 8 [--quick]
"""
from __future__ import annotations

import argparse
import json
import time
from dataclasses import replace
from itertools import product
from pathlib import Path

import numpy as np
import torch

from analyze import split_pooled
from evalcore import (HEAD, LINK_HEAD, Filter, add_agreement, fmt, link_fmt, link_summary, load, rescore_against,
                      run_filter, summarize, usable)
from feats import NAMES
from model import Spec, logprobs, n_params, train

HERE = Path(__file__).resolve().parent
RES = HERE.parent / "results"
TEMPS = (0.1, 0.2, 0.35, 0.6, 1.0)
EPSS = (0.002, 0.005, 0.01, 0.02, 0.05)
MARGINS = (0.2, 0.4, 0.6, 0.8)
LOG: list[str] = []


def say(s: str = "") -> None:
    print(s, flush=True)
    LOG.append(s)


def samples(tracks, beats: str):
    out = []
    for tr in tracks:
        if usable(tr, beats):
            _, _, rp, N = tr.stream(beats)
            out.append((N, rp))
    return out


def fit(spec: Spec, tracks, beats) -> torch.nn.Module:
    return train(spec, [s for b in beats.split("+") for s in samples(tracks, b)])


def predict(spec, net, tracks, beats="online") -> dict:
    return {tr.name: logprobs(spec, net, tr.stream(beats)[3]) for tr in tracks if usable(tr, beats)}


def crossfit(spec: Spec, dev, train_beats: str, eval_beats: str = "online", only: set | None = None) -> dict:
    """Predictions for every dev track from a model trained on the other fold (on its tracks in `only`, if given)."""
    a, b = dev[0::2], dev[1::2]

    def sub(x):
        return x if only is None else [t for t in x if t.name in only]
    lps = predict(spec, fit(spec, sub(a), train_beats), b, eval_beats)
    lps.update(predict(spec, fit(spec, sub(b), train_beats), a, eval_beats))
    return lps


def tune(dev, lps, delay: int, beats: str = "online") -> tuple[Filter, float]:
    """T and eps by dev mean accuracy (margin 0); then the largest margin that costs at most 1 point (as the study)."""
    best, best_acc = None, -1.0
    for T, e in product(TEMPS, EPSS):
        f = Filter(temp=T, eps=e, delay=delay, margin=0.0)
        acc = summarize(run_filter(dev, lps, beats, f)).get("acc_mean", 0.0)
        if acc > best_acc:
            best, best_acc = f, acc
    chosen = best
    for m in MARGINS:
        f = replace(best, margin=m)
        if summarize(run_filter(dev, lps, beats, f)).get("acc_mean", 0.0) >= best_acc - 0.01:
            chosen = f
    return chosen, best_acc


def baseline(dev, test) -> None:
    """The first study's rule from its weights.json, on this cache's features: does the 61 % reproduce?"""
    from analyze import LinearRule
    from linear import decide_linear
    from rules import score
    w = json.loads((RES / "weights.json").read_text())
    rule = LinearRule("all 22", tuple(range(22)), w["bars"], w["mode"], tuple(w["w"]), w["margin"], "traktor dev")
    for name, part in (("dev", dev), ("test", test)):
        res = []
        for tr in part:
            if usable(tr, "online"):
                off = decide_linear(tr.S_on, tr.c_on, list(rule.cols), rule.bars, np.asarray(rule.w), rule.mode,
                                    rule.margin)
                res.append((tr, score(tr.t_on, tr.c_on, off, tr.rp_on)))
        say(f"| first study: {rule.label}, {name} half (recomputed from this cache) {fmt(summarize(res))}")


def model_grid(dev, quick: bool) -> list:
    specs = [Spec("lin", 4, 1), Spec("mlp", 4, 1, 32), Spec("mlp", 8, 1, 32), Spec("mlp", 8, 2, 64),
             Spec("gru", 0, 1, 24, epochs=20), Spec("gru", 0, 2, 32, epochs=20)]
    if quick:
        specs = specs[:2] + specs[4:5]
    say(f"\n### Per-beat models, dev half, online beats (2-fold cross-fitted; filter tuned on dev)\n\n{HEAD}")
    rows = []
    for spec in specs:
        t0 = time.time()
        lps = crossfit(spec, dev, "online")
        f, _ = tune(dev, lps, spec.delay)
        d = summarize(run_filter(dev, lps, "online", f))
        rows.append((d["acc_mean"], spec, f))
        say(f"| {spec.label}, {f.label} {fmt(d)}  ({time.time() - t0:.0f} s)")
    rows.sort(key=lambda r: -r[0])
    return rows


def ablation(dev, spec: Spec, f: Filter) -> None:
    groups = {"no chroma change (cc1, cc2)": ("cc1", "cc2"), "no onsets O*": tuple(f"O{g}" for g in range(8)),
              "no levels E*": tuple(f"E{g}" for g in range(8)), "no level changes dE*, nov2": (
                  *(f"dE{g}" for g in range(8)), "nov2"), "no tracker timing tr": ("tr",)}
    say(f"\n### Ablation, dev half, {spec.label} (cross-fitted, filter {f.label})\n\n{HEAD}")
    for name, drop in groups.items():
        s = replace(spec, cols=tuple(i for i, n in enumerate(NAMES) if n not in drop))
        lps = crossfit(s, dev, "online")
        say(f"| {name} {fmt(summarize(run_filter(dev, lps, 'online', f)))}")


def training_variants(dev, spec: Spec, f: Filter) -> dict:
    """Which beats and which tracks to learn from. Scored on dev online beats, cross-fitted."""
    say(f"\n### What to train on, dev half, online beats, {spec.label} (cross-fitted, filter {f.label})\n\n{HEAD}")
    out = {}
    agree = [tr for tr in dev if tr.agree >= 0.9]
    for name, beats, subset in (("online beats (madmom's online tracker places the beats)", "online", None),
                                ("reference beats shifted by the tracker's lag (no madmom output)", "ref", None),
                                ("both", "online+ref", None),
                                ("online beats, only tracks where Traktor and madmom agree", "online", agree)):
        if subset is not None and len(subset) < 20:
            say(f"| {name}: too few tracks ({len(subset)}) |")
            continue
        lps = crossfit(spec, dev, beats, only=None if subset is None else {tr.name for tr in subset})
        d = summarize(run_filter(dev, lps, "online", f))
        da = summarize(run_filter(agree, lps, "online", f))
        out[name] = d["acc_mean"]
        say(f"| {name}: all dev {fmt(d)}")
        say(f"| {name}: dev tracks where Traktor and madmom agree ({len(agree)}) {fmt(da)}")
    return out


def self_clean(dev, spec: Spec, f: Filter, beats: str) -> list:
    """Drop the dev tracks whose Traktor '1' the model (cross-fitted, on the reference beats) confidently contradicts:
    right on < 25 % of the reference beats. No madmom output involved."""
    lps = crossfit(spec, dev, beats, "ref")
    keep, dropped = [], 0
    for tr, s in run_filter(dev, lps, "ref", f):
        if s.n and s.correct / s.n < 0.25:
            dropped += 1
        else:
            keep.append(tr)
    say(f"\nSelf-cleaning: {dropped} of {len(dev)} dev tracks dropped (model right on < 25 % of their reference beats)")
    lps = crossfit(spec, dev, beats, only={t.name for t in keep})
    d = summarize(run_filter(dev, lps, "online", f))
    say(f"\n{HEAD}\n| self-cleaned training ({beats}), all dev (cross-fitted) {fmt(d)}")
    agree = [tr for tr in dev if tr.agree >= 0.9]
    say(f"| self-cleaned training ({beats}), dev tracks where Traktor and madmom agree {fmt(summarize(run_filter(agree, lps, 'online', f)))}")
    return keep


def link_tune(dev, lps, f: Filter) -> Filter:
    say(f"\n### Link's bar (moves only when confident), dev half, online beats\n\n{LINK_HEAD}")
    best, best_key = f, -1.0
    for commit, switch, hold in product((0.8, 0.9, 0.95, 0.98), (0.95, 0.98, 0.99), (4, 8, 16)):
        if switch < commit:
            continue
        g = replace(f, commit=commit, switch=switch, hold=hold)
        d = link_summary(run_filter(dev, lps, "online", g, link=True))
        key = d["acc_mean"] - 0.5 * d["wrong_moves_share"] * d["moves_per_min"]
        if key > best_key:
            best, best_key = g, key
        if hold == 8:
            say(f"| commit {commit} switch {switch} hold {hold} {link_fmt(d)}")
    say(f"\nChosen on dev: commit {best.commit}, switch {best.switch}, hold {best.hold}")
    return best


def test_section(test, spec, net, f, label: str) -> None:
    for beats in ("online", "ref"):
        lps = predict(spec, net, test, beats)
        say(f"| {label}, {beats} beats {fmt(summarize(run_filter(test, lps, beats, f)))}")


def splits(test, spec, net, f) -> None:
    lps = predict(spec, net, test)
    res = run_filter(test, lps, "online", f)
    g = split_pooled(res, lambda tr, t: tr.genre)
    say("- by genre (pooled): " + "; ".join(f"{k} {100 * v[0]:.0f} % ({sum(1 for x in test if x.genre == k)} tracks)"
                                           for k, v in g.items()))
    b = split_pooled(res, lambda tr, t: "in a break" if any(a <= t < e for a, e in tr.breaks) else "not in a break")
    say("- breaks (pooled): " + "; ".join(f"{k} {100 * v[0]:.0f} % ({v[1]} beats)" for k, v in b.items()))


def export(spec: Spec, net, f: Filter, path: Path, trained_on: str) -> None:
    sd = {k: v.detach().numpy().round(6).tolist() for k, v in net.state_dict().items()}
    path.write_text(json.dumps({"spec": spec.__dict__, "filter": f.__dict__, "features": list(NAMES),
                                "params": n_params(net), "trained_on": trained_on, "state_dict": sd}))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--threads", type=int, default=8)
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    torch.set_num_threads(a.threads)
    t0 = time.time()
    tk = load(a.cache / "traktor_desc.pkl", "traktor")
    add_agreement(tk, a.cache / "mmdb_traktor")
    dev, test = tk[0::2], tk[1::2]
    if a.quick:
        dev, test = dev[:120], test[:120]
    say(f"## Traktor: {len(tk)} tracks (dev = even {len(dev)}, test = odd {len(test)}); madmom offline measured on "
        f"{sum(np.isfinite(t.agree) for t in tk)}, agreeing (>= 90 %) on {sum(t.agree >= 0.9 for t in tk)}\n")
    say(HEAD)
    baseline(dev, test)

    rows = model_grid(dev, a.quick)
    _, spec, f = rows[0]
    say(f"\nBest on dev: {spec.label}, {f.label}")
    ablation(dev, spec, f)
    variants = training_variants(dev, spec, f)
    clean_beats = "ref"
    keep = self_clean(dev, spec, f, clean_beats)

    say(f"\n## TEST half (odd), {len(test)} tracks; models trained on the whole dev half\n\n{HEAD}")
    finals = {}
    agree_test = [tr for tr in test if tr.agree >= 0.9]
    for label, beats, part in (("online beats, all dev", "online", dev), ("reference beats, all dev", "ref", dev),
                               ("reference beats, self-cleaned dev", "ref", keep)):
        net = fit(spec, part, beats)
        finals[label] = (net, beats, part)
        test_section(test, spec, net, f, f"{spec.label} {f.label} [trained: {label}]")
        lps = predict(spec, net, agree_test)
        say(f"| ... same, test tracks where Traktor and madmom agree ({len(agree_test)}) "
            f"{fmt(summarize(run_filter(agree_test, lps, 'online', f)))}")
        mm = rescore_against(test)
        lps = predict(spec, net, mm)
        say(f"| ... same, reference = madmom offline ({len(mm)} 4/4 test tracks) "
            f"{fmt(summarize(run_filter(mm, lps, 'online', f)))}")
    say(f"\nTraining-variant dev scores: {json.dumps({k: round(v, 3) for k, v in variants.items()})}")

    ship = "reference beats, self-cleaned dev"
    net, beats, part = finals[ship]
    say(f"\n### Splits, TEST half, online beats, [trained: {ship}]\n")
    splits(test, spec, net, f)
    lf = link_tune(dev, crossfit(spec, dev, beats, only={t.name for t in part}), f)
    say(f"\n### Link's bar, TEST half, online beats, [trained: {ship}]\n\n{LINK_HEAD}")
    say(f"| {spec.label} commit {lf.commit} switch {lf.switch} hold {lf.hold} "
        f"{link_fmt(link_summary(run_filter(test, predict(spec, net, test), 'online', lf, link=True)))}")
    export(spec, net, lf, RES / "v2_model.json", f"Traktor dev half ({ship})")

    say("\n## GTZAN (human downbeats, 30 s clips)\n")
    gz = load(a.cache / "gtzan_desc.pkl", "gtzan")
    gdev, gtest = gz[0::2], gz[1::2]
    say(HEAD)
    say(f"| Traktor-trained [{ship}], GTZAN odd half {fmt(summarize(run_filter(gtest, predict(spec, net, gtest), 'online', f)))}")
    gspec = spec
    gl = crossfit(gspec, gdev, "online")
    gf, _ = tune(gdev, gl, gspec.delay)
    gnet = fit(gspec, gdev, "online")
    say(f"| GTZAN-trained (even half, online beats), {gf.label}, GTZAN odd half "
        f"{fmt(summarize(run_filter(gtest, predict(gspec, gnet, gtest), 'online', gf)))}")
    res = run_filter(gtest, predict(spec, net, gtest), "online", f)
    g = split_pooled(res, lambda tr, t: tr.genre)
    say("- Traktor-trained, by genre (pooled): " + "; ".join(f"{k} {100 * v[0]:.0f}" for k, v in g.items()))
    say(f"\n{n_params(net)} parameters; {time.time() - t0:.0f} s")
    (RES / ("v2_tables_quick.md" if a.quick else "v2_tables.md")).write_text("\n".join(LOG) + "\n")


if __name__ == "__main__":
    main()
