"""The live estimator (live/downbeat.py: streaming windows, Link's beats counted in real time from ModeBpm's grid) against
this study's offline pipeline (linkbeats.py + extract.beat_features + exp_linear.rel_multi + final.decide_link with the
exported "stable" policy), on the same audio and the same tracked beats:
  - Link's bar as heard (evalcore.link_score: right % of the scored beats, the bar arbitrary until the first move) for
    both, against the Traktor reference;
  - the features both compute for the same beat (matched by time): Pearson r and the median relative difference;
  - where both have a bar: how often they put the same beat on the 1;
  - variants of the live estimator (VARIANTS): the reset rules and the first move. A variant is adopted only when it
    measures better on two disjoint sets of test-half tracks (--offset 0 and 2: every 5th track from 0, from 2).

    PYTHONPATH=..:../../.. python stream_check.py --tracks 80 --offset 0 --jobs 4 --out ../results/v2_stream_check.md
"""
from __future__ import annotations

import argparse
import os
import time
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

from live.downbeat import FEATURES, LISTENING, LOCKED, Downbeat, Driver, frame_of

HERE = Path(__file__).resolve().parent
MATCH = 0.02            # s: a streaming beat and an offline Link beat are the same beat
ONE_THREAD = ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS")


class FirstHold(Downbeat):
    """The first move only when the same best phase has led the second by the commit level for `k` decisions in a row
    (against a lock on one noisy early score)."""
    k = 2

    def _policy(self, count, t):
        if self._state != LISTENING:
            self._fh = (-1, 0)
            return super()._policy(count, t)
        self._seq += 1
        self._dec_count, self._dec_time = count, t
        s = self.scores()
        self._scores = s
        if s is None:
            self._lead, self._fh = 0.0, (-1, 0)
            return
        best = int(np.argmax(s))
        self._lead = float(s[best] - np.sort(s)[-2])
        prev, run = getattr(self, "_fh", (-1, 0))
        run = (run + 1 if best == prev else 1) if self._lead >= self.model.commit else 0
        self._fh = (best, run)
        if run >= self.k:
            self._lock(best, s)


class FirstHold4(FirstHold):
    k = 4


class MinPerPosition(Downbeat):
    """Scores only once every bar position has `m` beats in its accumulator (the first scores rest on 1 beat each)."""
    m = 2

    def scores(self):
        return None if (self._acc_n < self.m).any() else super().scores()


class LateStart(Downbeat):
    """As first built: the shadow beat played on the frame where counting starts is not counted, so the count starts a
    beat after the offline study's in about a fifth of the tracks."""
    promote_first = False


class Pause(Driver):
    """The estimator stopping (30 s without a steady beat, ModeBpm stopped) pauses the count instead of resetting it:
    the count goes on over the gap rounded when the grid comes back, as linkbeats.py (the offline study) counts."""

    def event(self, kind, frame, *args):
        if kind != "stop":
            return super().event(kind, frame, *args)
        db = self.db
        db._grid, db._running, db._next, db._open = None, False, None, False


# name -> (Downbeat class, Driver class, jump_reset); the first is the live default
VARIANTS = {
    "live (default)": (Downbeat, Driver, False),
    "count starts a beat later (as first built)": (LateStart, Driver, False),
    "jump reset (first design)": (Downbeat, Driver, True),
    "stop pauses, no reset": (Downbeat, Pause, False),
    "first move held 2 beats": (FirstHold, Driver, False),
    "first move held 4 beats": (FirstHold4, Driver, False),
    "scores from 2 beats a position": (MinPerPosition, Driver, False),
}


