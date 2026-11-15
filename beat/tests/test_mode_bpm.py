import numpy as np

from eval.mode_bpm import ModeBpm
from tests.test_glide_bpm import ramp_beats


def feed(est: ModeBpm, beats) -> list[str]:
    modes = []
    for t in beats:
        est.update(float(t))
        modes.append(est.mode)
    return modes


def test_steady_music_runs_steady():
    rng = np.random.default_rng(5)
    beats = np.arange(0, 300, 60 / 128) + rng.normal(0, 0.005, 640)
    est = ModeBpm()
    modes = feed(est, beats)
    assert modes[-1] == "steady" and modes.count("steady") > 0.95 * len(modes)
    assert abs(est.bpm() - 128) < 0.01


def test_ramp_is_classified_by_direction():
    beats, tempo = ramp_beats(124, 130, 60, 60, 60)
    est = ModeBpm()
    modes = np.array(feed(est, beats))
    assert np.mean(modes[(beats > 90) & (beats < 120)] == "speeding_up") > 0.6
    down, _ = ramp_beats(130, 124, 60, 60, 60)
    assert "slowing_down" in feed(ModeBpm(), down)


def test_cut_is_a_jump_back_to_steady():
    a = np.arange(0, 90, 60 / 124)
    b = a[-1] + np.cumsum(np.full(300, 60 / 132))
    est = ModeBpm()
    modes = feed(est, np.concatenate([a, b]))
    assert est.jumps >= 1 and modes[-1] == "steady" and abs(est.bpm() - 132) < 0.05


def test_long_silence_stops_the_transport():
    a = np.arange(0, 60, 0.5)
    est = ModeBpm(hold_s=30.0)
    feed(est, a)
    assert est.mode_at(70.0) == "steady"          # a 10 s break: hold
    assert est.mode_at(100.0) == "stopped"        # 40 s of nothing: stop
    assert est.bpm_at(100.0) is None


def test_random_onsets_never_run():
    rng = np.random.default_rng(7)
    onsets = np.sort(rng.uniform(0, 120, 200))      # no pulse at all
    est = ModeBpm()
    modes = feed(est, onsets)
    assert modes.count("stopped") > 0.8 * len(modes)
