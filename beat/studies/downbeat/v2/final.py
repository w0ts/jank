"""The second study's final pipeline -> ../results/v2_tables.md, ../results/v2_weights.json.

The recipe found by exp_linear.py: the study's linear accumulator rule, TRAINED on the reference beats (Traktor's grid
shifted by the tracker's lag: no madmom output in the weights), COUNTED and SCORED on Link's own beats (linkbeats.py:
the estimator's grid that Link plays). Here: the margin and the label self-cleaning chosen on dev (2-fold
cross-fitted), Link's confident-only moves tuned on dev, then the test half, the noisy-reference checks, a track-change
simulation, GTZAN, and the weights export.

    PYTHONPATH=..:$BL_REPO python final.py --cache ../cache --jobs 6
"""
from __future__ import annotations

import argparse
import json
import pickle
import time
from itertools import product
from pathlib import Path

import numpy as np

import exp_linear as xl
from analyze import HEAD, fmt, split_pooled, summarize, traktor_group
from evalcore import LINK_HEAD, _breaks, link_fmt, link_score, link_summary
from rules import MATCH_S, SKIP_BEATS, ref_positions, score

HERE = Path(__file__).resolve().parent
RES = HERE.parent / "results"
LOG: list[str] = []
SPECS = {"a5": xl.VARIANTS["a5"], "d5": xl.VARIANTS["d5"]}
MARGINS = (0.0, 0.5, 1.0, 1.5, 2.0)


def say(s: str = "") -> None:
    print(s, flush=True)
    LOG.append(s)


# ---- scores per track (workers see xl.ROWS) ----
def sc_of(F, c, v, w):
    rel = xl.rel_multi(F, c, v.scales, v.delay)
    ready = ~np.isnan(rel).any(axis=(1, 2))
    return xl.rotations(np.nan_to_num(rel)) @ w, ready


def _scores(job):
    name, beats, v, w = job
    r = xl.ROWS[name]
    _, c, _ = xl.stream(r, beats)
    return name, *sc_of(xl.feat_matrix(r, beats, v.feats), c, v, w)


def decide(sc, ready, margin):
    off = np.full(len(sc), -1)
    cur = -1
    for i in np.flatnonzero(ready):
        b = int(np.argmax(sc[i]))
        if cur < 0 or (b != cur and sc[i, b] - sc[i, cur] > margin):
            cur = b
        off[i] = cur
    return off


def decide_link(sc, ready, commit, switch, hold, wait=0, early=0, switch2=None, hold2=None):
    """Link's bar: untouched until the best phase leads the second by `commit` (score units); later it moves only when
    another phase has led the current one by `switch` for `hold` beats in a row; the first move waits for `wait` beats
    of evidence. From `early` beats after the first move on, the later-move rule becomes (switch2, hold2), if given.
    moved[i]: Link's bar moved at beat i."""
    n = len(sc)
    off, moved = np.full(n, -1), np.zeros(n, bool)
    cur, cand, run, first = -1, -1, 0, -1
    rd = np.flatnonzero(ready)
    for i in rd:
        s = sc[i]
        b = int(np.argmax(s))
        if cur < 0:
            if i - rd[0] >= wait and s[b] - np.sort(s)[-2] >= commit:
                cur, moved[i], first = b, True, i
            continue
        late = switch2 is not None and i - first >= early
        sw, hd = (switch2, hold2) if late else (switch, hold)
        if b != cur and s[b] - s[cur] >= sw:
            run = run + 1 if b == cand else 1
            cand = b
            if run >= hd:
                cur, run, moved[i] = b, 0, True
        else:
            cand, run = -1, 0
        off[i] = cur
    return off, moved


class Final(xl.Lab):
    def scores(self, v, w, names, beats="link") -> dict:
        jobs = [(n, beats, v, w) for n in names if len(xl.stream(xl.ROWS[n], beats)[0]) >= SKIP_BEATS + 4]
        return {n: (s, rd) for n, s, rd in self.pool.map(_scores, jobs, chunksize=4)}

    def crossfit_scores(self, v, dev, beats="link", train_beats="ref", only=None) -> dict:
        a, b = dev[0::2], dev[1::2]
        sub = (lambda x: x) if only is None else (lambda x: [n for n in x if n in only])
        out = self.scores(v, self.fit(v, sub(a), train_beats), b, beats)
        out.update(self.scores(v, self.fit(v, sub(b), train_beats), a, beats))
        return out


