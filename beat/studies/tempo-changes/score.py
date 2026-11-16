"""Scores of one replay (chain.Run) against the audio reference (reftempo segments of madmom's offline beats).

What is judged is what Link / MIDI get: the FOLDED output tempo, against the reference tempo folded into the output's
octave (AUTO RANGE keeps the range; an octave relation is no error, the user 2026-11-15). Octaves count only when the
output itself jumps an octave (the fold failed to keep the range: `range_breaks`).

  stopped       share of the music's time (first reference beat + WARM_S .. last) with the outputs stopped
  right / held  running with the folded tempo within RIGHT_REL (1 %) / HELD_REL (0.5 %) of the reference
  wrong         running more than WRONG_REL (2 %) off once folded (a wrong tempo, not an octave)
  wrong_locks   starts (stopped -> running, or a jump of the output by more than WRONG_REL while running) whose tempo is
                wrong and stays wrong for WRONG_HOLD_S
  follow        per real change (reftempo: step / ramp): seconds and beats from the change (a ramp: its end) until the
                output is right and stays right for STAY_S; None when not within FOLLOW_MAX_S
  in phase      running at the right tempo and the grid within IN_MS of the reference beat, after the track's constant
                offset (median; removes a batch / decoder offset and a half-beat-out reference)
  flips         changes between in phase / off-beat / off / wrong held DEBOUNCE beats or more
  quiet         the beat-less passages (the live activation's 1 s max under QUIET_REL of the track's level for QUIET_S or
                more, music playing): share stopped in them, and at the first strong beat after each one whether the
                outputs run and the phase error there (and the median of the first 4 strong beats)
  half          the reference's octave sections (a half-time / double-time feel): share right and running in them
"""
from __future__ import annotations

import math

import numpy as np

import reftempo as RT

WARM_S = 10.0
RIGHT_REL = 0.01
HELD_REL = 0.005
WRONG_REL = 0.02
WRONG_HOLD_S = 2.0
STAY_S = 4.0
FOLLOW_MAX_S = 30.0
IN_MS = 50.0
DEBOUNCE = 4
QUIET_REL = 0.25
QUIET_S = 4.0
STRONG_REL = 0.5
FPS = 100


def folded_dev(out: np.ndarray, ref: np.ndarray) -> np.ndarray:
    """|log| of out / ref folded into an octave (NaN where either is missing)."""
    with np.errstate(invalid="ignore", divide="ignore"):
        lr = np.log2(out / ref)
        return np.abs(lr - np.round(lr)) * math.log(2)


def runs_of(mask: np.ndarray) -> list[tuple[int, int]]:
    """[start, end) index pairs of the True runs."""
    m = np.concatenate([[False], mask, [False]]).astype(np.int8)
    d = np.diff(m)
    return list(zip(np.where(d == 1)[0], np.where(d == -1)[0]))


def stays(ok: np.ndarray, start: int, n_stay: int) -> int | None:
    """The first index >= start from which ok holds for n_stay ticks."""
    run = 0
    for i in range(start, ok.size):
        run = run + 1 if ok[i] else 0
        if run >= n_stay:
            return i - n_stay + 1
    return None


def phase_errors(run, ref_beats: np.ndarray, ref_period: np.ndarray) -> np.ndarray:
    """Per reference beat: signed ms to the output grid's nearest line, wrapped by the smaller of the two periods (an
    octave-folded output marks every beat of the music's level, or every other one: which ones is a bar question, not
    the beat's); NaN when stopped."""
    idx = np.searchsorted(run.t, ref_beats, side="right") - 1
    err = np.full(ref_beats.size, np.nan)
    for i, (r, k) in enumerate(zip(ref_beats, idx)):
        if k < 0 or not run.running[k] or np.isnan(run.gp[k]):
            continue
        g0, gp = run.g0[k], run.gp[k]
        q = min(gp, ref_period[i]) if ref_period[i] == ref_period[i] else gp
        e = (r - g0) % q
        err[i] = (e - q if e > q / 2 else e) * -1000.0
    return err


