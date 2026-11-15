"""Which beat is the 1: the downbeat estimator of the second study (studies/downbeat/v2, final.py), live.

It counts Link's own beats: the estimator's grid (ModeBpm.grid after each tracked beat, a beat at tb then every p), one
count per grid beat played, the count following a jump of the grid by its gap rounded (studies/downbeat/v2/linkbeats.py).
At each counted beat it takes the first study's 22 accent features (studies/downbeat/extract.py, FEATURES) from the
tracker's own 81-band log spectrum, and keeps 22 x 4 accumulators (one per feature and bar position, count mod 4) with
an exponential memory of 16 bars. The score of "the 1 is at position r" is sum over features f and bar positions p of
w[f * 4 + p] * rel_f((r + p) mod 4), rel = (acc - mean of the 4) / (|mean| + 1e-6): 88 weights, trained on the user's
Traktor grids and audio (studies/downbeat/results/v2_weights.json).

Causal timing. Beat j's features need the 4 beats after it (the bar after against the bar before), so they are complete
at beat j + 4; at each beat the decision is taken first, then beat j = i - 4 enters: a beat's features weigh in 5 beats
later (the study's delay 5).

Streaming windows (the device's, core/beat/jb_down.c, does exactly this; tests/test_down.c checks it against this file):
  - the peak features (e150, e100, flux, the group fluxes: max; e150_mean: mean) over the frames k - 5 .. k + 10 around
    the beat's frame k (the study's -50 .. +100 ms);
  - each grid beat opens a SEGMENT, its frames up to the next grid beat's; the level novelty is segment j against j - 1,
    the bar novelty segments j .. j + 3 against j - 4 .. j - 1, the group means segment j's (the study: windows of the
    median period). The ring of 9 segments' 81 band sums is the whole spectral memory (2.9 KB as float32). It runs on
    every grid beat, also while ModeBpm is not steady yet (its provisional grid: "shadow" beats, not counted) and across
    resets, so that the first counted beats have the bar before them, as the study's windows do; a segment counts as
    "before" only when it joins the next one without a gap.

States: OFF (AUTO BAR off), LISTENING (until every bar position has features, about 9 beats, and then until the best
phase leads the second by COMMIT), LOCKED(phi), MANUAL (the user nudged the beat: no more moves until a reset or until
AUTO BAR is picked again). Moves (whole beats, the tempo untouched): the first when the best phase leads by COMMIT; later
ones when another phase has led the current one by SWITCH for HOLD beats in a row (the "stable" policy). A reset (the
estimator stopped, a resync, silence) clears the count, the accumulators and the state, and listens again; Link's
bar stays where it is until the new bar is known. A tempo jump (ModeBpm.jumps) is not a reset by default: those are
mostly short excursions in breaks, and resetting on them cost 3 points mean and 2.4x the moves (results/v2_stream_check.md).
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np

FPS = 100
BANDS = 81
LOW150, LOW100 = 5, 3                     # bands 0-4: 43-129 Hz; 0-2: 43-86 Hz
GROUPS = ((0, 5), (5, 17), (17, 33), (33, 52), (52, 69), (69, 81))
PRE, POST = 5, 10                         # the peak window, frames before and after the beat's frame
RING = PRE + POST + 1                     # frames of curves kept
CURVES = 10                               # e150, e100, flux150, flux_all, flux_g0..5
PEAKS = 11                                # e150, e100, e150_mean, flux150, flux_all, flux_g0..5
SEGS = 9                                  # segments j-4 .. j+4 around beat j = i - 4: the bars before and after it
BEATS = 8                                 # counted beat records kept (i-4 .. i are needed)
DELAY = 4                                 # beat i-4 is complete at beat i (it enters after the decision at i)
FEATURES = ("e150", "e100", "e150_mean", "flux150", "flux_all", "nov_all", "nov_low", "nov_all_pos",
            *(f"flux_g{g}" for g in range(6)), *(f"mean_g{g}" for g in range(6)), "nov_bar_all", "nov_bar_low")
NF = len(FEATURES)
WEIGHTS = Path(__file__).resolve().parent.parent / "studies" / "downbeat" / "results" / "v2_weights.json"
OFF, LISTENING, LOCKED, MANUAL = "off", "listening", "locked", "manual"


@dataclass(frozen=True)
class Model:
    w: np.ndarray                         # (NF * 4,): w[f * 4 + p]
    bars: float                           # the accumulators' memory, bars (exponential)
    commit: float                         # the first move: the best phase leads the second by this
    switch: float                         # a later move: another phase leads the current one by this ...
    hold: int                             # ... for this many beats in a row


def load_model(path: Path = WEIGHTS) -> Model:
    """The study's export, checked against what this file computes."""
    d = json.loads(path.read_text())
    if tuple(d["features"]) != FEATURES:
        raise ValueError(f"{path}: features {d['features']} are not this estimator's {FEATURES}")
    if d["mode"] != "exp" or len(d["bars"]) != 1 or d["delay"] != DELAY + 1 or len(d["w"]) != NF * 4:
        raise ValueError(f"{path}: needs one exponential memory, delay {DELAY + 1}, {NF * 4} weights")
    link = d["link"]
    if link.get("wait", 0) != 0:
        raise ValueError(f"{path}: a first-move wait is not supported")
    return Model(np.asarray(d["w"], np.float64), float(d["bars"][0]), float(link["commit"]), float(link["switch"]),
                 int(link["hold"]))


