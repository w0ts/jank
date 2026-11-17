"""The playback rate from the spectrum, live, frame by frame: the reference for jank's core/beat/jb_rate.c.

studies/advanced-follow/ratio_probe.py's measurement, made causal and incremental: each frame's |FFT| (the tracker's
own 2048-point Hann frames, 100 a second) resampled to BINS_PER_OCT bins an octave from F_LO to F_HI, log(1 + x), the
frames of a 0.1 s block averaged; at each block's end the mean of the last CUR_N blocks (0.5 s) is matched against the
mean of the REF_N before them (4 s): the correlation of the mean-removed spectra at shifts within +-MAX_REL, the peak
refined by a parabola, ratio = 2^(shift / BINS_PER_OCT). An event (a rate step) is CONFIRM estimates in a row past
EVENT_TH that agree within AGREE; after one, REFRACTORY_S without estimates (until the reference holds only the new rate).
The event goes to the follower as eval.follow_bpm.FollowBpm.propose_ratio(ratio, t, since=t - DETECT_LAG_S), which takes
it only if the next tracked beat backs it.

`step`: only every step-th frame enters the blocks (the device's decimation: 1 = every frame as the probe, 2 = every
second frame). `every`: an estimate at every every-th block only (1: each 0.1 s as the probe; 2: each 0.2 s). Frames are
numbered from the stream's start; a block is frames 10 j .. 10 j + 9 and its estimate is dated (10 j + 10) / 100 s, as the
probe dates block j's (it ends there); with every 2, only blocks with an even end, (10 j + 10) / 100 s a multiple of 0.2.
"""
from __future__ import annotations

import math
from collections import deque

import numpy as np

SR, FRAME = 44100, 2048
BINS_PER_OCT = 96
F_LO, F_HI = 300.0, 8000.0
N_BINS = int(np.log2(F_HI / F_LO) * BINS_PER_OCT)        # 454
MAX_REL = 0.10
MAX_SHIFT = int(np.ceil(np.log2(1 + MAX_REL) * BINS_PER_OCT))    # 14
BLOCK = 10              # frames a block (0.1 s)
CUR_N, REF_N = 5, 40    # blocks: the last 0.5 s against the 4 s before
EVENT_TH = 0.006
CONFIRM = 2
AGREE = 0.003
REFRACTORY_S = (REF_N + CUR_N) * BLOCK / 100.0
DETECT_LAG_S = 0.50
FRAME_STEP = 2          # the device's default decimation (studies/advanced-follow/rate_decimate.py)
EST_EVERY = 2           # ... and its estimates every 0.2 s


def bin_table() -> tuple[np.ndarray, np.ndarray]:
    """(index, fraction) of each log-frequency bin's position on the FFT's bins: |X| there is
    mag[i] + frac * (mag[i + 1] - mag[i]) (np.interp's line)."""
    freqs = F_LO * 2.0 ** (np.arange(N_BINS) / BINS_PER_OCT)
    pos = freqs / (SR / FRAME)
    idx = np.floor(pos).astype(np.int64)
    return idx, pos - idx


def match_ratio(ref: np.ndarray, cur: np.ndarray) -> tuple[float, float]:
    """(ratio of cur against ref, peak correlation): ratio_probe.match_ratio."""
    a = ref - ref.mean()
    b = cur - cur.mean()
    n = a.size
    corr = np.empty(2 * MAX_SHIFT + 1)
    for i, s in enumerate(range(-MAX_SHIFT, MAX_SHIFT + 1)):
        if s >= 0:
            x, z = a[:n - s], b[s:]
        else:
            x, z = a[-s:], b[:n + s]
        corr[i] = float(x @ z / (np.linalg.norm(x) * np.linalg.norm(z) + 1e-12))
    k = int(np.argmax(corr))
    frac = 0.0
    if 0 < k < corr.size - 1:
        d = corr[k - 1] - 2 * corr[k] + corr[k + 1]
        frac = 0.5 * (corr[k - 1] - corr[k + 1]) / d if d < 0 else 0.0
    return float(2 ** ((k - MAX_SHIFT + frac) / BINS_PER_OCT)), float(corr[k])


