"""Tracker + ModeBpm behind one lock: the audio thread feeds samples, the control thread reads snapshots.

Two gates on top of the estimator decide whether Link runs:
- silence: input under SILENCE_RMS for SILENCE_S stops at once (the estimator's 30 s hold is for breaks in music,
  which are never digital silence) and the estimator starts over: what comes after silence is new music;
- confidence (live.confidence) under `min_confidence` stops the transport; it runs again once confidence is back at
  `min_confidence + HYSTERESIS` (and the controller starts it on the next bar). The device's Start (resync) wins over
  it, as jank's firmware (M2g, core/jank_hold.h gate_off): the gate is disarmed until the confidence has reached
  `min_confidence + HYSTERESIS` once; a Stop (hold) or silence (M2h: the automatic layer starts over) ends that.
  Silence is never disarmed.

The downbeat estimator (live.downbeat) runs beside them on the same frames: the tracker's 81 bands (BandTap) and, after
each tracked beat, ModeBpm's grid. Its reset events: silence, the estimator stopped (30 s without a steady beat), a
resync (not a tempo jump: live.downbeat.Driver)."""
from __future__ import annotations

import threading
from collections import deque
from dataclasses import dataclass

import numpy as np

from eval.mode_bpm import ModeBpm
from live.confidence import grid_confidence
from live.controller import Snapshot
from live.downbeat import LISTENING, LOCKED, BandTap, BarView, Downbeat, Driver, autobar_mode
from live.fold import Fold
from live.tracker import SAMPLE_RATE, OnlineTracker

SILENCE_RMS = 1e-3      # -60 dBFS
SILENCE_S = 0.5
MIN_LEVEL_DB = -120.0   # the meter's floor (digital silence)
LEVEL_WINDOW_S = 5.0    # the steady level averages this long (about 4 beats at 48 BPM and up; the BPM is not known)
LEVEL_FAST_S = 0.3      # the meter's short-term level
HYSTERESIS = 0.05
KEEP_BEATS = 256        # the confidence window needs far fewer
START_WAIT_BEATS = 8    # AUTO BAR AT START: a start waits this many tracked beats for the estimator's lock


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
    bar: BarView            # the downbeat estimator
    fold: float = 1.0       # the fold in force (x2 / /2, the user's or AUTO RANGE's)
    fold_auto: bool = False  # AUTO RANGE set it
    autobar: str = "start"   # off / start / always