@dataclass(frozen=True)
class BarView:
    """What the controller, the page and the nudge log get."""
    state: str                            # off / listening / locked / manual
    phi: int | None                       # LOCKED: the count position (count mod 4) of the 1
    lead: float                           # LOCKED: the 1's score over the best other; else the best over the second
    conf: float                           # 0..1, the lead against the commit level (0 before the scores are ready)
    seq: int                              # decisions taken since the start (one per counted beat)
    count: int | None                     # the count of the beat the last decision was taken at
    t_beat: float | None                  # its stream time, s
    scores: tuple[float, ...] | None      # the 4 phases' scores at that beat (None before they are ready)
    moves: int                            # moves asked for since the start


def frame_curves(L: np.ndarray, prev: np.ndarray | None) -> np.ndarray:
    """The 10 per-frame curves of the first study (extract.frame_curves): e150, e100, flux150, flux_all, flux_g0..5.
    The first frame's flux is 0."""
    lin = np.power(10.0, L[:LOW150]) - 1.0
    e150 = math.log10(1.0 + float(lin.sum()))
    e100 = math.log10(1.0 + float(lin[:LOW100].sum()))
    pos = np.zeros(BANDS) if prev is None else np.maximum(L - prev, 0.0)
    return np.array([e150, e100, pos[:LOW150].sum(), pos.sum(), *(pos[a:b].sum() for a, b in GROUPS)])


