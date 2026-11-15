"""Loading build.py's pickles and scoring, with the first study's own functions (rules.score, analyze.summarize), so the
columns mean exactly what they meant there."""
from __future__ import annotations

import json
import pickle
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from analyze import HEAD, fmt, summarize, traktor_group
from feats import running_norm
from phase import Filter, decide_estimator, decide_link, posteriors
from rules import MATCH_S, SKIP_BEATS, Score, ref_positions, score

HERE = Path(__file__).resolve().parent

__all__ = ["HEAD", "fmt", "summarize", "Filter"]


@dataclass
class Tr:
    name: str
    genre: str
    breaks: list
    t_on: np.ndarray
    c_on: np.ndarray
    rp_on: np.ndarray
    N_on: np.ndarray            # normalised descriptors at the online beats
    t_ref: np.ndarray
    c_ref: np.ndarray
    rp_ref: np.ndarray
    N_ref: np.ndarray
    S_on: np.ndarray            # the first study's 22 features at the online beats
    ref_pos: np.ndarray
    valid: np.ndarray
    agree: float = np.nan       # share of beats where madmom offline's bar position = the reference's
    extra: dict = field(default_factory=dict)

    def stream(self, beats: str):
        if beats == "online":
            return self.t_on, self.c_on, self.rp_on, self.N_on
        return self.t_ref, self.c_ref, self.rp_ref, self.N_ref


def _breaks(meta: dict) -> list:
    drops = sorted(meta.get("drops", []))
    out = []
    for b in meta.get("breaks", []):
        nxt = [d for d in drops if d > b]
        out.append((b, nxt[0] if nxt else b + 30.0))
    return out


def load(pkl: Path, dataset: str, tau: float = 64.0) -> list[Tr]:
    rows = pickle.loads(pkl.read_bytes())["rows"]
    meta, genres = {}, {}
    if dataset == "traktor":
        from eval.data import DATA
        meta = {r["name"]: r for r in json.loads((DATA / "traktor" / "index.json").read_text())}
        genres = json.loads((HERE.parent / "genres.json").read_text())
    out = []
    for r in rows:
        if "error" in r:
            print("error", r["name"], file=sys.stderr)
            continue
        g = traktor_group(genres.get(r["name"], "")) if dataset == "traktor" else r["name"].split("_")[1]
        out.append(Tr(r["name"], g, _breaks(meta.get(r["name"], {})), r["t_on"], r["c_on"], r["rp_on"],
                      running_norm(r["D_on"], tau) if len(r["D_on"]) else r["D_on"], r["t_ref"], r["c_ref"],
                      r["rp_ref"], running_norm(r["D_ref"], tau), r["S_on"], r["ref_pos"], r["valid"]))
    return out


def add_agreement(tracks: list[Tr], mmdb_dir: Path) -> None:
    """madmom offline (a measuring instrument) against the reference's bar positions, as the study's ref_check.py."""
    for tr in tracks:
        f = mmdb_dir / f"{tr.name}.npz"
        if not f.exists():
            continue
        z = np.load(f)
        mb, mp = z["beats"], z["pos"].astype(int)
        if len(mb) < 16 or len(tr.t_ref) < 2:
            continue
        j = np.clip(np.searchsorted(tr.t_ref, mb), 1, len(tr.t_ref) - 1)
        j = np.where(np.abs(tr.t_ref[j - 1] - mb) <= np.abs(tr.t_ref[j] - mb), j - 1, j)
        ok = np.abs(tr.t_ref[j] - mb) <= MATCH_S
        if ok.sum() >= 16:
            tr.agree = float(np.mean(((mp[ok] - 1) - (tr.ref_pos[j[ok]] - 1)) % 4 == 0))
            tr.extra["mm_beats"], tr.extra["mm_pos"] = mb, mp


def usable(tr: Tr, beats: str) -> bool:
    return len(tr.stream(beats)[0]) >= SKIP_BEATS + 4


