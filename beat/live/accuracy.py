"""How well the beat follows the music as HEARD, scored live against Traktor's beat grid (`player --xiao`).

The reference: Traktor's beats of the track (data/traktor/index.json, file seconds), mapped through what the player
actually played: each output block's file position, rate and the time it is heard at the speakers (`Block`, from
varispeed's callback: its DAC time). Three things are scored against it, all on the same clock (the player's Link
clock, microseconds):
  - "xiao":   the XIAO's beat reports, each the instant the XIAO takes the beat to be heard (sample arrival +
              its beat_latency), as the page receives and dates them;
  - "player": this program's own tracker, its beats mapped through its stream-to-Link clock map;
  - "link":   the Link session's beat (what Live follows), as the XIAO reports it with each of its beats.
Offsets are estimate - reference (+ = the estimate is late). Pure functions first; `Accuracy` keeps the state.
"""
from __future__ import annotations

import bisect
import csv
import math
import statistics
import threading
import time
from collections import deque
from dataclasses import asdict, dataclass, field
from pathlib import Path

SAMPLE_RATE = 44100
WINDOW_BEATS = 16          # the sliding window the panel shows
TIGHT_MS = 20.0            # "within": the eval's tight band
LOCK_RUN = 8               # beats in a row within TIGHT_MS of their median offset = locked
SOURCES = ("xiao", "player", "link")


@dataclass(frozen=True)
class Block:
    path: str
    pos: float             # file sample at the block's first output sample
    rate: float            # file samples per output sample
    frames: int
    heard_us: float        # when the block's first sample is heard


@dataclass(frozen=True)
class Metrics:
    n: int                 # matched beats in the window
    mean_ms: float | None  # + = late
    sd_ms: float | None
    within_pct: float | None
    bpm: float | None
    ref_bpm: float | None
    bpm_err: float | None
    lock_s: float | None   # from the first heard reference beat to the start of the first locked run


def heard_times(beats_s: list[float], blocks: list[Block]) -> list[tuple[float, float]]:
    """(heard us, file s) of every reference beat that falls inside one of the blocks (in block order)."""
    out = []
    for b in blocks:
        lo = b.pos / SAMPLE_RATE
        hi = (b.pos + b.rate * b.frames) / SAMPLE_RATE
        i = bisect.bisect_left(beats_s, lo)
        while i < len(beats_s) and beats_s[i] < hi:
            s = beats_s[i]
            out.append((b.heard_us + (s * SAMPLE_RATE - b.pos) / b.rate / SAMPLE_RATE * 1e6, s))
            i += 1
    return out


def match_offsets(est_us: list[float], ref_us: list[float]) -> list[tuple[float, float]]:
    """(estimate us, offset ms) of each estimate whose nearest reference beat is within half a reference period."""
    ref = sorted(ref_us)
    out = []
    if len(ref) < 2:
        return out
    for e in est_us:
        i = bisect.bisect_left(ref, e)
        best = min((r for r in ref[max(i - 1, 0):i + 1]), key=lambda r: abs(r - e))
        j = ref.index(best)
        period = ref[j + 1] - ref[j] if j + 1 < len(ref) else ref[j] - ref[j - 1]
        if abs(e - best) <= period / 2:
            out.append((e, (e - best) / 1000.0))
    return out


def fold_bpm(bpm: float, lo: float = 90.0, hi: float = 180.0) -> float:
    if bpm <= 0:
        raise ValueError(f"bad BPM {bpm}")
    while bpm < lo:
        bpm *= 2
    while bpm > hi:
        bpm /= 2
    return bpm


def lock_time(offsets: list[tuple[float, float]], start_us: float, run: int = LOCK_RUN, tight_ms: float = TIGHT_MS) -> float | None:
    """Seconds from start_us to the first beat that begins `run` matched beats in a row within tight_ms of their median
    offset (phase-locked, whatever the constant offset), or None."""
    offs = sorted(offsets)
    for i in range(len(offs) - run + 1):
        seg = [o for _, o in offs[i:i + run]]
        med = statistics.median(seg)
        if all(abs(o - med) <= tight_ms for o in seg):
            return max(0.0, (offs[i][0] - start_us) / 1e6)
    return None


def window_metrics(offsets: list[tuple[float, float]], bpm: float | None, ref_bpm: float | None,
                   start_us: float | None, window: int = WINDOW_BEATS, tight_ms: float = TIGHT_MS) -> Metrics:
    recent = [o for _, o in sorted(offsets)[-window:]]
    n = len(recent)
    mean = statistics.fmean(recent) if n else None
    sd = statistics.pstdev(recent) if n > 1 else (0.0 if n else None)
    within = 100.0 * sum(abs(o) <= tight_ms for o in recent) / n if n else None
    err = bpm - ref_bpm if bpm is not None and ref_bpm is not None else None
    lock = lock_time(offsets, start_us) if start_us is not None else None
    return Metrics(n, mean, sd, within, bpm, ref_bpm, err, lock)


def _r(v: float | None, nd: int = 2) -> float | None:
    return None if v is None or (isinstance(v, float) and math.isnan(v)) else round(v, nd)


@dataclass
class Track:
    path: str
    beats_s: list[float]
    traktor_bpm: float | None
    start_us: float | None = None
    ref: list[float] = field(default_factory=list)                 # heard us of the reference beats, in order
    est: dict = field(default_factory=lambda: {s: [] for s in SOURCES})
    bpm: dict = field(default_factory=lambda: {s: None for s in SOURCES})
    stats0: dict | None = None                                     # the XIAO's counters when the track started
    stats: dict | None = None


