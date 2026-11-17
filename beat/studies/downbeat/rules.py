"""The accent rule and the scoring, shared by analyze.py.

A beat stream is counted the way Link counts: each beat advances the count by round(gap / period), the period being the
median of the last 8 gaps, so a dropped or extra tracked beat does not shift the bar. The rule keeps one accumulator per
position (count mod 4) and calls the position with the largest one the "1". At a beat it uses only the beats before it.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

MATCH_S = 0.07
SKIP_BEATS = 8          # the first 2 bars are not scored
LOCK_BEATS = 8          # "locked" = right for this many scored beats in a row


@dataclass(frozen=True)
class Rule:
    feature: str            # a column of extract.FEATURES, or "a+b" for the sum of two running z-scores
    bars: int               # N
    mode: str               # "window" (the last N bars) or "exp" (forgetting, time constant N bars)
    margin: float = 0.0     # hysteresis: switch only when the new best beats the current one by this share

    @property
    def label(self) -> str:
        m = f" hyst{int(self.margin * 100)}%" if self.margin else ""
        return f"{self.feature} {self.mode} N={self.bars}{m}"


def count_beats(t: np.ndarray) -> np.ndarray:
    c = np.zeros(len(t), dtype=int)
    for i in range(1, len(t)):
        gaps = np.diff(t[max(0, i - 8):i + 1])
        per = float(np.median(gaps))
        c[i] = c[i - 1] + max(1, int(round((t[i] - t[i - 1]) / per)))
    return c


def running_z(x: np.ndarray, n: int = 64) -> np.ndarray:
    """Causal z-score over the last n values (the value itself included)."""
    x = np.asarray(x, dtype=float)
    cs = np.concatenate([[0.0], np.cumsum(x)])
    cs2 = np.concatenate([[0.0], np.cumsum(x * x)])
    i = np.arange(1, len(x) + 1)
    lo = np.maximum(0, i - n)
    cnt = i - lo
    mean = (cs[i] - cs[lo]) / cnt
    var = np.maximum((cs2[i] - cs2[lo]) / cnt - mean ** 2, 0.0)
    sd = np.sqrt(var)
    return np.where(sd > 1e-9, (x - mean) / np.where(sd > 1e-9, sd, 1.0), 0.0)


def accent(feats: np.ndarray, names: tuple[str, ...], feature: str) -> np.ndarray:
    if "+" in feature:
        return sum(running_z(feats[:, names.index(f)]) for f in feature.split("+"))
    return feats[:, names.index(feature)].astype(float)


def position_scores(x: np.ndarray, c: np.ndarray, rule: Rule) -> np.ndarray:
    """(n, 4): each position's accumulated accent from the beats before beat i; nan until every position has one."""
    from scipy.signal import lfilter
    n = len(x)
    out = np.full((n, 4), np.nan)
    i = np.arange(n)
    for q in range(4):
        idx = np.flatnonzero(c % 4 == q)
        k = np.searchsorted(idx, i)                    # how many beats of position q come before beat i
        v = x[idx].astype(float)
        if rule.mode == "window":
            cs = np.concatenate([[0.0], np.cumsum(v)])
            lo = np.maximum(0, k - rule.bars)
            with np.errstate(invalid="ignore", divide="ignore"):
                out[:, q] = np.where(k > 0, (cs[k] - cs[lo]) / np.maximum(k - lo, 1), np.nan)
        else:
            acc = lfilter([1.0], [1.0, -np.exp(-1.0 / rule.bars)], v) if len(v) else v
            out[:, q] = np.where(k > 0, np.concatenate([[np.nan], acc])[k], np.nan)
    return out


def decide(x: np.ndarray, c: np.ndarray, rule: Rule) -> np.ndarray:
    """off[i] = the count (mod 4) the rule calls the "1", from beats before i; -1 before every position was heard."""
    s = position_scores(x, c, rule)
    ready = ~np.isnan(s).any(axis=1)
    best = np.where(ready, np.argmax(np.nan_to_num(s, nan=-np.inf), axis=1), -1)
    if rule.margin <= 0:
        return best
    off = np.full(len(x), -1, dtype=int)
    cur = -1
    for i in np.flatnonzero(ready):
        b = best[i]
        if cur < 0 or (b != cur and s[i, b] - s[i, cur] > rule.margin * np.abs(s[i]).mean()):
            cur = b
        off[i] = cur
    return off


