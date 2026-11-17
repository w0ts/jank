"""Scores of one replayed item (live_replay.Run) against its reference beats.

Phase is judged RELATIVE to the reference: each part of an item (a track, or A and B of a sequence) gets its constant
offset from the tracker's own beats (median of tracked beat - nearest reference beat), so Traktor's batch offsets and
its half-beat-out grids drop out of the "in phase / off-beat / flip" metrics. The harness kinds (eval.grid) are kept as
the earlier studies score them, against Traktor's grid as is.
"""
from __future__ import annotations

import numpy as np

from eval.grid import KINDS, _classify, _fold
from eval.locked_failures import in_break

IN_MS = 50.0
OCT = (0.5, 2.0)
TRIP = (2 / 3, 1.5, 0.75, 4 / 3)
RATIO_TOL = 0.04
HELD_REL = 0.005
DEBOUNCE = 4
RELOCK_RUN = 8


def offset_ms(beats: np.ndarray, ref: np.ndarray, lo: float, hi: float) -> float:
    """Median signed ms from each tracked beat in [lo, hi) to its nearest reference beat in the same span."""
    r = ref[(ref >= lo) & (ref < hi)]
    b = beats[(beats >= lo + 5) & (beats < hi)]
    if r.size < 4 or b.size < 4:
        return float("nan")
    j = np.clip(np.searchsorted(r, b), 1, r.size - 1)
    near = np.where(np.abs(b - r[j - 1]) < np.abs(b - r[j]), r[j - 1], r[j])
    p = float(np.median(np.diff(r)))
    d = (b - near) * 1000.0
    d = d[np.abs(d) < 0.5 * p * 1000]
    return float(np.median(d)) if d.size else float("nan")


def true_period(ref: np.ndarray) -> np.ndarray:
    """Per reference beat: the local period (median of the intervals around it)."""
    d = np.diff(ref)
    out = np.empty(ref.size)
    for i in range(ref.size):
        w = d[max(0, i - 2):i + 2]
        out[i] = np.median(w) if w.size else np.nan
    return out


