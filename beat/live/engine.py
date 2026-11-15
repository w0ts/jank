"""Tracker + ModeBpm behind one lock: the audio thread feeds samples, the control thread reads snapshots.

Two gates on top of the estimator decide whether Link runs:
- silence: input under SILENCE_RMS for SILENCE_S stops at once (the estimator's 30 s hold is for breaks in music,
  which are never digital silence) and the estimator starts over: what comes after silence is new music;
- confidence (live.confidence) under `min_confidence` stops the transport; it runs again once confidence is back at
  `min_confidence + HYSTERESIS` (and the controller starts it on the next bar)."""
from __future__ import annotations

import threading
from collections import deque
from dataclasses import dataclass

import numpy as np

from eval.mode_bpm import ModeBpm
from live.confidence import grid_confidence
from live.controller import Snapshot
from live.tracker import SAMPLE_RATE, OnlineTracker

SILENCE_RMS = 1e-3      # -60 dBFS
SILENCE_S = 0.5
MIN_LEVEL_DB = -120.0   # the meter's floor (digital silence)
LEVEL_WINDOW_S = 5.0    # the steady level averages this long (about 4 beats at 48 BPM and up; the BPM is not known)
LEVEL_FAST_S = 0.3      # the meter's short-term level
HYSTERESIS = 0.05
KEEP_BEATS = 256        # the confidence window needs far fewer


@dataclass(frozen=True)
class EngineStatus:
    mode: str               # the estimator's own mode
    bpm: float | None
    confidence: float       # 0..1
    silent: bool
    gated: bool             # confidence under the threshold: transport held stopped
    held: bool              # the user holds the transport stopped (device menu: Stop)
    grid: tuple[float, float] | None    # the estimator's (beat time, period), even while gated
    published: Snapshot     # what the Link controller gets


class Engine:
    def __init__(self, min_bpm: float = 90.0, max_bpm: float = 180.0, min_confidence: float = 0.0) -> None:
        self.tracker = OnlineTracker(min_bpm, max_bpm)
        self.estimator = ModeBpm()
        self.beats: list[float] = []
        self.min_confidence = min_confidence
        self._silent_s = 0.0
        self._gated = False
        self.hold = False       # while True the published snapshot is "stopped"
        self.fold = 1.0         # the device's x2 / /2 (HOME UP / DOWN): what Link gets is the tracked tempo times this
        self._level_blocks: deque[tuple[int, float]] = deque()   # (samples, sum of squares), the last LEVEL_WINDOW_S
        self._level_n = 0            # running totals over _level_blocks: no re-summing per block
        self._level_energy = 0.0
        self._lock = threading.Lock()

    def feed(self, samples: np.ndarray) -> list[float]:
        samples = np.asarray(samples, dtype=np.float32)
        found = self.tracker.feed(samples)       # the slow part, outside the lock
        sumsq = float(np.sum(samples.astype(np.float64) ** 2))
        rms = float(np.sqrt(sumsq / len(samples))) if len(samples) > 0 else 0.0
        quiet = len(samples) > 0 and rms < SILENCE_RMS
        with self._lock:
            if len(samples) > 0:
                self._level_blocks.append((len(samples), sumsq))
                self._level_n += len(samples)
                self._level_energy += sumsq
                while self._level_n - self._level_blocks[0][0] >= LEVEL_WINDOW_S * SAMPLE_RATE:
                    k, e = self._level_blocks.popleft()
                    self._level_n -= k
                    self._level_energy -= e
            was_silent = self.silent
            self._silent_s = self._silent_s + len(samples) / SAMPLE_RATE if quiet else 0.0
            if self.silent and not was_silent:
                self.estimator = ModeBpm()       # new music after silence: start over
                self.beats = []
            if not self.silent:
                for t in found:
                    self.estimator.update(t)
                self.beats.extend(found)
                del self.beats[:-KEEP_BEATS]
        return found

    @property
    def level_db(self) -> float:
        """RMS over the last LEVEL_WINDOW_S in dBFS (the steady average; the silence gate sits at -60)."""
        with self._lock:
            n, energy = self._level_n, max(self._level_energy, 0.0)     # (float drift can leave a tiny negative)
        return _db(np.sqrt(energy / n)) if n else MIN_LEVEL_DB

    @property
    def level_fast_db(self) -> float:
        """RMS over the last LEVEL_FAST_S in dBFS: what the device's meter smooths and takes its min / max lines from."""
        return self._rms_db(LEVEL_FAST_S)

    def _rms_db(self, seconds: float) -> float:
        """RMS of the newest blocks covering the given seconds (a short span: a few dozen blocks)."""
        want = seconds * SAMPLE_RATE
        n = energy = 0.0
        with self._lock:
            for k, e in reversed(self._level_blocks):     # newest first, whole blocks, until the span is covered
                if n >= want:
                    break
                n, energy = n + k, energy + e
        return _db(np.sqrt(energy / n)) if n else MIN_LEVEL_DB

    @property
    def silent(self) -> bool:
        return self._silent_s >= SILENCE_S

    def status(self) -> EngineStatus:
        with self._lock:
            now = self.tracker.time
            mode, bpm, grid = self.estimator.mode_at(now), self.estimator.bpm_at(now), self.estimator.grid()
            conf = 0.0 if self.silent else grid_confidence(self.beats, grid, now)
            threshold = self.min_confidence
            if threshold <= 0:
                self._gated = False
            elif self._gated:
                self._gated = conf < min(threshold + HYSTERESIS, 1.0)
            else:
                self._gated = conf < threshold
            stop = self.silent or self._gated or self.hold
            published = Snapshot("stopped", None, None) if stop else Snapshot(mode, *self._folded(bpm, grid))
            return EngineStatus(mode, bpm, conf, self.silent, self._gated, self.hold, grid, published)

    def _folded(self, bpm: float | None, grid: tuple[float, float] | None):
        """The tempo and grid Link gets: x2 puts a beat between each tracked pair, /2 keeps every other one."""
        if self.fold == 1.0:
            return bpm, grid
        return (bpm * self.fold if bpm else bpm), ((grid[0], grid[1] / self.fold) if grid else grid)

    def resync(self) -> None:
        """Forget the tempo and listen afresh (the device's Start); the x2 / /2 fold goes with it."""
        with self._lock:
            self.estimator = ModeBpm()
            self.beats = []
            self._gated = False
            self.fold = 1.0

    def snapshot(self) -> Snapshot:
        return self.status().published

    @property
    def stream_time(self) -> float:
        return self.tracker.time


def _db(amplitude: float) -> float:
    return max(MIN_LEVEL_DB, 20.0 * float(np.log10(max(float(amplitude), 1e-12))))