class Accuracy:
    """The live state: blocks in, estimates in, metrics and CSV rows out. Thread-safe."""

    def __init__(self, beats_by_path: dict[str, tuple[list[float], float]], csv_path: Path | None = None,
                 bpm_range: tuple[float, float] = (90.0, 180.0)) -> None:
        self.beats_by_path = beats_by_path
        self.csv_path = csv_path
        self.range = bpm_range
        self.track: Track | None = None
        self.rate = 1.0
        self._pending: deque[Block] = deque()
        self._lock = threading.Lock()
        self._last_row_beats = 0

    # ---- inputs ----
    def add_block(self, b: Block) -> None:
        self._pending.append(b)             # from the audio callback: no lock, no work

    def add_estimate(self, source: str, heard_us: float, bpm: float | None = None) -> None:
        if source not in SOURCES:
            raise ValueError(f"unknown source {source!r}")
        with self._lock:
            if self.track is None:
                return
            self.track.est[source].append(heard_us)
            del self.track.est[source][:-512]
            if bpm:
                self.track.bpm[source] = bpm

    def set_bpm(self, source: str, bpm: float | None) -> None:
        with self._lock:
            if self.track is not None:
                self.track.bpm[source] = bpm

    def set_xiao_stats(self, stats: dict) -> None:
        with self._lock:
            if self.track is not None:
                if self.track.stats0 is None:
                    self.track.stats0 = dict(stats)
                self.track.stats = dict(stats)

    # ---- the work, from one thread ----
    def update(self) -> None:
        """Take the blocks the callback queued: a new path starts a new track; reference beats get heard times."""
        while self._pending:
            b = self._pending.popleft()
            with self._lock:
                self.rate = b.rate
                if self.track is None or self.track.path != b.path:
                    self._finish_track()
                    beats, bpm = self.beats_by_path.get(b.path, ([], None))
                    self.track = Track(b.path, list(beats), bpm)
                t = self.track
                for heard, _ in heard_times(t.beats_s, [b]):
                    if t.start_us is None:
                        t.start_us = heard
                    if not t.ref or heard > t.ref[-1]:
                        t.ref.append(heard)

    def ref_bpm(self) -> float | None:
        t = self.track
        if t is None or not t.traktor_bpm:
            return None
        return fold_bpm(t.traktor_bpm * self.rate, *self.range)

    def metrics(self) -> dict:
        with self._lock:
            t = self.track
            if t is None:
                return {"path": None}
            ref_bpm = self.ref_bpm()
            out = {"path": t.path, "ref_beats": len(t.ref), "ref_bpm": _r(ref_bpm), "rate": self.rate}
            for k in ("phase_forces", "tempo_sets", "gaps"):        # this track's share of the XIAO's counters
                now, base = (t.stats or {}).get(k), (t.stats0 or {}).get(k)
                out[f"xiao_{k}"] = None if now is None or base is None else now - base
            for s in SOURCES:
                m = window_metrics(match_offsets(t.est[s], t.ref), t.bpm[s], ref_bpm, t.start_us)
                out[s] = {k: _r(v) if isinstance(v, float) else v for k, v in asdict(m).items()}
            return out

    def maybe_log(self, extra: dict | None = None) -> None:
        """A CSV row every WINDOW_BEATS reference beats (and one when the track ends)."""
        with self._lock:
            t = self.track
            due = t is not None and len(t.ref) >= self._last_row_beats + WINDOW_BEATS
        if due:
            self._log_row("window", extra)
            with self._lock:
                self._last_row_beats = len(self.track.ref) if self.track else 0

    def _finish_track(self) -> None:
        if self.track is not None and self.track.ref:
            self._log_row_locked("track", None)
        self._last_row_beats = 0

    def _log_row(self, kind: str, extra: dict | None) -> None:
        with self._lock:
            self._log_row_locked(kind, extra)

    def _log_row_locked(self, kind: str, extra: dict | None) -> None:
        if self.csv_path is None or self.track is None:
            return
        t = self.track
        ref_bpm = self.ref_bpm()
        row = {"time": time.strftime("%Y-%m-%d %H:%M:%S"), "kind": kind, "track": Path(t.path).name, "rate": self.rate,
               "ref_beats": len(t.ref), "ref_bpm": _r(ref_bpm)}
        for s in SOURCES:
            m = window_metrics(match_offsets(t.est[s], t.ref), t.bpm[s], ref_bpm, t.start_us)
            for k, v in asdict(m).items():
                row[f"{s}_{k}"] = _r(v) if isinstance(v, float) else v
        for k in ("hop_max_us", "hop_avg_us", "cpu_pct", "gaps", "overflows", "tempo_sets", "phase_forces", "err_us",
                  "heap_min", "latency_us"):
            now = (t.stats or {}).get(k)
            base = (t.stats0 or {}).get(k)
            row[f"xiao_{k}"] = now
            if k in ("gaps", "overflows", "tempo_sets", "phase_forces") and now is not None and base is not None:
                row[f"xiao_{k}_track"] = now - base            # this track's share of the counter
        row.update(extra or {})
        self.csv_path.parent.mkdir(parents=True, exist_ok=True)
        new = not self.csv_path.exists()
        with self.csv_path.open("a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(row), extrasaction="ignore")
            if new:
                w.writeheader()
            w.writerow(row)


def load_beats(index: Path, tracks: Path) -> dict[str, tuple[list[float], float]]:
    """path -> (Traktor's beats in file seconds, its BPM), for the tracks the library has."""
    import json
    if not index.is_file():
        return {}
    out = {}
    for r in json.loads(index.read_text()):
        beats = r.get("beats")
        if beats:
            out[str(tracks / r["file"])] = (sorted(float(b) for b in beats), float(r["bpm"]) if r.get("bpm") else None)
    return out
