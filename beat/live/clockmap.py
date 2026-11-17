"""Maps stream time (seconds of audio since the start) to Ableton Link's clock (microseconds).

Every audio chunk reports when its first sample is heard (output) or was heard (input) on the Link clock. The mapping
is the minimum of (link time - stream time) over a window: callback jitter only ever makes a report late, so the
minimum is the least disturbed estimate; the window follows slow drift between the audio and host clocks.
"""
from __future__ import annotations

from collections import deque

WINDOW = 500   # chunks (about 5 s at 100 chunks/s)


class ClockMap:
    def __init__(self, offset_ms: float = 0.0, window: int = WINDOW) -> None:
        self.user_offset_us = offset_ms * 1000.0
        self._seen: deque[float] = deque(maxlen=window)

    def report(self, stream_s: float, link_us: float) -> None:
        """Chunk starting at `stream_s` seconds of audio is audible at `link_us` on the Link clock."""
        self._seen.append(link_us - stream_s * 1e6)

    @property
    def ready(self) -> bool:
        return bool(self._seen)

    def to_link_us(self, stream_s: float) -> float:
        return stream_s * 1e6 + min(self._seen) + self.user_offset_us

    def to_stream_s(self, link_us: float) -> float:
        return (link_us - min(self._seen) - self.user_offset_us) / 1e6