def stream(L, ob, t_link, c_link, rp, variant):
    """A live variant streamed; its bar on the offline beats (the bar position it gives each beat, as an offset in the
    offline count; a reset leaves Link's bar where it is)."""
    from eval.mode_bpm import ModeBpm
    from evalcore import link_score
    db_cls, drv_cls, jump_reset = VARIANTS[variant]
    db = db_cls()
    db.log = []
    drv = drv_cls(db, jump_reset)
    drv.trace = []
    est = ModeBpm()
    by_frame: dict[int, list[float]] = {}
    for t in ob:
        by_frame.setdefault(frame_of(t), []).append(float(t))
    for f0 in range(0, len(L), 10):
        rows = L[f0:f0 + 10]
        found = [t for f in range(f0, f0 + len(rows)) for t in by_frame.get(f, ())]
        drv.chunk(est, f0, rows, found, (f0 + len(rows) - 1) / 100.0, silent=False)
    dec = [r for r in db.log if r[0] == "d"]
    t_d = np.array([r[2] for r in dec])
    off = np.full(len(t_link), -1)
    cur = -1
    for i, t in enumerate(t_link):
        j = int(np.argmin(np.abs(t_d - t))) if len(t_d) else -1
        if j >= 0 and abs(t_d[j] - t) <= MATCH and dec[j][4] == LOCKED:
            cur = int((c_link[i] - (dec[j][1] - dec[j][5]) % 4) % 4)
        off[i] = cur
    moved = np.r_[off[0] >= 0, (off[1:] != off[:-1])]
    s = link_score(t_link, c_link, off, moved, rp)
    return {"right": s.correct / max(s.n, 1), "moves": int(moved.sum()), "first_right": s.first_right,
            "first_s": s.first_move_s, "off": off, "log": db.log, "trace": drv.trace, "dec": dec}


def one(job) -> dict:
    import json

    from evalcore import link_score
    from extract import audio_path, beat_features, frame_curves, frames_logbands, load_audio, online_beats
    from extract import traktor_positions
    from final import decide_link
    from linear import rotations
    from linkbeats import link_beats
    from rules import ref_positions, valid_4

    import exp_linear as xl
    piece, file_rel = job
    t0 = time.perf_counter()
    y = load_audio(audio_path(piece, file_rel))
    L = frames_logbands(y)
    ob = online_beats(y)
    positions = traktor_positions(piece.beats, piece.downbeats)
    valid, _, _ = valid_4(positions)
    w_json = json.loads((HERE.parent / "results" / "v2_weights.json").read_text())
    w, pol = np.asarray(w_json["w"]), w_json["link"]

    # the study, offline
    t_link, c_link = link_beats(ob)
    if len(t_link) < 32:
        return {"name": piece.name, "error": "too few Link beats"}
    S = beat_features(t_link, L, frame_curves(L)).astype(float)
    rp = ref_positions(t_link, piece.beats, positions, valid)
    rel = xl.rel_multi(S, c_link, (16,), 5)
    ready = ~np.isnan(rel).any(axis=(1, 2))
    sc = rotations(np.nan_to_num(rel)) @ w
    off_a, moved_a = decide_link(sc, ready, pol["commit"], pol["switch"], pol["hold"])
    s_a = link_score(t_link, c_link, off_a, moved_a, rp)

    var = {name: stream(L, ob, t_link, c_link, rp, name) for name in VARIANTS}
    base = var["live (default)"]
    both = (off_a >= 0) & (base["off"] >= 0)
    agree = float(np.mean(off_a[both] == base["off"][both])) if both.any() else float("nan")

    # features of the same beats (the default variant)
    pairs_a, pairs_b = [], []
    for _, _, t, x in [r for r in base["log"] if r[0] == "x"]:
        i = int(np.argmin(np.abs(t_link - t)))
        if abs(t_link[i] - t) <= MATCH:
            pairs_a.append(S[i])
            pairs_b.append(x)
    A, B = np.array(pairs_a), np.array(pairs_b)
    r = [float(np.corrcoef(A[:, k], B[:, k])[0, 1]) if A[:, k].std() > 0 and B[:, k].std() > 0 else float("nan")
         for k in range(len(FEATURES))]
    rd = [float(np.median(np.abs(A[:, k] - B[:, k])) / max(np.median(np.abs(A[:, k])), 1e-9))
          for k in range(len(FEATURES))]
    dec = base["dec"]
    return {"name": piece.name, "n": s_a.n, "minutes": s_a.minutes, "agree": agree, "r": r, "rd": rd,
            "matched": len(A) / max(sum(1 for x in base["log"] if x[0] == "x"), 1),
            "offline": {"right": s_a.correct / max(s_a.n, 1), "moves": int(moved_a.sum()), "first_right": s_a.first_right,
                        "first_s": s_a.first_move_s},
            "var": {k: {kk: v[kk] for kk in ("right", "moves", "first_right", "first_s")} for k, v in var.items()},
            "resets": sum(1 for k in range(1, len(dec)) if dec[k][1] < dec[k - 1][1]),
            "jumps": sum(1 for e in var["jump reset (first design)"]["trace"] if e[0] == "reset"),
            "stops": sum(1 for e in base["trace"] if e[0] == "stop"), "s": time.perf_counter() - t0}


