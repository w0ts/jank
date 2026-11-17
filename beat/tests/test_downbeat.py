"""The downbeat estimator (live.downbeat): Link-beat counting, the move policy, a nudge winning, resets, causal timing,
and the controller's whole-beat moves."""
from pathlib import Path

import numpy as np
import pytest

from live.clockmap import ClockMap
from live.controller import LinkController, Snapshot
from live.downbeat import (BANDS, DELAY, FPS, LISTENING, LOCKED, MANUAL, NF, OFF, BarView, Downbeat, Driver, Model,
                           frame_of, link_shift, load_model)
from live.link_port import SimLink

P = 0.5                       # 120 BPM
TB = 2.0                      # the grid's first beat
MODEL = Model(np.zeros(NF * 4), 16.0, 2.0, 3.0, 8)
GTZAN = Path(__file__).resolve().parent.parent / "data" / "gtzan" / "genres" / "disco" / "disco.00005.wav"


def bands(f: int, accent_every: int = 0) -> np.ndarray:
    """Quiet noise-free bands, with a low-band hit on the frames right after every 4th beat when accent_every."""
    L = np.full(BANDS, 0.2)
    if accent_every and f >= frame_of(TB):
        k = round((f / FPS - TB) / P)
        if abs(f / FPS - (TB + k * P)) < 0.03 and k % accent_every == 0:
            L[:5] = 2.0
    return L


def play(db: Downbeat, seconds: float, start: int = 0, period: float = P, tb: float = TB, frame=bands) -> int:
    """Frames from `start`, and a tracked beat on the grid (tb, period) at each of its beats from tb on."""
    end = start + int(seconds * FPS)
    beat_frames = {frame_of(tb + n * period): tb + n * period for n in range(int((end / FPS - tb) / period) + 2)}
    for f in range(start, end):
        db.frame(f, frame(f))
        if f in beat_frames:
            db.set_grid(beat_frames[f], tb, period)
    return end


def decisions(db):
    return [r for r in db.log if r[0] == "d"]


def entries(db):
    return [r for r in db.log if r[0] == "x"]


def fresh(model=MODEL) -> Downbeat:
    db = Downbeat(model)
    db.log = []
    return db


def test_the_weights_load_and_match_the_estimator():
    m = load_model()
    assert m.w.shape == (NF * 4,) and (m.commit, m.switch, m.hold) == (2.0, 3.0, 8) and m.bars == 16.0


def test_one_count_per_grid_beat_at_its_frame():
    db = fresh()
    play(db, 10.0)
    d = decisions(db)
    assert [r[1] for r in d] == list(range(len(d)))                    # one count a beat
    assert [r[2] for r in d] == pytest.approx([TB + k * P for k in range(len(d))])
    assert len(d) == pytest.approx((10.0 - TB) / P, abs=1)


def test_a_phase_jump_of_the_grid_moves_the_count_by_the_gap_rounded():
    db = fresh()
    end = play(db, 6.0)
    # a new grid 0.3 periods later from 6.15 s on: the old grid still plays 6.0, the new one's first beat 6.15 is too
    # close to it, so the next is 6.65: 1.3 periods after, and Link's count goes up by round(1.3) = 1
    play(db, 3.0, start=end, tb=TB + 0.3 * P)
    d = decisions(db)
    assert [r[1] for r in d] == list(range(len(d)))
    gaps = np.diff([r[2] for r in d])
    assert np.sum(np.isclose(gaps, 1.3 * P)) == 1 and np.sum(np.isclose(gaps, P)) == len(gaps) - 1
    big = [r for r in d if r[2] > 6.5]
    assert all((r[2] - (TB + 0.3 * P)) / P == pytest.approx(round((r[2] - (TB + 0.3 * P)) / P)) for r in big)


def test_ready_after_every_position_has_complete_features():
    db = fresh()
    play(db, 12.0)
    d = decisions(db)
    first = next(i for i, r in enumerate(d) if r[3] is not None)
    assert first == DELAY + 4                                          # beats 0..3 enter at beats 4..7: ready at 8
    assert all(r[3] is None for r in d[:first])


