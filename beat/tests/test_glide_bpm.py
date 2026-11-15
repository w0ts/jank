import numpy as np

from eval.glide_bpm import GlideBpm
from eval.locked_bpm import LockedBpm


def ramp_beats(bpm0: float, bpm1: float, hold0: float, ramp: float, hold1: float) -> tuple[np.ndarray, callable]:
    """Beat times for a tempo holding at bpm0, gliding linearly (in tempo) to bpm1, then holding."""
    def tempo(t):
        return np.interp(t, [0, hold0, hold0 + ramp, hold0 + ramp + hold1], [bpm0, bpm0, bpm1, bpm1])
    t, beats = 0.0, []
    while t < hold0 + ramp + hold1:
        beats.append(t)
        t += 60.0 / tempo(t)
    return np.array(beats), tempo


def run(est, beats):
    out = []
    for b in beats:
        est.update(float(b))
        out.append(est.bpm() or np.nan)
    return np.array(out)


def test_glide_follows_a_ramp_better_than_constant_model():
    beats, tempo = ramp_beats(126, 130, 60, 40, 60)
    during = (beats > 75) & (beats < 100)
    err_glide = np.nanmedian(np.abs(run(GlideBpm(), beats)[during] - tempo(beats[during])))
    err_const = np.nanmedian(np.abs(run(LockedBpm(), beats)[during] - tempo(beats[during])))
    assert err_glide < 0.3 and err_glide < err_const


def test_glide_settles_back_to_precise_constant_tempo():
    beats, tempo = ramp_beats(126, 130, 60, 40, 120)
    est = GlideBpm()
    run(est, beats)
    assert not est.gliding and abs(est.bpm() - 130) < 0.02


def test_restart_trigger_follows_a_ramp():
    beats, tempo = ramp_beats(126, 130, 60, 40, 60)
    during = (beats > 80) & (beats < 100)
    err = np.nanmedian(np.abs(run(GlideBpm(trigger="restarts"), beats)[during] - tempo(beats[during])))
    assert err < 0.3


def test_steady_jittery_tempo_does_not_glide():
    rng = np.random.default_rng(3)
    beats = np.arange(0, 300, 60 / 140) + rng.normal(0, 0.005, 700)
    est = GlideBpm()
    run(est, beats)
    assert est.glides == 0 and abs(est.bpm() - 140) < 0.01
