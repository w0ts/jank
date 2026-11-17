"""The small surface of Ableton Link the controller needs, and its real implementation (LinkPython-extern).

Times are microseconds on Link's own clock. Every call captures the app session state, changes it and commits it,
so the controller can run in any thread.
"""
from __future__ import annotations

from typing import Protocol

QUANTUM = 4.0


class LinkPort(Protocol):
    def now_us(self) -> float: ...
    def peers(self) -> int: ...
    def tempo(self) -> float: ...
    def set_tempo(self, bpm: float, at_us: float) -> None: ...
    def beat_at(self, at_us: float) -> float: ...
    def time_at_beat(self, beat: float) -> float: ...
    def force_beat(self, beat: float, at_us: float) -> None: ...
    def playing(self) -> bool: ...
    def set_playing(self, playing: bool, at_us: float, beat: float | None = None) -> None: ...


class RealLink:
    def __init__(self, bpm: float = 120.0, quantum: float = QUANTUM, enabled: bool = True) -> None:
        import link
        self.quantum = quantum
        self._link = link.Link(bpm)
        self._link.startStopSyncEnabled = True
        self._link.enabled = enabled

    def _state(self):
        return self._link.captureAppSessionState()

    def now_us(self) -> float:
        return float(self._link.clock().micros())

    def peers(self) -> int:
        return int(self._link.numPeers())

    def tempo(self) -> float:
        return float(self._state().tempo())

    def set_tempo(self, bpm: float, at_us: float) -> None:
        s = self._state()
        s.setTempo(bpm, int(at_us))
        self._link.commitAppSessionState(s)

    def beat_at(self, at_us: float) -> float:
        return float(self._state().beatAtTime(int(at_us), self.quantum))

    def time_at_beat(self, beat: float) -> float:
        return float(self._state().timeAtBeat(beat, self.quantum))

    def force_beat(self, beat: float, at_us: float) -> None:
        s = self._state()
        s.forceBeatAtTime(beat, int(at_us), self.quantum)
        self._link.commitAppSessionState(s)

    def playing(self) -> bool:
        return bool(self._state().isPlaying())

    def set_playing(self, playing: bool, at_us: float, beat: float | None = None) -> None:
        s = self._state()
        if playing and beat is not None:
            s.setIsPlayingAndRequestBeatAtTime(True, int(at_us), beat, self.quantum)
        else:
            s.setIsPlaying(playing, int(at_us))
        self._link.commitAppSessionState(s)

    def close(self) -> None:
        self._link.enabled = False


class SimLink:
    """An in-memory Link session: the same surface, no network. Used for --dry-run and as the tests' fake.

    `calls` records every change as (name, args) so tests can check what the controller did.
    """

    def __init__(self, now=None, bpm: float = 120.0, peers: int = 0) -> None:
        import time
        self._now = now or (lambda: time.monotonic() * 1e6)
        self._bpm, self._peers = bpm, peers
        self._t0, self._beat0 = self._now(), 0.0
        self._playing = False
        self.calls: list[tuple] = []

    def now_us(self) -> float:
        return self._now()

    def peers(self) -> int:
        return self._peers

    def tempo(self) -> float:
        return self._bpm

    def beat_at(self, at_us: float) -> float:
        return self._beat0 + (at_us - self._t0) * self._bpm / 60e6

    def time_at_beat(self, beat: float) -> float:
        return self._t0 + (beat - self._beat0) * 60e6 / self._bpm

    def set_tempo(self, bpm: float, at_us: float) -> None:
        self._beat0, self._t0, self._bpm = self.beat_at(at_us), at_us, bpm
        self.calls.append(("set_tempo", bpm))

    def force_beat(self, beat: float, at_us: float) -> None:
        self._beat0, self._t0 = beat, at_us
        self.calls.append(("force_beat", beat))

    def playing(self) -> bool:
        return self._playing

    def set_playing(self, playing: bool, at_us: float, beat: float | None = None) -> None:
        self._playing = playing
        self.calls.append(("set_playing", playing, at_us, beat))

    def close(self) -> None:
        pass