def test_features_enter_after_the_decision_four_beats_later():
    db = fresh()
    play(db, 10.0)
    order = [(r[0], r[1]) for r in db.log]
    for kind, count in order:
        if kind == "x":
            assert order.index(("d", count + DELAY)) == order.index(("x", count)) - 1


def test_causal_a_beats_features_ignore_the_audio_after_beat_j_plus_4():
    def loud_late(cut):
        return lambda f: bands(f) + (3.0 if f > cut else 0.0)
    a, b = fresh(), fresh()
    play(a, 12.0)
    j = 3                                                              # beat 3 at TB + 1.5; beat 7 at TB + 3.5
    cut = frame_of(TB + (j + DELAY) * P) - 1                           # the last frame of beat j + 3's segment
    play(b, 12.0, frame=loud_late(cut))
    xa = next(r[3] for r in entries(a) if r[1] == j)
    xb = next(r[3] for r in entries(b) if r[1] == j)
    np.testing.assert_allclose(xa, xb)
    xa4 = next(r[3] for r in entries(a) if r[1] == j + 1)
    xb4 = next(r[3] for r in entries(b) if r[1] == j + 1)
    assert not np.allclose(xa4, xb4)                                   # the next beat's window reaches past the cut


def scripted(db: Downbeat, rows):
    """The policy fed scripted scores, one decision per row."""
    it = iter(rows)
    db.scores = lambda: np.asarray(next(it), float)
    for k in range(len(rows)):
        db._decide(k, TB + k * P)


def test_first_move_at_a_lead_of_commit_then_a_held_lead_of_switch():
    db = fresh()
    rows = [[1.0, 2.9, 0.0, 0.0],                                      # lead 1.9: still listening
            [0.0, 2.0, 0.0, 0.0]]                                      # lead 2.0: the first move, phase 1
    rows += [[0.0, 1.0, 4.05, 0.0]] * 7 + [[0.0, 1.0, 3.9, 0.0]]      # phase 2 leads by 3.05 for 7 beats, then 2.9
    rows += [[0.0, 1.0, 4.05, 0.0]] * 8                                # 8 in a row: the second move
    scripted(db, rows)
    states = [(r[4], r[5]) for r in decisions(db)]
    assert states[0] == (LISTENING, None) and states[1] == (LOCKED, 1)
    assert all(s == (LOCKED, 1) for s in states[2:17])
    assert states[17] == (LOCKED, 2) and db.view().moves == 2


def test_a_nudge_wins_until_auto_bar_is_picked_again_or_a_reset():
    db = fresh()
    strong = [[0.0, 5.0, 0.0, 0.0]] * 3
    scripted(db, strong[:1])
    assert db.view().state == LOCKED
    before = db.nudge()
    assert before.state == LOCKED and db.view().state == MANUAL
    db.log = []
    scripted(db, strong)
    assert db.view().state == MANUAL and db.view().moves == 1          # no move while MANUAL
    db.set_auto(True)                                                  # AUTO BAR picked again
    assert db.view().state == LISTENING
    scripted(db, strong[:1])
    assert db.view().state == LOCKED and db.view().moves == 2
    db.nudge()
    db.reset()
    assert db.view().state == LISTENING
    db.set_auto(False)
    scripted(db, strong)
    assert db.view().state == OFF
    db.set_auto(True)
    assert db.view().state == LISTENING


class FakeEstimator:
    def __init__(self):
        self.jumps, self.mode, self.tb, self.jump_next = 0, "steady", TB, False

    def update(self, t):
        if self.jump_next:                       # the next beat restarts the fit on a new tempo
            self.jumps += 1
            self.jump_next = False

    def grid(self):
        return (self.tb, P)

    def mode_at(self, now):
        return self.mode


