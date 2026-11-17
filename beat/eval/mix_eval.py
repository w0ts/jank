"""The live pipeline on DJ sets and live recordings, against a reference where two offline trackers agree.

Mixes have no annotations. The reference at time t: Beat This! final0 and madmom offline (chunked), each fitted
over [t - 8 s, t + 8 s] (offline: both sides allowed). They AGREE when their tempos match within 0.3 % and their beats
sit within 30 ms of each other (median); the reference BPM is then Beat This!'s local fit. Only agreed time is
scored. The live pipeline is madmom online -> ModeBpm, published on a 0.5 s clock, octave-folded (a BPM range).

    .venv/bin/python -m eval.mix_eval
"""
from __future__ import annotations

import json

import numpy as np

from eval.data import DATA
from eval.locked_eval import folded_error
from eval.mode_eval import STEP_S, trace
from eval.rescore import _rows
from eval.tempo import _period

HALF_S = 8.0
TEMPO_AGREE = 0.003
PHASE_AGREE_S = 0.030
REFERENCES = ("beat_this_final0", "madmom_dbn_offline_chunked")
LIVE = "madmom_online"


def local_bpm(beats: np.ndarray, t: float) -> float | None:
    p = _period(beats[(beats >= t - HALF_S) & (beats <= t + HALF_S)])
    return 60.0 / p if p else None


def reference(a: np.ndarray, b: np.ndarray, times: np.ndarray) -> np.ndarray:
    """Reference BPM at each time where the two offline trackers agree, NaN elsewhere."""
    ref = np.full(times.shape, np.nan)
    for i, t in enumerate(times):
        ba, bb = local_bpm(a, t), local_bpm(b, t)
        if ba is None or bb is None or abs(ba / bb - 1) > TEMPO_AGREE:
            continue
        wa = a[(a >= t - HALF_S) & (a <= t + HALF_S)]
        wb = b[(b >= t - HALF_S) & (b <= t + HALF_S)]
        if wa.size < 4 or wb.size < 4:
            continue
        idx = np.clip(np.searchsorted(wb, wa), 1, wb.size - 1)
        near = np.minimum(np.abs(wb[idx] - wa), np.abs(wb[idx - 1] - wa))
        if np.median(near) <= PHASE_AGREE_S:
            ref[i] = ba
    return ref


def main() -> None:
    index = {r["name"]: r for r in json.loads((DATA / "mixes" / "index.json").read_text())}
    beats = {name: {r["name"]: np.array(r["est_beats"]) for r in _rows("mixes", name) if "error" not in r}
             for name in REFERENCES + (LIVE,)}
    print(f"{'mix':44} {'min':>5} agreed | running right±0.1 ±0.5 wrong>0.5")
    tot = np.zeros(5)
    for name, meta in sorted(index.items(), key=lambda kv: kv[1]["src"]):
        if not all(name in beats[k] for k in REFERENCES + (LIVE,)):
            continue
        times, bpms, _ = trace(beats[LIVE][name], meta["dur"])
        ref = reference(beats[REFERENCES[0]][name], beats[REFERENCES[1]][name], times)
        agreed = ~np.isnan(ref)
        err = np.full(times.shape, np.nan)
        err[agreed] = folded_error(bpms[agreed], ref[agreed])
        run = agreed & ~np.isnan(err)
        n_ag, n_run = int(agreed.sum()), int(run.sum())
        right01, right05, wrong = (int(np.sum(err[run] <= 0.1)), int(np.sum(err[run] <= 0.5)), int(np.sum(err[run] > 0.5)))
        tot += (n_ag, n_run, right01, right05, wrong)
        label = meta["src"].split("/")[-1][:44]
        print(f"{label:44} {meta['dur'] / 60:5.0f} {n_ag / len(times):5.0%} | {n_run / max(n_ag, 1):6.0%} "
              f"{right01 / max(n_run, 1):8.0%} {right05 / max(n_run, 1):5.0%} {wrong / max(n_run, 1):6.1%}")
    n_ag, n_run = tot[0], max(tot[1], 1)
    print(f"{'ALL (pooled over agreed time)':44} {'':5} {'':6} | {n_run / max(n_ag, 1):6.0%} {tot[2] / n_run:8.0%} "
          f"{tot[3] / n_run:5.0%} {tot[4] / n_run:6.1%}")
    print(f"step {STEP_S} s; reference = {REFERENCES[0]} and {REFERENCES[1]} agreeing (tempo {TEMPO_AGREE:.1%}, phase "
          f"{PHASE_AGREE_S * 1000:.0f} ms); 'right/wrong' are shares of running time")


if __name__ == "__main__":
    main()
