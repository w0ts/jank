"""Media keys: live/mediakeys.py (MPRemoteCommandCenter / MPNowPlayingInfoCenter, stood in by a fake framework) and the
player's side of them (App.media_command: play, pause, toggle, next, previous; the page's list and "then" choice)."""
import random
from types import SimpleNamespace

import pytest

from live.controller import LinkController
from live.clockmap import ClockMap
from live.engine import Engine
from live.mediakeys import MediaKeys
from live.player import App
from live.link_port import SimLink


class FakeCommand:
    def __init__(self):
        self.handler, self.enabled = None, None

    def addTargetWithHandler_(self, handler):
        self.handler = handler

    def setEnabled_(self, on):
        self.enabled = on


class FakeFramework:
    MPRemoteCommandHandlerStatusSuccess = 0
    MPRemoteCommandHandlerStatusCommandFailed = 200
    MPMediaItemPropertyTitle, MPMediaItemPropertyArtist = "title", "artist"
    MPMediaItemPropertyPlaybackDuration = "duration"
    MPNowPlayingInfoPropertyElapsedPlaybackTime, MPNowPlayingInfoPropertyPlaybackRate = "elapsed", "rate"
    MPNowPlayingPlaybackStatePlaying, MPNowPlayingPlaybackStatePaused, MPNowPlayingPlaybackStateStopped = 1, 2, 3

    def __init__(self):
        self.cmds = {k: FakeCommand() for k in ("play", "pause", "toggle", "next", "previous")}
        self.info, self.state = "unset", None
        fw = self

        class Center:
            def playCommand(self): return fw.cmds["play"]
            def pauseCommand(self): return fw.cmds["pause"]
            def togglePlayPauseCommand(self): return fw.cmds["toggle"]
            def nextTrackCommand(self): return fw.cmds["next"]
            def previousTrackCommand(self): return fw.cmds["previous"]

        class Now:
            def setNowPlayingInfo_(self, info): fw.info = info
            def setPlaybackState_(self, st): fw.state = st

        self.MPRemoteCommandCenter = SimpleNamespace(sharedCommandCenter=lambda: Center())
        self.MPNowPlayingInfoCenter = SimpleNamespace(defaultCenter=lambda: Now())


def test_every_media_key_runs_its_action_and_reports_success():
    fw, calls = FakeFramework(), []
    mk = MediaKeys({k: (lambda k=k: calls.append(k)) for k in ("play", "pause", "toggle", "next", "previous")},
                   info=lambda: None, framework=fw)
    assert mk.start() is True
    for name, cmd in fw.cmds.items():
        assert cmd.enabled is True
        assert cmd.handler(object()) == 0
    assert calls == ["play", "pause", "toggle", "next", "previous"]


def test_an_action_that_fails_makes_the_key_report_failure_and_never_raises():
    fw = FakeFramework()

    def boom():
        raise RuntimeError("no")

    MediaKeys({"toggle": boom}, info=lambda: None, framework=fw).start()
    assert fw.cmds["toggle"].handler(object()) == 200


def test_a_key_without_an_action_is_left_disabled():
    fw = FakeFramework()
    MediaKeys({"toggle": lambda: None}, info=lambda: None, framework=fw).start()
    assert fw.cmds["next"].enabled is False and fw.cmds["toggle"].enabled is True


def test_the_now_playing_info_has_the_title_the_bpm_and_the_time():
    fw = FakeFramework()
    info = {"path": "/m/Artist - Song.mp3", "bpm": 127.96, "position_s": 12.5, "duration_s": 300.0, "playing": True, "rate": 1.04}
    mk = MediaKeys({}, info=lambda: info, framework=fw)
    mk.update()
    assert fw.info == {"title": "Artist - Song", "artist": "128.0 BPM", "duration": 300.0, "elapsed": 12.5, "rate": 1.04}
    assert fw.state == 1
    info["playing"] = False
    mk.update()
    assert fw.info["rate"] == 0.0 and fw.state == 2