def score(run, segs: list[RT.Segment], chs: list[RT.Change], ref_beats: np.ndarray, act: np.ndarray,
          rms: np.ndarray) -> dict:
    t = run.t
    ref = RT.ref_bpm(segs, t)
    lo = float(ref_beats[0]) + WARM_S if ref_beats.size else 0.0
    hi = float(ref_beats[-1]) if ref_beats.size else 0.0
    blk = np.minimum((t / 0.1).astype(int), max(rms.size - 1, 0))
    music = (t >= lo) & (t <= hi) & ~np.isnan(ref) & (rms[blk] >= 1e-3 if rms.size else True)
    dev = folded_dev(run.out_bpm, ref)
    running = run.running & music
    right = running & (dev <= math.log(1 + RIGHT_REL))
    held = running & (dev <= math.log(1 + HELD_REL))
    wrong = running & (dev > math.log(1 + WRONG_REL))
    n = max(int(music.sum()), 1)
    row = {"music_s": float(music.sum() * (t[1] - t[0])) if t.size > 1 else 0.0,
           "stopped": float((music & ~run.running).sum() / n), "right": float(right.sum() / n),
           "held": float(held.sum() / n), "wrong": float(wrong.sum() / n)}
    row.update(locks(run, dev, music))
    row["range_breaks"] = range_breaks(run)
    row["follow"] = follow(run, right, chs)
    ph = phase(run, segs, ref_beats, right)
    row.update(ph)
    row.update(quiet(run, segs, ref_beats, act, rms, ph["base_ms"]))
    row.update(half(run, segs, right, music))
    return row


def locks(run, dev: np.ndarray, music: np.ndarray) -> dict:
    tick = float(run.t[1] - run.t[0]) if run.t.size > 1 else 0.05
    hold = int(round(WRONG_HOLD_S / tick))
    starts = []
    for i in range(1, run.t.size):
        if not music[i] or not run.running[i]:
            continue
        if not run.running[i - 1]:
            starts.append(i)
        elif run.out_bpm[i - 1] == run.out_bpm[i - 1] and folded_dev(np.array([run.out_bpm[i]]),
                                                                      np.array([run.out_bpm[i - 1]]))[0] \
                > math.log(1 + WRONG_REL):
            starts.append(i)
    bad = 0
    for i in starts:
        w = dev[i:i + hold]
        r = run.running[i:i + hold]
        if w.size == hold and r.all() and np.all(w > math.log(1 + WRONG_REL)):
            bad += 1
    return {"starts": len(starts), "wrong_locks": bad}


def range_breaks(run) -> int:
    """The output jumping by an octave (x2 / /2 within 3 %) between running ticks, a stop of under 120 s between."""
    n, last, last_t = 0, None, -1e9
    for t, r, b in zip(run.t, run.running, run.out_bpm):
        if not r or b != b:
            continue
        if last is not None and t - last_t < 120.0:
            lr = abs(math.log2(b / last))
            if abs(lr - round(lr)) <= math.log2(1.03) and round(lr) >= 1:
                n += 1
        last, last_t = b, t
    return n


def follow(run, right: np.ndarray, chs: list[RT.Change]) -> list[dict]:
    tick = float(run.t[1] - run.t[0]) if run.t.size > 1 else 0.05
    out = []
    for c in chs:
        if c.kind not in ("step", "ramp") or c.confidence not in ("both", "madmom"):
            continue
        start = int(np.searchsorted(run.t, c.t_end))
        before = right[max(0, int(np.searchsorted(run.t, c.t - 5.0))):int(np.searchsorted(run.t, c.t))]
        k = stays(right, start, int(round(STAY_S / tick)))
        delay = None if k is None else float(run.t[k] - c.t_end)
        if delay is not None and delay > FOLLOW_MAX_S:
            delay = None
        out.append({"t": c.t, "kind": c.kind, "ratio": c.bpm_after / c.bpm_before, "conf": c.confidence,
                    "right_before": float(before.mean()) if before.size else 0.0,
                    "delay_s": delay, "delay_beats": None if delay is None else delay * c.bpm_after / 60.0,
                    "right_during": float(right[int(np.searchsorted(run.t, c.t)):start].mean())
                    if c.kind == "ramp" and start > int(np.searchsorted(run.t, c.t)) else None})
    return out


def _ref_period(ref_beats: np.ndarray) -> np.ndarray:
    d = np.diff(ref_beats)
    out = np.empty(ref_beats.size)
    for i in range(ref_beats.size):
        w = d[max(0, i - 2):i + 2]
        out[i] = np.median(w) if w.size else np.nan
    return out


