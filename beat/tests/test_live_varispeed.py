"""The varispeed player: rendering, the source with a stand-in sound card, the web API, and that pitch moves the BPM
the tracker finds. No device is opened, nothing is heard."""
import json
import threading
import urllib.error
import urllib.request
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import pytest
import soundfile as sf

from live.audio import load_audio
from live.engine import Engine
from live.link_port import SimLink
from live.player import App, fold, load_library, serve
from live.controller import LinkController
from live.clockmap import ClockMap
from live.tracker import HOP, SAMPLE_RATE
from live.varispeed import VarispeedSource, render

GTZAN = Path(__file__).resolve().parent.parent / "data" / "gtzan" / "genres" / "disco" / "disco.00000.wav"


def ramp(n):
    x = np.arange(n, dtype=np.float32)
    return np.stack([x, -x], axis=1)


def test_render_rate_one_is_the_audio():
    block, nxt, ended = render(ramp(100), 10.0, 1.0, 5)
    assert np.allclose(block[:, 0], [10, 11, 12, 13, 14]) and np.allclose(block[:, 1], -block[:, 0])
    assert nxt == 15.0 and not ended


def test_render_interpolates_between_frames():
    block, nxt, _ = render(ramp(100), 0.0, 1.5, 4)
    assert np.allclose(block[:, 0], [0, 1.5, 3, 4.5]) and nxt == 6.0


def test_render_past_the_end_is_silent_and_ended():
    block, _, ended = render(ramp(10), 7.0, 1.0, 5)
    assert np.allclose(block[:2, 0], [7, 8]) and np.all(block[2:] == 0) and ended


def test_fold_into_the_tracker_range():
    assert fold(64.0, 90, 180) == 128.0 and fold(256.0, 90, 180) == 128.0 and fold(128.0, 90, 180) == 128.0


def test_library_keeps_tracks_that_exist(tmp_path):
    (tmp_path / "Sub").mkdir()
    (tmp_path / "Sub" / "a.mp3").write_bytes(b"")
    index = tmp_path / "index.json"
    index.write_text(json.dumps([{"file": "Sub/a.mp3", "bpm": 128.0}, {"file": "gone.mp3", "bpm": 120.0}]))
    assert load_library(index, tmp_path) == [{"path": str(tmp_path / "Sub" / "a.mp3"), "name": "Sub/a.mp3",
                                              "bpm": 128.0}]
    assert load_library(tmp_path / "missing.json", tmp_path) == []


class FakeStream:
    latency = 0.01

    def __init__(self, callback, **_):
        self.callback = callback

    def pull(self, frames=HOP):
        out = np.zeros((frames, 2), dtype=np.float32)
        self.callback(out, frames, SimpleNamespace(currentTime=0.0, outputBufferDacTime=0.01), None)
        return out

    def close(self):
        pass


@pytest.fixture
def source(monkeypatch, tmp_path):
    import sounddevice as sd
    monkeypatch.setattr(sd, "OutputStream", lambda **kw: FakeStream(**kw))
    clip = tmp_path / "clip.wav"
    sf.write(clip, np.linspace(-0.5, 0.5, SAMPLE_RATE, dtype=np.float32), SAMPLE_RATE)
    src = VarispeedSource(now_us=lambda: 1e6)
    src.clip = str(clip)
    return src


def test_source_plays_silence_until_play_and_feeds_what_is_heard(source):
    source.load(source.clip)
    assert np.all(source._stream.pull() == 0)               # loaded, paused
    source.set_playing(True)
    out = source._stream.pull()
    assert np.any(out != 0)
    chunks = [source._queue.get_nowait() for _ in range(2)]
    assert [c.start_s for c in chunks] == [0.0, HOP / SAMPLE_RATE]     # the output's own time runs on
    assert np.allclose(chunks[1].samples, out.mean(axis=1))             # the tracker gets exactly the output
    assert chunks[1].link_us == pytest.approx(1e6 + 1e4)


def test_volume_scales_what_is_heard_not_what_is_tracked(source):
    source.load(source.clip)
    source.set_playing(True)
    full = source._stream.pull()
    source.seek(0.0)
    source.set_volume(0.25)
    quiet = source._stream.pull()
    assert np.allclose(quiet, full * 0.25)
    chunks = [source._queue.get_nowait() for _ in range(2)]
    assert np.allclose(chunks[0].samples, chunks[1].samples)            # the tracker got the same full signal
    with pytest.raises(ValueError):
        source.set_volume(1.5)


