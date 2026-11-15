"""Link master control: the estimator's tempo, beat phase and run state become the Link session's.

Called every ~10 ms with a Snapshot of the estimator. Tempo changes only outside a small deadband and the beat phase
only when it is off by more than a few ms (a force is felt by every peer, so it is rare and then holds off), so the
session does not jitter. Stopped -> Link's playing state stops; when running again it starts on the next bar.
"""
from __future__ import annotations

import math
from dataclasses import dataclass

from live.clockmap import ClockMap
from live.link_port import QUANTUM, LinkPort

DEADBAND_BPM = 0.02
PHASE_TOLERANCE_MS = 5.0
PHASE_HOLDOFF_S = 0.5
BAR_MARGIN_BEATS = 0.25     # a bar line closer than this is too close to schedule; use the next one
MIN_BPM, MAX_BPM = 20.0, 999.0     # Link's own limits


@dataclass(frozen=True)
class Snapshot:
    mode: str                                  # steady / speeding_up / slowing_down / stopped
    bpm: float | None
    grid: tuple[float, float] | None           # (stream time of a beat in s, period in s)


class LinkController:
    def __init__(self, link: LinkPort, clock: ClockMap, deadband_bpm: float = DEADBAND_BPM,
                 phase_ms: float = PHASE_TOLERANCE_MS, quantum: float = QUANTUM) -> None:
        self.link, self.clock = link, clock
        self.deadband_bpm, self.phase_ms, self.quantum = deadband_bpm, phase_ms, quantum
        self.phase_err_ms: float | None = None
        self.passive = False        # follow the session instead of leading it: tick() pushes nothing to Link
        self.pending_start = True
        self._last_force_us = -1e18
        self.beat_offset = 0.0      # the user's NUDGE BEAT, in beats (halves): Link's beats sit this far from the tracked ones

    def tick(self, snap: Snapshot) -> None:
        if not self.clock.ready:
            return
        if self.passive:
            self.phase_err_ms = None
            self.pending_start = True       # leading again later: start on the next bar
            return
        now = self.link.now_us()
        if snap.mode == "stopped" or snap.bpm is None:
            self.phase_err_ms = None
            if self.link.playing():
                self.link.set_playing(False, now)
            self.pending_start = True
            return
        self._follow_tempo(snap.bpm, now)
        locked = self._follow_phase(snap.grid, now)
        if self.pending_start and locked:
            self._start_on_next_bar(now)

    def _follow_tempo(self, bpm: float, now: float) -> None:
        bpm = min(max(bpm, MIN_BPM), MAX_BPM)
        if abs(bpm - self.link.tempo()) > self.deadband_bpm:
            self.link.set_tempo(bpm, now)

    def _follow_phase(self, grid: tuple[float, float] | None, now: float) -> bool:
        """Align Link's beats to the estimator's grid. True once the phase is known to be within tolerance."""
        if grid is None:
            return False
        t_last, period = grid
        t_last -= (self.beat_offset % 1.0) * period     # the half-beat part of the user's nudge (the whole beats are in the count)
        n = round((self.clock.to_stream_s(now) - t_last) / period)
        ref_us = self.clock.to_link_us(t_last + n * period)
        beat = self.link.beat_at(ref_us)
        target = round(beat)
        err_ms = (beat - target) * 60.0 / self.link.tempo() * 1000.0     # + : Link's beat is early
        self.phase_err_ms = err_ms
        if abs(err_ms) <= self.phase_ms:
            return True
        if now - self._last_force_us < PHASE_HOLDOFF_S * 1e6:
            return False
        self.link.force_beat(float(target), ref_us)
        self._last_force_us = now
        return False

    def shift_beats(self, n: float) -> None:
        """NUDGE BEAT (the device's NO + UP/DOWN), n in beats, halves allowed: Link's beats jump by n now, and the
        phase follower keeps them n beats from the tracked ones from then on. A half beat moves Link onto the off-beats
        (the fix for a tracker locked on the off-beat); two halves move the bar's 1 by a beat."""
        if self.passive:
            raise ValueError("following the Link session: the bar is the session's")
        now = self.link.now_us()
        self.link.force_beat(self.link.beat_at(now) + n, now)
        self.beat_offset += n

    def set_session_playing(self, playing: bool) -> None:
        """LINK mode with the transport set to FOLLOW + LEAD: the device's Stop / Start also starts the Link session's
        transport on the next bar, or stops it now. In AUDIO mode the tracker decides instead (the engine's hold)."""
        now = self.link.now_us()
        if playing:
            self._start_on_next_bar(now)
        else:
            self.link.set_playing(False, now)

    def _start_on_next_bar(self, now: float) -> None:
        if self.link.playing():
            self.pending_start = False
            return
        beat = self.link.beat_at(now)
        bar = (math.floor(beat / self.quantum) + 1) * self.quantum
        if bar - beat < BAR_MARGIN_BEATS:
            bar += self.quantum
        self.link.set_playing(True, self.link.time_at_beat(bar), 0.0)
        self.pending_start = False