def test_no_track_clears_the_now_playing_info():
    fw = FakeFramework()
    MediaKeys({}, info=lambda: None, framework=fw).update()
    assert fw.info is None and fw.state == 3


def test_without_the_framework_start_says_so_and_does_nothing():
    mk = MediaKeys({}, info=lambda: None, framework=False)
    assert mk.start() is False and mk.available is False


# ---- the player's side ----

class FakeSource:
    def __init__(self, path=None, playing=False, pos=0.0):
        self.path, self.playing, self.pos, self.loaded, self.seeks = path, playing, pos, [], []

    def state(self):
        return SimpleNamespace(path=self.path, position_s=self.pos, duration_s=200.0, playing=self.playing, rate=1.0, volume=1.0)

    def set_playing(self, on):
        self.playing = on

    def load(self, path):
        self.loaded.append(path)
        self.path, self.pos = path, 0.0

    def seek(self, s):
        self.seeks.append(s)
        self.pos = s


def app_with(source, paths):
    link = SimLink(now=lambda: 0, bpm=120.0)
    clock = ClockMap()
    lib = [{"path": p, "bpm": 120.0, "name": p} for p in paths]
    return App(source, Engine(90, 180), link, LinkController(link, clock), clock, lib, (90.0, 180.0))


def test_play_pause_and_toggle_follow_the_source():
    src = FakeSource("/a.mp3")
    app = app_with(src, ["/a.mp3", "/b.mp3"])
    assert app.media_command("toggle") and src.playing is True
    assert app.media_command("toggle") and src.playing is False
    app.media_command("play")
    assert src.playing is True
    app.media_command("pause")
    assert src.playing is False


def test_next_goes_on_in_the_pages_list_and_keeps_playing():
    src = FakeSource("/a.mp3", playing=True)
    app = app_with(src, ["/a.mp3", "/b.mp3", "/c.mp3"])
    app.command("queue", {"paths": ["/c.mp3", "/a.mp3", "/b.mp3"], "mode": "next"})
    app.media_command("next")
    assert src.loaded == ["/b.mp3"] and src.playing is True


def test_next_without_a_page_list_follows_the_library_and_wraps():
    src = FakeSource("/c.mp3", playing=True)
    app = app_with(src, ["/a.mp3", "/b.mp3", "/c.mp3"])
    app.media_command("next")
    assert src.loaded == ["/a.mp3"]


def test_the_pages_random_choice_picks_another_track():
    src = FakeSource("/a.mp3", playing=True)
    app = app_with(src, ["/a.mp3", "/b.mp3", "/c.mp3"])
    app.rng = random.Random(1)
    app.command("queue", {"paths": ["/a.mp3", "/b.mp3", "/c.mp3"], "mode": "random"})
    for _ in range(20):
        app.media_command("next")
        assert src.loaded[-1] != (src.loaded[-2] if len(src.loaded) > 1 else "/a.mp3")


def test_previous_restarts_a_track_played_a_while_and_otherwise_goes_back():
    src = FakeSource("/b.mp3", playing=True, pos=40.0)
    app = app_with(src, ["/a.mp3", "/b.mp3", "/c.mp3"])
    app.media_command("previous")
    assert src.seeks == [0.0] and not src.loaded
    src.pos = 1.0
    app.media_command("previous")
    assert src.loaded == ["/a.mp3"]


def test_a_command_with_nothing_to_do_returns_false_not_an_error():
    src = FakeSource(None)
    app = app_with(src, [])
    assert app.media_command("next") is False
    with pytest.raises(KeyError):
        app.media_command("bogus")


def test_the_queue_command_refuses_nonsense():
    app = app_with(FakeSource(None), [])
    with pytest.raises(ValueError):
        app.command("queue", {"paths": "x", "mode": "next"})
    with pytest.raises(ValueError):
        app.command("queue", {"paths": [], "mode": "sideways"})