def test_source_rate_seek_and_end(source):
    source.load(source.clip)
    source.set_rate(1.5)
    source.set_playing(True)
    source._stream.pull()
    assert source.state().position_s == pytest.approx(1.5 * HOP / SAMPLE_RATE)
    source.seek(0.999)
    source._stream.pull()
    assert not source.state().playing                        # ran off the end: stops
    source.set_playing(True)
    assert source.state().position_s == 0.0                  # play at the end starts over
    with pytest.raises(ValueError):
        source.set_rate(3.0)


def test_web_api(source):
    link = SimLink(bpm=120.0)
    clock = ClockMap()
    app = App(source, Engine(90, 180), link, LinkController(link, clock), clock,
              [{"path": source.clip, "name": "clip.wav", "bpm": 64.0}], (90.0, 180.0))
    server = serve(0, app)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{server.server_address[1]}"

    def post(name, body):
        req = urllib.request.Request(base + "/api/" + name, json.dumps(body).encode(), method="POST",
                                     headers={"Content-Type": "application/json"})
        return json.loads(urllib.request.urlopen(req).read())
    try:
        assert b"Beat Player" in urllib.request.urlopen(base + "/").read()
        post("load", {"path": source.clip})
        s = post("rate", {"rate": 1.04})
        assert s["traktor_bpm"] == 64.0 and s["expected_bpm"] == pytest.approx(133.12)   # folded, times pitch
        assert post("play", {"playing": True})["playing"]
        assert post("volume", {"volume": 0.3})["volume"] == 0.3
        assert post("threshold", {"min_confidence": 0.7})["min_confidence"] == 0.7
        assert post("offset", {"ms": -3})["offset_ms"] == -3.0 and clock.user_offset_us == -3000.0
        s = post("resync", {})
        assert s["beat_from"] is None and s["beat"] is None          # nothing heard yet: no beat for the LED
        for bad in (("rate", {"rate": 9}), ("volume", {"volume": 2}), ("threshold", {"min_confidence": -1}),
                    ("offset", {"ms": 500}), ("load", {"path": "/no/such.mp3"}), ("nope", {})):
            with pytest.raises(urllib.error.HTTPError):
                post(*bad)
    finally:
        server.shutdown()


def make_app(source, bpm=120.0):
    link = SimLink(bpm=bpm)
    clock = ClockMap()
    clock.report(0.0, link.now_us())
    return App(source, Engine(90, 180), link, LinkController(link, clock), clock, [], (90.0, 180.0)), link


def test_input_level_is_the_rms_over_the_last_5_s_in_dbfs(source):
    app, _ = make_app(source)
    assert app.status()["level_db"] == -120.0                          # nothing fed yet: the meter's floor
    second = SAMPLE_RATE
    app.engine.feed(np.full(second, 0.1, dtype=np.float32))
    assert app.status()["level_db"] == pytest.approx(-20.0, abs=0.01)
    app.engine.feed(np.zeros(second, dtype=np.float32))                # half the window loud: half the energy
    assert app.status()["level_db"] == pytest.approx(-20.0 - 10 * np.log10(2), abs=0.01)
    for _ in range(5):                                                 # the loud second has left the 5 s window
        app.engine.feed(np.zeros(second, dtype=np.float32))
    assert app.status()["level_db"] == -120.0


def test_input_fast_level_follows_the_last_0_3_s(source):
    app, _ = make_app(source)
    assert app.status()["level_fast_db"] == -120.0
    block = HOP                                                        # the audio thread feeds 10 ms blocks
    for _ in range(100):                                               # a loud second
        app.engine.feed(np.full(block, 0.1, dtype=np.float32))
    for _ in range(40):                                                # then 0.4 s quiet: longer than the fast span
        app.engine.feed(np.full(block, 0.01, dtype=np.float32))
    assert app.status()["level_fast_db"] == pytest.approx(-40.0, abs=0.01)          # only the quiet part
    assert app.status()["level_db"] > -25.0                                        # the 5 s average still holds the loud second


def test_hold_stops_the_published_snapshot_until_released(source):
    app, _ = make_app(source)
    assert app.status()["hold"] is False
    app.command("hold", {"hold": True})
    assert app.status()["hold"] is True and app.engine.snapshot().mode == "stopped"
    app.command("hold", {"hold": False})
    assert app.status()["hold"] is False
    for bad in ({"hold": "yes"}, {"hold": 1}, {}):
        with pytest.raises((ValueError, KeyError)):
            app.command("hold", bad)


