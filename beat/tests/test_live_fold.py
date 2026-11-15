"""AUTO RANGE and the /2 anchor (live.fold, mirroring the firmware's core/jank_fold.c) and AUTO BAR's three settings
(engine + controller). The estimator is a stub whose tempo, jumps and grid the tests set."""
import types

import numpy as np
import pytest

from live.clockmap import ClockMap
from live.controller import LinkController, Snapshot
from live.downbeat import LISTENING, LOCKED, BarView, Driver, autobar_mode
from live.engine import START_WAIT_BEATS, Engine
from live.fold import Fold, best_exp, range_exp
from live.link_port import SimLink

P128 = 60.0 / 128.0


class Est:
    def __init__(self):
        self.bpm, self.jumps, self.grid_ = None, 0, None

    def mode_at(self, now):
        return "steady" if self.bpm else "stopped"

    def bpm_at(self, now):
        return self.bpm

    def grid(self):
        return self.grid_

    def tempo(self, bpm, tb=100.0, jump=False):
        self.bpm = bpm
        self.grid_ = None if bpm is None else (tb, 60.0 / bpm)
        self.jumps += int(jump)


class FakeBar:
    """Stands in for the downbeat estimator: the tests set its state."""
    auto = True

    def __init__(self):
        self.state = LISTENING

    def view(self):
        return BarView(self.state, 0 if self.state == LOCKED else None, 0.0, 0.0, 1, 1, 1.0, None, 0)

    def set_auto(self, on):
        self.auto = on

    def reset(self):
        self.state = LISTENING


def make(min_confidence=0.0):
    e = Engine(90, 180, min_confidence=min_confidence, downbeat=FakeBar())
    e.estimator, e.tracker = Est(), types.SimpleNamespace(time=0.0, feed=lambda s: [])
    e.bands = types.SimpleNamespace(feed=lambda s: (0, np.zeros((0, 81))), frames=0)
    e.driver.chunk = lambda *a, **k: None
    return e


def at(e, bpm, now, tb=100.0, jump=False):
    e.estimator.tempo(bpm, tb, jump)
    e.tracker.time = now
    return e.status()


def test_best_exp_and_range():
    assert best_exp(70, 140) == 1 and best_exp(280, 140) == -1 and best_exp(140, 140) == 0
    assert range_exp(90, 175) == 1 and range_exp(90, 132) == 0 and range_exp(135, 128) == 0


def test_tie_goes_to_the_smaller_fold():
    ref = 100.0
    assert best_exp(ref * 2 ** 0.5, ref) == -1      # sqrt(2) above: x1 and /2 are equally far, /2 is the smaller


@pytest.mark.parametrize("first,second,expect_bpm", [(140, 70, 140), (128, 64, 128), (175, 90, 180),
                                                     (128, 132, 132), (128, 135, 135), (132, 90, 90)])
def test_auto_range_on_a_new_tempo(first, second, expect_bpm):
    e = make()
    st = at(e, first, 10.0)
    assert st.fold == 1.0 and st.published.bpm == first
    st = at(e, second, 20.0, jump=True)
    assert st.published.bpm == pytest.approx(expect_bpm)
    assert st.fold_auto == (st.fold != 1.0)


def test_a_flip_from_a_lock_after_a_pause_uses_the_last_output_within_120_s():
    e = make()
    at(e, 128, 10.0)
    at(e, None, 20.0)                                         # no tempo: the track ends
    st = at(e, 64, 100.0)                                     # 80 s later: a new lock, ranged against 128
    assert st.fold == 2.0 and st.published.bpm == pytest.approx(128)
    e = make()
    at(e, 128, 10.0)
    at(e, None, 20.0)
    st = at(e, 64, 200.0)                                     # the output is older than 120 s: no reference, no fold
    assert st.fold == 1.0 and st.published.bpm == 64


def test_the_playing_session_is_the_reference():
    e = make()
    e.session_bpm = lambda: 100.0
    st = at(e, 50, 1.0)
    assert st.fold == 2.0 and st.published.bpm == pytest.approx(100)
    e = make()
    e.session_bpm = lambda: 100.0
    assert at(e, 90, 1.0).fold == 1.0      # 90 x 2 is 180: no range match


def test_the_user_wins_until_the_next_new_tempo_which_ranges_against_the_user_output():
    e = make()
    at(e, 100, 1.0)
    e.fold = 2.0                                              # UP: the output is 200
    assert at(e, 100, 2.0).published.bpm == pytest.approx(200)
    st = at(e, 90, 3.0, jump=True)                            # 90 x 2 = 180: 10 % off 200, no range match: user's kept
    assert st.fold == 2.0 and not st.fold_auto and st.published.bpm == pytest.approx(180)
    st = at(e, 98, 4.0, jump=True)                            # 98 x 2 = 196 is within 6 % of 180? 8.9 %: no, kept
    assert st.fold == 2.0
    st = at(e, 95, 5.0, jump=True)                            # 190 against 196: within 6 %, a range match: AUTO RANGE
    assert st.fold == 2.0 and st.fold_auto
    st = at(e, 190, 6.0, jump=True)                           # 190 against 190: ranged to 1 (a real match, not a user's)
    assert st.fold == 1.0 and not st.fold_auto