class Engine:
    def __init__(self, min_bpm: float = 90.0, max_bpm: float = 180.0, min_confidence: float = 0.0,
                 downbeat: Downbeat | None = None) -> None:
        self.tracker = OnlineTracker(min_bpm, max_bpm)
        self.bands = BandTap()
        self.downbeat = downbeat if downbeat is not None else Downbeat()
        self.driver = Driver(self.downbeat)       # its .trace: the downbeat's events (golden vectors)
        self.estimator = ModeBpm()
        self.beats: list[float] = []
        self.min_confidence = min_confidence
        self._silent_s = 0.0
        self._gated = False
        self._disarmed = False  # the device's Start disarmed the confidence gate (M2g)
        self._hold = False
        self._fold = Fold()     # the device's x2 / /2 (HOME UP / DOWN) and AUTO RANGE: Link gets the tracked tempo times it
        self.session_bpm = lambda: None    # the Link session's tempo while it plays, else None (AUTO RANGE's reference)
        self.place_fn = lambda t: None     # the Link session's place in beats at stream time t, None when not playing
        self._last_out: tuple[float, float] | None = None    # the last published output grid (beat, period)
        self._bar_epoch = 0     # AT START: counts the new starts for the bar that found the estimator locked
        self._wait = 0          # AT START: tracked beats left that a start waits for the estimator's lock
        self._level_blocks: deque[tuple[int, float]] = deque()   # (samples, sum of squares), the last LEVEL_WINDOW_S
        self._level_n = 0            # running totals over _level_blocks: no re-summing per block
        self._level_energy = 0.0
        self._lock = threading.Lock()

    def feed(self, samples: np.ndarray) -> list[float]:
        samples = np.asarray(samples, dtype=np.float32)
        found = self.tracker.feed(samples)       # the slow part, outside the lock
        f0, bands = self.bands.feed(samples)
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
            self._wait = max(0, self._wait - len(found))
            self.driver.chunk(self.estimator, f0, bands, found, self.tracker.time, self.silent)
            if not self.silent:
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
            if self._disarmed and (self.silent or threshold <= 0 or conf >= min(threshold + HYSTERESIS, 1.0)):
                self._disarmed = False                     # threshold + hysteresis reached, or silence (M2h): re-armed
            gated = self._gated and not self._disarmed
            stop = self.silent or gated or self.hold
            bar = self.downbeat.view()
            self._range(now, bpm, bar)
            ab = self.driver.autobar
            wait = ab == "start" and self._wait > 0 and bar.state == LISTENING
            # the bar moves only with Link's beats = the tracked ones (no x2 / /2 fold: the count is the tracked grid's)
            move = ab != "off" and self._fold.exp == 0
            if stop:
                published = Snapshot("stopped", None, None)
            else:
                fbpm, fgrid = self._folded(bpm, grid)
                if fgrid is not None:
                    self._last_out = fgrid
                published = Snapshot(mode, fbpm, fgrid, bar if move else None, "always" if ab == "always" else "start",
                                     self._bar_epoch, wait and move)
            return EngineStatus(mode, bpm, conf, self.silent, gated, self.hold, grid, published, bar,
                                self._fold.factor, self._fold.auto, ab)

    def _range(self, now: float, bpm: float | None, bar: BarView) -> None:
        """AUTO RANGE's step on the estimator's tempo, and AT START's "new start for the bar" events: a new lock opens
        the start wait when the estimator still listens; a lock or a fold change with the estimator already locked
        counts an epoch (one mid-play move)."""
        was = self._fold.had_tempo
        new, changed = self._fold.step(bpm, getattr(self.estimator, "jumps", 0), self.session_bpm(), now)
        if bpm is None:
            self._wait = 0
        elif new and not was:
            if bar.state == LOCKED:
                self._bar_epoch += 1
            elif bar.state == LISTENING:
                self._wait = START_WAIT_BEATS
        if changed and bar.state == LOCKED:
            self._bar_epoch += 1

    def _folded(self, bpm: float | None,
                grid: tuple[float, float] | None) -> tuple[float | None, tuple[float, float] | None]:
        """The tempo and grid Link gets: x2 puts a beat between each tracked pair, /2 keeps every other one."""
        f = self._fold
        if f.exp == 0 or grid is None:
            return bpm, grid
        out = None
        if self._last_out is not None and not f.anchored and f.exp < 0:
            t0, period = self._last_out
            place = self.place_fn(grid[0])
            out = ((grid[0] - t0) / period if place is None else place, period)
        return (bpm * f.factor if bpm else bpm), f.grid(grid[0], grid[1], out)

    def resync(self) -> None:
        """Forget the tempo and listen afresh (the device's Start); the x2 / /2 fold goes with it, and the bar."""
        with self._lock:
            self.estimator = ModeBpm()
            self.beats = []
            self._gated = False
            self._disarmed = True                          # the device's Start wins over the confidence gate (M2g)
            self._fold.reset()
            self._wait = 0
            self.driver.event("reset", self.bands.frames - 1)

    @property
    def hold(self) -> bool:
        """The device's Stop: while True the published snapshot is "stopped"."""
        return self._hold

    @hold.setter
    def hold(self, on: bool) -> None:
        with self._lock:
            self._hold = bool(on)
            if on:
                self._disarmed = False                     # a Stop ends a Start's disarm

    def bar_nudge(self) -> BarView:
        """The user nudged the beat: the downbeat estimator goes MANUAL. Its view before the nudge (the hand label)."""
        with self._lock:
            return self.downbeat.nudge()

    def set_autobar(self, value) -> str:
        """AUTO BAR: off / start / always (true = start, false = off). Returns the mode set."""
        with self._lock:
            return self.driver.set_autobar(value)

    @property
    def autobar(self) -> str:
        return self.driver.autobar

    @property
    def fold(self) -> float:
        return self._fold.factor

    @fold.setter
    def fold(self, factor: float) -> None:
        """The user's fold (the device's UP / DOWN): wins until the next new tempo, which AUTO RANGE ranges against the
        output the user chose."""
        with self._lock:
            if self._fold.user(factor) and self.downbeat.view().state == LOCKED:
                self._bar_epoch += 1

    @property
    def fold_auto(self) -> bool:
        return self._fold.auto

    def snapshot(self) -> Snapshot:
        return self.status().published

    @property
    def stream_time(self) -> float:
        return self.tracker.time


def _db(amplitude: float) -> float:
    return max(MIN_LEVEL_DB, 20.0 * float(np.log10(max(float(amplitude), 1e-12))))