def ref_positions(t: np.ndarray, ref_t: np.ndarray, ref_pos: np.ndarray, valid: np.ndarray) -> np.ndarray:
    """0-based bar position of the reference beat within MATCH_S of each beat; -1 when none or not in a 4-beat bar."""
    if len(ref_t) == 0 or len(t) == 0:
        return np.full(len(t), -1)
    j = np.clip(np.searchsorted(ref_t, t), 1, len(ref_t) - 1)
    j = np.where(np.abs(ref_t[j - 1] - t) <= np.abs(ref_t[j] - t), j - 1, j)
    ok = (np.abs(ref_t[j] - t) <= MATCH_S) & valid[j]
    return np.where(ok, ref_pos[j] - 1, -1)


def valid_4(ref_pos: np.ndarray) -> tuple[np.ndarray, int, int]:
    """Beats inside 4-beat bars (a pickup counts with the first full bar); also (bars, non-4 bars)."""
    starts = np.flatnonzero(ref_pos == 1)
    valid = np.zeros(len(ref_pos), bool)
    if len(starts) == 0:
        return valid, 0, 0
    bounds = list(starts) + [len(ref_pos)]
    lens = [bounds[k + 1] - bounds[k] for k in range(len(starts))]
    full = lens[:-1] if len(lens) > 1 else lens       # the last bar may be cut by the end of the piece
    for k, n in enumerate(lens):
        last = k == len(lens) - 1
        if n == 4 or (last and n < 4 and ref_pos[bounds[k]:bounds[k + 1]].max() <= 4):
            valid[bounds[k]:bounds[k + 1]] = True
    if lens[0] == 4 and ref_pos[:starts[0]].max(initial=0) <= 4:
        valid[:starts[0]] = True
    return valid, len(full), sum(1 for n in full if n != 4)


@dataclass
class Score:
    n: int              # scored beats (matched, in 4-beat bars, after the first 2 bars)
    beats: int          # tracked beats after the first 2 bars
    correct: int
    lock_s: float       # seconds from the first tracked beat to the start of the first lock; nan = never
    flips: int
    minutes: float
    per_beat: np.ndarray   # 1 correct / 0 wrong for each scored beat
    times: np.ndarray      # their times


def score(t: np.ndarray, c: np.ndarray, off: np.ndarray, rp: np.ndarray) -> Score:
    keep = c >= SKIP_BEATS
    scored = keep & (rp >= 0)
    pred = np.where(off >= 0, (c - off) % 4, -9)
    ok = (pred == rp)[scored]
    tt = t[scored]
    lock = np.nan
    run = 0
    for k in range(len(ok)):
        run = run + 1 if ok[k] else 0
        if run == LOCK_BEATS:
            lock = float(tt[k - LOCK_BEATS + 1] - t[0])
            break
    o = off[keep]
    flips = int(np.sum((o[1:] != o[:-1]) & (o[1:] >= 0) & (o[:-1] >= 0))) if len(o) > 1 else 0
    mins = float(t[keep][-1] - t[keep][0]) / 60 if keep.sum() > 1 else 0.0
    return Score(int(scored.sum()), int(keep.sum()), int(ok.sum()), lock, flips, mins, ok.astype(np.int8), tt)


def offsets_from_downbeats(t: np.ndarray, c: np.ndarray, down: np.ndarray) -> np.ndarray:
    """For a model that marks downbeats itself: off[i] = the count (mod 4) of its latest downbeat at or before beat i
    (what it calls the "1" at that moment); -1 before its first one."""
    isdown = np.zeros(len(t), bool)
    if len(down):
        j = np.clip(np.searchsorted(t, down), 0, len(t) - 1)
        j2 = np.clip(j - 1, 0, len(t) - 1)
        j = np.where(np.abs(t[j2] - down) < np.abs(t[j] - down), j2, j)
        isdown[j[np.abs(t[j] - down) < 0.02]] = True
    off = np.full(len(t), -1)
    cur = -1
    for i in range(len(t)):
        if isdown[i]:
            cur = c[i] % 4
        off[i] = cur
    return off