class RateProbe:
    """Frames in (frame(index, mag)), rate events out. Times are the frames' stream seconds (index / 100)."""

    def __init__(self, step: int = FRAME_STEP, every: int = EST_EVERY) -> None:
        if step < 1 or BLOCK % step:
            raise ValueError(f"step {step}: must divide {BLOCK}")
        if every < 1:
            raise ValueError(f"every {every}: must be 1 or more")
        self.step, self.every = step, every
        self.idx, self.frac = bin_table()
        self.estimates: list[tuple[float, float, float]] = []     # (t, ratio, peak), for the golden vectors
        self.reset()

    def reset(self) -> None:
        self._acc = np.zeros(N_BINS)
        self._acc_n = 0
        self._in_block = False
        self._blocks: deque[np.ndarray] = deque(maxlen=REF_N + CUR_N)
        self._run: list[float] = []
        self._last = -math.inf

    def spectrum(self, mag: np.ndarray) -> np.ndarray:
        """One frame's log-frequency spectrum from its |FFT| (bins 0 .. at least 373)."""
        lo = mag[self.idx]
        return np.log1p(lo + self.frac * (mag[self.idx + 1] - lo))

    def taken(self, index: int) -> bool:
        """Whether frame `index` enters a block (the caller computes its |FFT| only then)."""
        return index % BLOCK % self.step == 0

    def frame(self, index: int, mag: np.ndarray | None) -> tuple[float, float] | None:
        """One frame's |FFT| (None for a frame not taken); returns (t, ratio) when a rate step is decided at this frame."""
        pos = index % BLOCK
        if pos == 0:
            self._acc[:] = 0.0
            self._acc_n = 0
            self._in_block = True
        if not self._in_block:
            return None                     # started mid-block: from the next one on
        if pos % self.step == 0:
            if mag is None:
                raise ValueError(f"frame {index} is taken: its |FFT| is needed")
            self._acc += self.spectrum(np.asarray(mag, np.float64))
            self._acc_n += 1
        if pos != BLOCK - 1:
            return None
        self._blocks.append(self._acc / self._acc_n)
        if ((index + 1) // BLOCK) % self.every:
            return None
        return self._estimate((index + 1) / 100.0)

    def _estimate(self, t: float) -> tuple[float, float] | None:
        if len(self._blocks) < REF_N + CUR_N or t - self._last < REFRACTORY_S:
            return None
        blocks = list(self._blocks)
        ref = np.mean(blocks[:REF_N], axis=0)
        cur = np.mean(blocks[REF_N:], axis=0)
        r, peak = match_ratio(ref, cur)
        self.estimates.append((t, r, peak))
        if abs(r - 1) > EVENT_TH and (not self._run or abs(r / self._run[-1] - 1) < AGREE):
            self._run.append(r)
        else:
            self._run = [r] if abs(r - 1) > EVENT_TH else []
        if len(self._run) >= CONFIRM:
            ratio = float(np.median(self._run))
            self._last, self._run = t, []
            return t, ratio
        return None


def centred_magnitudes(y: np.ndarray, frames: int, first: int = 0) -> np.ndarray:
    """(frames, 1025) |FFT| of jank's frames first .. first + frames - 1 (core/beat/jb_fe.h: frame k centred on sample
    441 k of y, zeros outside y, np.hanning(2048) in float32), as float64."""
    y = np.asarray(y, np.float32)
    win = np.hanning(FRAME).astype(np.float32)
    lo = first * 441 - FRAME // 2
    hi = (first + frames - 1) * 441 + FRAME // 2
    seg = np.zeros(hi - lo, np.float32)
    a, b = max(lo, 0), min(hi, len(y))
    if b > a:
        seg[a - lo:b - lo] = y[a:b]
    idx = (np.arange(frames) * 441)[:, None] + np.arange(FRAME)[None, :]
    return np.abs(np.fft.rfft(seg[idx] * win, axis=1))


class RateTap:
    """The live engine's rate: samples in, rate events out, on the tracker's own frames (live.tracker: frame k = the 2048
    samples ending at sample k * 441, dated k / 100 s; jank's frames are centred instead, 23 ms apart, each on its own
    tracker's clock). Only while active (the follower glides); going inactive drops the history, as jank's
    jb_rate_set_active. The |FFT| is computed for the frames taken only."""

    def __init__(self, step: int = FRAME_STEP, every: int = EST_EVERY) -> None:
        self.probe = RateProbe(step, every)
        self.window = np.hanning(FRAME).astype(np.float32)
        self._hist = np.zeros(FRAME, np.float32)
        self._total = 0
        self.frames = 0            # the next frame's index
        self.active = False

    def feed(self, samples: np.ndarray, active: bool) -> list[tuple[float, float]]:
        """The (t, ratio) events the frames these samples complete decide."""
        if active != self.active:
            self.active = active
            self.probe.reset()
        samples = np.asarray(samples, np.float32)
        self._hist = np.concatenate([self._hist, samples])
        self._total += len(samples)
        out = []
        while self._total > self.frames * 441:
            k = self.frames
            self.frames += 1
            if not active:
                continue
            mag = None
            if self.probe.taken(k):
                end = len(self._hist) - (self._total - 1 - k * 441)
                frame = self._hist[max(end - FRAME, 0):end]
                if len(frame) < FRAME:
                    frame = np.concatenate([np.zeros(FRAME - len(frame), np.float32), frame])
                mag = np.abs(np.fft.rfft(frame * self.window))
            event = self.probe.frame(k, mag)
            if event is not None:
                out.append(event)
        self._hist = self._hist[-(FRAME + 441):]
        return out