def drive(drv: Driver, est, f0: int, seconds: float) -> int:
    n = int(seconds * FPS)
    beats = [TB + k * P for k in range(1000)]
    for f in range(f0, f0 + n, 10):
        found = [t for t in beats if f <= frame_of(t) < f + 10]
        drv.chunk(est, f, np.array([bands(g) for g in range(f, f + 10)]), found, (f + 9) / FPS, silent=False)
    return f0 + n


def test_a_stop_and_silence_reset_the_count_and_a_tempo_jump_when_asked():
    db = fresh()
    drv, est = Driver(db, jump_reset=True), FakeEstimator()
    drv.trace = []
    f = drive(drv, est, 0, 8.0)
    assert decisions(db)[-1][1] > 8
    est.jump_next = True                                               # ModeBpm restarts its fit on a new tempo
    f = drive(drv, est, f, 2.0)
    assert ("reset" in [e[0] for e in drv.trace]) and decisions(db)[-1][1] < 6
    db.log = []
    est.mode = "stopped"                                               # 30 s without a steady beat
    f = drive(drv, est, f, 2.0)
    assert not db.counting and len(decisions(db)) <= 1                 # (a beat on its frame before the tracked one)
    est.mode = "steady"
    db.log = []
    f = drive(drv, est, f, 3.0)
    assert decisions(db)[0][1] == 0                                    # counted afresh
    drv.chunk(est, f, np.array([bands(g) for g in range(f, f + 10)]), [], (f + 9) / FPS, silent=True)
    assert not db.counting


def test_a_tempo_jump_is_not_a_reset_by_default():
    db = fresh()
    drv, est = Driver(db), FakeEstimator()
    f = drive(drv, est, 0, 8.0)
    est.jump_next = True
    drive(drv, est, f, 2.0)
    counts = [r[1] for r in decisions(db)]
    assert counts == list(range(len(counts)))


def test_shadow_beats_keep_the_segments_before_the_first_counted_beat():
    """While ModeBpm is not steady its grid's beats are not counted, but the first counted beat has the bar before it."""
    db = fresh()
    end = 0
    beat_frames = {frame_of(TB + n * P): TB + n * P for n in range(40)}
    for f in range(0, 1000):
        db.frame(f, bands(f, accent_every=4))
        if f in beat_frames:
            db.set_grid(beat_frames[f], TB, P, running=beat_frames[f] >= TB + 4 * P)
        end = f
    first = entries(db)[0]
    assert first[1] == 0 and first[2] == pytest.approx(TB + 4 * P)     # the shadow beat on the frame counting starts at
    assert first[3][NF - 2] > 0                                        # nov_bar_all: a "before" bar existed
    assert end == 999


def test_link_shift_puts_link_s_bar_on_the_1():
    assert link_shift(7, 2, 100.0) == 1                                # beat 7 is position 1: Link 100 -> 101
    assert link_shift(7, 3, 100.0) == 0
    assert link_shift(7, 0, 100.0) == -1                               # position 3: Link 100 -> 99
    assert link_shift(7, 1, 100.0) == 2
    assert link_shift(7, 3, 100.5) == 0                                # half a beat off (a half nudge): the same beat
    assert link_shift(7, 3, 99.98) == 0


START_US = 1_000_000.0


class Clock:
    def __init__(self):
        self.us = START_US

    def __call__(self):
        return self.us


def bar_view(seq, count, t_beat, phi, state=LOCKED):
    return BarView(state, phi, 3.0, 1.0, seq, count, t_beat, (0.0, 0.0, 0.0, 0.0), 1)


def test_the_controller_moves_link_s_bar_whole_beats_once_a_decision():
    t = Clock()
    link = SimLink(now=t, bpm=120.0)                                   # beat 0 at START_US
    cm = ClockMap()
    cm.report(0.0, START_US)                                           # stream 0 s = Link beat 0; 50 s = beat 100
    ctl = LinkController(link, cm)
    grid = (50.0, P)
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(5, 7, 50.0, 2)))
    assert ctl.bar_moves == 1 and link.beat_at(cm.to_link_us(50.0)) == pytest.approx(101.0)   # bar position 1
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(5, 7, 50.0, 2)))   # the same decision: nothing more
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(6, 8, 50.5, 2)))   # the next beat, already right
    assert ctl.bar_moves == 1
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(7, 9, 51.0, 2, state=LISTENING)))
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(8, 9, 51.0, 1, state=MANUAL)))
    assert ctl.bar_moves == 1 and ctl.beat_offset == 1.0               # whole beats only