def phase(run, segs, ref_beats: np.ndarray, right: np.ndarray) -> dict:
    rp = _ref_period(ref_beats)
    err = phase_errors(run, ref_beats, rp)
    idx = np.clip(np.searchsorted(run.t, ref_beats, side="right") - 1, 0, run.t.size - 1)
    ok = right[idx] & ~np.isnan(err)
    base = float(np.median(err[ok])) if ok.sum() >= 16 else 0.0
    rel = err - base
    cls = np.full(ref_beats.size, "stopped", dtype=object)
    for i in range(ref_beats.size):
        if np.isnan(err[i]) or not run.running[idx[i]]:
            continue
        if not right[idx[i]]:
            cls[i] = "wrong"
            continue
        q = rp[i] * 1000.0
        a = abs(rel[i])
        cls[i] = "in" if a <= IN_MS else "offbeat" if abs(a - q / 2) <= IN_MS else "off"
    running = cls != "stopped"
    return {"in_phase": float(np.mean(cls == "in")), "in_of_running": float(np.mean(cls[running] == "in"))
            if running.any() else 0.0, "flips": flips(cls), "base_ms": base, "_rel": rel, "_cls": cls}


def flips(cls: np.ndarray) -> int:
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


def quiet_passages(act: np.ndarray, ref_beats: np.ndarray, rms: np.ndarray) -> list[tuple[float, float]]:
    """Beat-less passages: the activation's 1 s rolling max under QUIET_REL of the level (75th percentile of the
    activation at the reference beats) for QUIET_S or more, music playing, inside the reference beats."""
    if act.size == 0 or ref_beats.size < 16:
        return []
    f = np.clip(np.round(ref_beats * FPS).astype(int), 0, act.size - 1)
    level = float(np.percentile([act[max(i - 3, 0):i + 4].max() for i in f], 75))
    w = FPS
    pad = np.concatenate([np.zeros(w // 2), act, np.zeros(w // 2)])
    from numpy.lib.stride_tricks import sliding_window_view
    roll = sliding_window_view(pad, w + 1).max(axis=1)[:act.size]
    blk = np.minimum(np.arange(act.size) // 10, max(rms.size - 1, 0))
    quiet = (roll < QUIET_REL * level) & (rms[blk] >= 1e-3)
    out = []
    for a, b in runs_of(quiet):
        t0, t1 = a / FPS, b / FPS
        if t1 - t0 >= QUIET_S and t0 > ref_beats[0] + WARM_S and t1 < ref_beats[-1] - 2.0:
            out.append((t0, t1))
    return out


def quiet(run, segs, ref_beats: np.ndarray, act: np.ndarray, rms: np.ndarray, base_ms: float = 0.0) -> dict:
    passages = quiet_passages(act, ref_beats, rms)
    if not passages:
        return {"quiet": []}
    f = np.clip(np.round(ref_beats * FPS).astype(int), 0, act.size - 1)
    pk = np.array([act[max(i - 3, 0):i + 4].max() for i in f])
    level = float(np.percentile(pk, 75))
    rp = _ref_period(ref_beats)
    err = phase_errors(run, ref_beats, rp) - base_ms
    q = rp * 1000.0
    err = (err + q / 2) % q - q / 2                    # re-wrapped after the track's offset
    idx = np.clip(np.searchsorted(run.t, ref_beats, side="right") - 1, 0, run.t.size - 1)
    out = []
    for t0, t1 in passages:
        sel = (run.t >= t0) & (run.t < t1)
        strong = np.where((ref_beats >= t1) & (pk >= STRONG_REL * level))[0][:4]
        if strong.size == 0:
            continue
        j = int(strong[0])
        out.append({"t0": t0, "t1": t1, "stopped": float((~run.running[sel]).mean()) if sel.any() else 0.0,
                    "running_after": bool(run.running[idx[j]]), "err_ms": float(err[j]),
                    "err4_ms": float(np.nanmedian(np.abs(err[strong]))) if np.any(~np.isnan(err[strong])) else None,
                    "t_strong": float(ref_beats[j])})
    return {"quiet": out}


def half(run, segs: list[RT.Segment], right: np.ndarray, music: np.ndarray) -> dict:
    """The reference's octave sections: sections whose tempo is an octave off the track's main (longest) level."""
    secs = RT.sections(segs)
    if not secs:
        return {"half_s": 0.0, "half_right": None}
    main = max(secs, key=lambda s: s.t1 - s.t0)
    sel = np.zeros(run.t.size, bool)
    for s in secs:
        if RT.relation(s.bpm0 / main.bpm0) == "octave":
            sel |= (run.t >= s.t0) & (run.t < s.t1)
    sel &= music
    if not sel.any():
        return {"half_s": 0.0, "half_right": None}
    return {"half_s": float(sel.sum() * (run.t[1] - run.t[0])), "half_right": float(right[sel].mean())}
