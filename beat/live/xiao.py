"""The jank XIAO as a second audio output (`python -m live.player --xiao`): the track's mid, mono 44.1 kHz, to the
USB speaker called "jank", so that the XIAO's own beat tracker hears what the speakers play.

The speakers' callback (varispeed) renders each block and hands it to `XiaoOutput.push`; the jank stream's own callback
takes the samples from a small FIFO (`DriftBuffer`). The two devices run on different clocks (jank, an adaptive USB
speaker, on the Mac's USB clock; the speakers on their own crystal), tens of ppm apart, so the FIFO's level drifts: it is
held near its target by dropping or repeating one sample (22.7 us) at a time, which the tracker cannot hear. The XIAO
time-stamps the packets as they arrive, so a slip only moves its sample-to-time map by that much.
"""
from __future__ import annotations

import threading
from dataclasses import dataclass
from typing import Any, Callable

import numpy as np

from live.audiostats import CallbackStats
from live.tracker import HOP, SAMPLE_RATE

DEVICE_NAME = "jank"
TARGET_BLOCKS = 3          # the FIFO's level (after a pull) the slips aim for, in 441-sample blocks: 30 ms
SLACK_BLOCKS = 1           # no slip while the smoothed level is within this of the target
CAPACITY_BLOCKS = 16       # 160 ms: more than that and the oldest samples go
MAX_TARGET_BLOCKS = 8      # an underrun deepens the target a block at a time, up to this (80 ms)
LEVEL_SMOOTH = 1.0 / 100   # the level is averaged over ~100 pulls (~1 s), so the callbacks' jitter makes no slips


@dataclass(frozen=True)
class DriftStatus:
    level: int             # samples waiting
    slips_dropped: int     # samples dropped to bring the level down (jank's clock is slower)
    slips_repeated: int    # samples repeated to bring it up (jank's clock is faster)
    underruns: int         # callbacks that found too few samples (silence filled in)
    overflows: int         # pushes that found the FIFO full (the oldest samples went)
    target: int = 0        # the level aimed at now (grows after underruns)
    avg_level: float = 0.0 # the smoothed level after a pull: the FIFO's share of the jank path's latency
    ratio: float = 1.0     # ResampleBuffer: input samples used per output sample (1 + the drift it follows)


class DriftBuffer:
    """A mono FIFO between two audio callbacks on two clocks, held near a target level by one-sample slips.

    The two callbacks do not alternate neatly (CoreAudio may call one of them twice in a row), so the level after a pull
    jitters by a block or more: the slips follow its average, and the target leaves room for that jitter. An underrun
    (the jank callback found too little) deepens the target by a block, up to MAX_TARGET_BLOCKS."""

    def __init__(self, target: int = TARGET_BLOCKS * HOP, slack: int = SLACK_BLOCKS * HOP,
                 capacity: int = CAPACITY_BLOCKS * HOP, max_target: int = MAX_TARGET_BLOCKS * HOP) -> None:
        if not 0 < target <= max_target < capacity or slack < 1:
            raise ValueError(f"bad FIFO sizes: target {target}, slack {slack}, max {max_target}, capacity {capacity}")
        self.target, self.slack, self.capacity, self.max_target = target, slack, capacity, max_target
        self._buf = np.zeros(0, dtype=np.float32)
        self._avg: float | None = None
        self._slipping = 0                  # -1 dropping, +1 repeating, 0 in the band
        self._lock = threading.Lock()
        self._dropped = self._repeated = self._underruns = self._overflows = 0

    def push(self, samples: np.ndarray) -> None:
        x = np.asarray(samples, dtype=np.float32).reshape(-1)
        with self._lock:
            buf = np.concatenate([self._buf, x])
            if len(buf) > self.capacity:
                self._overflows += 1
                buf = buf[-self.capacity:]
            self._buf = buf

    def pull(self, n: int) -> np.ndarray:
        """n samples for the output callback: one slip at most per call, silence for what is missing."""
        with self._lock:
            buf = self._buf
            left = len(buf) - n
            self._avg = float(left) if self._avg is None else self._avg + (left - self._avg) * LEVEL_SMOOTH
            # out of the band: slip one sample a pull until the average is back at the target (hysteresis: the level
            # then sits in the middle of the band, where the callbacks' jitter cannot reach its edges)
            if self._avg > self.target + self.slack:
                self._slipping = -1
            elif self._avg < self.target - self.slack:
                self._slipping = 1
            elif (self._slipping < 0 and self._avg <= self.target) or (self._slipping > 0 and self._avg >= self.target):
                self._slipping = 0
            if self._slipping < 0 and left > 0:
                buf = buf[1:]                                   # too much waiting: drop one
                self._dropped += 1
                self._avg -= 1.0
            elif self._slipping > 0 and len(buf) >= 1:
                buf = np.concatenate([buf[:1], buf])            # too little: repeat one
                self._repeated += 1
                self._avg += 1.0
            if len(buf) >= n:
                out, self._buf = buf[:n].copy(), buf[n:]
            else:
                self._underruns += 1
                self.target = min(self.target + HOP, self.max_target)
                out = np.zeros(n, dtype=np.float32)
                out[:len(buf)] = buf
                self._buf = np.zeros(0, dtype=np.float32)
            return out

    def status(self) -> DriftStatus:
        with self._lock:
            return DriftStatus(len(self._buf), self._dropped, self._repeated, self._underruns, self._overflows,
                               self.target, self._avg if self._avg is not None else 0.0)