def classify(run, ref: np.ndarray, base: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Per reference beat: (class, relative ms). class: stopped / in / offbeat / off / octave / triplet."""
    idx = np.searchsorted(run.t, ref, side="right") - 1
    tp = true_period(ref)
    cls = np.empty(ref.size, dtype=object)
    rel = np.full(ref.size, np.nan)
    for i, r in enumerate(ref):
        k = idx[i]
        if k < 0 or not run.running[k] or np.isnan(run.gp[k]):
            cls[i] = "stopped"
            continue
        ratio = run.gp[k] / tp[i]
        if any(abs(ratio / o - 1) <= RATIO_TOL for o in OCT):
            cls[i] = "octave"
            continue
        if any(abs(ratio / o - 1) <= RATIO_TOL for o in TRIP):
            cls[i] = "triplet"
            continue
        g0, gp = run.g0[k], run.gp[k]
        e = (g0 + np.round((r - g0) / gp) * gp - r) * 1000.0 - (base[i] if base[i] == base[i] else 0.0)
        e = (e + gp * 500) % (gp * 1000) - gp * 500                # wrapped to +- half a period
        rel[i] = e
        cls[i] = "in" if abs(e) <= IN_MS else "offbeat" if abs(abs(e) - gp * 500) <= IN_MS else "off"
    return cls, rel


def flips(cls: np.ndarray) -> int:
    """Changes between running classes held DEBOUNCE beats or more (stopped stretches skipped)."""
    runs, prev, n = [], None, 0
    for c in cls:
        if c == "stopped":
            continue
        if c == prev:
            n += 1
        else:
            if prev is not None:
                runs.append((prev, n))
            prev, n = c, 1
    if prev is not None:
        runs.append((prev, n))
    kept = [c for c, k in runs if k >= DEBOUNCE]
    return sum(1 for a, b in zip(kept, kept[1:]) if a != b)


def held(run, ref: np.ndarray, lo: float, hi: float) -> float:
    """Share of the ticks in [lo, hi) running at the reference tempo within HELD_REL (folded into its octave)."""
    sel = (run.t >= lo) & (run.t < hi)
    if not sel.any():
        return float("nan")
    tp = np.interp(run.t[sel], ref[1:], np.diff(ref))
    gp = run.gp[sel]
    ok = run.running[sel] & ~np.isnan(gp)
    good = np.zeros(sel.sum(), bool)
    for i in np.where(ok)[0]:
        good[i] = abs(_fold(gp[i], tp[i]) / tp[i] - 1) <= HELD_REL
    return float(good.mean())


def harness(run, ref: np.ndarray) -> dict:
    """eval.grid's kinds with the published grid (stopped = lost), shares of the reference beats after 5 s."""
    beats = ref[ref >= 5.0]
    idx = np.searchsorted(run.t, beats, side="right") - 1
    kinds = []
    for r, k in zip(beats, idx):
        fit = None
        if k >= 0 and run.running[k] and not np.isnan(run.gp[k]):
            fit = (run.gp[k], run.g0[k])
        kinds.append(_classify(ref, r, fit)[0])
    share = {kd: kinds.count(kd) / max(len(kinds), 1) for kd in KINDS}
    share["loose"] += share["tight"]
    return share


def score(run, ref: np.ndarray, beats: np.ndarray, meta: dict) -> dict:
    fam = meta["family"]
    dur = float(meta["dur"])
    out: dict = {"name": meta["name"], "family": fam, "kind": meta["kind"], "split": meta["split"],
                 "different_resets": [t for t, why in run.resets if why == "different"], "dur": dur}
    if fam == "diff":
        ts = float(meta["t_switch"])
        fade = float(meta.get("fade_s", 0.0))
        parts = [(0.0, ts), (ts, dur)]
    else:
        parts = [(0.0, dur)]
    base = np.full(ref.size, np.nan)
    for lo, hi in parts:
        o = offset_ms(beats, ref, lo, hi)
        base[(ref >= lo) & (ref < hi)] = o
    cls, rel = classify(run, ref, base)
    out["harness"] = harness(run, ref)
    out["held_all"] = held(run, ref, 20.0, dur)
    out["flips"] = flips(cls[ref >= 20.0])
    out["stopped_share"] = float(np.mean(cls[ref >= 20.0] == "stopped")) if np.any(ref >= 20.0) else float("nan")
    if fam == "real":
        brk = in_break(ref, meta["breaks"], meta["drops"])
        out["break_beats"] = int(brk.sum())
        out["break_stopped"] = float(np.mean(cls[brk] == "stopped")) if brk.any() else None
        out["break_in"] = float(np.mean(cls[brk] == "in")) if brk.any() else None
        out["break_flips"] = flips(cls[brk]) if brk.any() else 0
    if fam == "same" and "t_var" in meta:
        a, b = meta["t_var"]
        w = (ref >= a) & (ref < b + 10)
        out["var_held"] = held(run, ref, a, b + 10)
        out["var_stopped"] = float(np.mean(cls[w] == "stopped"))
        out["var_in"] = float(np.mean(cls[w] == "in"))
        out["var_flips"] = flips(cls[(ref >= a - 20) & (ref < b + 20)])
    if fam == "same" and "t_change" in meta:
        a = float(meta["t_change"])
        out["change"] = meta["change"]
        out["ramp_s"] = meta["ramp_s"]
        w = (ref >= a) & (ref < a + 30)
        out["chg_stopped"] = float(np.mean(cls[w] == "stopped"))
        out["chg_in"] = float(np.mean(cls[w] == "in"))
        out["chg_worst_ms"] = float(np.nanmax(np.abs(rel[w]))) if np.any(~np.isnan(rel[w])) else None
        out["chg_held"] = held(run, ref, a, a + 30)
    if fam == "diff":
        after = ref >= ts
        ok = cls == "in"
        ok_bpm = np.array([held_tick(run, ref, r) for r in ref])
        good = ok & ok_bpm
        relock = None
        idx = np.where(after)[0]
        for j in range(idx.size - RELOCK_RUN + 1):
            if good[idx[j:j + RELOCK_RUN]].all():
                relock = float(ref[idx[j]] - ts)
                break
        out["relock_s"] = relock
        out["fade_s"] = fade
        out["t_switch"] = ts
        out["held_after60"] = held(run, ref, ts, ts + 60)
        sel = (run.t >= ts) & (run.t < ts + 60)
        tp = np.interp(run.t[sel], ref[1:], np.diff(ref))
        gp = run.gp[sel]
        wrong = [run.running[sel][i] and not np.isnan(gp[i]) and abs(_fold(gp[i], tp[i]) / tp[i] - 1) > HELD_REL
                 for i in range(int(sel.sum()))]
        out["wrong_bpm_s_60"] = float(np.sum(wrong) * (run.t[1] - run.t[0]))
        out["held_before"] = held(run, ref, 20.0, ts - fade / 2)
        out["flips_before"] = flips(cls[(ref >= 20.0) & (ref < ts - fade / 2)])
    return out


def held_tick(run, ref: np.ndarray, r: float) -> bool:
    k = int(np.searchsorted(run.t, r, side="right") - 1)
    if k < 0 or not run.running[k] or np.isnan(run.gp[k]):
        return False
    j = int(np.clip(np.searchsorted(ref, r), 1, ref.size - 1))
    tp = ref[j] - ref[j - 1] if ref[j] - ref[j - 1] > 0.2 else np.median(np.diff(ref))
    return abs(_fold(run.gp[k], tp) / tp - 1) <= HELD_REL
