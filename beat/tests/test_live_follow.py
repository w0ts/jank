import numpy as np

from live.confidence import grid_confidence
from live.follow import LineLog, line_confidence


def run_lines(grids, ticks):
    """grids: function now -> (t0, period); returns the LineLog after ticking through `ticks`."""
    log = LineLog()
    for now in ticks:
        log.tick(grids(now), now)
    return log


def test_on_a_constant_grid_it_equals_the_grid_confidence():
    period = 60 / 128
    beats = list(np.arange(0, 30, period) + 0.003)
    ticks = np.arange(0.01, 30, 0.01)
    log = run_lines(lambda now: (0.0, period), ticks)
    now = 29.99
    assert abs(line_confidence(beats, log, period, now) - grid_confidence(beats, (0.0, period), now)) < 0.02
    assert line_confidence(beats, log, period, now) > 0.95


def test_after_a_followed_tempo_step_it_stays_high_where_the_grid_confidence_drops():
    p0, p1 = 60 / 135, 60 / (135 * 1.05)
    step = 20.0
    beats = list(np.arange(0, step, p0))
    last = beats[-1]
    beats += list(last + p1 * np.arange(1, 40))
    def grid(now):                     # a follower that took the new tempo at the step, phase kept
        return (0.0, p0) if now < step else (last, p1)
    ticks = np.arange(0.01, 25, 0.01)
    log = run_lines(grid, ticks)
    now = 24.99
    lc = line_confidence(beats, log, p1, now)
    gc = grid_confidence(beats, grid(now), now)
    assert lc > 0.95
    assert gc < 0.8                    # today's measure judges the old beats against the new tempo, extended back


def _run_engine(engine, beats, until):
    """Feed beats straight into the engine's estimator and poll status() every 10 ms, as the runner does; returns the
    share of polls after the step published running."""
    from live.tracker import HOP, SAMPLE_RATE
    i, running = 0, []
    for now in np.arange(1.0, until, 0.01):
        while i < len(beats) and beats[i] <= now:
            engine.estimator.update(float(beats[i]))
            engine.beats.append(float(beats[i]))
            i += 1
        engine.tracker.frames = int(round(now * SAMPLE_RATE / HOP)) + 1
        st = engine.status()
        if now > 60.0:
            running.append(st.published.mode != "stopped")
    return float(np.mean(running))


def test_the_follow_engine_keeps_running_through_a_three_percent_step():
    from live.engine import Engine
    from live.follow import follow_engine
    p0, p1 = 60 / 135, 60 / (135 * 1.03)
    rng = np.random.default_rng(2)
    steps = np.where(np.arange(260) < 135, p0, p1)
    beats = list(np.cumsum(steps) + rng.normal(0, 0.004, 260))
    step_t = float(np.cumsum(steps)[134])
    assert 59 < step_t < 61
    today = _run_engine(Engine(90, 180, min_confidence=0.8), beats, 90.0)
    follow = _run_engine(follow_engine(90, 180, min_confidence=0.8), beats, 90.0)
    assert follow == 1.0
    assert today < 0.95                     # today's engine stops the outputs after the step (2026-11-15)


def test_no_grid_no_lines_no_confidence():
    log = LineLog()
    log.tick(None, 1.0)
    assert line_confidence([0.5, 1.0], log, 0.5, 1.0) == 0.0


def test_a_grid_that_moves_a_little_does_not_double_its_lines():
    log = LineLog()
    for k, now in enumerate(np.arange(0.01, 10, 0.01)):
        log.tick((0.0 + 0.004 * (k % 2), 0.5), now)
    gaps = np.diff(log.times())
    assert gaps.min() > 0.4
