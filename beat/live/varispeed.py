"""A turntable-style player as the tracker's audio source: what it plays (with its pitch) is what the tracker hears.

Pitch is varispeed, as on a turntable or Traktor without key lock: rate 1.04 plays 4 % faster and 4 % higher, so a
128 BPM track becomes 133.12 BPM. The output stream runs all the time; paused or with no track it plays silence, and
the tracker hears that silence too (it goes to "stopped", like a real line input would).
"""
from __future__ import annotations

import threading
import time
from dataclasses import dataclass

import numpy as np

from live.audio import load_audio
from live.sources import Chunk, NowUs, _StreamSource
from live.tracker import HOP, SAMPLE_RATE

MIN_RATE, MAX_RATE = 0.5, 1.5
TAB_HOLD_S = 0.5        # tab audio counts as flowing this long after its last block (the page sends ~10 a second)


def render(audio: np.ndarray, pos: float, rate: float, frames: int) -> tuple[np.ndarray, float, bool]:
    """`frames` output frames from (n, 2) `audio`, starting at fractional frame `pos`, stepping `rate` frames each,
    linearly interpolated. Returns (block, next position, ended); past the end the block is silent."""
    idx = pos + rate * np.arange(frames, dtype=np.float64)
    out = np.zeros((frames, 2), dtype=np.float32)
    valid = idx < len(audio) - 1
    if valid.any():
        i = idx[valid]
        i0 = i.astype(np.int64)
        frac = (i - i0).astype(np.float32)[:, None]
        out[valid] = audio[i0] * (1.0 - frac) + audio[i0 + 1] * frac
    nxt = pos + rate * frames
    return out, nxt, nxt >= len(audio) - 1


@dataclass(frozen=True)
class PlayerState:
    path: str | None
    position_s: float
    duration_s: float
    playing: bool
    rate: float
    volume: float


class VarispeedSource(_StreamSource):
    """Plays a track at a variable rate through the default output and feeds the output to the tracker."""

    def __init__(self, now_us: NowUs, device: int | str | None = None) -> None:
        import sounddevice as sd
        super().__init__(now_us)
        self._lock = threading.Lock()
        self._audio: np.ndarray | None = None
        self._path: str | None = None
        self._pos = 0.0
        self._rate = 1.0
        self._playing = False
        self._volume = 1.0
        self._frames_out = 0
        self._ext_lock = threading.Lock()     # the stream clock and the tracker queue, shared with the tab feed
        self._ext_at = -1e9                   # monotonic time of the last tab block
        self.tap = None      # called with each block as rendered (full level), e.g. live.xiao.XiaoOutput.push
        self.on_block = None  # called with (path, file sample, rate, frames, heard us) of each block played
        self._played = None
        self._stream = sd.OutputStream(samplerate=SAMPLE_RATE, channels=2, dtype="float32", blocksize=HOP,
                                       device=device, callback=self._callback)
        self.latency_ms = self._stream.latency * 1000.0

    def load(self, path: str) -> None:
        """Decode `path` (outside the lock: it takes a moment) and swap it in, paused at the start."""
        audio = load_audio(path)
        with self._lock:
            self._audio, self._path, self._pos, self._playing = audio, path, 0.0, False

    def set_playing(self, playing: bool) -> None:
        with self._lock:
            if playing and self._audio is not None and self._pos >= len(self._audio) - 1:
                self._pos = 0.0          # play at the end starts over
            self._playing = playing and self._audio is not None

    def set_rate(self, rate: float) -> None:
        if not MIN_RATE <= rate <= MAX_RATE:
            raise ValueError(f"rate {rate} outside {MIN_RATE}..{MAX_RATE}")
        with self._lock:
            self._rate = rate

    def set_volume(self, volume: float) -> None:
        """Gain of what is heard, 0..1. The tracker always gets the full-level signal (a line input sits before
        the speakers' volume)."""
        if not 0.0 <= volume <= 1.0:
            raise ValueError(f"volume {volume} outside 0..1")
        with self._lock:
            self._volume = volume

    def tab_active(self) -> bool:
        """True while a browser tab's audio feeds the tracker (the player's own stream then stays out of it)."""
        return time.monotonic() - self._ext_at < TAB_HOLD_S

    def feed_tab(self, samples: np.ndarray, end_link_us: float) -> None:
        """Mono 44.1 kHz samples from a browser tab, the last one heard at `end_link_us` on the Link clock, go to the
        tracker in hops in place of the player's own output (which is silent then: the page stops the track)."""
        n = len(samples)
        with self._ext_lock:
            self._ext_at = time.monotonic()
            for i in range(0, n, HOP):
                part = samples[i:i + HOP]
                heard = end_link_us - (n - i) / SAMPLE_RATE * 1e6
                self._queue.put(Chunk(part, self._frames_out / SAMPLE_RATE, heard))
                self._frames_out += len(part)

    def seek(self, seconds: float) -> None:
        with self._lock:
            if self._audio is not None:
                self._pos = float(np.clip(seconds * SAMPLE_RATE, 0, len(self._audio) - 1))

    def state(self) -> PlayerState:
        with self._lock:
            dur = len(self._audio) / SAMPLE_RATE if self._audio is not None else 0.0
            return PlayerState(self._path, self._pos / SAMPLE_RATE, dur, self._playing, self._rate, self._volume)

    def _next_block(self, frames: int) -> np.ndarray:
        with self._lock:
            self._played = None
            if self._audio is None or not self._playing:
                return np.zeros((frames, 2), dtype=np.float32)
            self._played = (self._path, self._pos, self._rate)      # what this block is: for on_block
            block, self._pos, ended = render(self._audio, self._pos, self._rate, frames)
            if ended:
                self._playing = False
            return block

    def _callback(self, outdata, frames, time_info, status) -> None:
        block = self._next_block(frames)
        outdata[:] = block * self._volume
        if self.tap is not None:
            self.tap(block)
        heard =self.now_us() + (time_info.outputBufferDacTime - time_info.currentTime) * 1e6
        if self.on_block is not None and self._played is not None:
            path, pos, rate = self._played
            self.on_block(path, pos, rate, frames, heard)      # the file position heard when (live.accuracy)
        with self._ext_lock:
            if self.tab_active():
                return              # the tab's audio is the tracker's input: no silence chunks in between
            self._queue.put(Chunk(block.mean(axis=1, dtype=np.float32), self._frames_out / SAMPLE_RATE, heard))
            self._frames_out += frames
