"""madmom's online beat tracker fed frame by frame from any chunked source.

The chain is the one `DBNBeatTracker --online` wires around madmom.audio.signal.Stream: every 441 samples (100 fps)
the last 2048 samples go through RNNBeatProcessor(online=True) (state kept, reset=False) and the activation into
DBNBeatTrackingProcessor(online=True). Differences from the stock script: ONE LSTM net (measured equal to 8) and the
DBN tempo range narrowed to the user's BPM range.
"""
from __future__ import annotations

import numpy as np

SAMPLE_RATE = 44100
HOP = 441          # 100 fps
FRAME = 2048


class OnlineTracker:
    def __init__(self, min_bpm: float = 90.0, max_bpm: float = 180.0, one_net: bool = True) -> None:
        if not 0 < min_bpm < max_bpm:
            raise ValueError(f"bad BPM range {min_bpm}-{max_bpm}")
        from madmom.audio.signal import Signal
        from madmom.features.beats import DBNBeatTrackingProcessor, RNNBeatProcessor
        from madmom.models import BEATS_LSTM
        self._signal = Signal
        self.rnn = RNNBeatProcessor(online=True, nn_files=[BEATS_LSTM[0]] if one_net else None,
                                    origin="stream", num_frames=1, fps=100)
        self.dbn = DBNBeatTrackingProcessor(fps=100, online=True, min_bpm=min_bpm, max_bpm=max_bpm)
        self.dbn.reset()
        self._hist = np.zeros(FRAME, dtype=np.float32)   # the newest samples, zero-padded at the start
        self._total = 0                                  # samples received
        self.frames = 0                                  # frames processed; frame k ends at sample k * HOP

    @property
    def time(self) -> float:
        """Stream seconds of the last processed frame's end."""
        return max(self.frames - 1, 0) * HOP / SAMPLE_RATE

    def feed(self, samples: np.ndarray) -> list[float]:
        """Mono float32 samples at 44.1 kHz, any length. Returns the beat times (stream seconds) found.

        Frame k covers the 2048 samples ending at sample k * HOP inclusive (madmom's origin 'online': frame 0 is one
        sample behind zeros), and the DBN labels it k / 100 s, the stream time of its newest sample."""
        samples = np.asarray(samples, dtype=np.float32)
        self._hist = np.concatenate([self._hist, samples])
        self._total += len(samples)
        beats: list[float] = []
        while self._total > self.frames * HOP:
            back = self._total - 1 - self.frames * HOP            # newest sample of the frame, counted from the end
            end = len(self._hist) - back
            window = self._hist[max(end - FRAME, 0):end]
            if len(window) < FRAME:
                window = np.concatenate([np.zeros(FRAME - len(window), dtype=np.float32), window])
            self.frames += 1
            act = self.rnn.process(self._signal(window, sample_rate=SAMPLE_RATE, num_channels=1), reset=False)
            beats.extend(float(b) for b in self.dbn.process_online(np.atleast_1d(act), reset=False))
        self._hist = self._hist[-(FRAME + HOP):]
        return beats
