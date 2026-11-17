"""Audio sources. Each yields Chunks: mono samples, the stream time of the first sample, and when that sample is
heard on the Link clock (microseconds; None when the source does not run in real time)."""
from __future__ import annotations

import queue
import time
from dataclasses import dataclass
from typing import Callable, Iterator

import numpy as np

from live.audio import find_input_device, load_audio, to_mono
from live.tracker import HOP, SAMPLE_RATE

NowUs = Callable[[], float]


@dataclass(frozen=True)
class Chunk:
    samples: np.ndarray
    start_s: float
    link_us: float | None


class FileSource:
    """A file through the tracker, silently: as fast as possible, or paced like real time."""

    def __init__(self, path, realtime: bool = False, now_us: NowUs | None = None) -> None:
        self.mono = to_mono(load_audio(path))
        self.realtime = realtime
        self.now_us = now_us or (lambda: time.monotonic() * 1e6)

    def chunks(self) -> Iterator[Chunk]:
        t0 = time.monotonic()
        for i in range(0, len(self.mono), HOP):
            start = i / SAMPLE_RATE
            if self.realtime:
                wait = t0 + start - time.monotonic()
                if wait > 0:
                    time.sleep(wait)
            yield Chunk(self.mono[i:i + HOP], start, self.now_us() if self.realtime else None)

    def close(self) -> None:
        pass


class _StreamSource:
    """Shared by the sources fed from a PortAudio callback through a queue."""

    def __init__(self, now_us: NowUs) -> None:
        self.now_us = now_us
        self._queue: queue.Queue = queue.Queue()
        self._stream = None
        self.latency_ms = 0.0

    def chunks(self) -> Iterator[Chunk]:
        self._stream.start()
        while True:
            item = self._queue.get()
            if item is None:
                return
            yield item

    def close(self) -> None:
        if self._stream is not None:
            self._stream.close()
            self._stream = None


class PlaySource(_StreamSource):
    """Plays the file through the default output (so it is heard) and feeds the same samples to the tracker.
    A chunk's link time is when the speaker gets it: the callback time plus PortAudio's output latency."""

    def __init__(self, path, now_us: NowUs, device: int | str | None = None) -> None:
        import sounddevice as sd
        super().__init__(now_us)
        self.audio = load_audio(path)
        self.mono = to_mono(self.audio)
        self._pos = 0
        self._stream = sd.OutputStream(samplerate=SAMPLE_RATE, channels=2, dtype="float32", blocksize=HOP,
                                       device=device, callback=self._callback)
        self.latency_ms = self._stream.latency * 1000.0

    def _callback(self, outdata, frames, time_info, status) -> None:
        import sounddevice as sd
        start = self._pos
        block = self.audio[start:start + frames]
        outdata[:len(block)] = block
        outdata[len(block):] = 0.0
        if len(block):
            heard = self.now_us() + (time_info.outputBufferDacTime - time_info.currentTime) * 1e6
            self._queue.put(Chunk(self.mono[start:start + len(block)], start / SAMPLE_RATE, heard))
        self._pos += frames
        if len(block) < frames:
            self._queue.put(None)
            raise sd.CallbackStop


class DeviceSource(_StreamSource):
    """A line input (a loopback device such as BlackHole). A chunk's link time is the callback time minus the
    time PortAudio says the samples have been in the input path."""

    def __init__(self, name: str | None, now_us: NowUs) -> None:
        import sounddevice as sd
        super().__init__(now_us)
        index = find_input_device(name)
        info = sd.query_devices(index)
        self.device_name = info["name"]
        self._channels = min(2, info["max_input_channels"])
        self._stream = sd.InputStream(samplerate=SAMPLE_RATE, channels=self._channels, dtype="float32",
                                      blocksize=HOP, device=index, callback=self._callback)
        self.latency_ms = self._stream.latency * 1000.0
        self._pos = 0

    def _callback(self, indata, frames, time_info, status) -> None:
        heard = self.now_us() - (time_info.currentTime - time_info.inputBufferAdcTime) * 1e6
        self._queue.put(Chunk(indata.mean(axis=1, dtype=np.float32), self._pos / SAMPLE_RATE, heard))
        self._pos += frames
