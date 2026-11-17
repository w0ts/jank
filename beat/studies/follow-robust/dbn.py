"""The firmware's online beat DBN (core/beat/jb_dbn.c = madmom's DBNBeatTrackingProcessor.process_online, 90-180 BPM,
transition lambda 100, observation lambda 16, forward argmax) run frame by frame in numba, so that a replay can act on it
(change 2, TEMPO MEMORY): the cached beats of extract_live.py come from one uninterrupted run and cannot be steered.

Bit-exact with the cache on the same activation (madmom's float64 forward, its float32 densities): `test_follow_robust`
checks it on a synthetic activation, rdbn_check.py on the real cache.

The tempo prior (`entry`): a weight per beat interval applied to the mass that ENTERS the interval's first state (a new
beat starting at that tempo), every frame while set; ones = off (exactly the plain DBN). On the device: 35 multiplies per
frame (the first states only), nothing else. `reweight` applies weights once to every state of each interval (a one-shot
"seed" that keeps the phase), `reseed` puts the forward vector back to a uniform phase with the weights over the tempi
(jb_dbn_reset with a prior instead of the uniform one).
"""
from __future__ import annotations

import math

import numpy as np
from numba import njit

TRANSITION_LAMBDA = 100.0
OBSERVATION_LAMBDA = 16.0


@njit(cache=True)
def _run(fwd, nxt, dens_off, dens_beat, f0, f1, first, intervals, prev_last, trans_t, is_beat, entry,
         last_beat, fps, min_gap, out):
    """Frames f0..f1-1. Returns (beats written to out, last_beat). trans_t[i, j]: from interval j's last state to i's
    first (CSR order of madmom: ascending previous state)."""
    n_int = intervals.size
    n_states = fwd.size
    nb = 0
    for f in range(f0, f1):
        for i in range(n_int):
            v = 0.0
            for j in range(n_int):
                p = trans_t[i, j]
                if p != 0.0:
                    v += fwd[prev_last[j]] * p
            s0 = first[i]
            nxt[s0] = v
            for k in range(1, intervals[i]):
                nxt[s0 + k] = fwd[s0 + k - 1]
        total = 0.0
        db, do = dens_beat[f], dens_off[f]
        for i in range(n_int):
            s0 = first[i]
            nxt[s0] *= entry[i]
        for s in range(n_states):
            nxt[s] *= db if is_beat[s] else do
            total += nxt[s]
        if not total > 0.0:
            for s in range(n_states):
                nxt[s] = 1.0 / n_states
            total = 1.0
        norm = 1.0 / total
        best = 0
        for s in range(n_states):
            fwd[s] = nxt[s] * norm
            if fwd[s] > fwd[best]:
                best = s
        cur = f / fps
        if is_beat[best] and cur >= last_beat + min_gap:
            last_beat = cur
            out[nb] = cur
            nb += 1
    return nb, last_beat


def densities(act: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """madmom's observation densities (no beat, beat) per frame: float32 arithmetic, float64 log densities, exp."""
    a = np.asarray(act, dtype=np.float32)
    with np.errstate(divide="ignore"):
        off = np.log((np.float32(1.0) - a) / np.float32(OBSERVATION_LAMBDA - 1)).astype(np.float64)
        beat = np.log(a).astype(np.float64)
    return np.exp(off), np.exp(beat)


class OnlineDbn:
    """One track's activation, stepped up to a frame at a time. Frame k is time k / fps (the cache's beats)."""

    def __init__(self, act: np.ndarray, min_bpm: float = 90.0, max_bpm: float = 180.0, fps: float = 100.0) -> None:
        lo, hi = int(np.round(60.0 * fps / max_bpm)), int(np.round(60.0 * fps / min_bpm))
        self.intervals = np.arange(lo, hi + 1, dtype=np.int64)
        self.first = np.concatenate([[0], np.cumsum(self.intervals)[:-1]]).astype(np.int64)
        self.last = (np.cumsum(self.intervals) - 1).astype(np.int64)
        self.n_states = int(self.intervals.sum())
        pos = np.concatenate([np.arange(i) / i for i in self.intervals])
        self.is_beat = (pos < 1.0 / OBSERVATION_LAMBDA).astype(np.bool_)
        iv = self.intervals.astype(float)
        ratio = iv[None, :] / iv[:, None]                       # [from j, to i]
        prob = np.exp(-TRANSITION_LAMBDA * np.abs(ratio - 1.0))
        prob[prob <= np.spacing(1)] = 0
        prob /= prob.sum(axis=1)[:, None]
        self.trans_t = np.ascontiguousarray(prob.T)
        self.fps, self.min_gap = float(fps), 60.0 / max_bpm
        self.dens_off, self.dens_beat = densities(act)
        self.n_frames = int(self.dens_off.size)
        self.fwd = np.full(self.n_states, 1.0 / self.n_states)
        self._nxt = np.empty(self.n_states)
        self.entry = np.ones(self.intervals.size)
        self.frame = 0
        self.last_beat = 0.0
        self._out = np.empty(64)

    @property
    def bpms(self) -> np.ndarray:
        return 60.0 * self.fps / self.intervals

    def advance(self, to_frame: int) -> list[float]:
        """Frames up to to_frame inclusive; the beats found."""
        stop = min(to_frame + 1, self.n_frames)
        if stop <= self.frame:
            return []
        need = (stop - self.frame) // 33 + 2
        if self._out.size < need:
            self._out = np.empty(need)
        nb, self.last_beat = _run(self.fwd, self._nxt, self.dens_off, self.dens_beat, self.frame, stop, self.first,
                                  self.intervals, self.last, self.trans_t, self.is_beat, self.entry, self.last_beat,
                                  self.fps, self.min_gap, self._out)
        self.frame = stop
        return [float(x) for x in self._out[:nb]]

    def tempo_mass(self) -> np.ndarray:
        """The forward probability per interval (sums to 1)."""
        return np.add.reduceat(self.fwd, self.first)

    def set_prior(self, weights: np.ndarray | None) -> None:
        self.entry = np.ones(self.intervals.size) if weights is None else np.asarray(weights, dtype=float)

    def reweight(self, weights: np.ndarray) -> None:
        w = np.repeat(np.asarray(weights, dtype=float), self.intervals)
        f = self.fwd * w
        s = f.sum()
        if s > 0:
            self.fwd[:] = f / s

    def reseed(self, weights: np.ndarray) -> None:
        w = np.asarray(weights, dtype=float) / self.intervals
        f = np.repeat(w, self.intervals)
        self.fwd[:] = f / f.sum()


def tempo_prior(bpms: np.ndarray, mem_bpm: float, width: float, floor: float, octaves: bool = True) -> np.ndarray:
    """Weights per DBN tempo: floor + (1 - floor) x a Gaussian in log2 tempo (sd `width` octaves) around the remembered
    tempo and (octaves) its x2 / /2. Unrelated tempi keep `floor`."""
    lr = np.log2(np.asarray(bpms, dtype=float) / mem_bpm)
    centres = (-1.0, 0.0, 1.0) if octaves else (0.0,)
    g = np.max([np.exp(-0.5 * ((lr - c) / width) ** 2) for c in centres], axis=0)
    return floor + (1.0 - floor) * g


def fold_ratio(r: float) -> float:
    """r brought into [1/sqrt 2, sqrt 2) by octaves."""
    return r / 2.0 ** math.floor(math.log2(r) + 0.5)
