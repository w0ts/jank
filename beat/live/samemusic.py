"""Is this still the same music? A causal comparison of the last few seconds against a memory of the past minute.

Input: the tracker's own 81 log bands (live.downbeat.BandTap; the device's jb_fe computes the same), averaged over
blocks of 0.1 s. Every SEG_BLOCKS blocks (1 s) a segment descriptor goes into a ring of MEMORY_SEGS (64 s):
  timbre   DCT-II of the segment's mean log spectrum, coefficients 1..12 (0, the loudness, left out: a gain dip is not
           new music); an MFCC-like spectral envelope, coarse enough to move little under a few % of varispeed
  chroma   the bands from 800 Hz to 5 kHz (about a semitone apart there) folded onto the 12 pitch classes (soft, by
           each band's centre), square-root compressed, unit length: the harmony / key, which survives filters, drum
           mutes and breakdowns
novelty(): the mean of the last NOW_SEGS segments against the memory older than NOW_SEGS + GAP_SEGS:
  timbre   RMS of the z-scored difference to the memory's mean (the memory's own spread per coefficient)
  chroma   1 - cosine to the memory's mean chroma, the better of as is and shifted by the varispeed ratio `rate`
           (set by the caller from the follower: a pitch fader moves the pitch with the tempo, a time-stretch does not)
Foote's novelty (ICME 2000) compares the past and future halves of a self-similarity kernel; this is its causal
half: the "future" is the last few seconds, the past is the memory.
"""
from __future__ import annotations

from collections import deque
from functools import lru_cache

import numpy as np

BANDS = 81
SEG_BLOCKS = 10        # 0.1 s blocks per segment (1 s)
NOW_SEGS = 4
GAP_SEGS = 2
MEMORY_SEGS = 64
MIN_PAST = 8           # segments of memory before novelty means anything
N_TIMBRE = 12
CHROMA_LO, CHROMA_HI = 800.0, 5000.0


@lru_cache(maxsize=1)
def centres() -> np.ndarray:
    """The 81 bands' centre frequencies (Hz), as the tracker's filterbank lays them out."""
    from madmom.audio.filters import LogarithmicFilterbank
    bins = np.fft.rfftfreq(2048, 1 / 44100)[:1024]
    fb = LogarithmicFilterbank(bins, num_bands=12, fmin=30, fmax=17000, norm_filters=True, unique_filters=True)
    return np.asarray(fb.center_frequencies, dtype=float)


@lru_cache(maxsize=1)
def _dct() -> np.ndarray:
    k = np.arange(1, N_TIMBRE + 1)[:, None]
    n = np.arange(BANDS)[None, :]
    return np.cos(np.pi / BANDS * (n + 0.5) * k) * np.sqrt(2 / BANDS)


@lru_cache(maxsize=1)
def _fold() -> np.ndarray:
    """(81, 12) soft assignment of each band in the chroma range to its two nearest pitch classes."""
    c = centres()
    w = np.zeros((BANDS, 12))
    for i, f in enumerate(c):
        if CHROMA_LO <= f <= CHROMA_HI:
            pc = (12 * np.log2(f / 440.0) + 9) % 12          # 0 = C
            lo = int(np.floor(pc)) % 12
            frac = pc - np.floor(pc)
            w[i, lo] += 1 - frac
            w[i, (lo + 1) % 12] += frac
    return w


def timbre_of(logbands: np.ndarray) -> np.ndarray:
    return _dct() @ np.asarray(logbands, dtype=float)


def chroma_of(logbands: np.ndarray) -> np.ndarray:
    lin = np.maximum(10.0 ** np.asarray(logbands, dtype=float) - 1.0, 0.0)
    c = np.sqrt(lin @ _fold())
    n = np.linalg.norm(c)
    return c / n if n > 0 else c


def shift_chroma(c: np.ndarray, semitones: float) -> np.ndarray:
    """Circular shift by a fractional number of semitones (linear interpolation), renormalised."""
    idx = (np.arange(12) - semitones) % 12
    lo = np.floor(idx).astype(int) % 12
    frac = idx - np.floor(idx)
    out = c[lo] * (1 - frac) + c[(lo + 1) % 12] * frac
    n = np.linalg.norm(out)
    return out / n if n > 0 else out


class SameMusic:
    def __init__(self) -> None:
        self._acc = np.zeros(BANDS)
        self._n = 0
        self._segs: deque[tuple[np.ndarray, np.ndarray]] = deque(maxlen=MEMORY_SEGS)
        self.rate = 1.0          # the tempo ratio now / when the memory was laid down (from the follower)
        self.segments = 0

    def reset(self) -> None:
        self._segs.clear()
        self._acc[:] = 0
        self._n = 0
        self.rate = 1.0

    def feed(self, blocks: np.ndarray) -> int:
        """(n, 81) mean log bands of 0.1 s blocks. Returns how many segments completed."""
        done = 0
        for b in np.atleast_2d(blocks):
            self._acc += b
            self._n += 1
            if self._n == SEG_BLOCKS:
                mean = self._acc / SEG_BLOCKS
                self._segs.append((timbre_of(mean), chroma_of(mean)))
                self._acc[:] = 0
                self._n = 0
                self.segments += 1
                done += 1
        return done

    def novelty(self) -> dict | None:
        """{'timbre': z distance, 'chroma': 1 - cosine} of the last NOW_SEGS segments against the memory, or None."""
        segs = list(self._segs)
        past = segs[:max(0, len(segs) - NOW_SEGS - GAP_SEGS)]
        if len(past) < MIN_PAST:
            return None
        now = segs[-NOW_SEGS:]
        pt = np.array([s[0] for s in past])
        mu, sd = pt.mean(axis=0), pt.std(axis=0) + 0.05
        nt = np.mean([s[0] for s in now], axis=0)
        timbre = float(np.sqrt(np.mean(((nt - mu) / sd) ** 2)))
        pc = np.mean([s[1] for s in past], axis=0)
        nc = np.mean([s[1] for s in now], axis=0)
        sims = [_cos(pc, nc)]
        if abs(self.rate - 1) > 1e-3:
            sims.append(_cos(shift_chroma(pc, 12 * np.log2(self.rate)), nc))
        return {"timbre": timbre, "chroma": 1.0 - max(sims)}


def _cos(a: np.ndarray, b: np.ndarray) -> float:
    na, nb = np.linalg.norm(a), np.linalg.norm(b)
    return float(a @ b / (na * nb)) if na > 0 and nb > 0 else 0.0
