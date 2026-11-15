"""The live estimator (live/downbeat.py: streaming windows, Link's beats counted in real time from ModeBpm's grid) against
this study's offline pipeline (linkbeats.py + extract.beat_features + exp_linear.rel_multi + final.decide_link with the
exported "stable" policy), on the same audio and the same tracked beats:
  - Link's bar as heard (evalcore.link_score: right % of the scored beats, the bar arbitrary until the first move) for
    both, against the Traktor reference;
  - the features both compute for the same beat (matched by time): Pearson r and the median relative difference;
  - where both have a bar: how often they put the same beat on the 1.

    PYTHONPATH=..:../../.. python stream_check.py --tracks 40 --jobs 4 --out ../results/v2_stream_check.md
"""
from __future__ import annotations

import argparse
import os
import time
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
MATCH = 0.02            # s: a streaming beat and an offline Link beat are the same beat
ONE_THREAD = ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS")


def one(job) -> dict:
    import json

    import numpy as np

    from eval.mode_bpm import ModeBpm
    from evalcore import link_score
    from extract import audio_path, beat_features, frame_curves, frames_logbands, load_audio, online_beats
    from extract import traktor_positions
    from final import decide_link
    from linear import rotations
    from linkbeats import link_beats
    from rules import ref_positions, valid_4

    import exp_linear as xl
    from live.downbeat import FEATURES, LOCKED, Downbeat, Driver, frame_of
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

    def live(jump_reset: bool):
        """The live estimator, streamed; its bar on the offline beats (the bar position it gives each beat, as an
        offset in the offline count; a reset leaves Link's bar where it is)."""
        db = Downbeat()
        db.log = []
        drv = Driver(db, jump_reset)
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
        return off, moved, link_score(t_link, c_link, off, moved, rp), db.log, drv.trace, dec

    off_c, moved_c, s_c, _, _, _ = live(False)
    off_b, moved_b, s_b, log, trace, dec = live(True)
    ent = [r for r in log if r[0] == "x"]
    both = (off_a >= 0) & (off_b >= 0)
    agree = float(np.mean(off_a[both] == off_b[both])) if both.any() else float("nan")

    # features of the same beats
    pairs_a, pairs_b = [], []
    for _, _, t, x in ent:
        i = int(np.argmin(np.abs(t_link - t)))
        if abs(t_link[i] - t) <= MATCH:
            pairs_a.append(S[i])
            pairs_b.append(x)
    A, B = np.array(pairs_a), np.array(pairs_b)
    r = [float(np.corrcoef(A[:, k], B[:, k])[0, 1]) if A[:, k].std() > 0 and B[:, k].std() > 0 else float("nan")
         for k in range(len(FEATURES))]
    rd = [float(np.median(np.abs(A[:, k] - B[:, k])) / max(np.median(np.abs(A[:, k])), 1e-9))
          for k in range(len(FEATURES))]
    return {"name": piece.name, "right_a": s_a.correct / max(s_a.n, 1), "right_b": s_b.correct / max(s_b.n, 1),
            "n": s_a.n, "moves_a": int(moved_a.sum()), "moves_b": int(moved_b.sum()), "agree": agree,
            "matched": len(A) / max(len(ent), 1), "r": r, "rd": rd, "minutes": s_a.minutes,
            "resets": sum(1 for k in range(1, len(dec)) if dec[k][1] < dec[k - 1][1]),
            "jumps": sum(1 for e in trace if e[0] == "reset"), "stops": sum(1 for e in trace if e[0] == "stop"),
            "right_c": s_c.correct / max(s_c.n, 1), "moves_c": int(moved_c.sum()), "s": time.perf_counter() - t0}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracks", type=int, default=40)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    os.nice(19)
    for v in ONE_THREAD:                         # before numpy loads, here and in the workers (they inherit it)
        os.environ[v] = "1"
    import json

    import numpy as np

    from eval import data
    from live.downbeat import FEATURES
    files = {r["name"]: r["file"] for r in json.loads((data.DATA / "traktor" / "index.json").read_text())}
    test = data.traktor()[1::2]
    pick = test[::max(1, len(test) // a.tracks)][:a.tracks]
    print(f"load {os.getloadavg()[0]:.1f}, {len(pick)} test-half tracks, {a.jobs} workers x 1 thread, nice 19", flush=True)
    t0 = time.time()
    with ProcessPoolExecutor(a.jobs) as pool:
        res = list(pool.map(one, [(p, files[p.name]) for p in pick], chunksize=1))
    ok = [r for r in res if "error" not in r]
    lines = ["# Live estimator (live/downbeat.py) against the study's offline pipeline\n",
             f"{len(ok)} Traktor test-half tracks (every {max(1, len(test) // a.tracks)}th), same audio and tracked "
             f"beats; Link's bar as heard (evalcore.link_score), the exported weights and the stable policy "
             f"(commit 2, switch 3 held 8). Run {time.strftime('%Y-%m-%d')}, {time.time() - t0:.0f} s.\n",
             "| | offline (study) | live, reset on tempo jumps | live, no reset on tempo jumps |", "|---|---|---|---|",
             "| right, mean % | " + " | ".join(f"{100 * np.mean([r[k] for r in ok]):.1f}"
                                              for k in ("right_a", "right_b", "right_c")) + " |",
             "| right, median % | " + " | ".join(f"{100 * np.median([r[k] for r in ok]):.1f}"
                                                for k in ("right_a", "right_b", "right_c")) + " |",
             "| tracks >= 90 % | " + " | ".join(f"{sum(r[k] >= 0.9 for r in ok)}"
                                              for k in ("right_a", "right_b", "right_c")) + " |",
             "| moves (all tracks) | " + " | ".join(f"{sum(r[k] for r in ok)}"
                                                  for k in ("moves_a", "moves_b", "moves_c")) + " |",
             f"\nSame 1 where both have a bar: {100 * np.nanmean([r['agree'] for r in ok]):.1f} % (mean over tracks). "
             f"Live resets (tempo jumps, stops) on {sum(r['resets'] > 0 for r in ok)} tracks. Live beats matched to "
             f"an offline beat: {100 * np.mean([r['matched'] for r in ok]):.1f} %.\n",
             "| feature | Pearson r, median over tracks | median relative difference, median over tracks |", "|---|---|---|"]
    for k, f in enumerate(FEATURES):
        lines.append(f"| {f} | {np.nanmedian([r['r'][k] for r in ok]):.3f} | {np.nanmedian([r['rd'][k] for r in ok]):.3f} |")
    lines += ["\n| track | right offline % | right live % | same 1 % | live resets (tempo jumps, stops) | "
              "right live, no jump reset % |", "|---|---|---|---|---|---|"]
    for r in ok:
        lines.append(f"| {r['name'][:60]} | {100 * r['right_a']:.0f} | {100 * r['right_b']:.0f} | "
                     f"{100 * r['agree']:.0f} | {r['resets']} ({r['jumps']}, {r['stops']}) | {100 * r['right_c']:.0f} |")
    for r in res:
        if "error" in r:
            lines.append(f"\nerror {r['name']}: {r['error']}")
    a.out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines[:12]), flush=True)


if __name__ == "__main__":
    main()
