import pytest

from live.clockmap import ClockMap
from live.controller import LinkController, Snapshot
from live.link_port import SimLink

START_US = 1_000_000.0


class Clock:
    def __init__(self):
        self.us = START_US

    def __call__(self):
        return self.us


def make(offset_ms=0.0):
    t = Clock()
    link = SimLink(now=t, bpm=120.0)          # beat 0 at START_US, a beat every 500 ms
    cm = ClockMap(offset_ms)
    cm.report(0.0, START_US)                  # stream second 0 is heard at START_US
    return t, link, cm, LinkController(link, cm)


def snap(phase_s=0.0, mode="steady", bpm=120.0):
    """A grid with a beat every 0.5 s of stream time, shifted by phase_s."""
    return Snapshot(mode, bpm, (50.0 + phase_s, 0.5))


def calls(link, name):
    return [c for c in link.calls if c[0] == name]


def test_tempo_inside_deadband_is_left_alone():
    t, link, cm, ctl = make()
    ctl.tick(snap(bpm=120.01))
    assert not calls(link, "set_tempo")
    ctl.tick(snap(bpm=120.05))
    assert calls(link, "set_tempo") == [("set_tempo", 120.05)]


def test_phase_error_above_tolerance_is_forced_once():
    t, link, cm, ctl = make()
    t.us += 2_000_000
    ctl.tick(snap(phase_s=0.020))
    assert len(calls(link, "force_beat")) == 1
    ctl.tick(snap(phase_s=0.020))                 # aligned now: no second force
    assert len(calls(link, "force_beat")) == 1
    assert abs(ctl.phase_err_ms) <= 5.0


def test_phase_error_under_tolerance_is_not_touched():
    t, link, cm, ctl = make()
    t.us += 2_000_000
    ctl.tick(snap(phase_s=0.003))
    assert not calls(link, "force_beat")
    assert abs(ctl.phase_err_ms) == pytest.approx(3.0, abs=0.01)


def test_offset_moves_the_link_grid():
    t, link, cm, ctl = make(offset_ms=40.0)
    t.us += 2_000_000
    ctl.tick(snap())
    forced = calls(link, "force_beat")
    assert len(forced) == 1
    assert (link.time_at_beat(forced[0][1]) - START_US) % 500_000 == pytest.approx(40_000, abs=1)


def test_stop_then_start_on_next_bar():
    t, link, cm, ctl = make()
    t.us += 2_000_000
    ctl.tick(snap())
    starts = calls(link, "set_playing")
    assert len(starts) == 1 and starts[0][1] is True and starts[0][3] == 0.0
    assert starts[0][2] > t.us and link.beat_at(starts[0][2]) % 4 == pytest.approx(0.0, abs=1e-6)
    ctl.tick(snap())
    assert len(calls(link, "set_playing")) == 1           # no repeated start
    t.us += 100_000
    ctl.tick(Snapshot("stopped", None, None))
    assert calls(link, "set_playing")[-1][1] is False
    t.us += 100_000
    ctl.tick(Snapshot("stopped", None, None))
    assert len(calls(link, "set_playing")) == 2           # stop sent once
    t.us += 100_000
    ctl.tick(snap())
    assert calls(link, "set_playing")[-1][1] is True      # running again: start on a bar
    assert len(calls(link, "set_playing")) == 3


def test_nothing_happens_before_the_clock_is_mapped():
    t = Clock()
    link = SimLink(now=t)
    LinkController(link, ClockMap()).tick(Snapshot("steady", 130.0, (0.0, 0.46)))
    assert link.calls == []


def test_shift_beats_moves_the_bar_by_one_beat_and_keeps_the_phase():
    t, link, cm, ctl = make()
    before = link.beat_at(t())
    ctl.shift_beats(1)
    assert link.beat_at(t()) == pytest.approx(before + 1)          # the count jumps a beat: the bar line moves
    t.us += 250_000
    assert link.beat_at(t()) % 1 == pytest.approx(0.5)               # the phase inside the beat is untouched
    ctl.shift_beats(-1)
    assert link.beat_at(t()) == pytest.approx(before + 0.5)


def test_shift_beats_refused_while_following_the_session():
    _, _, _, ctl = make()
    ctl.passive = True
    with pytest.raises(ValueError):
        ctl.shift_beats(1)


def test_following_the_session_the_device_can_lead_its_transport():
    t, link, cm, ctl = make()
    ctl.passive = True                                                  # LINK mode, transport FOLLOW + LEAD
    t.us += 2_000_000
    ctl.set_session_playing(True)
    start = calls(link, "set_playing")[-1]
    assert start[1] is True and start[2] > t.us and link.beat_at(start[2]) % 4 == pytest.approx(0.0, abs=1e-6)
    t.us += 100_000
    ctl.set_session_playing(False)
    assert calls(link, "set_playing")[-1][1] is False and not link.playing()


def test_half_beat_nudge_moves_link_onto_the_off_beats_and_the_phase_follower_keeps_it():
    from live.controller import PHASE_HOLDOFF_S
    t, link, cm, ctl = make()
    t.us = START_US + 60_000_000                                       # stream second 60: the grid's beats are on .0 / .5 s
    for _ in range(3):
        ctl.tick(snap())
    on_beat = lambda: link.beat_at(cm.to_link_us(60.0))               # Link's count at a tracked beat
    assert on_beat() % 1 == pytest.approx(0.0, abs=1e-6)
    ctl.shift_beats(0.5)                                               # the device's NUDGE BEAT: half a beat
    assert on_beat() % 1 == pytest.approx(0.5, abs=1e-6)               # Link's beats now fall between the tracked ones
    for _ in range(5):                                                 # long after the hold-off: the follower keeps it
        t.us += (PHASE_HOLDOFF_S + 0.5) * 1e6
        ctl.tick(snap())
    assert on_beat() % 1 == pytest.approx(0.5, abs=1e-6)
    ctl.shift_beats(0.5)                                               # two halves = the bar one beat on
    assert on_beat() % 1 == pytest.approx(0.0, abs=1e-6)


def test_the_beat_nudge_stays_relative_to_the_tracked_phase_when_it_moves():
    from live.controller import PHASE_HOLDOFF_S
    t, link, cm, ctl = make()
    t.us = START_US + 60_000_000
    ctl.tick(snap())
    ctl.shift_beats(0.5)
    t.us += (PHASE_HOLDOFF_S + 0.5) * 1e6
    for _ in range(3):                                                 # the tracker re-locks 0.1 s later
        ctl.tick(snap(phase_s=0.1))
        t.us += (PHASE_HOLDOFF_S + 0.5) * 1e6
    assert link.beat_at(cm.to_link_us(60.1)) % 1 == pytest.approx(0.5, abs=1e-3)    # still half a beat off the new phase
