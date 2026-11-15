"""Golden vectors for the device's advanced beat follow (jank core/beat/jb_follow.c, core/jank_conf.c's line
confidence, core/beat/jb_rate.c; tests/test_follow.c): eval/follow_bpm.py FollowBpm, live/follow.py LineLog +
line_confidence and live/rate.py RateProbe on the same inputs.

Written to OUT (jank's tests/follow_ref):
    NAME.txt       a follower run: tracked beats fed one at a time to FollowBpm, rate proposals between them, and a query
                   every 0.25 s (the device's step) that ticks the LineLog with the follower's grid while it gives a
                   tempo (studies/advanced-follow/replay.py) and takes the line confidence of the last 64 beats and the
                   gate at 0.8 and 0.5:
                     b t                                            a beat
                     s kf mode t p d jumps misses in_a_row manoeuvres follow_beats ratios refused
                                                                    the state after it (kf 0: ModeBpm's own, t p d 0)
                     r ratio t since                                a proposal (propose_ratio)
                     q now has_bpm bpm has_grid t0 p conf g80 g50 lines
    rate_NAME.txt  a rate run on a synthetic signal that the C test makes again from its description: the frames' |FFT|
                   as the device takes them (jb_fe's centred frames) into RateProbe(step, every):
                     case dur t0 ratio ramp_s tchord step every
                     p f a / q f a                                  partials before / after tchord
                     e t ratio peak                                 each estimate
                     v t ratio                                      each event
Numbers are written with repr: C reads the very same doubles.

    .venv/bin/python scripts/follow_golden.py --out DIR --beats BEATS_DIR
"""
from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

STEP_S = 0.25          # the device's step (firmware/main/beat.c STEP_EVERY_US)
TAIL_S = 10.0
KEEP = 64              # the beats the confidence sees (jank_conf.h JCF_KEEP, replay.py fed[-64:])
MODES = {"stopped": 0, "steady": 1, "speeding_up": 2, "slowing_down": 3}
SR = 44100


def gate(gated: bool, conf: float, threshold: float) -> bool:
    from live.engine import HYSTERESIS
    if threshold <= 0:
        return False
    return conf < min(threshold + HYSTERESIS, 1.0) if gated else conf < threshold


def state_line(est) -> str:
    kf = est.kf
    x = kf.x if kf is not None else np.zeros(3)
    man = kf.manoeuvres if kf is not None else -1
    ratios = sum(1 for _, e in est.log if e == "ratio")
    refused = sum(1 for _, e in est.log if e == "ratio refused")
    return (f"s {int(kf is not None)} {MODES[est.mode]} {float(x[0])!r} {float(x[1])!r} {float(x[2])!r} {est.jumps} "
            f"{est.misses} {est.in_a_row} {man} {est.follow_beats} {ratios} {refused}")


def run_lines(beats: list[float], proposals: list[tuple[float, float, float | None]]) -> list[str]:
    """beats ascending; proposals (ratio, t, since), delivered after the beats up to t (replay.py's on_tick order)."""
    from eval.follow_bpm import FollowBpm
    from live.follow import LineLog, line_confidence
    est, log, kept, out = FollowBpm(), LineLog(), [], []
    g80 = g50 = False
    props = sorted(proposals, key=lambda p: p[1])
    i, j, now, end = 0, 0, STEP_S, beats[-1] + TAIL_S
    while now <= end:
        while i < len(beats) and beats[i] <= now:
            est.update(beats[i])
            kept.append(beats[i])
            del kept[:-KEEP]
            out.append(f"b {beats[i]!r}")
            out.append(state_line(est))
            i += 1
        while j < len(props) and props[j][1] <= now:
            r, t, since = props[j]
            est.propose_ratio(r, t, since)
            out.append(f"r {r!r} {t!r} {float('nan') if since is None else since!r}")
            j += 1
        bpm, grid = est.bpm_at(now), est.grid()
        log.tick(grid if bpm is not None else None, now)
        conf = line_confidence(kept, log, grid[1] if grid else None, now)
        g80, g50 = gate(g80, conf, 0.8), gate(g50, conf, 0.5)
        t0, p = grid if grid is not None else (0.0, 0.0)
        out.append(f"q {now!r} {int(bpm is not None)} {float(bpm or 0.0)!r} {int(grid is not None)} {float(t0)!r} "
                   f"{float(p)!r} {conf!r} {int(g80)} {int(g50)} {len(log.times())}")
        now = round(now + STEP_S, 6)
    return out