def results(scs, margin, beats="link", rp_of=None):
    res = []
    for n, (sc, ready) in scs.items():
        r = xl.ROWS[n]
        t, c, rp = xl.stream(r, beats)
        if rp_of is not None:
            rp = rp_of(r)
        res.append((r, score(t, c, decide(sc, ready, margin), rp)))
    return res


def link_results(scs, f):
    res = []
    for n, (sc, ready) in scs.items():
        t, c, rp = xl.stream(xl.ROWS[n], "link")
        off, moved = decide_link(sc, ready, *f)
        res.append((xl.ROWS[n], link_score(t, c, off, moved, rp)))
    return res


def acc(res) -> float:
    return summarize(res).get("acc_mean", 0.0)


def pick_margin(scs) -> float:
    a = {m: acc(results(scs, m)) for m in MARGINS}
    return max(m for m in MARGINS if a[m] >= max(a.values()) - 0.01)


def self_clean(lab, v, dev, margin) -> list:
    """Dev tracks whose Traktor '1' the cross-fitted rule contradicts on their own reference beats (right < 25 %) are
    left out of training. No madmom output involved."""
    res = results(lab.crossfit_scores(v, dev, beats="ref"), margin, "ref")
    drop = {r["name"] for r, s in res if s.n and s.correct / s.n < 0.25}
    return [n for n in dev if n not in drop]


def agreement(names, mmdb: Path) -> dict:
    out = {}
    for n in names:
        f = mmdb / f"{n}.npz"
        r = xl.ROWS[n]
        if not f.exists() or len(r["t_ref"]) < 2:
            continue
        z = np.load(f)
        mb, mp = z["beats"], z["pos"].astype(int)
        if len(mb) < 16:
            continue
        j = np.clip(np.searchsorted(r["t_ref"], mb), 1, len(r["t_ref"]) - 1)
        j = np.where(np.abs(r["t_ref"][j - 1] - mb) <= np.abs(r["t_ref"][j] - mb), j - 1, j)
        ok = np.abs(r["t_ref"][j] - mb) <= MATCH_S
        if ok.sum() >= 16:
            r["mm_beats"], r["mm_pos"] = mb, mp
            out[n] = float(np.mean(((mp[ok] - 1) - (r["ref_pos"][j[ok]] - 1)) % 4 == 0))
    return out


def mm_rp(r):
    return ref_positions(r["t_link"], r["mm_beats"], r["mm_pos"], np.ones(len(r["mm_beats"]), bool))


def mm_rp_on(r):
    return ref_positions(r["t_on"], r["mm_beats"], r["mm_pos"], np.ones(len(r["mm_beats"]), bool))


def track_changes(v, w, test, margin, rng) -> None:
    """A DJ cut at the same tempo: the end of track A, then track B with Link's count running on (B's bar at a random
    phase against it). Re-lock = seconds after the cut to the first 8 right scored beats of B."""
    say("\n### Track change (simulated cut, same tempo), TEST half pairs, Link beats\n")
    for reset in (False, True):
        locks = []
        for a, b in zip(test[0::2], test[1::2]):
            ra, rb = xl.ROWS[a], xl.ROWS[b]
            if len(ra["t_link"]) < 64 or len(rb["t_link"]) < 64:
                continue
            Fa, Fb = xl.feat_matrix(ra, "link", v.feats), xl.feat_matrix(rb, "link", v.feats)
            cb = rb["c_link"] - rb["c_link"][0] + ra["c_link"][-1] + 1 + int(rng.integers(4))
            if reset:
                sc, ready = sc_of(Fb, cb, v, w)
            else:
                sc, ready = sc_of(np.vstack([Fa, Fb]), np.concatenate([ra["c_link"], cb]), v, w)
                sc, ready = sc[len(Fa):], ready[len(Fa):]
            off = decide(sc, ready, margin)
            rp = rb["rp_link"]
            ok = (off >= 0) & (rp >= 0) & ((cb - off) % 4 == rp)
            sc_mask = rp >= 0
            t = rb["t_link"] - rb["t_link"][0]
            run, lock = 0, np.nan
            for i in np.flatnonzero(sc_mask):
                run = run + 1 if ok[i] else 0
                if run == 8:
                    lock = t[i]
                    break
            locks.append(lock)
        L = np.array(locks)
        f = np.isfinite(L)
        say(f"- {'accumulators reset at the cut (a detected change)' if reset else 'no reset (memory carries over)'}: "
            f"{len(L)} cuts, re-lock median {np.nanmedian(L):.0f} s; within 15 s {100 * np.mean(L <= 15):.0f} %, "
            f"30 s {100 * np.mean(L <= 30):.0f} %, 60 s {100 * np.mean(L <= 60):.0f} %, never {100 * np.mean(~f):.0f} %")