class Downbeat:
    promote_first = True                               # count the shadow beat played on the frame counting starts at

    def __init__(self, model: Model | None = None) -> None:
        self.model = model or load_model()
        self.decay = math.exp(-1.0 / self.model.bars)
        self.auto = True
        self.log: list[tuple] | None = None            # set to a list: ("x", count, t, features) as a beat enters,
                                                       # ("d", count, t, scores, state, phi, lead) at each decision
        self._seq = 0
        self._moves = 0
        # the frame stream
        self._prev: np.ndarray | None = None          # the previous frame's bands (the flux)
        self._ring = np.zeros((RING, CURVES))          # curves of the last RING frames, frame f at f % RING
        self._first_frame = -1                         # the first frame seen (-1: none yet)
        # the grid beat stream (shadow or counted) and its segments: they survive resets
        self._grid: tuple[float, float] | None = None
        self._running = False                          # the grid is ModeBpm's running one: its beats are counted
        self._k = 0                                    # the grid's index of the next candidate beat
        self._next: float | None = None                # its time
        self._b_last: float | None = None              # the last grid beat played ...
        self._b_last_frame = -1                        # ... at this frame
        self._index = -1                               # the last frame taken
        self._sn = 0                                 # segments opened so far; segment m at m % SEGS
        self._open = False                             # a segment takes frames
        self._seg = np.zeros((SEGS, BANDS))            # band sums
        self._seg_n = np.zeros(SEGS, int)              # frames
        self._seg_start = np.full(SEGS, -1)            # first frame
        self._pending: list[tuple[int, int]] = []      # (frame, record) of counted beats whose peaks are not done
        self.reset()

    # ---- state ----
    def reset(self) -> None:
        """A reset event: the count, the accumulators and the state start over (MANUAL ends too); the frame and segment
        memory stays. Link's bar is left where it is."""
        self._n = 0                                    # beats counted since the reset
        self._c_last = 0
        self._rec_count = np.zeros(BEATS, int)         # counted beat records, beat j at j % BEATS
        self._rec_time = np.zeros(BEATS)
        self._rec_seg = np.zeros(BEATS, int)           # its segment
        self._rec_peak = np.zeros((BEATS, PEAKS))
        self._pending = []
        self._acc = np.zeros((NF, 4))
        self._acc_n = np.zeros(4, int)                 # beats entered per bar position
        self._state = OFF if not self.auto else LISTENING
        self._phi: int | None = None
        self._cand, self._run = -1, 0
        self._scores: np.ndarray | None = None
        self._lead = 0.0
        self._dec_count: int | None = None
        self._dec_time: float | None = None

    def set_auto(self, on: bool) -> None:
        """AUTO BAR: off -> OFF (the features keep running, nothing moves); picked on while OFF or MANUAL -> LISTENING:
        the first move comes as soon as the best phase leads by COMMIT."""
        self.auto = bool(on)
        if not self.auto:
            self._state, self._phi = OFF, None
        elif self._state in (OFF, MANUAL):
            self._state, self._phi = LISTENING, None
        else:
            return                                     # listening or locked already: nothing changes
        self._cand, self._run = -1, 0

    @property
    def counting(self) -> bool:
        """Beats are counted: a running grid is set, or beats were counted since the last reset."""
        return self._running or self._n > 0

    @property
    def playing(self) -> bool:
        """A grid is set (counted or shadow beats)."""
        return self._grid is not None

    def nudge(self) -> BarView:
        """The user moved the beat by hand (NUDGE BEAT, whole or half): MANUAL until a reset or AUTO BAR picked again.
        Returns the view before the nudge (for the hand-label log)."""
        before = self.view()
        if self.auto:
            self._state, self._phi = MANUAL, None
            self._cand, self._run = -1, 0
        return before

    def view(self) -> BarView:
        sc = None if self._scores is None else tuple(float(x) for x in self._scores)
        conf = 0.0 if sc is None else min(max(self._lead / self.model.commit, 0.0), 1.0)
        return BarView(self._state, self._phi, float(self._lead), conf, self._seq, self._dec_count, self._dec_time, sc,
                       self._moves)

    # ---- input ----
    def set_grid(self, t: float, tb: float, period: float, running: bool = True) -> None:
        """After a tracked beat at t: the estimator's grid, a beat at tb and every period from it; its beats from t on
        are played (linkbeats.py). running: ModeBpm runs (not stopped), so they are counted; else they are shadow beats
        that only keep the segments going."""
        if not period > 0:
            raise ValueError(f"grid period {period}")
        # counting starts on a beat played on this very frame as a shadow beat (frames come before their tracked beat):
        # that is the first beat Link plays from the running grid (linkbeats.py), so it is counted (beat 0)
        first = running and not self._running and self._n == 0 and self.promote_first
        self._grid, self._running = (tb, period), bool(running)
        if first and self._b_last is not None and self._b_last_frame == self._index:
            self._count(self._b_last_frame, self._b_last, None)
        k = math.ceil((t - tb) / period - 1e-9)
        while self._b_last is not None and tb + k * period <= self._b_last + 0.5 * period:
            k += 1
        self._k, self._next = k, tb + k * period

    def stop(self) -> None:
        """No grid (the estimator stopped): no beat is played, the open segment closes; a reset when beats were being
        counted."""
        self._grid, self._running, self._next, self._open = None, False, None, False
        if self._n:
            self.reset()

    def frame(self, index: int, L: np.ndarray) -> None:
        """One frame of the tracker's 81 log bands (log10(1 + filterbank)), frame `index` at index / FPS s."""
        L = np.asarray(L, np.float64)
        self._index = index
        if self._first_frame < 0:
            self._first_frame = index
        self._ring[index % RING] = frame_curves(L, self._prev)
        self._prev = L.copy()
        if self._next is not None and self._grid is not None and math.floor(self._next * FPS + 0.5) <= index:
            self._beat(index, self._grid, self._next)
        if self._open:
            s = (self._sn - 1) % SEGS
            self._seg[s] += L
            self._seg_n[s] += 1
        while self._pending and self._pending[0][0] + POST <= index:
            _, r = self._pending.pop(0)                # a counted beat's peak window ends POST frames after it
            self._rec_peak[r] = self._peaks(index)

    # ---- the work ----
    def _peaks(self, index: int) -> np.ndarray:
        lo = max(index - POST - PRE, self._first_frame)
        rows = self._ring[[f % RING for f in range(lo, index + 1)]]
        mx = rows.max(0)
        return np.array([mx[0], mx[1], rows[:, 0].mean(), mx[2], mx[3], *mx[4:10]])

    def _beat(self, index: int, grid: tuple[float, float], b: float) -> None:
        tb, p = grid
        gap = None if self._b_last is None else max(1, math.floor((b - self._b_last) / p + 0.5))
        self._b_last, self._b_last_frame = b, index
        self._k += 1
        self._next = tb + self._k * p
        s = self._sn % SEGS                            # a new segment from this frame on (the frame is added after)
        self._seg[s], self._seg_n[s], self._seg_start[s] = 0.0, 0, index
        self._sn += 1
        self._open = True
        if self._running:                              # (a shadow beat: segments only)
            self._count(index, b, gap)

    def _count(self, index: int, b: float, gap: int | None) -> None:
        """A counted beat (its segment is the newest): a decision, then beat i - 4 enters."""
        c = 0 if self._n == 0 or gap is None else self._c_last + gap
        self._c_last = c
        i = self._n
        self._n += 1
        r = i % BEATS
        self._rec_count[r], self._rec_time[r], self._rec_seg[r] = c, b, self._sn - 1
        self._pending.append((index, r))
        self._decide(c, b)
        j = i - DELAY
        if j >= 0:
            rj = j % BEATS
            x = self._features(int(self._rec_seg[rj]), self._rec_peak[rj])
            self._enter(int(self._rec_count[rj]), x)
            if self.log is not None:
                self.log.append(("x", int(self._rec_count[rj]), float(self._rec_time[rj]), x))

    def _joined(self, m: int) -> bool:
        """Segment m ends where segment m + 1 starts (both still in the ring)."""
        if m < 0 or m < self._sn - SEGS:
            return False
        a, b = m % SEGS, (m + 1) % SEGS
        return self._seg_start[a] >= 0 and self._seg_start[a] + self._seg_n[a] == self._seg_start[b]

    def _mean(self, ms: list[int]) -> np.ndarray:
        idx = [m % SEGS for m in ms]
        return self._seg[idx].sum(0) / max(int(self._seg_n[idx].sum()), 1)

    def _features(self, m: int, pk: np.ndarray) -> np.ndarray:
        """The 22 features of the counted beat whose segment is m (segments m .. m + 3 closed)."""
        after = self._mean([m])
        if self._joined(m - 1):
            dm = after - self._mean([m - 1])
            nov = (np.abs(dm).sum(), np.abs(dm[:LOW150]).sum(), np.maximum(dm, 0.0).sum())
        else:
            nov = (0.0, 0.0, 0.0)
        mean_g = [after[a:b].mean() for a, b in GROUPS]
        before: list[int] = []
        while len(before) < 4 and self._joined(m - 1 - len(before)):
            before.append(m - 1 - len(before))
        if before:
            db = self._mean(list(range(m, m + 4))) - self._mean(before)
            nb = (np.abs(db).sum(), np.abs(db[:LOW150]).sum())
        else:
            nb = (0.0, 0.0)
        return np.array([pk[0], pk[1], pk[2], pk[3], pk[4], *nov, *pk[5:11], *mean_g, *nb])

    def _enter(self, count: int, x: np.ndarray) -> None:
        q = count % 4
        self._acc[:, q] = x + self.decay * self._acc[:, q]
        self._acc_n[q] += 1

    def scores(self) -> np.ndarray | None:
        """The 4 phases' scores from the accumulators, None until every bar position has a beat."""
        if (self._acc_n == 0).any():
            return None
        m = self._acc.mean(1, keepdims=True)
        rel = (self._acc - m) / (np.abs(m) + 1e-6)
        w = self.model.w.reshape(NF, 4)
        return np.array([(w * rel[:, (r + np.arange(4)) % 4]).sum() for r in range(4)])

    def _decide(self, count: int, t: float) -> None:
        self._policy(count, t)
        if self.log is not None:
            self.log.append(("d", count, t, self._scores, self._state, self._phi, self._lead))

    def _policy(self, count: int, t: float) -> None:
        self._seq += 1
        self._dec_count, self._dec_time = count, t
        s = self.scores()
        self._scores = s
        if s is None:
            self._lead = 0.0
            return
        best = int(np.argmax(s))
        if self._state == LOCKED:
            self._lead = float(s[self._phi] - np.max(np.delete(s, self._phi)))
        else:
            self._lead = float(s[best] - np.sort(s)[-2])
        if self._state == LISTENING:
            if self._lead >= self.model.commit:
                self._lock(best, s)
        elif self._state == LOCKED:
            if best != self._phi and s[best] - s[self._phi] >= self.model.switch:
                self._run = self._run + 1 if best == self._cand else 1
                self._cand = best
                if self._run >= self.model.hold:
                    self._lock(best, s)
            else:
                self._cand, self._run = -1, 0

    def _lock(self, phi: int, s: np.ndarray) -> None:
        self._state, self._phi = LOCKED, phi
        self._cand, self._run = -1, 0
        self._moves += 1
        self._lead = float(s[phi] - np.max(np.delete(s, phi)))


