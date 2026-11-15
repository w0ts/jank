from pathlib import Path

import numpy as np
import pytest

from live.tracker import HOP, SAMPLE_RATE, OnlineTracker

GTZAN = Path(__file__).resolve().parent.parent / "data" / "gtzan" / "genres"
FILES = [GTZAN / "disco" / "disco.00000.wav", GTZAN / "classical" / "classical.00001.wav",
         GTZAN / "hiphop" / "hiphop.00002.wav"]
SECONDS = 20
RANGE = (90.0, 180.0)


def load(path: Path) -> np.ndarray:
    from eval.trackers import _signal
    return np.asarray(_signal(path), dtype=np.float32)[:SECONDS * SAMPLE_RATE]


def batch_online(y: np.ndarray) -> np.ndarray:
    """madmom's own online path (the DBNBeatTracker --online wiring) over the whole signal at once, with the frame
    ending at the current sample (origin 'online'), one net and the same range."""
    from madmom.audio.signal import Signal
    from madmom.features.beats import DBNBeatTrackingProcessor, RNNBeatProcessor
    from madmom.models import BEATS_LSTM
    rnn = RNNBeatProcessor(online=True, nn_files=[BEATS_LSTM[0]], origin="online")
    dbn = DBNBeatTrackingProcessor(fps=100, online=True, min_bpm=RANGE[0], max_bpm=RANGE[1])
    act = rnn(Signal(y, sample_rate=SAMPLE_RATE, num_channels=1))
    return np.asarray(dbn.process_online(act, reset=True))


def stream(y: np.ndarray, chunk: int) -> np.ndarray:
    tracker = OnlineTracker(*RANGE)
    beats: list[float] = []
    for i in range(0, len(y), chunk):
        beats += tracker.feed(y[i:i + chunk])
    return np.asarray(beats)


@pytest.mark.parametrize("path", FILES, ids=lambda p: p.name)
def test_streaming_matches_batch_online(path):
    if not path.exists():
        pytest.skip("GTZAN not downloaded")
    y = load(path)
    ref, live = batch_online(y), stream(y, 1000)       # chunks that do not line up with the 441 hop
    assert len(ref) > 15
    nearest = np.abs(live[None, :] - ref[:, None]).min(axis=1)
    assert np.mean(nearest <= 1.5 / 100) >= 0.95, (len(ref), len(live), nearest.max())
    assert abs(len(ref) - len(live)) <= 1


def test_chunking_does_not_change_the_beats():
    if not FILES[0].exists():
        pytest.skip("GTZAN not downloaded")
    y = load(FILES[0])
    assert np.array_equal(stream(y, HOP), stream(y, 4410))


def test_bad_range_is_rejected():
    with pytest.raises(ValueError):
        OnlineTracker(180, 90)
