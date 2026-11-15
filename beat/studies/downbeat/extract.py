"""Per track: the live tracker's ONLINE beats, and cheap accent features at every online and every reference beat.

The features come from the tracker's own front end: a 2048-point Hann frame every 441 samples (frame k ends at sample
k * 441, as live/tracker.py frames it), the magnitude spectrum and madmom's logarithmic filterbank (12 bands an octave,
30 Hz-17 kHz, 81 bands; bands 0-4 are the single FFT bins at 43, 65, 86, 108 and 129 Hz). Nothing else is computed per
frame, so on the device the rule reuses jb_fe's band values.

    python extract.py --dataset traktor --shard 0/4 --jobs 16 --out results/traktor.0of4.pkl [--limit N] [--madmom-db]
"""
from __future__ import annotations

import os

for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "NUMBA_NUM_THREADS"):
    os.environ[_v] = "1"

import argparse
import pickle
import sys
import time
import traceback
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
REPO = Path(os.environ.get("BL_REPO", str(HERE.parent.parent)))
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(HERE))          # on the farm a copy of the Mac's live/ sits next to this script

SR, HOP, FRAME, FPS = 44100, 441, 2048, 100
LOW150 = slice(0, 5)                    # 43-129 Hz
LOW100 = slice(0, 3)                    # 43-86 Hz
WIN = (-0.05, 0.10)                     # the accent window around a beat, s
MP3_ROOT = Path.home() / "Music" / "Tracks"
# band groups of the 81 bands (by centre frequency): 43-129 Hz, 150-400, 400-1k, 1-3k, 3-8k, 8-17k
GROUPS = ((0, 5), (5, 17), (17, 33), (33, 52), (52, 69), (69, 81))
GNAMES = ("g0", "g1", "g2", "g3", "g4", "g5")
FEATURES = ("e150", "e100", "e150_mean", "flux150", "flux_all", "nov_all", "nov_low", "nov_all_pos",
            *(f"flux_{g}" for g in GNAMES), *(f"mean_{g}" for g in GNAMES), "nov_bar_all", "nov_bar_low")


def load_audio(path: Path) -> np.ndarray:
    import soundfile as sf
    import soxr
    y, sr = sf.read(str(path), dtype="float32", always_2d=True)
    y = y.mean(axis=1)
    return soxr.resample(y, sr, SR).astype(np.float32) if sr != SR else y


