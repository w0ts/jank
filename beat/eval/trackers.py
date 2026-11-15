"""Trackers under test. Each maps an audio file to (beats, downbeats) in seconds.

causal=True means the tracker decides each beat from the past only (usable live as is);
causal=False trackers are ceilings: what is reachable when the whole piece is known.
"""
from __future__ import annotations

import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

import numpy as np

_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_ROOT / "stubs"))                      # pyaudio stand-in for BeatNet
os.environ.setdefault("TORCH_HOME", str(_ROOT / "data" / "torch"))   # checkpoints stay in the project

Result = tuple[np.ndarray, np.ndarray]


@dataclass(frozen=True)
class Tracker:
    name: str
    causal: bool
    make: Callable[[], Callable[[Path], Result]]   # built once per worker process


def _beat_this(ckpt: str):
    def make():
        from beat_this.inference import File2Beats
        f2b = File2Beats(checkpoint_path=ckpt, device="cpu", dbn=False)
        return lambda p: tuple(np.asarray(x) for x in f2b(str(p)))
    return make


def _signal(p: Path):
    """The audio as a madmom Signal at 44.1 kHz mono. madmom's own loader needs ffmpeg for anything it can't
    read itself (and to resample); decoding with soundfile + soxr works the same on the Mac and the farm."""
    import soundfile as sf
    import soxr
    from madmom.audio.signal import Signal
    y, sr = sf.read(str(p), dtype="float32", always_2d=True)
    y = y.mean(axis=1)
    if sr != 44100:
        y = soxr.resample(y, sr, 44100)
    return Signal(y, sample_rate=44100, num_channels=1)


def _madmom(online: bool):
    def make():
        from madmom.features.beats import DBNBeatTrackingProcessor, RNNBeatProcessor
        rnn = RNNBeatProcessor(online=online)
        def run(p):
            dbn = DBNBeatTrackingProcessor(fps=100, online=online)   # fresh state per piece
            act = rnn(_signal(p))
            beats = dbn.process_online(act, reset=True) if online else dbn(act)
            return np.asarray(beats), np.array([])
        return run
    return make


def _madmom_offline_chunked(chunk_s: float = 300.0, overlap_s: float = 30.0):
    """madmom offline on long recordings: its Viterbi table grows with frames x states (about 12 GB for 1.5 h), so
    run it on overlapping chunks and keep each chunk's interior (a reference only needs local agreement)."""
    def make():
        from madmom.audio.signal import Signal
        from madmom.features.beats import DBNBeatTrackingProcessor, RNNBeatProcessor
        rnn = RNNBeatProcessor(online=False)

        def run(p):
            sig = _signal(p)
            sr, n = 44100, len(sig)
            step, half = int((chunk_s - overlap_s) * sr), overlap_s / 2
            beats = []
            for start in range(0, n, step):
                end = min(n, start + int(chunk_s * sr))
                part = Signal(np.asarray(sig[start:end]), sample_rate=sr, num_channels=1)
                b = DBNBeatTrackingProcessor(fps=100)(rnn(part)) + start / sr
                lo = start / sr + (half if start > 0 else 0.0)
                hi = end / sr - (half if end < n else 0.0)
                beats.extend(b[(b >= lo) & (b < hi)].tolist())
                if end >= n:
                    break
            return np.array(sorted(beats)), np.array([])
        return run
    return make


def _madmom_downbeats():
    from madmom.features.downbeats import DBNDownBeatTrackingProcessor, RNNDownBeatProcessor
    rnn = RNNDownBeatProcessor()
    dbn = DBNDownBeatTrackingProcessor(beats_per_bar=[3, 4], fps=100)
    def run(p):
        out = dbn(rnn(_signal(p)))
        return out[:, 0], out[out[:, 1] == 1, 0]
    return run


def _beatnet(mode: str, inference: str):
    def make():
        if not hasattr(np, "in1d"):   # BeatNet 1.1.3 calls np.in1d, removed in numpy 2.4; np.isin is its replacement
            np.in1d = np.isin
        from BeatNet.BeatNet import BeatNet
        def run(p):
            est = BeatNet(1, mode=mode, inference_model=inference, plot=[], thread=False)
            out = np.asarray(est.process(str(p)))
            if out.size == 0:
                return np.array([]), np.array([])
            return out[:, 0], out[out[:, 1] == 1, 0]
        return run
    return make


def _via_lookahead():
    raise RuntimeError("streaming Beat This! runs through eval.lookahead, not eval.run")


# Live candidates first. The offline ones (causal=False) are only a reference ceiling: the product hears a live
# feed, never a file (the user, 2026-11-13: "drop the offline, we will never use it").
TRACKERS = {t.name: t for t in [
    Tracker("madmom_online", True, _madmom(True)),
    Tracker("beatnet_online_pf", True, _beatnet("online", "PF")),
    *[Tracker(f"beat_this_small0_stream_L{L}", True, _via_lookahead) for L in (0, 5, 12)],
    Tracker("beat_this_final0", False, _beat_this("final0")),
    Tracker("beat_this_small0", False, _beat_this("small0")),
    Tracker("madmom_dbn_offline", False, _madmom(False)),
    Tracker("madmom_dbn_offline_chunked", False, _madmom_offline_chunked()),
    Tracker("madmom_downbeat_offline", False, _madmom_downbeats),
]}