def export(v, w, margin, f, trained_on) -> None:
    from extract import FEATURES
    feats = list(FEATURES) if "S" in v.feats.split("+") else []
    if "Dp" in v.feats.split("+"):
        from feats import NAMES
        feats += [NAMES[i] for i in xl.POS_D] + [f"dE{g}+" for g in range(8)] + [f"dE{g}-" for g in range(8)] + ["|tr-1|"]
    (RES / "v2_weights.json").write_text(json.dumps({
        "about": "downbeat linear accumulator rule (first study's structure); REPORT2.md; derived from the user's "
                 "Traktor grids and audio only (reference beats, no madmom output)",
        "trained_on": trained_on, "features": feats, "bars": list(v.scales), "mode": "exp", "l2": v.l2,
        "rel": "(acc - mean4) / (|mean4| + 1e-6)", "w_layout": "w[f * 4 + p]: feature f at bar position p from the "
                                                                "candidate 1", "w": [float(x) for x in w],
        "margin": margin, "link": dict(zip(("commit", "switch", "hold", "wait", "early", "switch2", "hold2"), f))}, indent=1))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=6)
    a = ap.parse_args()
    t0 = time.time()
    names = {}
    for ds in ("traktor", "gtzan"):
        rows = [r for r in pickle.loads((a.cache / f"{ds}_desc.pkl").read_bytes())["rows"] if "error" not in r]
        names[ds] = [r["name"] for r in rows]
        for r in rows:
            xl.ROWS[r["name"]] = r
    from eval.data import DATA
    meta = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
    genres = json.loads((HERE.parent / "genres.json").read_text())
    for n in names["traktor"]:
        xl.ROWS[n]["genre"] = traktor_group(genres.get(n, ""))
        xl.ROWS[n]["breaks"] = _breaks(meta.get(n, {}))
    for n in names["gtzan"]:
        xl.ROWS[n]["genre"] = n.split("_")[1]
    agree = agreement(names["traktor"], a.cache / "mmdb_traktor")
    lab = Final(a.jobs)
    dev, test = names["traktor"][0::2], names["traktor"][1::2]
    say(f"## Traktor: {len(names['traktor'])} tracks, dev = even {len(dev)}, test = odd {len(test)}. madmom offline "
        f"measured on {len(agree)}; Traktor and madmom agree (>= 90 % of beats) on {sum(x >= 0.9 for x in agree.values())}"
        f" (dev {sum(agree.get(n, 0) >= 0.9 for n in dev)}, test {sum(agree.get(n, 0) >= 0.9 for n in test)})\n")

    say(f"### Dev half, Link beats, 2-fold cross-fitted, trained on reference beats\n\n{HEAD}")
    best = None
    for key, v in SPECS.items():
        scs = lab.crossfit_scores(v, dev)
        m = pick_margin(scs)
        say(f"| {key}: {v.name}, margin {m:g}, all dev tracks for training {fmt(summarize(results(scs, m)))}")
        keep = self_clean(lab, v, dev, m)
        scs_c = lab.crossfit_scores(v, dev, only=set(keep))
        d = summarize(results(scs_c, m))
        say(f"| {key}: {v.name}, margin {m:g}, self-cleaned training ({len(keep)} of {len(dev)} dev tracks kept) {fmt(d)}")
        dropped = [agree[n] for n in dev if n not in keep and n in agree]
        say(f"|   (madmom measured {len(dropped)} of the dropped tracks: agreeing with Traktor on "
            f"{sum(x >= 0.9 for x in dropped)}, mean agreement {100 * np.mean(dropped) if dropped else float('nan'):.0f} %) |")
        for clean, s_ in ((False, scs), (True, scs_c)):
            sc_acc = acc(results(s_, m))
            if best is None or sc_acc > best[0]:
                best = (sc_acc, key, v, m, keep if clean else dev, s_)
    _, key, v, margin, train_set, dev_scs = best
    say(f"\nChosen on dev: {key} ({v.name}), margin {margin:g}, training on {len(train_set)} dev tracks")

    say(f"\n### Link's bar: moves only when confident, dev half, Link beats (cross-fitted)\n\n{LINK_HEAD}")
    grid_rows = []
    for commit, switch, hold, wait in product((1.0, 2.0, 3.0), (1.0, 2.0, 3.0), (8, 16, 32), (0, 16, 32, 64)):
        d = link_summary(link_results(dev_scs, (commit, switch, hold, wait)))
        grid_rows.append(((commit, switch, hold, wait), d))
        if commit == 2.0 and hold == 16:
            say(f"| commit {commit} switch {switch} hold {hold} wait {wait} {link_fmt(d)}")
    for early, sw2, hd2 in product((32, 64, 128), (2.0, 3.0), (16, 32)):
        f = (2.0, 1.0, 8, 0, early, sw2, hd2)
        d = link_summary(link_results(dev_scs, f))
        grid_rows.append((f, d))
        say(f"| hybrid: commit 2 switch 1 hold 8 for {early} beats, then switch {sw2} hold {hd2} {link_fmt(d)}")
    responsive = max(grid_rows, key=lambda x: x[1]["acc_mean"])[0]
    stable_rows = [x for x in grid_rows if x[1]["later_per_min"] <= 0.1]
    bestf = max(stable_rows, key=lambda x: x[1]["acc_mean"])[0] if stable_rows else responsive
    say(f"\nChosen on dev: responsive (best mean right) commit/switch/hold/wait {responsive}; stable (best "
        f"mean right with at most 0.1 later moves a minute) {bestf}")

    w = lab.fit(v, train_set, "ref")
    say(f"\n## TEST half (odd, {len(test)} tracks), weights trained on {len(train_set)} dev tracks' reference beats\n\n{HEAD}")
    base = json.loads((RES / "weights.json").read_text())
    for beats in ("online", "link", "ref"):
        sb = lab.scores(xl.VARIANTS["a"], np.asarray(base["w"]), test, beats)
        say(f"| first study's weights, its protocol (features 4 beats ahead), {beats} beats "
            f"{fmt(summarize(results(sb, base['margin'], beats)))}")
    sb = lab.scores(xl.VARIANTS["a5"], np.asarray(base["w"]), test, "online")
    say(f"| first study's weights, causal (delay 5), online beats {fmt(summarize(results(sb, base['margin'], 'online')))}")
    test_scs = {}
    for beats in ("link", "online", "ref"):
        test_scs[beats] = lab.scores(v, w, test, beats)
        say(f"| NEW: {v.name}, margin {margin:g}, {beats} beats {fmt(summarize(results(test_scs[beats], margin, beats)))}")
    ag = [n for n in test if agree.get(n, 0) >= 0.9]
    sub = {n: test_scs["link"][n] for n in ag if n in test_scs["link"]}
    sb = lab.scores(xl.VARIANTS["a"], np.asarray(base["w"]), ag, "online")
    say(f"| first study's weights, online beats, {len(ag)} test tracks where Traktor and madmom agree "
        f"{fmt(summarize(results(sb, base['margin'], 'online')))}")
    say(f"| NEW, Link beats, the same {len(ag)} agreeing test tracks {fmt(summarize(results(sub, margin)))}")
    mm = [n for n in test if "mm_pos" in xl.ROWS[n] and xl.ROWS[n]["mm_pos"].max() == 4 and n in test_scs["link"]]
    say(f"| NEW, Link beats, reference = madmom offline ({len(mm)} 4/4 test tracks) "
        f"{fmt(summarize(results({n: test_scs['link'][n] for n in mm}, margin, rp_of=mm_rp)))}")
    sb = lab.scores(xl.VARIANTS["a"], np.asarray(base["w"]), mm, "online")
    say(f"| first study's weights, online beats, reference = madmom offline (same {len(mm)} tracks) "
        f"{fmt(summarize(results(sb, base['margin'], 'online', rp_of=mm_rp_on)))}")
    say(f"| Traktor's own '1' scored against madmom offline on Link beats (same {len(mm)} tracks): see the agreement "
        f"line above; mean agreement {100 * np.mean([agree[n] for n in mm]):.0f} % |")
    same = [n for n in test if len(xl.ROWS[n]['t_on']) > 16 and abs(np.median(np.diff(xl.ROWS[n]['t_on'])) /
                                                                       np.median(np.diff(xl.ROWS[n]['t_ref'])) - 1) < 0.06]
    say(f"| NEW, Link beats, test tracks where the tracker's tempo = Traktor's ({len(same)}) "
        f"{fmt(summarize(results({n: test_scs['link'][n] for n in same if n in test_scs['link']}, margin)))}")

    res = results(test_scs["link"], margin)
    g = split_pooled(res, lambda r, t: r["genre"])
    say("\nSplits, TEST, Link beats, pooled: by genre " + "; ".join(
        f"{k} {100 * x[0]:.0f} % ({sum(1 for n in test if xl.ROWS[n]['genre'] == k)} tracks)" for k, x in g.items()))
    b = split_pooled(res, lambda r, t: "in a break" if any(x <= t < e for x, e in r["breaks"]) else "not in a break")
    say("; ".join(f"{k} {100 * x[0]:.0f} % ({x[1]} beats)" for k, x in b.items()))

    say(f"\n### Link's bar, TEST half, Link beats\n\n{LINK_HEAD}")
    say("| today: Link's arbitrary bar | - | 25.0 | 25.0 | 0 | - | 100 | 0 | - |")
    for name, f in (("responsive", responsive), ("stable", bestf)):
        say(f"| NEW {name}: {f} "
            f"{link_fmt(link_summary(link_results(test_scs['link'], f)))}")
    track_changes(v, w, test, margin, np.random.default_rng(1))
    vi = xl.VARIANTS["i5"]
    wi = lab.fit(vi, train_set, "ref")
    mi = 1.5
    say(f"\nAlternative with half the memory ({vi.name}, margin {mi:g}, same training tracks), TEST:\n\n{HEAD}")
    say(f"| {vi.name}, margin {mi:g}, link beats {fmt(summarize(results(lab.scores(vi, wi, test, 'link'), mi)))}")
    track_changes(vi, wi, test, mi, np.random.default_rng(1))
    export(v, w, margin, bestf, f"Traktor dev half, reference beats, {len(train_set)} tracks (self-cleaned: "
                                f"{len(train_set) < len(dev)})")

    gdev, gtest = names["gtzan"][0::2], names["gtzan"][1::2]
    say(f"\n## GTZAN (human downbeats, 30 s clips), odd half ({len(gtest)})\n\n{HEAD}")
    sb = lab.scores(xl.VARIANTS["a"], np.asarray(base["w"]), gtest, "online")
    say(f"| first study's weights, online beats {fmt(summarize(results(sb, base['margin'], 'online')))}")
    for beats in ("link", "online"):
        say(f"| NEW (Traktor-trained), {beats} beats "
            f"{fmt(summarize(results(lab.scores(v, w, gtest, beats), margin, beats)))}")
    gw = lab.fit(v, gdev, "ref")
    say(f"| NEW structure trained on GTZAN even half (reference beats), link beats "
        f"{fmt(summarize(results(lab.scores(v, gw, gtest, 'link'), margin)))}")
    res = results(lab.scores(v, w, gtest, "link"), margin)
    say("- NEW (Traktor-trained), Link beats, by genre: " + "; ".join(
        f"{k} {100 * x[0]:.0f}" for k, x in split_pooled(res, lambda r, t: r["genre"]).items()))
    say(f"\n{len(w)} weights. Run time {time.time() - t0:.0f} s.")
    (RES / "v2_tables.md").write_text("\n".join(LOG) + "\n")


if __name__ == "__main__":
    main()