def link_shift(count: int, phi: int, link_beat: float) -> int:
    """The whole-beat move that puts Link's bar on the 1: Link's beat (its session beat at the counted beat's moment,
    possibly half a beat off after a half-beat nudge) should sit at bar position (count - phi) mod 4.
    Returns n in {-1, 0, 1, 2}."""
    whole = math.floor(link_beat + 0.25)
    n = ((count - phi) - whole) % 4
    return n - 4 if n == 3 else n


class BandTap:
    """The tracker's 81-band log spectrum, frame by frame (live.tracker's frames: frame k = the 2048 samples ending at
    sample k * 441, a Hann window, madmom's logarithmic filterbank 12 bands an octave 30 Hz-17 kHz, log10(1 + x))."""

    def __init__(self) -> None:
        from madmom.audio.filters import LogarithmicFilterbank

        from live.tracker import FRAME, HOP, SAMPLE_RATE
        self.frame_len, self.hop = FRAME, HOP
        bins = np.fft.rfftfreq(FRAME, 1 / SAMPLE_RATE)[:FRAME // 2]
        self.fb = np.asarray(LogarithmicFilterbank(bins, num_bands=12, fmin=30, fmax=17000, norm_filters=True,
                                                   unique_filters=True), dtype=np.float32)
        if self.fb.shape[1] != BANDS:
            raise RuntimeError(f"the filterbank has {self.fb.shape[1]} bands, not {BANDS}")
        self.window = np.hanning(FRAME).astype(np.float32)
        self._hist = np.zeros(FRAME, np.float32)
        self._total = 0
        self.frames = 0                                # the next frame's index

    def feed(self, samples: np.ndarray) -> tuple[int, np.ndarray]:
        """(index of the first frame returned, (n, 81) float32 log bands) for the frames these samples complete."""
        samples = np.asarray(samples, np.float32)
        self._hist = np.concatenate([self._hist, samples])
        self._total += len(samples)
        first = self.frames
        ends = []
        while self._total > self.frames * self.hop:
            ends.append(len(self._hist) - (self._total - 1 - self.frames * self.hop))
            self.frames += 1
        out = np.zeros((len(ends), BANDS), np.float32)
        if ends:
            idx = np.asarray(ends)[:, None] - self.frame_len + np.arange(self.frame_len)[None, :]
            pad = np.concatenate([np.zeros(self.frame_len, np.float32), self._hist])
            mag = np.abs(np.fft.rfft(pad[idx + self.frame_len] * self.window, axis=1))[:, :self.frame_len // 2]
            out = np.log10(1.0 + mag.astype(np.float32) @ self.fb)
        self._hist = self._hist[-(self.frame_len + self.hop):]
        return first, out


def frame_of(t: float) -> int:
    """The frame a tracked beat at t belongs to (the tracker dates a beat with its frame's index / FPS)."""
    return math.floor(t * FPS + 0.5)


class Driver:
    """Feeds a Downbeat from a tracker's stream the way the engine does: each frame, and after its own frame each tracked
    beat through ModeBpm (`estimator`, the caller's: the engine replaces it on silence and resync), then the grid
    (counted while ModeBpm runs, shadow beats while it is not steady yet) or a stop; with jump_reset a tempo jump
    (ModeBpm.jumps) is a reset too (off by default: see the module's docstring). `trace`, when a list, gets every event
    as (kind, frame, *args): the golden vectors replay them in C."""

    def __init__(self, downbeat: Downbeat, jump_reset: bool = False) -> None:
        self.db = downbeat
        self.jump_reset = jump_reset             # a tempo jump (ModeBpm.jumps) resets the bar
        self.trace: list[tuple] | None = None

    def chunk(self, estimator, f0: int, bands: np.ndarray, found: list[float], now: float, silent: bool) -> None:
        last = f0 + len(bands) - 1
        if silent:
            self.event("stop", last)
            found = []
        by_frame: dict[int, list[float]] = {}
        for t in found:
            by_frame.setdefault(min(max(frame_of(t), f0), last), []).append(t)
        for i, row in enumerate(bands):
            self.db.frame(f0 + i, row)
            for t in by_frame.get(f0 + i, ()):
                self.tracked(estimator, t, f0 + i)
        if not len(bands):                       # (no frame completed: the beats still count)
            for t in found:
                self.tracked(estimator, t, last)
        if not silent and self.db.counting and estimator.mode_at(now) == "stopped":
            self.event("stop", last)             # 30 s without a steady beat

    def tracked(self, estimator, t: float, frame: int) -> None:
        jumps = estimator.jumps
        estimator.update(t)
        if estimator.jumps != jumps and self.jump_reset and self.db.counting:
            self.event("reset", frame)           # a tempo jump: a new bar to find
        grid = estimator.grid()
        running = estimator.mode_at(t) != "stopped"
        if grid is None:
            self.event("stop", frame)
        elif not running and self.db.counting:
            self.event("stop", frame)            # the estimator stopped: a reset; shadow beats from the next beat on
        else:
            self.event("grid", frame, t, grid[0], grid[1], float(running))

    def event(self, kind: str, frame: int, *args: float) -> None:
        if kind == "grid":
            self.db.set_grid(args[0], args[1], args[2], bool(args[3]))
        elif kind == "stop":
            if not self.db.playing and not self.db.counting:
                return                           # nothing to stop
            self.db.stop()
        elif kind == "reset":
            self.db.reset()
        elif kind == "nudge":
            self.db.nudge()
        elif kind == "auto":
            self.db.set_auto(bool(args[0]))
        else:
            raise ValueError(kind)
        if self.trace is not None:
            self.trace.append((kind, frame, *args))