def frames_logbands(y: np.ndarray) -> np.ndarray:
    """(n_frames, 81) log10(1 + filterbank(|STFT|)); frame k = the 2048 samples ending at sample k * HOP."""
    from madmom.audio.filters import LogarithmicFilterbank
    bins = np.fft.rfftfreq(FRAME, 1 / SR)[:FRAME // 2]
    fb = np.asarray(LogarithmicFilterbank(bins, num_bands=12, fmin=30, fmax=17000, norm_filters=True,
                                          unique_filters=True), dtype=np.float32)
    n = len(y) // HOP + 1
    pad = np.concatenate([np.zeros(FRAME - 1, np.float32), y])
    win = np.hanning(FRAME).astype(np.float32)
    out = np.empty((n, fb.shape[1]), np.float32)
    step = 4096
    for s in range(0, n, step):
        ks = np.arange(s, min(n, s + step))
        idx = ks[:, None] * HOP + np.arange(FRAME)[None, :]          # into pad: frame k ends at pad[k*HOP + FRAME-1]
        idx = np.minimum(idx, len(pad) - 1)
        mag = np.abs(np.fft.rfft(pad[idx] * win, axis=1))[:, :FRAME // 2].astype(np.float32)
        out[ks] = np.log10(1.0 + mag @ fb)
    return out


def frame_curves(L: np.ndarray) -> dict[str, np.ndarray]:
    lin = 10.0 ** L - 1.0
    d = np.vstack([np.zeros((1, L.shape[1]), np.float32), np.diff(L, axis=0)])
    pos = np.maximum(d, 0.0)
    cur = {"e150": np.log10(1 + lin[:, LOW150].sum(1)), "e100": np.log10(1 + lin[:, LOW100].sum(1)),
           "flux150": pos[:, LOW150].sum(1), "flux_all": pos.sum(1)}
    for g, (a, b) in zip(GNAMES, GROUPS):
        cur[f"flux_{g}"] = pos[:, a:b].sum(1)
        cur[f"logmean_{g}"] = L[:, a:b].mean(1)
    return cur


def beat_features(beats: np.ndarray, L: np.ndarray, cur: dict[str, np.ndarray]) -> np.ndarray:
    """(n_beats, len(FEATURES)). Peak features: max over WIN around the beat. Novelty: the band means of the beat
    interval after the beat ([t, t + period)) against the one before ([t - period, t)), period = the median of the
    last 8 inter-beat intervals (known at the beat: a live rule can only use the past period)."""
    n = len(L)
    out = np.zeros((len(beats), len(FEATURES)), np.float32)
    for i, t in enumerate(beats):
        a, b = int(np.floor((t + WIN[0]) * FPS)), int(np.ceil((t + WIN[1]) * FPS)) + 1
        a, b = max(a, 0), min(b, n)
        if b <= a:
            continue
        ibis = np.diff(beats[max(0, i - 8):i + 1])
        per = float(np.median(ibis)) if len(ibis) else 0.5
        k = int(round(t * FPS))
        m = max(1, int(round(per * FPS)))
        before, after = L[max(0, k - m):k], L[k:min(n, k + m)]
        if len(before) and len(after):
            dm = after.mean(0) - before.mean(0)
            nov = (np.abs(dm).sum(), np.abs(dm[LOW150]).sum(), np.maximum(dm, 0).sum())
        else:
            nov = (0.0, 0.0, 0.0)
        bar_b, bar_a = L[max(0, k - 4 * m):k], L[k:min(n, k + 4 * m)]     # the bar after against the bar before
        if len(bar_b) and len(bar_a):
            db = bar_a.mean(0) - bar_b.mean(0)
            nov_bar = (np.abs(db).sum(), np.abs(db[LOW150]).sum())
        else:
            nov_bar = (0.0, 0.0)
        e = min(n, k + m)
        flux_g = [cur[f"flux_{g}"][a:b].max() for g in GNAMES]
        mean_g = [cur[f"logmean_{g}"][k:e].mean() if e > k else 0.0 for g in GNAMES]
        out[i] = (cur["e150"][a:b].max(), cur["e100"][a:b].max(), cur["e150"][a:b].mean(), cur["flux150"][a:b].max(),
                  cur["flux_all"][a:b].max(), *nov, *flux_g, *mean_g, *nov_bar)
    return out


def online_beats(y: np.ndarray) -> np.ndarray:
    from live.tracker import OnlineTracker
    tr = OnlineTracker(90.0, 180.0)          # the live default range (live/__main__.py --range 90 180)
    beats: list[float] = []
    chunk = HOP * 10
    for s in range(0, len(y), chunk):
        beats.extend(tr.feed(y[s:s + chunk]))
    return np.asarray(beats)


def madmom_downbeats(y: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    from madmom.audio.signal import Signal
    from madmom.features.downbeats import DBNDownBeatTrackingProcessor, RNNDownBeatProcessor
    act = RNNDownBeatProcessor()(Signal(y, sample_rate=SR, num_channels=1))
    out = DBNDownBeatTrackingProcessor(beats_per_bar=[3, 4], fps=100)(act)
    return out[:, 0], out[:, 1].astype(int)


def audio_path(piece, file_rel: str | None) -> Path:
    if piece.audio.exists():
        return piece.audio
    if file_rel and (MP3_ROOT / file_rel).exists():
        return MP3_ROOT / file_rel
    raise FileNotFoundError(piece.audio)


def one(job) -> dict:
    piece, file_rel, positions, do_db = job
    t0 = time.perf_counter()
    try:
        y = load_audio(audio_path(piece, file_rel))
        L = frames_logbands(y)
        cur = frame_curves(L)
        t1 = time.perf_counter()
        ob = online_beats(y)
        t2 = time.perf_counter()
        rec = {"name": piece.name, "dur": len(y) / SR, "ref_beats": piece.beats, "ref_pos": positions,
               "online": ob, "f_online": beat_features(ob, L, cur), "f_ref": beat_features(piece.beats, L, cur),
               "t_features": t1 - t0, "t_tracker": t2 - t1}
        if do_db:
            rec["mm_beats"], rec["mm_pos"] = madmom_downbeats(y)
            rec["t_madmom_db"] = time.perf_counter() - t2
        return rec
    except Exception:
        return {"name": piece.name, "error": traceback.format_exc(limit=3)}


def gtzan_positions(name: str) -> np.ndarray:
    from eval.data import ANN
    rows = [ln.split() for ln in (ANN / "gtzan" / "annotations" / "beats" / f"{name}.beats").read_text().splitlines()
            if ln.strip()]
    return np.array([int(float(r[1])) if len(r) > 1 else 0 for r in rows])


def traktor_positions(beats: np.ndarray, downbeats: np.ndarray) -> np.ndarray:
    """1-based bar positions like the GTZAN files: Traktor's downbeats are every 4th beat from the grid marker."""
    j0 = int(np.argmin(np.abs(beats - downbeats[0])))
    return (np.arange(len(beats)) - j0) % 4 + 1


def jobs_for(dataset: str, limit: int | None, shard: str, do_db: bool) -> list:
    import json
    from eval import data
    i, n = (int(x) for x in shard.split("/"))
    if dataset == "traktor":
        pieces = data.traktor(limit)[i::n]
        files = {r["name"]: r["file"] for r in json.loads((data.DATA / "traktor" / "index.json").read_text())}
        return [(p, files[p.name], traktor_positions(p.beats, p.downbeats), do_db) for p in pieces]
    pieces = data.gtzan(limit)[i::n]
    return [(p, None, gtzan_positions(p.name), do_db) for p in pieces]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", choices=["traktor", "gtzan"], required=True)
    ap.add_argument("--limit", type=int)
    ap.add_argument("--shard", default="0/1")
    ap.add_argument("--jobs", type=int, required=True)
    ap.add_argument("--madmom-db", action="store_true", help="also madmom's offline RNN downbeat tracker")
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    os.nice(19)
    jobs = jobs_for(a.dataset, a.limit, a.shard, a.madmom_db)
    print(f"load {os.getloadavg()[0]:.1f}, {len(jobs)} pieces, {a.jobs} workers x 1 thread, nice 19", flush=True)
    t0 = time.time()
    with ProcessPoolExecutor(a.jobs) as pool:
        rows = []
        for k, r in enumerate(pool.map(one, jobs, chunksize=1)):
            rows.append(r)
            if (k + 1) % 20 == 0 or "error" in r:
                print(f"{k + 1}/{len(jobs)} {time.time() - t0:.0f} s {r.get('error', '')[-300:]}", flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_bytes(pickle.dumps({"dataset": a.dataset, "features": FEATURES, "rows": rows}))
    ok = [r for r in rows if "error" not in r]
    if ok:
        rtf = np.median([(r["t_features"] + r["t_tracker"]) / r["dur"] for r in ok])
        print(f"done {len(ok)} ok, {len(rows) - len(ok)} errors, median rtf {rtf:.3f}, {time.time() - t0:.0f} s",
              flush=True)


if __name__ == "__main__":
    main()