def run_filter(tracks, lps: dict, beats: str, f: Filter, link: bool = False) -> list:
    """lps[name] = (n, 4) log-probs on that beat stream. Returns [(track, Score)] (estimator) or Link-view scores."""
    res = []
    for tr in tracks:
        if not usable(tr, beats):
            continue
        t, c, rp, _ = tr.stream(beats)
        post = posteriors(lps[tr.name], c, f)
        if not link:
            res.append((tr, score(t, c, decide_estimator(post, f), rp)))
        else:
            off, moved = decide_link(post, f)
            res.append((tr, link_score(t, c, off, moved, rp)))
    return res


def link_score(t, c, off, moved, rp) -> Score:
    """What the user hears: Link's bar is arbitrary (each of the 4 offsets, averaged) until the first move."""
    ss = [score(t, c, np.where(off >= 0, off, (c[0] + o) % 4), rp) for o in range(4)]
    s = ss[0]
    keep = c >= SKIP_BEATS
    lock = [x.lock_s for x in ss]
    sc = Score(s.n, s.beats, int(round(np.mean([x.correct for x in ss]))),
               float(np.nanmean(lock)) if np.isfinite(lock).any() else np.nan, int(moved[keep].sum()), s.minutes,
               np.mean([x.per_beat for x in ss], axis=0), s.times)
    moves = np.flatnonzero(moved)
    sc.first_move_s = float(t[moves[0]] - t[0]) if len(moves) else np.nan
    sc.never_moved = len(moves) == 0
    # a move is right when it lands on the reference's majority phase over the 32 beats after it
    judged = []
    for i in moves:
        sl = slice(i, min(len(c), i + 32))
        m = rp[sl] >= 0
        judged.append(None if m.sum() < 4 else
                      bool(off[i] == np.bincount(((c[sl] - rp[sl])[m]) % 4, minlength=4).argmax()))
    sc.first_right = judged[0] if judged else None
    later = [j for j in judged[1:] if j is not None]
    sc.later_moves, sc.later_wrong = len(moves) - 1 if len(moves) else 0, sum(1 for j in later if not j)
    sc.later_judged = len(later)
    return sc


def link_summary(res) -> dict:
    d = summarize(res)
    sc = [s for _, s in res if s.n > 0]
    d["first_move_median_s"] = float(np.nanmedian([s.first_move_s for s in sc]))
    d["never_moved"] = float(np.mean([s.never_moved for s in sc]))
    fr = [s.first_right for s in sc if s.first_right is not None]
    d["first_right"] = float(np.mean(fr)) if fr else float("nan")
    d["later_per_min"] = float(sum(s.later_moves for s in sc) / max(sum(s.minutes for s in sc), 1e-9))
    d["later_wrong"] = float(sum(s.later_wrong for s in sc) / max(sum(s.later_judged for s in sc), 1))
    d["moves_per_min"] = d["later_per_min"]
    d["wrong_moves_share"] = d["later_wrong"]
    return d


def link_fmt(d: dict) -> str:
    return (f"| {d['tracks']} | {100 * d['acc_mean']:.1f} | {100 * d['acc_median']:.1f} | {100 * d['tracks_90']:.0f} | "
            f"{d['first_move_median_s']:.0f} | {100 * d['never_moved']:.0f} | {100 * d['first_right']:.0f} | "
            f"{d['later_per_min']:.2f} | {100 * d['later_wrong']:.0f} |")


LINK_HEAD = ("| method | tracks | right (Link's bar as heard), mean % | median % | tracks >= 90 % | median s to first move | "
             "never moved % | first move right % | later moves/min | later moves wrong % |"
             "\n|---|---|---|---|---|---|---|---|---|---|")


def rescore_against(tracks: list[Tr]) -> list[Tr]:
    """Copies of the tracks whose reference bar positions are madmom offline's (4/4 tracks only)."""
    out = []
    for tr in tracks:
        if "mm_pos" not in tr.extra or tr.extra["mm_pos"].max() != 4:
            continue
        mb, mp = tr.extra["mm_beats"], tr.extra["mm_pos"]
        valid = np.ones(len(mb), bool)
        n = Tr(**{k: getattr(tr, k) for k in tr.__dataclass_fields__})
        n.rp_on = ref_positions(tr.t_on, mb, mp, valid)
        out.append(n)
    return out
