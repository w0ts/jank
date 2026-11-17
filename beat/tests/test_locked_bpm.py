import numpy as np

from eval.locked_bpm import LockedBpm


def feed(beats) -> LockedBpm:
    est = LockedBpm()
    for t in beats:
        est.update(float(t))
    return est


def test_precision_grows_with_listening_time():
    rng = np.random.default_rng(0)
    beats = np.arange(0, 600, 60 / 140) + rng.normal(0, 0.01, 1400)   # 10 ms jitter
    short = feed(beats[:40]).bpm()
    long = feed(beats).bpm()
    assert abs(long - 140) < abs(short - 140)
    assert abs(long - 140) < 0.005


def test_missed_beats_and_breakdown_hold_the_tempo():
    beats = np.arange(0, 120, 60 / 128)
    keep = (beats < 50) | (beats > 80)              # 30 s breakdown with no beats
    keep &= np.arange(beats.size) % 7 != 3          # and every 7th beat missed
    assert abs(feed(beats[keep]).bpm() - 128) < 0.01


def test_off_beats_are_ignored():
    beats = list(np.arange(0, 60, 0.5))
    beats += [10.25, 20.25, 30.25]                  # three spurious off-beat detections
    assert abs(feed(sorted(beats)).bpm() - 120) < 0.01


def test_follows_a_tempo_change():
    a = np.arange(0, 120, 60 / 124)
    b = a[-1] + np.cumsum(np.full(400, 60 / 130))
    est = LockedBpm()
    times = np.concatenate([a, b])
    for t in times[times < a[-1] + 30]:             # 30 s after the change
        est.update(float(t))
    assert abs(est.bpm() - 130) < 0.1 and est.restarts >= 2