TAU_S = 3.0                # ResampleBuffer: the level error is worked off over this long (the ratio is 1 + error / (TAU_S rate))
MAX_RATIO = 0.01           # and never more than 1 % off (17 cents)
RESAMPLE_MAX_TARGET_BLOCKS = 16     # the speakers' stream with a big buffer (--speaker-latency 0.1) feeds the FIFO in bursts of
RESAMPLE_CAPACITY_BLOCKS = 32       # ~8 blocks; underruns deepen the target to ride them out, so room for it (160 / 320 ms)


class ResampleBuffer:
    """The same FIFO as DriftBuffer, following the two clocks' drift by resampling: each pull reads `ratio` input samples per
    output sample (linear interpolation), the ratio being 1 plus the smoothed level's distance from the target over TAU_S
    seconds. DriftBuffer repeats or drops one sample per pull at most (0.23 % of drift, as a 100 Hz buzz at its limit);
    this follows up to MAX_RATIO smoothly. An underrun deepens the target as there."""

    def __init__(self, target: int = TARGET_BLOCKS * HOP, capacity: int = RESAMPLE_CAPACITY_BLOCKS * HOP,
                 max_target: int = RESAMPLE_MAX_TARGET_BLOCKS * HOP) -> None:
        if not 0 < target <= max_target < capacity:
            raise ValueError(f"bad FIFO sizes: target {target}, max {max_target}, capacity {capacity}")
        self.target, self.capacity, self.max_target = target, capacity, max_target
        self._buf = np.zeros(0, dtype=np.float32)
        self._pos = 0.0                    # the read position inside _buf (the fraction past its first sample)
        self._avg: float | None = None
        self._ratio = 1.0
        self._lock = threading.Lock()
        self._underruns = self._overflows = 0

    def push(self, samples: np.ndarray) -> None:
        x = np.asarray(samples, dtype=np.float32).reshape(-1)
        with self._lock:
            buf = np.concatenate([self._buf, x])
            if len(buf) > self.capacity:
                self._overflows += 1
                cut = len(buf) - self.capacity
                buf = buf[cut:]
                self._pos = max(self._pos - cut, 0.0)
            self._buf = buf

    def pull(self, n: int) -> np.ndarray:
        with self._lock:
            buf, pos = self._buf, self._pos
            left = (len(buf) - pos) - n
            self._avg = float(left) if self._avg is None else self._avg + (left - self._avg) * LEVEL_SMOOTH
            err = self._avg - self.target
            self._ratio = 1.0 + float(np.clip(err / (TAU_S * SAMPLE_RATE), -MAX_RATIO, MAX_RATIO))
            idx = pos + self._ratio * np.arange(n, dtype=np.float64)
            i0 = idx.astype(np.int64)
            ok = i0 + 1 < len(buf)
            out = np.zeros(n, dtype=np.float32)
            if ok.all():
                frac = (idx - i0).astype(np.float32)
                out = buf[i0] * (1.0 - frac) + buf[i0 + 1] * frac
                used = pos + self._ratio * n
                k = int(used)
                self._buf, self._pos = buf[k:], used - k
                return out.astype(np.float32, copy=False)
            m = int(ok.sum())
            if m:
                frac = (idx[:m] - i0[:m]).astype(np.float32)
                out[:m] = buf[i0[:m]] * (1.0 - frac) + buf[i0[:m] + 1] * frac
            self._underruns += 1
            self.target = min(self.target + HOP, self.max_target)
            self._buf, self._pos = np.zeros(0, dtype=np.float32), 0.0
            return out

    def status(self) -> DriftStatus:
        with self._lock:
            return DriftStatus(int(len(self._buf) - self._pos), 0, 0, self._underruns, self._overflows, self.target,
                               self._avg if self._avg is not None else 0.0, self._ratio)


