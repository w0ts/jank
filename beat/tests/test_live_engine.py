"""The engine's gates: confidence, the threshold with hysteresis, and the fast stop on silence."""
from pathlib import Path

import numpy as np
import pytest

import live.engine as engine_mod
from live.audio import load_audio, to_mono
from live.confidence import grid_confidence
from live.engine import SILENCE_S, Engine
from live.tracker import HOP, SAMPLE_RATE

GTZAN = Path(__file__).resolve().parent.parent / "data" / "gtzan" / "genres" / "disco" / "disco.00000.wav"
P = 0.5          # 120 BPM
GRID = (0.0, P)


def beats_until(t, period=P, offset=0.0):
    return list(np.arange(offset, t, period))


def test_confidence_full_on_the_grid():
    assert grid_confidence(beats_until(20.0), GRID, 20.0) == pytest.approx(1.0)


def test_confidence_drops_with_missed_extra_and_off_grid_beats():
    every_other = beats_until(20.0, 2 * P)
    assert grid_confidence(every_other, GRID, 20.0) == pytest.approx(2 / 3, abs=0.05)      # recall 1/2
    doubled = beats_until(20.0, P / 2)
    assert grid_confidence(doubled, GRID, 20.0) == pytest.approx(2 / 3, abs=0.05)          # precision 1/2
    assert grid_confidence(beats_until(20.0, offset=0.3 * P), GRID, 20.0) == 0.0         # off the grid
    assert grid_confidence(beats_until(10.0), GRID, 20.0) == 0.0                          # nothing lately


def test_confidence_without_a_grid_is_zero():
    assert grid_confidence(beats_until(20.0), None, 20.0) == 0.0


class StubEstimator:
    def mode_at(self, now):
        return "steady"

    def bpm_at(self, now):
        return 120.0

    def grid(self):
        return GRID


@pytest.fixture
def engine(monkeypatch):
    e = Engine(90, 180, min_confidence=0.5)
    e.estimator = StubEstimator()
    conf = {"v": 1.0}
    monkeypatch.setattr(engine_mod, "grid_confidence", lambda *a, **k: conf["v"])
    e.conf = conf
    return e


def test_threshold_gates_with_hysteresis(engine):
    assert engine.status().published.mode == "steady"
    engine.conf["v"] = 0.49
    st = engine.status()
    assert st.gated and st.published.mode == "stopped" and st.published.bpm is None
    engine.conf["v"] = 0.52                       # above the threshold, under threshold + hysteresis
    assert engine.status().gated
    engine.conf["v"] = 0.56
    assert not engine.status().gated
    engine.min_confidence = 0.0                   # 0 = never gate
    engine.conf["v"] = 0.0
    assert engine.status().published.mode == "steady"


@pytest.mark.skipif(not GTZAN.exists(), reason="GTZAN not downloaded")
def test_silence_stops_within_half_a_second_and_starts_over():
    e = Engine(90, 180)
    music = to_mono(load_audio(GTZAN))[:15 * SAMPLE_RATE]
    for i in range(0, len(music), HOP):
        e.feed(music[i:i + HOP])
    assert e.status().published.mode != "stopped" and e.status().confidence > 0.5
    fed = 0
    while e.status().published.mode != "stopped":
        e.feed(np.zeros(HOP, dtype=np.float32))
        fed += HOP
        assert fed < SAMPLE_RATE, "still running after 1 s of silence"
    assert fed / SAMPLE_RATE == pytest.approx(SILENCE_S, abs=0.02)
    assert e.beats == [] and e.status().confidence == 0.0
    e.feed(music[:HOP])                           # sound again: no longer silent
    assert not e.silent


def test_fold_doubles_or_halves_what_link_gets_and_resync_clears_it(engine):
    t_beat, period = engine.status().published.grid
    engine.fold = 2.0                                                   # the device's UP on HOME: x2
    pub = engine.status().published
    assert pub.bpm == pytest.approx(240.0) and pub.grid == pytest.approx((t_beat, period / 2))
    assert engine.status().bpm == pytest.approx(120.0)                  # the tracker's own tempo stays as heard
    engine.fold = 0.5                                                   # DOWN: /2
    pub = engine.status().published
    assert pub.bpm == pytest.approx(60.0) and pub.grid == pytest.approx((t_beat, period * 2))
    engine.resync()                                                     # Start / resync: listen afresh, no fold
    assert engine.fold == 1.0
