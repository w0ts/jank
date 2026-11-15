"""Score the accent rules, the baselines and the downbeat models from extract.py's output; tables -> results/tables.md.

    ../../.venv/bin/python analyze.py
"""
from __future__ import annotations

import json
import pickle
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from itertools import product
from pathlib import Path

import numpy as np

from linear import decide_linear, track_samples, train
from rules import (SKIP_BEATS, Rule, Score, accent, count_beats, decide, offsets_from_downbeats, ref_positions, score,
                   valid_4)

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
RES = HERE / "results"
COMBOS = ("e150+nov_all", "flux150+nov_all", "e150+flux_all", "flux_g3+nov_all", "flux_g0+nov_bar_all")
BARS = (2, 4, 8, 16)
LIN_SETS = {
    "all 22": None,
    "6 band fluxes + nov_all + nov_bar_all": ["flux_g0", "flux_g1", "flux_g2", "flux_g3", "flux_g4", "flux_g5",
                                              "nov_all", "nov_bar_all"],
    "e150 + flux_g3 + nov_all": ["e150", "flux_g3", "nov_all"],
}


@dataclass(frozen=True)
class LinearRule:
    set_name: str
    cols: tuple[int, ...]
    bars: int
    mode: str
    w: tuple[float, ...]
    margin: float = 0.0
    trained_on: str = "dev"

    @property
    def label(self) -> str:
        m = f" hyst{self.margin:g}" if self.margin else ""
        return f"linear [{self.set_name}] {self.mode} N={self.bars}{m} (weights: {self.trained_on})"


@dataclass
class Track:
    name: str
    genre: str
    t_on: np.ndarray
    f_on: np.ndarray
    t_ref: np.ndarray
    f_ref: np.ndarray
    ref_pos: np.ndarray
    valid: np.ndarray
    bars: int
    non4: int
    breaks: list[tuple[float, float]] = field(default_factory=list)
    c_on: np.ndarray = field(default=None)
    c_ref: np.ndarray = field(default=None)
    rp_on: np.ndarray = field(default=None)
    rp_ref: np.ndarray = field(default=None)


def traktor_group(g: str) -> str:
    g = g.lower()
    for key, grp in (("techno", "techno"), ("house", "house"), ("pop", "pop / hyperpop"), ("electro", "electro / breaks"),
                     ("break", "electro / breaks"), ("dubstep", "electro / breaks"), ("bass", "electro / breaks"),
                     ("garage", "electro / breaks"), ("grime", "electro / breaks"), ("trance", "techno"),
                     ("transe", "techno"), ("ambient", "ambient / idm / exp."), ("idm", "ambient / idm / exp."),
                     ("experimental", "ambient / idm / exp."), ("downtempo", "ambient / idm / exp."),
                     ("abstract", "ambient / idm / exp."), ("classical", "ambient / idm / exp."),
                     ("glitch", "ambient / idm / exp."), ("leftfield", "ambient / idm / exp.")):
        if key in g:
            return grp
    return "other / untagged"


def break_segments(rec: dict) -> list[tuple[float, float]]:
    drops = sorted(rec.get("drops", []))
    segs = []
    for b in rec.get("breaks", []):
        nxt = [d for d in drops if d > b]
        segs.append((b, nxt[0] if nxt else b + 30.0))
    return segs


def load(dataset: str) -> list[Track]:
    rows = pickle.loads((RES / f"{dataset}.pkl").read_bytes())["rows"]
    meta, genres = {}, {}
    if dataset == "traktor":
        meta = {r["name"]: r for r in json.loads((REPO / "data" / "traktor" / "index.json").read_text())}
        genres = json.loads((HERE / "genres.json").read_text())
    out = []
    for r in rows:
        if "error" in r:
            print("error", r["name"], r["error"][-200:], file=sys.stderr)
            continue
        valid, bars, non4 = valid_4(r["ref_pos"])
        g = traktor_group(genres.get(r["name"], "")) if dataset == "traktor" else r["name"].split("_")[1]
        tr = Track(r["name"], g, np.asarray(r["online"]), r["f_online"], np.asarray(r["ref_beats"]), r["f_ref"],
                   np.asarray(r["ref_pos"]), valid, bars, non4, break_segments(meta.get(r["name"], {})))
        tr.c_on = count_beats(tr.t_on)
        tr.c_ref = np.arange(len(tr.t_ref))
        tr.rp_on = ref_positions(tr.t_on, tr.t_ref, tr.ref_pos, valid)
        tr.rp_ref = np.where(valid, tr.ref_pos - 1, -1)
        out.append(tr)
    return out


def streams(tr: Track, beats: str):
    return (tr.t_on, tr.f_on, tr.c_on, tr.rp_on) if beats == "online" else (tr.t_ref, tr.f_ref, tr.c_ref, tr.rp_ref)


def run_rule(tracks: list[Track], rule, beats: str, names: tuple[str, ...]) -> list[tuple[Track, Score]]:
    res = []
    for tr in tracks:
        t, f, c, rp = streams(tr, beats)
        if len(t) < SKIP_BEATS + 4:
            continue
        if isinstance(rule, LinearRule):
            off = decide_linear(f, c, list(rule.cols), rule.bars, np.asarray(rule.w), rule.mode, rule.margin)
        else:
            off = decide(accent(f, names, rule.feature), c, rule)
        res.append((tr, score(t, c, off, rp)))
    return res


