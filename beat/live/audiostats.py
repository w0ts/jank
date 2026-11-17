"""What an audio callback saw: PortAudio's status flags, how long it ran, and the gaps between calls.

The players' two output streams (the speakers in live/varispeed.py, the jank XIAO in live/xiao.py) call `enter` first thing
and `leave` last thing in their callbacks. A call later than 1.5 blocks after the one before it is a *late gap*: the sound
card's buffer ran on that long without a new block (a crackle if it ran dry). `snapshot` is what /api/status shows.
"""
from __future__ import annotations

import threading
import time
from collections import deque

import numpy as np

LATE_FACTOR = 1.5


class CallbackStats:
    def __init__(self, nominal_s: float, window: int = 2000) -> None:
        self.nominal_s = nominal_s
        self._lock = threading.Lock()
        self._durs: deque[float] = deque(maxlen=window)
        self._first: float | None = None
        self._last: float | None = None
        self.callbacks = self.underflows = self.other_flags = self.late_gaps = 0
        self.gap_max = self.dur_max = 0.0

    def enter(self, now: float | None = None) -> float:
        t = time.perf_counter() if now is None else now
        with self._lock:
            if self._last is not None:
                gap = t - self._last
                self.gap_max = max(self.gap_max, gap)
                if gap > LATE_FACTOR * self.nominal_s:
                    self.late_gaps += 1
            else:
                self._first = t
            self._last = t
            self.callbacks += 1
        return t

    def leave(self, t0: float, status=None, now: float | None = None) -> None:
        dur = (time.perf_counter() if now is None else now) - t0
        with self._lock:
            self._durs.append(dur)
            self.dur_max = max(self.dur_max, dur)
            if status:
                if getattr(status, "output_underflow", False):
                    self.underflows += 1
                else:
                    self.other_flags += 1

    def snapshot(self) -> dict:
        with self._lock:
            durs = np.asarray(self._durs) if self._durs else np.zeros(1)
            span = (self._last - self._first) if self._first is not None and self._last is not None else 0.0
            missing = max(0.0, span / self.nominal_s - (self.callbacks - 1)) if span > 0 else 0.0
            return {"callbacks": self.callbacks, "rate_hz": round((self.callbacks - 1) / span, 3) if span > 0 else 0.0,
                    "missing_blocks": round(missing), "missing_pct": round(100.0 * missing / (span / self.nominal_s), 3) if span > 0 else 0.0,
                    "underflows": self.underflows, "other_flags": self.other_flags, "late_gaps": self.late_gaps,
                    "gap_max_ms": round(self.gap_max * 1000.0, 3), "dur_max_ms": round(self.dur_max * 1000.0, 3),
                    "dur_p99_ms": round(float(np.percentile(durs, 99)) * 1000.0, 3)}


class StallWatch:
    """How long the interpreter keeps a sleeping thread waiting: a thread that sleeps `period_s` and measures how late it wakes.

    Python threads share one lock (the GIL), so a thread that holds it (pure Python, a garbage collection) delays the audio
    callbacks the same way it delays this one. A wake-up later than `report_s` is kept with the top frame of every thread at
    that moment (the holder is usually still where it was, or just past it); garbage-collection pauses are timed with a
    gc callback."""

    def __init__(self, period_s: float = 0.002, report_s: float = 0.03, keep: int = 12) -> None:
        self.period_s, self.report_s = period_s, report_s
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self.wakeups = 0
        self.max_s = 0.0
        self.buckets = {5: 0, 10: 0, 20: 0, 50: 0, 100: 0}         # wake-ups later than this many ms
        self.events: deque[dict] = deque(maxlen=keep)
        self.gc_collections = 0
        self.gc_max_s = 0.0
        self.gc_by_gen = [0.0, 0.0, 0.0]
        self._gc_t0 = 0.0
        self._t_start = 0.0

    def start(self) -> None:
        import gc

        self._t_start = time.perf_counter()
        gc.callbacks.append(self._gc_hook)
        self._thread = threading.Thread(target=self._run, name="stallwatch", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        import gc

        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1.0)
        if self._gc_hook in gc.callbacks:
            gc.callbacks.remove(self._gc_hook)

    def _gc_hook(self, phase: str, info: dict) -> None:
        if phase == "start":
            self._gc_t0 = time.perf_counter()
            return
        dt = time.perf_counter() - self._gc_t0
        self.gc_collections += 1
        self.gc_max_s = max(self.gc_max_s, dt)
        g = int(info.get("generation", 0))
        if 0 <= g < 3:
            self.gc_by_gen[g] = max(self.gc_by_gen[g], dt)

    def _run(self) -> None:
        import sys

        own = threading.get_ident()
        names = {}
        while not self._stop.is_set():
            t0 = time.perf_counter()
            time.sleep(self.period_s)
            late = time.perf_counter() - t0 - self.period_s
            with self._lock:
                self.wakeups += 1
                self.max_s = max(self.max_s, late)
                for ms in self.buckets:
                    if late * 1000.0 > ms:
                        self.buckets[ms] += 1
                if late >= self.report_s:
                    names.update({t.ident: t.name for t in threading.enumerate()})
                    stacks = {}
                    for tid, fr in sys._current_frames().items():
                        if tid == own:
                            continue
                        stacks[names.get(tid, str(tid))] = f"{fr.f_code.co_filename.split('/')[-1]}:{fr.f_lineno} {fr.f_code.co_name}"
                    self.events.append({"at_s": round(t0 - self._t_start, 1), "late_ms": round(late * 1000.0, 1), "threads": stacks})

    def snapshot(self) -> dict:
        with self._lock:
            return {"wakeups": self.wakeups, "max_ms": round(self.max_s * 1000.0, 2),
                    "later_than_ms": dict(self.buckets), "gc_collections": self.gc_collections,
                    "gc_max_ms": round(self.gc_max_s * 1000.0, 2), "gc_max_ms_by_gen": [round(x * 1000.0, 2) for x in self.gc_by_gen],
                    "events": list(self.events)}
