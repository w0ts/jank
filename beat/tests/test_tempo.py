import numpy as np

from eval.tempo import tempo_score

REF = np.arange(0.0, 60.0, 60.0 / 140.0)   # 140 BPM


def test_exact_tracker_is_within_0_1():
    s = tempo_score(REF, REF)
    assert s["8s"]["within_0.1"] == 1.0 and s["8s"]["lock_s"] < 5.0


def test_half_tempo_is_folded_to_the_right_bpm():
    s = tempo_score(REF, REF[::2])
    assert s["8s"]["within_0.1"] == 1.0


def test_wrong_tempo_never_locks():
    s = tempo_score(REF, np.arange(0.0, 60.0, 60.0 / 141.0))
    assert s["8s"]["within_0.5"] == 0.0 and np.isnan(s["8s"]["lock_s"])


def test_longer_window_is_steadier_with_jitter():
    rng = np.random.default_rng(1)
    est = REF + rng.normal(0, 0.01, REF.size)
    s = tempo_score(REF, est)
    assert s["16s"]["median_err"] < s["4s"]["median_err"]