def summary_row(name: str, rows: list[dict]) -> str:
    right = [x["right"] for x in rows]
    fr = [x["first_right"] for x in rows if x["first_right"] is not None]
    fs = [x["first_s"] for x in rows if np.isfinite(x["first_s"])]
    return (f"| {name} | {100 * np.mean(right):.1f} | {100 * np.median(right):.1f} | {sum(x >= 0.9 for x in right)} | "
            f"{sum(x['moves'] for x in rows)} | {100 * np.mean(fr):.0f} | {np.median(fs):.0f} |")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracks", type=int, default=80)
    ap.add_argument("--offset", type=int, default=0, help="the first test-half track of the set (disjoint sets: 0, 2)")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    os.nice(19)
    for v in ONE_THREAD:                         # here and in the workers (they inherit it)
        os.environ[v] = "1"
    import json

    from eval import data
    files = {r["name"]: r["file"] for r in json.loads((data.DATA / "traktor" / "index.json").read_text())}
    test = data.traktor()[1::2]
    step = max(1, len(test) // a.tracks)
    pick = test[a.offset::step][:a.tracks]
    print(f"load {os.getloadavg()[0]:.1f}, {len(pick)} test-half tracks, {a.jobs} workers x 1 thread, nice 19", flush=True)
    t0 = time.time()
    with ProcessPoolExecutor(a.jobs) as pool:
        res = list(pool.map(one, [(p, files[p.name]) for p in pick], chunksize=1))
    ok = [r for r in res if "error" not in r]
    lines = ["# Live estimator (live/downbeat.py) against the study's offline pipeline\n",
             f"{len(ok)} Traktor test-half tracks (every {step}th from #{a.offset} of the test half), same audio and "
             f"tracked beats; Link's bar as heard (evalcore.link_score), the exported weights and the stable policy "
             f"(commit 2, switch 3 held 8). Run {time.strftime('%Y-%m-%d')}, {time.time() - t0:.0f} s.\n",
             "| method | right, mean % | median % | tracks >= 90 % | moves | first move right % | median s to first move |",
             "|---|---|---|---|---|---|---|", summary_row("offline (study)", [r["offline"] for r in ok])]
    for name in VARIANTS:
        lines.append(summary_row(f"live: {name}", [r["var"][name] for r in ok]))
    lines += [f"\nlive (default): same 1 as offline where both have a bar {100 * np.nanmean([r['agree'] for r in ok]):.1f} % "
              f"(mean over tracks); resets on {sum(r['resets'] > 0 for r in ok)} tracks (stops: "
              f"{sum(r['stops'] for r in ok)} in all); beats matched to an offline beat "
              f"{100 * np.mean([r['matched'] for r in ok]):.1f} %.\n",
              "| feature | Pearson r, median over tracks | median relative difference, median over tracks |", "|---|---|---|"]
    for k, f in enumerate(FEATURES):
        lines.append(f"| {f} | {np.nanmedian([r['r'][k] for r in ok]):.3f} | {np.nanmedian([r['rd'][k] for r in ok]):.3f} |")
    names = list(VARIANTS)
    lines += ["\n| track | offline % | " + " | ".join(names) + " | resets (default) |",
              "|---" * (len(names) + 3) + "|"]
    for r in ok:
        lines.append(f"| {r['name'][:60]} | {100 * r['offline']['right']:.0f} | " +
                     " | ".join(f"{100 * r['var'][n]['right']:.0f}" for n in names) + f" | {r['resets']} |")
    for r in res:
        if "error" in r:
            lines.append(f"\nerror {r['name']}: {r['error']}")
    a.out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines[:12]), flush=True)


if __name__ == "__main__":
    main()
