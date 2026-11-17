"""Golden vectors for the device's LOW CONFIDENCE (jank core/jank_conf.c, tests/test_conf.c): live/confidence.py's
grid_confidence and live/engine.py's gate, on the same beats and grids.

Written to OUT (jank's tests/conf_ref):
    calls.txt    direct calls: c window delay tol has_grid t0 period now expected n b1 .. bn
    NAME.txt     a run: tracked beats fed one at a time to eval.mode_bpm.ModeBpm (as the engine does) and queried every
                 0.25 s: b t (a beat) | q now has_grid t0 period conf gated80 gated50 (ModeBpm's grid, the confidence
                 from it, and the engine's gate at thresholds 0.8 and 0.5, HYSTERESIS included)
The runs: the clips' tracked beats in jank's tests/beat_ref (BEATS_DIR) and synthetic ones (a break, a tempo change, a
wandering tracker, off-beat extras, missed beats). Numbers are written with repr: C reads the very same doubles.

    .venv/bin/python scripts/confidence_golden.py --out DIR --beats BEATS_DIR
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

STEP_S = 0.25          # the device looks after the session this often (firmware/main/beat.c STEP_EVERY_US)
TAIL_S = 10.0          # queries go on this long after the last beat: the confidence falls to 0


def gate(gated: bool, conf: float, threshold: float) -> bool:
    """live/engine.py Engine.status's gate, as written there."""
    from live.engine import HYSTERESIS
    if threshold <= 0:
        return False
    if gated:
        return conf < min(threshold + HYSTERESIS, 1.0)
    return conf < threshold


def run_lines(beats: list[float]) -> list[str]:
    from eval.mode_bpm import ModeBpm
    from live.confidence import grid_confidence
    from live.engine import KEEP_BEATS
    est, kept, lines = ModeBpm(), [], []
    g80 = g50 = False
    i, now, end = 0, STEP_S, beats[-1] + TAIL_S
    while now <= end:
        while i < len(beats) and beats[i] <= now:
            est.update(beats[i])
            kept.append(beats[i])
            del kept[:-KEEP_BEATS]
            lines.append(f"b {beats[i]!r}")
            i += 1
        grid = est.grid()
        conf = grid_confidence(kept, grid, now)
        g80, g50 = gate(g80, conf, 0.8), gate(g50, conf, 0.5)
        t0, period = grid if grid is not None else (0.0, 0.0)
        lines.append(f"q {now!r} {int(grid is not None)} {float(t0)!r} {float(period)!r} {conf!r} {int(g80)} {int(g50)}")
        now = round(now + STEP_S, 6)
    return lines


def grid_beats(bpm: float, start: float, end: float, rng: np.random.Generator, jitter: float = 0.005) -> list[float]:
    period = 60.0 / bpm
    t = np.arange(start, end, period)
    return list(t + rng.normal(0.0, jitter, len(t)))


def synthetic_runs(rng: np.random.Generator) -> dict[str, list[float]]:
    runs: dict[str, list[float]] = {}
    runs["break"] = grid_beats(120, 0.5, 25.0, rng) + grid_beats(120, 31.0, 50.0, rng)
    runs["tempo_change"] = grid_beats(128, 0.5, 20.0, rng) + grid_beats(100, 20.3, 40.0, rng)
    runs["wander"] = list(np.cumsum(rng.uniform(0.4, 0.7, 70)))
    base = grid_beats(120, 0.5, 30.0, rng)
    extras = [t + 0.25 for t in base if 10.0 <= t < 20.0]
    runs["offbeat_extras"] = base + extras
    runs["missed"] = [t for t in grid_beats(120, 0.5, 30.0, rng) if rng.random() > 0.3]
    return {k: sorted(float(t) for t in v) for k, v in runs.items()}     # (plain floats: repr gives the bare number)


def call_line(beats: list[float], grid: tuple[float, float] | None, now: float,
              window: float | None = None, delay: float | None = None, tol: float | None = None) -> str:
    from live.confidence import DELAY_S, TOL, WINDOW_S, grid_confidence
    window = WINDOW_S if window is None else window
    delay = DELAY_S if delay is None else delay
    tol = TOL if tol is None else tol
    conf = grid_confidence(beats, grid, now, window, delay, tol)
    t0, period = grid if grid is not None else (0.0, 0.0)
    head = f"c {window!r} {delay!r} {tol!r} {int(grid is not None)} {float(t0)!r} {float(period)!r} {now!r} {conf!r} {len(beats)}"
    return " ".join([head] + [repr(float(b)) for b in beats])


def calls(rng: np.random.Generator) -> list[str]:
    perfect = [0.5 * k for k in range(1, 40)]
    out = [
        call_line(perfect, (10.0, 0.5), 15.0),              # a perfect fit: 1.0
        call_line(perfect, None, 15.0),                     # no grid
        call_line(perfect, (10.0, 0.0), 15.0),              # period 0
        call_line(perfect, (10.0, -0.5), 15.0),             # negative period
        call_line([], (10.0, 0.5), 15.0),                   # no beats
        call_line(perfect, (10.0, 0.5), 15.0, delay=8.0),   # hi == lo: 0
        call_line(perfect, (10.0, 0.5), 15.0, window=0.05, delay=0.0),   # a window with no slot
        call_line(perfect, (10.0, 0.75), 15.0),             # the wrong tempo
        call_line(perfect, (10.25, 0.5), 15.0),             # the off-beat
        call_line(perfect, (10.0, 0.25), 15.0),             # double time: half the slots missed
        call_line(perfect, (10.0, 1.0), 15.0),              # half time: half the beats extra
        call_line([5.0, 5.05, 5.5, 6.0], (5.0, 0.5), 13.0), # a beat at the margin's edge
        call_line([7.05, 7.5, 8.0], (7.0, 0.5), 15.04),     # beats just in / just out of the window
        call_line(perfect, (10.0, 0.5), 5.0),               # the window before the beats' start
        call_line(perfect, (1000.0, 0.5), 15.0),            # the grid's beat far ahead
    ]
    for _ in range(40):                                     # random grids and jittered beats
        period = float(rng.uniform(60 / 180, 60 / 90))
        t0 = float(rng.uniform(0.0, 20.0))
        beats = sorted(float(t) for t in np.arange(0.0, 30.0, period * rng.uniform(0.9, 1.1)) + rng.normal(0, 0.02, 1))
        beats = sorted(float(b + rng.normal(0, period * 0.06)) for b in beats if rng.random() > 0.1)
        out.append(call_line(beats, (t0, period), float(rng.uniform(8.0, 30.0)), tol=float(rng.choice([0.05, 0.1, 0.2]))))
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--beats", type=Path, required=True, help="jank's tests/beat_ref (NAME.beats.txt, one time a line)")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(20261115)
    head = "# live/confidence.py grid_confidence (window delay tol has_grid t0 period now expected n beats)\n"
    (args.out / "calls.txt").write_text(head + "\n".join(calls(rng)) + "\n")
    runs = {p.name.removesuffix(".beats.txt"): [float(x) for x in p.read_text().split()]
            for p in sorted(args.beats.glob("*.beats.txt"))}
    runs.update(synthetic_runs(rng))
    for name, beats in runs.items():
        head = f"# {name}: ModeBpm + grid_confidence every {STEP_S} s, the gate at 0.8 and 0.5 (live/engine.py HYSTERESIS)\n"
        (args.out / f"{name}.txt").write_text(head + "\n".join(run_lines(beats)) + "\n")
        print(f"{name}: {len(beats)} beats")


if __name__ == "__main__":
    main()