def stepped(rng, bpm: float, change: float, at_beat: int, n: int, jitter: float = 0.004) -> np.ndarray:
    p0, p1 = 60 / bpm, 60 / (bpm * (1 + change))
    return np.cumsum(np.where(np.arange(n) < at_beat, p0, p1)) + rng.normal(0, jitter, n)


def ramp(start: float, end: float, t_from: float, span: float, until: float) -> list[float]:
    out, t = [], 0.0
    while t < until:
        t += 60 / (start + (end - start) * min(max((t - t_from) / span, 0), 1))
        out.append(t)
    return out


def synthetic(rng) -> dict[str, tuple[list[float], list]]:
    runs: dict[str, tuple[np.ndarray, list]] = {}
    runs["steady128"] = (stepped(rng, 128, 0.0, 0, 300), [])
    for c in (0.01, 0.02, 0.03, 0.05, 0.08, -0.02, -0.05):
        runs[f"step_{'p' if c > 0 else 'm'}{abs(c) * 100:.0f}"] = (stepped(rng, 135, c, 160, 280), [])
    b = stepped(rng, 135, 0.05, 160, 280)
    runs["step_p5_ratio"] = (b, [(1.05, float(b[160]) + 0.2, float(b[160]) - 0.3)])
    b = stepped(rng, 135, -0.03, 160, 280)
    runs["step_m3_ratio"] = (b, [(0.97, float(b[160]) + 0.2, float(b[160]) - 0.3)])
    b = stepped(rng, 135, 0.0, 0, 220)
    runs["false_ratio"] = (b, [(1.04, float(b[150]) + 0.1, float(b[150]) + 0.05)])
    b = stepped(rng, 128, 0.0, 0, 120)
    runs["ratio_untrusted"] = (b, [(1.03, float(b[12]) + 0.1, None), (1.03, float(b[80]) + 0.1, None)])
    rs = ramp(124, 130, 60, 60, 160)
    runs["ramp_slow"] = (np.array(rs) + rng.normal(0, 0.004, len(rs)), [])
    rs = ramp(128, 128 * 1.05, 40, 8, 90)
    runs["ramp_fast"] = (np.array(rs) + rng.normal(0, 0.004, len(rs)), [])
    a = np.arange(0.5, 30, 60 / 101.25)
    runs["lock43"] = (np.concatenate([a, a[-1] + np.cumsum(np.full(120, 60 / 135))]) + rng.normal(0, 0.004, a.size + 120), [])
    b = stepped(rng, 128, 0.0, 0, 300)
    keep = np.ones(b.size, bool)
    keep[100:116] = False
    runs["break16"] = (b[keep], [])
    a = np.arange(0.5, 40, 0.5)
    runs["break_long"] = (np.concatenate([a, 76 + np.arange(0, 40, 60 / 126)]) + rng.normal(0, 0.004, a.size + 84), [])
    a = np.arange(0.3, 60, 60 / 124)
    runs["cut"] = (np.concatenate([a, a[-1] + 0.21 + np.cumsum(np.full(150, 60 / 132))]), [])
    runs["cut_phase"] = (np.concatenate([a, a[-1] + 0.24 + np.cumsum(np.full(150, 60 / 124))]), [])
    b = stepped(rng, 128, 0.0, 0, 300)
    runs["offbeat_fill"] = (np.sort(np.concatenate([b, b[150:156] + 0.2])), [])
    runs["wander"] = (np.cumsum(rng.uniform(0.4, 0.7, 90)), [])
    return {k: (sorted(float(t) for t in v), p) for k, (v, p) in runs.items()}


# ---- the rate: synthetic signals ----

CHORD_A = [(330.0, 0.10), (415.3, 0.09), (495.0, 0.08), (660.0, 0.08), (830.6, 0.07), (990.0, 0.06), (1320.0, 0.05),
           (1980.0, 0.05), (2640.0, 0.04), (3960.0, 0.03), (5280.0, 0.02)]
