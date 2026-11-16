"""Where the 1 is when jank starts (AUTO BAR AT START with its START WAIT), on the user's tracks.

jank's tracker starts the Link session (and the MIDI clock) on a bar line once its estimator gives a tempo, after the
downbeat estimator (live/downbeat.py, the firmware's jb_down.c) found the 1 or after START WAIT tracked beats
(JBL_BAR_WAIT_BEATS = 8), whichever comes first (firmware/main/beat.c bar_rules, core/jank_beatlink.c). Found in time:
the start is placed on the estimator's 1. Not: it starts on the session's own bar line (an arbitrary beat of the music)
and AT START keeps it until the next start. Per track, streamed as the device does (the follower with its LOCK_BEATS,
the live estimator on its grid):
  - the start: when the tempo came, when the estimator first locked, when START WAIT ended;
  - the 1 at the start against the Traktor reference (the majority phase over the 32 beats after it): right, one beat
    off, two beats off (the 3: the classic 1-vs-3), three beats off; or unplaced (the session's arbitrary bar);
  - the same with a longer START WAIT (16, 32 beats) and with the estimator's first lock only.

    PYTHONPATH=..:../../.. python start_check.py --tracks 80 --offset 0 --jobs 2 --out ../results/v2_start_check.md
"""
from __future__ import annotations

import argparse
import os
import time
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

from live.downbeat import LOCKED, Downbeat, Driver, frame_of

HERE = Path(__file__).resolve().parent
MATCH = 0.02
WAITS = (8, 12, 16, 32)
LEAD_S = 0.15                       # jank_start.h JST_LEAD_US                 # START WAIT, tracked beats after the tempo (the firmware: 8)
ONE_THREAD = ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS")


def streamed(L, ob, lock_beats: int):
    """The live estimator on the follower's grid (eval.follow_bpm FollowBpm with LOCK_BEATS), as the device runs them.
    Returns the decisions log, the time the follower first gave a tempo and the tracked beats' times from then on."""
    import eval.follow_bpm as FB
    import eval.mode_bpm as MB
    saved = MB.LOCK_BEATS, FB.LOCK_BEATS
    MB.LOCK_BEATS = FB.LOCK_BEATS = lock_beats
    try:
        db = Downbeat()
        db.log = []
        drv = Driver(db, False)
        est = FB.FollowBpm()
        by_frame: dict[int, list[float]] = {}
        for t in ob:
            by_frame.setdefault(frame_of(t), []).append(float(t))
        t_run, after = None, []
        for f0 in range(0, len(L), 10):
            rows = L[f0:f0 + 10]
            found = [t for f in range(f0, f0 + len(rows)) for t in by_frame.get(f, ())]
            drv.chunk(est, f0, rows, found, (f0 + len(rows) - 1) / 100.0, silent=False)
            for t in found:
                if t_run is None and est.bpm_at(t) is not None:
                    t_run = t
                if t_run is not None:
                    after.append(t)
        return [r for r in db.log if r[0] == "d"], t_run, after
    finally:
        MB.LOCK_BEATS, FB.LOCK_BEATS = saved


def phase_error(dec, t, t_link, c_link, rp) -> int | None:
    """The decision's 1 against the reference's majority phase over the 32 Link beats from t: 0..3 beats, None unjudged."""
    i = int(np.argmin(np.abs(t_link - t)))          # the Link beat the decision was made on (stream_check's match)
    if abs(t_link[i] - t) > MATCH:
        return None
    sl = slice(i, min(len(c_link), i + 32))
    m = rp[sl] >= 0
    if m.sum() < 4:
        return None
    right = int(np.bincount(((c_link[sl] - rp[sl])[m]) % 4, minlength=4).argmax())
    off = int((c_link[i] - (dec[1] - dec[5]) % 4) % 4)   # the decision's offset in the Link count, as stream_check's
    return int((off - right) % 4)


def at_start(decs, t_run, after, wait, t_link, c_link, rp) -> dict:
    """The start's 1 with START WAIT `wait` tracked beats. The start then arms the session's next bar line at least
    LEAD_S ahead: the j-th beat after that (j 0..3: the session's bar is arbitrary), and a decision before it still
    places the start (jst_retarget). Weights over j: placed (error 0..3) or unplaced."""
    if t_run is None or not after:
        return {"kind": "no tempo"}
    t_wait = after[min(wait, len(after) - 1)]
    first = next((d for d in decs if d[4] == LOCKED and d[2] >= t_run - 1e-9), None)
    beats = np.asarray(after)
    i0 = int(np.searchsorted(beats, t_wait + LEAD_S))
    placed_w, start_s = 0.0, []
    for j in range(4):
        t_start = float(beats[min(i0 + j, len(beats) - 1)])
        start_s.append((first[2] if first is not None and first[2] < t_wait else t_start) - t_run)
        placed_w += 0.25 * (first is not None and first[2] < t_start)
    err = phase_error(first, first[2], t_link, c_link, rp) if first is not None and placed_w > 0 else None
    return {"kind": "placed" if placed_w > 0 else "unplaced", "w": placed_w, "err": err,
            "start_s": float(np.mean(start_s))}


