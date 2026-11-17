import numpy as np

from eval.grid import grid_score

REF = np.arange(0.0, 30.0, 0.5)   # 120 BPM


def test_perfect_tracker_is_tight():
    s = grid_score(REF, REF)
    assert s["tight"] == 1.0 and s["lost"] == 0.0


def test_double_tempo_is_octave_not_tight():
    s = grid_score(REF, np.arange(0.0, 30.0, 0.25))
    assert s["octave"] == 1.0 and s["tight"] == 0.0


def test_half_period_shift_is_offbeat():
    s = grid_score(REF, REF + 0.25)
    assert s["offbeat"] == 1.0


def test_latency_is_charged():
    # a tracker that needs 2 s look-ahead and stops tracking at 10 s: from 12 s on the grid is stale but
    # still right, so it stays tight on a steady tempo; nothing known before 5 s + 2 s -> early beats lost
    s = grid_score(REF, REF[REF < 10.0], latency_s=2.0)
    assert 0.9 < s["tight"] + s["lost"] <= 1.0


def test_missed_beat_is_bridged_not_octave():
    est = np.delete(REF, np.arange(3, REF.size, 5))   # every 5th beat missed
    s = grid_score(REF, est)
    assert s["octave"] == 0.0 and s["tight"] > 0.95


def test_fold_fixes_half_tempo():
    s = grid_score(REF, REF[::2], fold=True)    # tracker at 60 BPM on 120 BPM music, on the beats
    assert s["tight"] == 1.0


def test_fold_does_not_fix_triplet():
    s = grid_score(REF, np.arange(0.0, 30.0, 0.75), fold=True)
    assert s["triplet"] == 1.0


def test_jitter_counts_loose():
    rng = np.random.default_rng(0)
    s = grid_score(REF, REF + rng.normal(0, 0.03, REF.size))
    assert s["loose"] > s["tight"]