def test_resync_clears_the_fold_but_keeps_the_output_memory():
    e = make()
    at(e, 128, 1.0)
    e.fold = 2.0
    at(e, 128, 2.0)                                           # the output is 256
    e.resync()
    e.estimator = Est()                                       # (a resync starts a new estimator)
    assert e.fold == 1.0
    st = at(e, 64, 5.0)                                       # a lock after the resync: ranged against the 256 output
    assert st.fold == 4.0 and st.published.bpm == pytest.approx(256)


def beats_of(grid, t):
    """The grid's beat nearest after t."""
    t0, p = grid
    return t0 + np.ceil((t - t0) / p - 1e-9) * p


def test_a_flip_mid_track_keeps_the_output_tempo_and_phase():
    e = make()
    st = at(e, 128, 10.0, tb=100.0)
    t0, period = st.published.grid
    p256 = P128 / 2
    st = at(e, 256, 11.0, tb=100.0 + 40 * P128 + p256, jump=True)      # the tracker locks on the off-beats of 256
    assert st.fold == 0.5 and st.published.bpm == pytest.approx(128)
    beat, per = st.published.grid
    assert per == pytest.approx(P128)
    assert ((beat - t0) / P128) == pytest.approx(round((beat - t0) / P128), abs=1e-9)      # on the old output's beats
    for k in range(1, 30):                                              # kept beat to beat
        st = at(e, 256, 11.0 + k, tb=100.0 + 40 * P128 + p256 + k * p256)
        assert ((st.published.grid[0] - beat) / P128) == pytest.approx(round((st.published.grid[0] - beat) / P128), abs=1e-9)
    st = at(e, 128, 50.0, tb=100.0 + 80 * P128, jump=True)               # and back
    assert st.fold == 1.0 and st.published.bpm == pytest.approx(128)
    assert ((st.published.grid[0] - t0) / P128) == pytest.approx(round((st.published.grid[0] - t0) / P128), abs=1e-9)


def test_repeated_flips_do_not_drift():
    e = make()
    st = at(e, 128, 1.0, tb=100.0)
    t0 = st.published.grid[0]
    tb, now = 100.0, 1.0
    for i in range(20):
        now += 5.0
        tb += 8 * P128
        half = i % 2 == 0
        p = P128 / 2 if half else P128
        st = at(e, 256 if half else 128, now, tb=tb + (p if half else 0), jump=True)
        assert st.published.bpm == pytest.approx(128)
        assert st.published.grid[1] == pytest.approx(P128)
        assert (st.published.grid[0] - t0) / P128 == pytest.approx(round((st.published.grid[0] - t0) / P128), abs=1e-6)


def test_a_user_half_keeps_the_bars_one():
    e = make()
    one = 1.5                                       # the session's bar 1 (count 0) at this stream time; beats 0.5 s apart
    e.place_fn = lambda t: (t - one) / 0.5
    at(e, 120, 1.0, tb=10.0)
    e.fold = 0.5
    st = at(e, 120, 2.0, tb=10.0)
    beat, per = st.published.grid
    assert per == pytest.approx(1.0)
    assert round((beat - one) / 0.5) % 2 == 0       # counts 1 and 3 (even positions) stay
    st = at(e, 120, 3.0, tb=10.5)
    assert round((st.published.grid[0] - one) / 0.5) % 2 == 0 and st.published.grid[0] == pytest.approx(beat + 1.0)


def test_fold_without_an_output_grid_uses_the_tracked_beat():
    f = Fold()
    f.user(0.25)
    assert f.grid(7.0, 0.5, None) == (7.0, 2.0)
    assert f.grid(7.5, 0.5, None) == (7.0, 2.0)     # j = 1: the beat stays 3 beats back
    assert f.grid(9.0, 0.5, None) == (9.0, 2.0)


# ---- AUTO BAR ----

def test_autobar_values():
    assert [autobar_mode(v) for v in ("off", "start", "always", "on", True, False)] == \
        ["off", "start", "always", "start", "start", "off"]
    with pytest.raises(ValueError):
        autobar_mode("sometimes")
    d = Driver(FakeBar())
    assert d.autobar == "start" and d.set_autobar("always") == "always" and d.db.auto
    assert d.set_autobar(False) == "off" and not d.db.auto