def fit_linear(tracks: list[Track], names: tuple[str, ...], set_name: str, bars: int, mode: str,
               trained_on: str) -> LinearRule:
    cols = tuple(range(len(names))) if LIN_SETS[set_name] is None else tuple(names.index(f) for f in LIN_SETS[set_name])
    samples = [track_samples(tr.f_on, tr.c_on, tr.rp_on, list(cols), bars, mode) for tr in tracks
               if len(tr.t_on) >= SKIP_BEATS + 4]
    w = train([s for s in samples if len(s[1])])
    return LinearRule(set_name, cols, bars, mode, tuple(float(x) for x in w), 0.0, trained_on)


def fixed_offsets(tracks: list[Track], beats: str, offs: tuple[int, ...]) -> list[tuple[Track, Score]]:
    """A bar that never moves: Link's arbitrary bar (all 4 offsets, averaged per track) or 'the first beat is the 1'."""
    res = []
    for tr in tracks:
        t, c, rp = (tr.t_on, tr.c_on, tr.rp_on) if beats == "online" else (tr.t_ref, tr.c_ref, tr.rp_ref)
        if len(t) < SKIP_BEATS + 4:
            continue
        ss = [score(t, c, np.full(len(t), (c[0] + o) % 4), rp) for o in offs]
        s = ss[0]
        res.append((tr, Score(s.n, s.beats, int(round(np.mean([x.correct for x in ss]))),
                              float(np.nanmean([x.lock_s for x in ss])) if any(np.isfinite(x.lock_s) for x in ss)
                              else np.nan, 0, s.minutes, np.mean([x.per_beat for x in ss], axis=0), s.times)))
    return res


def summarize(res: list[tuple[Track, Score]]) -> dict:
    sc = [s for _, s in res if s.n > 0]
    if not sc:
        return {}
    acc = np.array([s.correct / s.n for s in sc])
    lock = np.array([s.lock_s for s in sc])
    flips = np.array([s.flips for s in sc])
    mins = np.array([s.minutes for s in sc])
    return {"tracks": len(sc), "acc_mean": acc.mean(), "acc_median": float(np.median(acc)),
            "acc_pooled": sum(s.correct for s in sc) / sum(s.n for s in sc),
            "tracks_90": float(np.mean(acc >= 0.9)), "matched": sum(s.n for s in sc) / sum(s.beats for s in sc),
            "lock_median_s": float(np.nanmedian(lock)) if np.isfinite(lock).any() else np.nan,
            "never_locked": float(np.mean(~np.isfinite(lock))), "flips_median": float(np.median(flips)),
            "flips_per_min": float(flips.sum() / max(mins.sum(), 1e-9))}


def split_pooled(res: list[tuple[Track, Score]], key) -> dict:
    """Pooled time-in-correct-phase by a per-beat or per-track key."""
    num, den = defaultdict(float), defaultdict(float)
    for tr, s in res:
        for ok, t in zip(s.per_beat, s.times):
            k = key(tr, t)
            num[k] += ok
            den[k] += 1
    return {k: (num[k] / den[k], int(den[k])) for k in sorted(den)}


def in_break(tr: Track, t: float) -> str:
    return "in a break" if any(a <= t < b for a, b in tr.breaks) else "not in a break"


def fmt(d: dict) -> str:
    if not d:
        return "| - |"
    lock = f"{d['lock_median_s']:.0f}" if np.isfinite(d["lock_median_s"]) else "-"
    return (f"| {d['tracks']} | {100 * d['acc_mean']:.1f} | {100 * d['acc_median']:.1f} | {100 * d['acc_pooled']:.1f} | "
            f"{100 * d['tracks_90']:.0f} | {lock} | {100 * d['never_locked']:.0f} | {d['flips_median']:.0f} | "
            f"{d['flips_per_min']:.2f} | {100 * d['matched']:.0f} |")


HEAD = ("| method | tracks | time in correct phase, mean % | median % | pooled % | tracks >= 90 % | median s to lock | "
        "never locked % | flips median | flips/min | beats matched % |\n|---|---|---|---|---|---|---|---|---|---|---|")


def model_tracks(tracks: list[Track], beats: dict[str, np.ndarray], downs: dict[str, np.ndarray]) -> list:
    """A downbeat model's own beats and "1"s, scored like the rule (counting, matching, first 2 bars skipped)."""
    res = []
    for tr in tracks:
        if tr.name not in beats:
            continue
        t, d = np.asarray(beats[tr.name]), np.asarray(downs[tr.name])
        if len(t) < SKIP_BEATS + 4:
            continue
        c = count_beats(t)
        rp = ref_positions(t, tr.t_ref, tr.ref_pos, tr.valid)
        res.append((tr, score(t, c, offsets_from_downbeats(t, c, d), rp)))
    return res


def run_json(pattern: str) -> tuple[dict, dict]:
    beats, downs = {}, {}
    for f in sorted((REPO / "runs").glob(pattern)):
        for p in json.loads(f.read_text())["pieces"]:
            if "error" not in p:
                beats[p["name"]], downs[p["name"]] = p["est_beats"], p["est_downbeats"]
    return beats, downs


def mmdb_traktor() -> tuple[dict, dict, dict]:
    path = RES / "traktor_mmdb.0of4.pkl"
    if not path.exists():
        return {}, {}, {}
    rows = [r for r in pickle.loads(path.read_bytes())["rows"] if "error" not in r]
    return ({r["name"]: r["mm_beats"] for r in rows}, {r["name"]: r["mm_beats"][r["mm_pos"] == 1] for r in rows},
            {r["name"]: r["mm_pos"] for r in rows})