def test_an_auto_move_keeps_the_user_s_half_beat():
    t = Clock()
    link = SimLink(now=t, bpm=120.0)
    cm = ClockMap()
    cm.report(0.0, START_US)
    ctl = LinkController(link, cm)
    ctl.shift_beats(0.5)                                               # the user's off-beat fix
    ctl.tick(Snapshot("steady", 120.0, (50.0, P)))                     # the phase follower takes it in
    ctl.tick(Snapshot("steady", 120.0, (50.0, P), bar_view(1, 7, 50.0, 0)))
    assert ctl.beat_offset % 1.0 == 0.5 and ctl.bar_moves == 1
    assert link_shift(7, 0, link.beat_at(cm.to_link_us(50.0))) == 0


def test_the_user_s_phase_rides_on_auto_bar():
    """jank M2h, the user: "beat is more phase. So zero is auto (again two layers)". The user's PHASE (NUDGE BEAT's total)
    is an offset on top of AUTO BAR's bar: AUTO BAR keeps placing the 1 (no MANUAL) with the offset kept, never undone;
    back to 0 is AUTO BAR's bar alone."""
    t = Clock()
    link = SimLink(now=t, bpm=120.0)
    cm = ClockMap()
    cm.report(0.0, START_US)
    ctl = LinkController(link, cm)
    grid = (50.0, P)
    at = cm.to_link_us(50.0)
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(5, 7, 50.0, 2)))
    assert link.beat_at(at) == pytest.approx(101.0)                    # AUTO BAR: bar position 1
    ctl.nudge_phase(1.0)                                               # PHASE +1
    assert ctl.phase == 1.0 and link.beat_at(at) == pytest.approx(102.0)
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(6, 7, 50.0, 2)))   # the next decision, the same 1: no move back
    assert link.beat_at(at) == pytest.approx(102.0) and ctl.bar_moves == 1
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(7, 7, 50.0, 3)))   # the music's 1 moves: followed, the PHASE on top
    assert ctl.bar_moves == 2 and link.beat_at(at) == pytest.approx(101.0)
    ctl.nudge_phase(-1.0)                                              # back to 0: AUTO
    assert ctl.phase == 0.0 and link.beat_at(at) == pytest.approx(100.0)
    ctl.tick(Snapshot("steady", 120.0, grid, bar_view(8, 7, 50.0, 3)))
    assert ctl.bar_moves == 2 and link.beat_at(at) == pytest.approx(100.0)   # (7 - 3) % 4 = 0: in place


def test_the_device_screen_shows_the_bar():
    """player.html's OLED module: the marks under the beat boxes and the AUTO BAR menu item (tests/oled_bar.js, node)."""
    import shutil
    import subprocess
    node = shutil.which("node")
    if node is None:
        pytest.fail("node not found: the device screen cannot be checked")
    here = Path(__file__).resolve().parent
    r = subprocess.run([node, str(here / "oled_bar.js"), str(here.parent / "live" / "player.html")],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "checks passed" in r.stdout


@pytest.mark.skipif(not GTZAN.exists(), reason="GTZAN not downloaded")
def test_the_engine_locks_a_bar_on_music():
    from live.audio import load_audio, to_mono
    from live.engine import Engine
    from live.tracker import HOP, SAMPLE_RATE
    e = Engine(90, 180)
    music = to_mono(load_audio(GTZAN))[:25 * SAMPLE_RATE]
    for i in range(0, len(music), 10 * HOP):
        e.feed(music[i:i + 10 * HOP])
    bar = e.status().bar
    assert bar.state == LOCKED and bar.phi is not None and bar.seq > 20 and bar.scores is not None
    assert e.status().published.bar == bar
