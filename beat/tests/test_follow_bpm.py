import numpy as np

from eval.follow_bpm import FollowBpm
from eval.mode_bpm import ModeBpm


def feed(est, beats) -> list[tuple[float, float | None, tuple | None]]:
    out = []
    for t in beats:
        est.update(float(t))
        out.append((float(t), est.bpm_at(float(t)), est.grid()))
    return out


def stepped(bpm: float, change: float, at_beat: int, n: int, jitter_s: float = 0.004, seed: int = 1) -> np.ndarray:
    """Beat times: bpm for at_beat beats, then bpm * (1 + change) (a pitch-fader step), with tracker jitter."""
    p0, p1 = 60 / bpm, 60 / (bpm * (1 + change))
    steps = np.where(np.arange(n) < at_beat, p0, p1)
    rng = np.random.default_rng(seed)
    return np.cumsum(steps) + rng.normal(0, jitter_s, n)


def phase_err_ms(grid, t_true: float) -> float:
    t0, p = grid
    return (t0 + np.round((t_true - t0) / p) * p - t_true) * 1000


def test_steady_is_as_precise_as_mode_bpm():
    beats = stepped(128, 0.0, 0, 640)
    f, m = FollowBpm(), ModeBpm()
    feed(f, beats)
    feed(m, beats)
    assert f.mode == "steady"
    assert abs(f.bpm() - 128) < 0.02
    assert abs(m.bpm() - 128) < 0.02


def test_a_two_percent_step_is_followed_without_losing_the_phase():
    beats = stepped(135, 0.02, 160, 260)
    clean = stepped(135, 0.02, 160, 260, jitter_s=0.0)
    trace = feed(FollowBpm(), beats)
    after = trace[160:]
    # the grid known after each beat, extrapolated to the next true beat: never more than 40 ms out
    worst = max(abs(phase_err_ms(g, clean[i + 161])) for i, (_, _, g) in enumerate(after[:-1]))
    assert worst < 40.0
    # within 0.5 % of the new tempo after 12 beats, and the mode never stops
    assert all(abs(b / (135 * 1.02) - 1) < 0.005 for _, b, _ in after[12:])
    assert all(b is not None for _, b, _ in after)


def test_today_mode_bpm_drifts_past_40_ms_on_the_same_step():
    beats = stepped(135, 0.02, 160, 260)
    clean = stepped(135, 0.02, 160, 260, jitter_s=0.0)
    trace = feed(ModeBpm(), beats)
    worst = max(abs(phase_err_ms(g, clean[i + 161])) for i, (_, _, g) in enumerate(trace[160:-1]))
    assert worst > 40.0          # the regression the follower fixes (measured 2026-11-15)


def test_a_measured_ratio_puts_the_grid_on_the_new_tempo_at_once():
    beats = stepped(135, 0.05, 160, 260)
    clean = stepped(135, 0.05, 160, 260, jitter_s=0.0)
    est = FollowBpm()
    trace = []
    for i, t in enumerate(beats):
        if i == 160:                                   # the spectrum's shift seen half a beat after the step
            assert est.apply_ratio(1.05, float(t) - 0.2)
        est.update(float(t))
        trace.append((float(t), est.bpm_at(float(t)), est.grid()))
    worst = max(abs(phase_err_ms(g, clean[i + 1])) for i, (_, _, g) in enumerate(trace[160:-1], start=160))
    assert worst < 15.0
    assert all(abs(b / (135 * 1.05) - 1) < 0.005 for _, b, _ in trace[160:])


def test_a_proposed_ratio_needs_the_next_beat_to_back_it():
    steady = stepped(135, 0.0, 0, 200, jitter_s=0.0)
    est = FollowBpm()
    for t in steady[:150]:
        est.update(float(t))
    est.propose_ratio(1.04, float(steady[149]) + 0.1, since=float(steady[149]) + 0.05)   # a chord change, no step
    for t in steady[150:]:
        est.update(float(t))
    assert abs(est.bpm() - 135) < 0.05 and est.log[-1][1] != "ratio" and any(e == "ratio refused" for _, e in est.log)
    stepped_beats = stepped(135, 0.04, 150, 200, jitter_s=0.0)
    est = FollowBpm()
    for t in stepped_beats[:150]:
        est.update(float(t))
    est.propose_ratio(1.04, float(stepped_beats[149]) + 0.3, since=float(stepped_beats[149]))
    est.update(float(stepped_beats[150]))
    assert any(e == "ratio" for _, e in est.log) and abs(est.bpm() / (135 * 1.04) - 1) < 0.003


def test_a_ratio_is_refused_without_a_trusted_grid():
    assert not FollowBpm().apply_ratio(1.03, 1.0)


def test_missed_beats_are_bridged():
    beats = stepped(128, 0.0, 0, 300)
    keep = np.ones(beats.size, bool)
    keep[100:116] = False                             # a 16-beat break without beats
    est = FollowBpm()
    trace = feed(est, beats[keep])
    t, b, g = trace[-1]
    assert abs(b - 128) < 0.05 and abs(phase_err_ms(g, stepped(128, 0, 0, 301, 0.0)[-1])) < 20


def test_off_grid_beats_do_not_move_the_tempo():
    beats = stepped(128, 0.0, 0, 300)
    odd = np.sort(np.concatenate([beats, beats[150:156] + 0.2]))   # 6 off-beat detections (a fill)
    est = FollowBpm()
    feed(est, odd)
    assert abs(est.bpm() - 128) < 0.05


def test_a_cut_to_a_new_tempo_and_phase_is_on_the_new_grid_within_16_beats():
    a = np.arange(0, 90, 60 / 124)
    b = a[-1] + 0.21 + np.cumsum(np.full(300, 60 / 132))   # new tempo AND new phase
    est = FollowBpm()
    trace = feed(est, np.concatenate([a, b]))
    for i in range(a.size + 16, a.size + 60):
        t, bpm, g = trace[i]
        assert est.mode == "steady" and abs(bpm - 132) < 0.5
        assert abs(phase_err_ms(g, trace[i + 1][0])) < 20


def test_a_cut_with_a_new_phase_far_off_is_a_jump():
    a = np.arange(0, 90, 60 / 124)
    b = a[-1] + 0.24 + np.cumsum(np.full(300, 60 / 124))   # same tempo, half a beat out (an unmatched cut)
    est = FollowBpm()
    trace = feed(est, np.concatenate([a, b]))
    t, bpm, g = trace[a.size + 40]
    assert est.jumps >= 1 and abs(phase_err_ms(g, trace[a.size + 41][0])) < 20


def test_a_slow_ramp_is_followed():
    # 124 -> 130 BPM over 60 s, then steady
    p = []
    t, bpm = 0.0, 124.0
    while t < 200:
        bpm = 124 + 6 * min(max((t - 60) / 60, 0), 1)
        t += 60 / bpm
        p.append((t, bpm))
    beats = np.array([x for x, _ in p])
    tempo = np.array([y for _, y in p])
    trace = feed(FollowBpm(), beats)
    errs = [abs(b / tempo[i] - 1) for i, (_, b, _) in enumerate(trace) if 70 < beats[i] < 200 and b]
    assert np.mean(np.array(errs) < 0.005) > 0.9
