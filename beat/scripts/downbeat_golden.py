"""Golden vectors for the device's downbeat estimator (jank core/beat/jb_down.c, tests/test_down.c).

For each audio file: the live chain (live.tracker, live.downbeat.BandTap, eval.mode_bpm.ModeBpm, live.downbeat.Driver)
in chunks of 10 frames, the bands quantised to 1/8192 so that C and Python read the very same numbers. Written per clip:
    NAME.bands        uint16 little-endian, frames x 81 (L = q / 8192)
    NAME.events.txt   the estimator's input events after their frame: g F t tb p running | s F | r F | n F | a F on
    NAME.expect.txt   x count t f0..f21 (a beat's features as they enter) | d count t state phi lead s0..s3 (a decision;
                      state 0 off 1 listening 2 locked 3 manual, phi -1 none, scores '-' before ready)
Extra events can be injected to cover the C paths (a reset, a nudge, AUTO BAR off and on): --inject 'r@1500,n@1900,a0@2100,a1@2200'.

    .venv/bin/python scripts/downbeat_golden.py --out DIR WAV [WAV ...] [--inject SPEC] [--seconds S]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

SCALE = 8192
BANDS = 81
STATES = {"off": 0, "listening": 1, "locked": 2, "manual": 3}     # live.downbeat's states


def parse_inject(spec: str) -> dict[int, list[tuple]]:
    out: dict[int, list[tuple]] = {}
    for item in filter(None, spec.split(",")):
        what, at = item.split("@")
        kind = {"r": ("reset",), "n": ("nudge",), "a0": ("auto", 0.0), "a1": ("auto", 1.0)}[what]
        out.setdefault(int(at), []).append(kind)
    return out


def run(wav: Path, seconds: float | None, inject: dict[int, list[tuple]]) -> tuple[np.ndarray, list, list]:
    from eval.mode_bpm import ModeBpm
    from live.audio import load_audio, to_mono
    from live.downbeat import BandTap, Downbeat, Driver
    from live.tracker import HOP, SAMPLE_RATE, OnlineTracker
    y = to_mono(load_audio(wav))
    if seconds:
        y = y[:int(seconds * SAMPLE_RATE)]
    tracker, tap, est = OnlineTracker(90.0, 180.0), BandTap(), ModeBpm()
    db = Downbeat()
    db.log = []
    drv = Driver(db)
    drv.trace = []
    q_all = []
    step = HOP * 10
    for s in range(0, len(y), step):
        chunk = y[s:s + step]
        found = tracker.feed(chunk)
        f0, bands = tap.feed(chunk)
        q = np.clip(np.round(bands * SCALE), 0, 65535).astype("<u2")
        q_all.append(q)
        drv.chunk(est, f0, q.astype(np.float32) / SCALE, found, tracker.time, silent=False)
        last = f0 + len(bands) - 1
        for f in [f for f in inject if f0 <= f <= last]:
            for ev in inject.pop(f):
                drv.event(ev[0], last, *ev[1:])
    return np.concatenate(q_all), drv.trace, db.log


def write(out: Path, name: str, q: np.ndarray, trace: list, log: list) -> None:
    out.mkdir(parents=True, exist_ok=True)
    if q.shape[1] != BANDS:
        raise ValueError(q.shape)
    (out / f"{name}.bands").write_bytes(q.tobytes())
    ev = [f"# {name}: {len(q)} frames, bands = uint16 / {SCALE}; events apply after their frame"]
    for kind, f, *args in trace:
        if kind == "grid":
            ev.append(f"g {f} {args[0]:.17g} {args[1]:.17g} {args[2]:.17g} {int(args[3])}")
        elif kind == "auto":
            ev.append(f"a {f} {int(args[0])}")
        else:
            ev.append(f"{kind[0]} {f}")
    (out / f"{name}.events.txt").write_text("\n".join(ev) + "\n")
    ex = [f"# {name}: the Python estimator's output (live/downbeat.py)"]
    for row in log:
        if row[0] == "x":
            _, c, t, x = row
            ex.append(f"x {c} {t:.17g} " + " ".join(f"{v:.9g}" for v in x))
        else:
            _, c, t, sc, state, phi, lead = row
            scs = " ".join(f"{v:.9g}" for v in sc) if sc is not None else "- - - -"
            ex.append(f"d {c} {t:.17g} {STATES[state]} {-1 if phi is None else phi} {lead:.9g} {scs}")
    (out / f"{name}.expect.txt").write_text("\n".join(ex) + "\n")
    n_d = sum(1 for r in log if r[0] == "d")
    locked = sum(1 for r in log if r[0] == "d" and r[4] == "locked")
    print(f"{name}: {len(q)} frames, {len(trace)} events, {n_d} decisions ({locked} locked), "
          f"{sum(1 for r in log if r[0] == 'x')} beats entered", flush=True)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--seconds", type=float)
    ap.add_argument("--inject", default="", help="events for the first file: r@F (reset), n@F (nudge), a0@F / a1@F")
    ap.add_argument("wav", type=Path, nargs="+")
    a = ap.parse_args()
    for i, wav in enumerate(a.wav):
        q, trace, log = run(wav, a.seconds, parse_inject(a.inject) if i == 0 else {})
        write(a.out, wav.stem, q, trace, log)


if __name__ == "__main__":
    main()
