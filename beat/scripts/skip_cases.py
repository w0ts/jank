"""Synthetic skips for the C port's golden vectors (jank: tests/skip_ref): spliced tracks in scripts/tempo_golden.py's format, and
what the host reference (studies/follow-robust rchain.replay, configs off / ACT / PAT of skip.json) does on them.

    .venv/bin/python scripts/skip_cases.py --out DIR            # NAME.track.txt like tests/tempo_ref's, + summary.json
    .venv/bin/python scripts/skip_cases.py --summary            # the host's numbers only

A bar has a pattern the pattern memory can learn: beat 1 loud, 2-4 softer, a faint off-beat after beats 2 and 4. Cases (124 BPM):
  jump    steady 55 s, then the same music from 22.37 beats later (a bar position and a phase that do not match)
  nudge   steady 55 s, then 0.37 beat later
  other   steady 55 s at 124, then 55 s at 140 (a new tempo: the tempo path, not the phase re-sync)
  gap     steady 55 s, 4 s of silence, then 0.5 beat off
Per case the summary gives, for each config: the seconds (and beats) until the output's tempo and phase are right again for 3 s,
whether the output ever stopped, and the counts (act_resyncs, resyncs, jumps).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent.parent
for p in (REPO, REPO / "studies" / "tempo-changes", REPO / "studies" / "follow-robust", REPO / "scripts"):
    sys.path.insert(0, str(p))

import chain as C          # noqa: E402
import rchain as R         # noqa: E402
import rconfigs as RC      # noqa: E402
import skiptest as ST      # noqa: E402
import tempo_golden as TG  # noqa: E402

BPM = 124.0
PRE_S = 55.0


def music(seed: int, bpm: float, dur: float) -> tuple[TG.Maker, np.ndarray]:
    """A steady pattern at `bpm` from 0.5 s: returns the maker (activation, rms) and the beat times."""
    m = TG.Maker(seed, dur)
    p = 60.0 / bpm
    beats = 0.5 + p * np.arange(int((dur - 1.0) / p))
    amp = np.where(np.arange(beats.size) % 4 == 0, 0.9, 0.6)
    m.peaks(beats + m.rng.normal(0, 0.004, beats.size), amp, 0.1)
    off = beats[(np.arange(beats.size) % 2) == 1] + p / 2
    m.peaks(off, 0.2, 0.2)
    return m, beats


def splice(first: tuple[TG.Maker, np.ndarray], second: tuple[TG.Maker, np.ndarray], start2: float, gap_s: float = 0.0) -> tuple[C.Track, np.ndarray]:
    """first up to PRE_S, [gap_s of silence], then second from start2 s on; returns the track and the true beats after the skip."""
    def cut(m: TG.Maker, a: float, b: float) -> dict:
        ia, ib = int(a * 100), int(b * 100)
        return {"act": m.act[ia:ib], "rms": m.rms[int(a * 10):int(b * 10)], "flux": np.zeros((ib - ia, 2), np.float32),
                "rate": np.zeros((0, 3)), "len": b - a}

    p1 = cut(first[0], 0.0, PRE_S)
    parts = [p1]
    if gap_s:
        n = int(gap_s * 100)
        parts.append({"act": np.full(n, 0.001), "rms": np.full(int(gap_s * 10), 0.0002), "flux": np.zeros((n, 2), np.float32),
                      "rate": np.zeros((0, 3)), "len": gap_s})
    end2 = min(start2 + PRE_S, second[0].dur - 0.5)
    parts.append(cut(second[0], start2, end2))
    track = ST.join(parts)
    b2 = second[1]
    after = b2[(b2 >= start2) & (b2 < end2)] - start2 + PRE_S + gap_s
    return track, after


def cases() -> dict[str, tuple[C.Track, np.ndarray, np.ndarray, float]]:
    """name -> (track, true beats before the skip, true beats after it, skip time)."""
    out = {}
    a = music(31, BPM, 130.0)
    p = 60.0 / BPM
    pre = a[1][a[1] < PRE_S]
    for name, off_beats, gap in (("jump", 22.37, 0.0), ("nudge", 0.37, 0.0), ("gap", 0.5, 4.0)):
        tr, after = splice(a, a, PRE_S + off_beats * p, gap)
        out[name] = (tr, pre, after, PRE_S)
    tr, after = splice(a, music(32, 140.0, 130.0), 30.0)
    out["other"] = (tr, pre, after, PRE_S)
    return out


def host_summary(track: C.Track, pre: np.ndarray, after: np.ndarray, skip_t: float, cfg: R.RConfig) -> dict:
    m = ST.metrics(R.replay(track, cfg), skip_t, pre, after) or {}
    keys = ("tempo_relock_beats", "phase_relock_beats", "wrong_out_beats", "gate_s")
    return {k: (None if m.get(k) is None else round(float(m[k]), 2)) for k in keys if k in m} | {"locked_before": bool(m.get("locked_before"))}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path)
    ap.add_argument("--summary", action="store_true")
    a = ap.parse_args()
    sweep = json.loads((REPO / "studies" / "follow-robust" / "skip.json").read_text())
    cfgs = RC.named(["all-d-f60", "all-act2-c1", "a2-pat"], sweep)
    cs = cases()
    summary = {}
    for name, (tr, pre, after, skip_t) in cs.items():
        summary[name] = {cfg.name: host_summary(tr, pre, after, skip_t, cfg) for cfg in cfgs}
        print(name, json.dumps(summary[name]))
    if a.out:
        a.out.mkdir(parents=True, exist_ok=True)
        for name, (tr, pre, after, skip_t) in cs.items():
            (a.out / f"{name}.track.txt").write_text("\n".join(TG.track_lines(tr)) + "\n")
            np.save(a.out / f"{name}.truth_after.npy", after)
        (a.out / "summary.json").write_text(json.dumps(summary, indent=1))
        print("wrote", a.out)


if __name__ == "__main__":
    main()