def find_device(devices: list[dict[str, Any]], name: str = DEVICE_NAME) -> int | None:
    """The index of the output device called `name` (exactly, ignoring case) or else whose name contains it."""
    outputs = [(i, d) for i, d in enumerate(devices) if int(d.get("max_output_channels", 0)) >= 1]
    for i, d in outputs:
        if str(d.get("name", "")).strip().lower() == name.lower():
            return i
    for i, d in outputs:
        if name.lower() in str(d.get("name", "")).lower():
            return i
    return None


class XiaoOutput:
    """The second output stream: mono, 44.1 kHz, 441-sample blocks, on the jank device."""

    def __init__(self, stream_factory: Callable[..., Any] | None = None,
                 devices: list[dict[str, Any]] | None = None, name: str = DEVICE_NAME, latency: float | None = None,
                 resample: bool = False) -> None:
        if stream_factory is None or devices is None:
            import sounddevice as sd
            stream_factory = stream_factory or sd.OutputStream
            devices = devices if devices is not None else [dict(d) for d in sd.query_devices()]
        index = find_device(devices, name)
        if index is None:
            raise RuntimeError(f'no audio output named "{name}": is the XIAO plugged in and running the USB audio build?')
        self.device_name = str(devices[index]["name"])
        self.fifo = ResampleBuffer() if resample else DriftBuffer()
        self.callbacks = 0
        self.cb_stats = CallbackStats(HOP / SAMPLE_RATE)
        kw = {} if latency is None else {"latency": latency}
        self._stream = stream_factory(samplerate=SAMPLE_RATE, channels=1, dtype="float32", blocksize=HOP,
                                      device=index, callback=self._callback, **kw)
        self.latency_ms = float(getattr(self._stream, "latency", 0.0)) * 1000.0

    def push(self, block: np.ndarray) -> None:
        """A block the speakers play, (n, 2) or (n,): its mid goes to the XIAO."""
        b = np.asarray(block, dtype=np.float32)
        self.fifo.push(b.mean(axis=1, dtype=np.float32) if b.ndim == 2 else b)

    def _callback(self, outdata, frames, time_info, status) -> None:
        t0 = self.cb_stats.enter()
        outdata[:, 0] = self.fifo.pull(frames)
        self.callbacks += 1
        self.cb_stats.leave(t0, status)

    def start(self) -> None:
        self._stream.start()

    def close(self) -> None:
        self._stream.stop()
        self._stream.close()

    def status(self) -> dict:
        s = self.fifo.status()
        return {"device": self.device_name, "callbacks": self.callbacks, "latency_ms": self.latency_ms,
                "fifo": s.level, "dropped": s.slips_dropped, "repeated": s.slips_repeated,
                "underruns": s.underruns, "overflows": s.overflows, "fifo_target": s.target,
                "fifo_ms": round(s.avg_level / SAMPLE_RATE * 1000.0, 2), "fifo_ratio": round(s.ratio, 5),
                "cb": self.cb_stats.snapshot()}
