"""studies/follow-robust/graded.py: the graded confidence's parts."""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "studies" / "follow-robust"))

from graded import graded, jitter, soft_f


def test_beats_on_the_lines_are_a_perfect_soft_f_and_off_by_half_a_period_are_zero():
    p = 0.5
    lines = np.arange(0.0, 10.0, p)
    beats = list(lines)
    assert soft_f(beats, lines, p, now=10.0) > 0.9
    assert soft_f([b + 0.5 * p for b in beats], lines, p, now=10.0) < 0.05


def test_soft_f_is_graded_between_the_two():
    p = 0.5
    lines = np.arange(0.0, 10.0, p)
    values = [soft_f([b + off * p for b in lines], lines, p, now=10.0) for off in (0.0, 0.1, 0.25, 0.4, 0.5)]
    assert all(a > b for a, b in zip(values, values[1:]))          # strictly falling


def test_even_beats_have_full_jitter_confidence_and_wandering_ones_less():
    even = [i * 0.5 for i in range(20)]
    rng = np.random.default_rng(0)
    wobble = [i * 0.5 + rng.normal(0, 0.05) for i in range(20)]
    assert jitter(even) > 0.99
    assert jitter(wobble) < 0.6
    assert jitter([0.0, 0.5, 1.0]) == 0.0                           # too few beats to say


def test_the_graded_value_is_the_mean_clipped_to_0_1():
    assert graded(1, 1, 1, 1) == 1.0
    assert abs(graded(1, 0.5, 0.5, 0) - 0.5) < 1e-9
    assert graded(0, 0, 0, 0) == 0.0