def test_engine_publishes_the_bar_per_setting_and_not_while_folded():
    e = make()
    e.downbeat.state = LOCKED
    st = at(e, 120, 1.0)
    assert st.autobar == "start" and st.published.bar_mode == "start" and st.published.bar is not None
    e.set_autobar("always")
    assert at(e, 120, 2.0).published.bar_mode == "always"
    e.set_autobar("off")
    assert at(e, 120, 3.0).published.bar is None
    e.set_autobar(True)
    e.fold = 2.0
    assert at(e, 120, 4.0).published.bar is None                 # no move while folded
    e.fold = 1.0
    assert at(e, 120, 5.0).published.bar is not None


def feed_beats(e, n):
    e.tracker.feed = lambda s: [0.0] * n
    e.feed(np.ones(100, np.float32))


def test_start_wait_runs_eight_tracked_beats_unless_the_estimator_locks():
    e = make()
    assert not at(e, None, 0.0).published.start_wait
    assert at(e, 120, 1.0).published.start_wait                  # a lock, the estimator still listening
    feed_beats(e, START_WAIT_BEATS - 1)
    assert at(e, 120, 2.0).published.start_wait
    feed_beats(e, 1)
    assert not at(e, 120, 3.0).published.start_wait              # 8 beats: the start goes on the current bar line
    e = make()
    at(e, 120, 1.0)
    e.downbeat.state = LOCKED
    assert not at(e, 120, 2.0).published.start_wait              # located in time: starts on the music's 1
    e = make()
    e.set_autobar("always")
    assert not at(e, 120, 1.0).published.start_wait              # only AT START waits


def test_a_new_lock_with_the_estimator_already_locked_counts_a_start_for_the_bar():
    e = make()
    e.downbeat.state = LOCKED
    first = at(e, 120, 1.0).published.bar_epoch
    at(e, None, 2.0)
    assert at(e, 120, 3.0).published.bar_epoch == first + 1
    e.fold = 2.0
    assert at(e, 120, 4.0).published.bar_epoch == first + 2


START_US = 1_000_000.0


class Clock:
    def __init__(self):
        self.us = START_US

    def __call__(self):
        return self.us


def controller():
    t = Clock()
    link = SimLink(now=t, bpm=120.0)
    cm = ClockMap()
    cm.report(0.0, START_US)
    return t, link, LinkController(link, cm)


def bar_snap(seq, count, mode="start", epoch=0, wait=False):
    """A locked bar whose decision at stream time 50 s asks for a one-beat move (Link's beat there is 100)."""
    bar = BarView(LOCKED, 0, 1.0, 1.0, seq, count, 50.0, None, 1)
    return Snapshot("steady", 120.0, (50.0, 0.5), bar, mode, epoch, wait)


def test_at_start_no_move_during_play_and_a_move_at_the_next_start():
    t, link, ctl = controller()
    t.us += 2_000_000
    ctl.tick(bar_snap(1, 1))
    assert ctl.bar_moves == 1 and link.playing()               # the session was stopped: the move, then the start
    t.us += 10_000_000                                         # the scheduled start is long reached
    ctl.tick(bar_snap(2, 2))
    assert ctl.bar_moves == 1                                  # mid-play: no move, the decision waits
    ctl.tick(Snapshot("stopped", None, None))                  # the start is over; the next lock starts anew
    t.us += 100_000
    ctl.tick(bar_snap(3, 3))
    assert ctl.bar_moves == 2


def test_at_start_a_new_start_for_the_bar_allows_one_move_mid_play():
    t, link, ctl = controller()
    t.us += 2_000_000
    ctl.tick(bar_snap(1, 0))                                   # no move needed; the session starts
    t.us += 10_000_000
    ctl.tick(bar_snap(2, 1))
    assert ctl.bar_moves == 0
    ctl.tick(bar_snap(3, 1, epoch=1))
    assert ctl.bar_moves == 1
    ctl.tick(bar_snap(4, 2, epoch=1))
    assert ctl.bar_moves == 1                                  # only once


def test_always_moves_mid_play():
    t, link, ctl = controller()
    t.us += 2_000_000
    ctl.tick(bar_snap(1, 0, mode="always"))
    t.us += 10_000_000
    ctl.tick(bar_snap(2, 1, mode="always"))
    assert ctl.bar_moves == 1


def test_the_start_waits_while_the_engine_says_so_and_lands_on_the_ones_bar_line():
    t, link, ctl = controller()
    t.us += 2_000_000
    ctl.tick(bar_snap(1, 0, wait=True))
    assert not link.playing()
    ctl.tick(bar_snap(1, 0, wait=True))
    assert not link.playing()
    ctl.tick(bar_snap(2, 1, wait=False))                       # locked in time: move first, then the start
    assert ctl.bar_moves == 1 and link.playing()
    t2, link2, ctl2 = controller()
    t2.us += 2_000_000
    ctl2.tick(Snapshot("steady", 120.0, (50.0, 0.5), None, "start", 0, False))   # not locked after 8 beats: starts as is
    assert link2.playing() and ctl2.bar_moves == 0