def test_mode_switch_and_bad_values(source):
    app, _ = make_app(source)
    assert app.status()["source"] == "audio"
    app.command("mode", {"mode": "link"})
    assert app.status()["source"] == "link" and app.controller.passive
    app.command("mode", {"mode": "audio"})
    assert app.status()["source"] == "audio" and not app.controller.passive
    for bad in ({"mode": "midi"}, {"mode": None}):
        with pytest.raises(ValueError):
            app.command("mode", bad)


def test_link_mode_leaves_the_session_alone_while_audio_mode_leads_it(source):
    from live.controller import Snapshot
    app, link = make_app(source, bpm=120.0)
    snap = Snapshot("steady", 150.0, (50.0, 0.4))                      # the engine hears 150 BPM
    app.command("mode", {"mode": "link"})
    app.controller.tick(snap)
    assert link.calls == [] and link.tempo() == 120.0 and not link.playing()
    s = app.status()                                                   # link mode: Link's own beat, even stopped (the
    assert s["beat_from"] == "link" and s["beat"] is not None and not s["link_playing"]   # clocks may run on it)
    app.command("mode", {"mode": "audio"})
    app.controller.tick(snap)
    assert link.tempo() == 150.0                                       # audio mode pushes the tracker's tempo


def test_tempo_command_is_for_link_mode_and_inside_the_range(source):
    app, link = make_app(source, bpm=120.0)
    with pytest.raises(ValueError):
        app.command("tempo", {"bpm": 130})                              # audio mode: the tracker sets it
    app.command("mode", {"mode": "link"})
    app.command("tempo", {"bpm": 131})
    assert link.tempo() == 131.0 and app.status()["link_bpm"] == 131.0
    for bad in (89, 181):
        with pytest.raises(ValueError):
            app.command("tempo", {"bpm": bad})


@pytest.mark.skipif(not GTZAN.exists(), reason="GTZAN not downloaded")
def test_pitch_moves_the_tracked_bpm_by_the_rate():
    audio = load_audio(GTZAN)          # 22.05 kHz mono -> 44.1 kHz stereo
    bpms = {}
    for rate in (1.0, 1.06):
        engine, pos = Engine(90, 180), 0.0
        for _ in range(int(25 * SAMPLE_RATE / HOP)):
            block, pos, _ = render(audio, pos, rate, HOP)
            engine.feed(block.mean(axis=1))
        bpms[rate] = engine.snapshot().bpm
    assert bpms[1.0] and bpms[1.06]
    assert bpms[1.06] / bpms[1.0] == pytest.approx(1.06, rel=0.004)


def test_shift_command_moves_the_link_bar_and_rejects_bad_values(source):
    app, link = make_app(source)
    before = link.beat_at(link.now_us())
    app.command("shift", {"beats": -1})
    assert link.beat_at(link.now_us()) == pytest.approx(before - 1, abs=0.01)
    app.command("shift", {"beats": 0.5})                              # halves: the off-beat fix
    assert link.beat_at(link.now_us()) == pytest.approx(before - 0.5, abs=0.01)
    for bad in ({"beats": 0}, {"beats": 5}, {"beats": 0.3}, {"beats": "x"}, {}):
        with pytest.raises((ValueError, KeyError)):
            app.command("shift", bad)


def test_transport_command_leads_the_session_in_link_mode_only(source):
    app, link = make_app(source)
    with pytest.raises(ValueError):                                    # AUDIO mode: the tracker decides (Stop = hold)
        app.command("transport", {"playing": True})
    app.command("mode", {"mode": "link"})
    app.command("transport", {"playing": True})
    assert link.playing()
    app.command("transport", {"playing": False})
    assert not link.playing()
    with pytest.raises(ValueError):
        app.command("transport", {"playing": "yes"})


def test_fold_command_doubles_or_halves_the_tempo_link_gets(source):
    app, _ = make_app(source)
    assert app.status()["fold"] == 1.0
    assert app.command("fold", {"factor": 2}) is None and app.engine.fold == 2.0
    app.command("fold", {"factor": 0.5})
    assert app.status()["fold"] == 0.5
    for bad in ({"factor": 3}, {"factor": 8}, {"factor": 0}, {"factor": "x"}, {}):
        with pytest.raises((ValueError, KeyError)):
            app.command("fold", bad)