CHORD_B = [(370.0, 0.10), (440.0, 0.09), (554.4, 0.08), (740.0, 0.08), (880.0, 0.07), (1108.7, 0.06), (1480.0, 0.05),
           (2217.5, 0.05), (2960.0, 0.04), (4434.9, 0.03), (5920.0, 0.02)]
RATE_CASES = {          # dur t0 ratio ramp_s tchord
    "steady": (14.0, 9.0, 1.0, 0.0, 1000.0),
    "p2": (14.0, 9.0, 1.02, 0.0, 1000.0),
    "m3": (14.0, 9.0, 0.97, 0.0, 1000.0),
    "p5_ramp": (14.0, 9.0, 1.05, 1.0, 1000.0),
    "chord": (14.0, 9.0, 1.0, 0.0, 9.0),
}


def synth(dur: float, t0: float, r: float, ramp_s: float, tchord: float) -> np.ndarray:
    """jank's tests/test_follow.c synth(): the same formula, term by term."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    if ramp_s > 0:
        u = t - t0
        g = np.where(t < t0, 0.0, np.where(t < t0 + ramp_s, u * u / (2 * ramp_s), ramp_s / 2 + (u - ramp_s)))
    else:
        g = np.where(t < t0, 0.0, t - t0)
    tau = t + (r - 1) * g
    env = 0.25 + 0.75 * np.exp(-np.mod(tau, 0.46875) / 0.06)
    out = []
    for chord in (CHORD_A, CHORD_B):
        s = np.zeros(n)
        for i, (f, a) in enumerate(chord):
            w = 2.0 * math.pi * f
            s = s + a * np.sin(w * tau + i)
        out.append(s)
    s = np.where(t < tchord, out[0], out[1])
    return (env * s).astype(np.float32)


def rate_lines(name: str, step: int, every: int) -> list[str]:
    from live.rate import RateProbe, centred_magnitudes
    dur, t0, r, ramp_s, tchord = RATE_CASES[name]
    y = synth(dur, t0, r, ramp_s, tchord)
    frames = (len(y) - 1024) // 441 + 1
    mag = centred_magnitudes(y, frames)
    probe = RateProbe(step, every)
    events = [e for k in range(frames) if (e := probe.frame(k, mag[k])) is not None]
    out = [f"case {dur!r} {t0!r} {r!r} {ramp_s!r} {tchord!r} {step} {every}"]
    out += [f"p {f!r} {a!r}" for f, a in CHORD_A] + [f"q {f!r} {a!r}" for f, a in CHORD_B]
    out += [f"e {t!r} {x!r} {p!r}" for t, x, p in probe.estimates]
    out += [f"v {t!r} {x!r}" for t, x in events]
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--beats", type=Path, required=True, help="jank's tests/beat_ref (NAME.beats.txt, one time a line)")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(20261116)
    runs = {p.name.removesuffix(".beats.txt"): ([float(x) for x in p.read_text().split()], [])
            for p in sorted(args.beats.glob("*.beats.txt"))}
    runs.update(synthetic(rng))
    for name, (beats, props) in runs.items():
        head = (f"# {name}: FollowBpm + LineLog / line_confidence every {STEP_S} s, the gate at 0.8 and 0.5 "
                f"(scripts/follow_golden.py)\n")
        lines = run_lines(beats, props)
        (args.out / f"{name}.txt").write_text(head + "\n".join(lines) + "\n")
        print(f"{name}: {len(beats)} beats, {len(props)} proposals")
    for name in RATE_CASES:
        for step, every in ((1, 1), (2, 1), (2, 2)):
            head = (f"# rate {name} step {step} every {every}: live/rate.py RateProbe on jb_fe's frames "
                    f"(scripts/follow_golden.py)\n")
            lines = rate_lines(name, step, every)
            (args.out / f"rate_{name}_s{step}e{every}.txt").write_text(head + "\n".join(lines) + "\n")
            print(f"rate_{name}_s{step}e{every}: {sum(1 for x in lines if x[0] == 'e')} estimates, "
                  f"{[x for x in lines if x[0] == 'v']}")


if __name__ == "__main__":
    main()