def one(job) -> dict:
    import json

    from extract import audio_path, frames_logbands, load_audio, online_beats, traktor_positions
    from linkbeats import link_beats
    from rules import ref_positions, valid_4
    piece, file_rel = job
    t0 = time.perf_counter()
    try:
        y = load_audio(audio_path(piece, file_rel))
    except FileNotFoundError:
        return {"name": piece.name, "error": "no audio"}
    L = frames_logbands(y)
    ob = online_beats(y)
    positions = traktor_positions(piece.beats, piece.downbeats)
    valid, _, _ = valid_4(positions)
    t_link, c_link = link_beats(ob)
    if len(t_link) < 32:
        return {"name": piece.name, "error": "too few Link beats"}
    rp = ref_positions(t_link, piece.beats, positions, valid)
    out = {"name": piece.name, "s": 0.0}
    for lb in (8, 16):
        decs, t_run, after = streamed(L, ob, lb)
        first = next((d for d in decs if d[4] == LOCKED), None)
        out[f"lb{lb}"] = {
            "waits": {w: at_start(decs, t_run, after, w, t_link, c_link, rp) for w in WAITS},
            "first": None if first is None else {"err": phase_error(first, first[2], t_link, c_link, rp),
                                                 "after_tempo_s": first[2] - (t_run or 0.0)},
            "t_run": t_run}
    out["s"] = time.perf_counter() - t0
    _ = json
    return out


def table(rows: list[dict], lb: int) -> list[str]:
    lines = [f"\n## LOCK_BEATS {lb} ({'M2j' if lb == 16 else 'M2i'})\n",
             "| START WAIT (tracked beats) | placed % | right % of placed | one off % | **two off (the 3) %** | three off % | "
             "unjudged (tracks) | unplaced (arbitrary bar) % | median s tempo -> start |", "|---|---|---|---|---|---|---|---|---|"]
    for w in WAITS:
        a = [r[f"lb{lb}"]["waits"][w] for r in rows]
        a = [x for x in a if x["kind"] != "no tempo"]
        n = len(a)
        wsum = sum(x.get("w", 0.0) for x in a)
        cnt = [sum(x["w"] for x in a if x.get("w", 0) > 0 and x["err"] == k) for k in range(4)]
        unj = sum(x["w"] for x in a if x.get("w", 0) > 0 and x["err"] is None)
        judged = max(sum(cnt), 1e-9)
        lines.append(f"| {w} | {100 * wsum / max(n, 1):.0f} | {100 * cnt[0] / judged:.0f} | {100 * cnt[1] / judged:.0f} | "
                     f"**{100 * cnt[2] / judged:.0f}** | {100 * cnt[3] / judged:.0f} | {unj:.1f} | "
                     f"{100 * (1 - wsum / max(n, 1)):.0f} | {np.median([x['start_s'] for x in a]):.1f} |")
    f = [r[f"lb{lb}"]["first"] for r in rows if r[f"lb{lb}"]["first"] is not None]
    e = [x["err"] for x in f if x["err"] is not None]
    cnt = [sum(1 for v in e if v == k) for k in range(4)]
    lines.append(f"\nThe estimator's first lock (any time): {len(f)} tracks, judged {len(e)}: right {cnt[0]}, one off "
                 f"{cnt[1]}, **two off {cnt[2]}**, three off {cnt[3]}; median {np.median([x['after_tempo_s'] for x in f]):.1f} s "
                 f"after the tempo.")
    return lines


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tracks", type=int, default=80)
    ap.add_argument("--offset", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    os.nice(19)
    for v in ONE_THREAD:
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
    lines = ["# Where the 1 is when jank starts (AUTO BAR AT START, START WAIT)\n",
             f"{len(ok)} Traktor test-half tracks (every {step}th from #{a.offset}), the live estimator (live/downbeat.py) "
             f"streamed on FollowBpm's grid; the start's 1 against the Traktor reference (majority over the 32 beats "
             f"after it). Run {time.strftime('%Y-%m-%d')}, {time.time() - t0:.0f} s."]
    for lb in (8, 16):
        lines += table(ok, lb)
    for r in res:
        if "error" in r:
            lines.append(f"\nerror {r['name']}: {r['error']}")
    a.out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines), flush=True)


if __name__ == "__main__":
    main()
