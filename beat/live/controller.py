"""Link master control: the estimator's tempo, beat phase and run state become the Link session's.

Called every ~10 ms with a Snapshot of the estimator. Tempo changes only outside a small deadband and the beat phase
only when it is off by more than a few ms (a force is felt by every peer, so it is rare and then holds off), so the
session does not jitter. Stopped -> Link's playing state stops; when running again it starts on the next bar.
"""
from __future__ import annotations

import math
from dataclasses import dataclass

from live.clockmap import ClockMap
from live.downbeat import LOCKED, BarView, link_shift
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
    bar: BarView | None = None                 # the downbeat estimator (None: the bar is not ours to move)
    bar_mode: str = "always"                   # AUTO BAR: always, or start (moves only at a start, see _bar_allowed)
    bar_epoch: int = 0                         # start mode: counts the "new starts for the bar" with the estimator locked
    start_wait: bool = False                   # start mode: the session's start waits for the estimator's lock


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
        self.phase = 0.0            # the user's PHASE (jank M2h): NUDGE BEAT's total alone (beat_offset has AUTO BAR's moves too)
        self.bar_moves = 0          # whole-beat moves made for the downbeat estimator
        self._bar_seq = -1          # its last decision acted on
        self._start_us: float | None = None     # when the session's scheduled start is reached
        self._epoch = 0             # the last bar_epoch seen
        self._once = False          # a "new start for the bar" is open: one move may come mid-play

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
            self._start_us = None
            return
        self._follow_tempo(snap.bpm, now)
        locked = self._follow_phase(snap.grid, now)
        if locked:
            self._follow_bar(snap.bar, snap, now)
        if self.pending_start and locked and not snap.start_wait:
            self._start_on_next_bar(now)

    def _bar_allowed(self, snap: Snapshot, now: float) -> tuple[bool, bool]:
        """(a move may be made now, it uses the mid-play "once"). AT START: only while the session is stopped or its
        start is not reached yet, or once at a new start for the bar (the engine counts them in bar_epoch)."""
        if snap.bar_mode == "always":
            return True, False
        if snap.bar_epoch != self._epoch:
            self._epoch, self._once = snap.bar_epoch, True
        if not self.link.playing() or (self._start_us is not None and now < self._start_us):
            return True, False
        return self._once, True

    def _follow_bar(self, bar: BarView | None, snap: Snapshot, now: float) -> None:
        """The downbeat estimator LOCKED on a phase: at each of its decisions (one a counted beat), Link's bar is put on
        its 1 by a whole-beat move, if it is not there (the first move, a later one, or Link's count slipped). Only
        with the phase in tolerance: then Link's beat at the counted beat is unambiguous."""
        if bar is None or bar.state != LOCKED or bar.seq == self._bar_seq:
            return
        if bar.t_beat is None or bar.count is None or bar.phi is None:
            return
        allowed, once = self._bar_allowed(snap, now)
        if not allowed:
            return
        self._bar_seq = bar.seq
        if once:
            self._once = False
        # the user's PHASE rides on AUTO BAR's bar (jank M2h): the 1 placed with its whole beats kept, never undone
        n = link_shift(bar.count, bar.phi - math.floor(self.phase + 0.25), self.link.beat_at(self.clock.to_link_us(bar.t_beat)))
        if n:
            self.shift_beats(n)
            self.bar_moves += 1

    def link_bar_phase(self, count: int, t_beat: float) -> int:
        """The count position (mod 4) that Link's bar calls its 1 at a counted beat (the nudge log's hand label)."""
        whole = math.floor(self.link.beat_at(self.clock.to_link_us(t_beat)) + 0.25)
        return (count - whole) % 4

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
        (the fix for a tracker locked on the off-beat); two halves move the bar's 1 by a beat. The downbeat estimator's
        moves come through here too, whole beats only: beat_offset's half-beat part stays as the user left it."""
        if self.passive:
            raise ValueError("following the Link session: the bar is the session's")
        now = self.link.now_us()
        self.link.force_beat(self.link.beat_at(now) + n, now)
        self.beat_offset += n

    def nudge_phase(self, n: float) -> None:
        """The user's NUDGE BEAT: the PHASE (jank M2h, two layers): Link's beats move n (shift_beats) and the offset rides on
        AUTO BAR's bar from then on; nothing automatic changes it; back to 0 is AUTO."""
        self.shift_beats(n)
        self.phase += n

    def set_session_playing(self, playing: bool) -> None:
        """LINK mode with the transport set to FOLLOW + LEAD: the device's Stop / Start also starts the Link session's
        transport on the next bar, or stops it now. In AUDIO mode the tracker decides instead (the engine's hold)."""
        now = self.link.now_us()
        if playing:
            self._start_on_next_bar(now)
        else:
            self.link.set_playing(False, now)
            self._start_us = None

    def _start_on_next_bar(self, now: float) -> None:
        if self.link.playing():
            self.pending_start = False
            return
        beat = self.link.beat_at(now)
        bar = (math.floor(beat / self.quantum) + 1) * self.quantum
        if bar - beat < BAR_MARGIN_BEATS:
            bar += self.quantum
        self._start_us = self.link.time_at_beat(bar)
        self.link.set_playing(True, self._start_us, 0.0)
        self.pending_start = False
