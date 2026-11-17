import numpy as np

from eval.traktor import grid_beats


def test_grid_covers_the_track_both_sides_of_the_marker():
    beats, down = grid_beats(grid_s=1.0, bpm=120.0, dur=10.0)
    assert beats[0] == 0.0 and beats[-1] == 9.5
    assert np.allclose(np.diff(beats), 0.5)


def test_downbeats_every_fourth_from_the_marker():
    beats, down = grid_beats(grid_s=1.0, bpm=120.0, dur=10.0)
    assert 1.0 in down and 3.0 in down and 5.0 in down
    assert 0.0 not in down and 0.5 not in down
