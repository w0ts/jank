"""Listen to a tab: the page posts a browser tab's audio to /api/tab_audio and it becomes the tracker's input.
No sound card (a stand-in output stream) and no browser; the page's own helpers are checked by tests/tab_listen.js."""
import json
import shutil
import subprocess
import threading
import urllib.error
import urllib.request
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import pytest

from live.clockmap import ClockMap
from live.controller import LinkController
from live.engine import Engine
from live.link_port import SimLink
from live.player import App, serve
from live.tracker import HOP, SAMPLE_RATE
from live.varispeed import VarispeedSource


class Clock:
    us = 5_000_000.0

    def __call__(self):
        return self.us


class IdleStream:
    latency = 0.01

    def __init__(self, callback, **_):
        self.callback = callback

    def close(self):
        pass


@pytest.fixture
def rig(monkeypatch):
    import sounddevice as sd
    monkeypatch.setattr(sd, "OutputStream", IdleStream)
    clock = Clock()
    link = SimLink(now=clock, bpm=120.0)
    source = VarispeedSource(clock)
    cmap = ClockMap()
    app = App(source, Engine(90, 180), link, LinkController(link, cmap), cmap, [], (90.0, 180.0))
    server = serve(0, app)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    yield SimpleNamespace(app=app, source=source, clock=clock, url=f"http://127.0.0.1:{server.server_address[1]}")
    server.shutdown()
    server.server_close()


def post(rig, samples_bytes, rate=SAMPLE_RATE):
    req = urllib.request.Request(f"{rig.url}/api/tab_audio?rate={rate}", data=samples_bytes, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())


def drain(source):
    out = []
    while not source._queue.empty():
        out.append(source._queue.get())
    return out


def test_a_posted_block_reaches_the_tracker_queue_in_hops_with_link_times(rig):
    n = HOP * 3 + 100
    tone = (0.5 * np.sin(np.arange(n) * 0.05)).astype("<f4")
    status, body = post(rig, tone.tobytes())
    assert status == 200 and body == {"ok": True}
    chunks = drain(rig.source)
    assert [len(c.samples) for c in chunks] == [HOP, HOP, HOP, 100]
    assert np.allclose(np.concatenate([c.samples for c in chunks]), tone)
    starts = [c.start_s for c in chunks]
    assert starts == pytest.approx([i * HOP / SAMPLE_RATE for i in range(4)])         # one continuous stream clock
    assert chunks[-1].link_us + 100 / SAMPLE_RATE * 1e6 == pytest.approx(rig.clock.us)   # the block ends 'now'
    assert chunks[1].link_us - chunks[0].link_us == pytest.approx(HOP / SAMPLE_RATE * 1e6)


def test_the_engine_hears_the_tab(rig):
    n = SAMPLE_RATE
    tone = (0.5 * np.sin(np.arange(n) * 0.3)).astype("<f4")
    assert post(rig, tone.tobytes())[0] == 200
    for c in drain(rig.source):
        rig.app.engine.feed(c.samples)
    assert rig.app.engine.stream_time == pytest.approx(1.0, abs=0.05)
    assert rig.app.engine.level_db > -20.0
    assert not rig.app.engine.silent


def test_while_a_tab_plays_the_players_own_silence_stays_out_of_the_tracker(rig):
    out = np.zeros((HOP, 2), dtype=np.float32)
    info = SimpleNamespace(currentTime=0.0, outputBufferDacTime=0.0)
    rig.source._callback(out, HOP, info, None)
    assert len(drain(rig.source)) == 1 and not rig.app.status()["tab"]            # no tab: the player's block feeds it
    assert post(rig, np.zeros(HOP, dtype="<f4").tobytes())[0] == 200
    assert rig.app.status()["tab"] is True
    drain(rig.source)
    rig.source._callback(out, HOP, info, None)
    assert drain(rig.source) == []                                                  # tab active: nothing interleaved


def test_bad_posts_are_refused_with_a_reason(rig):
    ok = np.zeros(HOP, dtype="<f4").tobytes()
    assert "44100" in post(rig, ok, rate=48000)[1]["error"]
    assert "whole float32" in post(rig, ok[:-1])[1]["error"]
    assert "whole float32" in post(rig, b"")[1]["error"]
    nan = np.full(HOP, np.nan, dtype="<f4").tobytes()
    assert "finite" in post(rig, nan)[1]["error"]
    big = np.zeros(SAMPLE_RATE * 3, dtype="<f4").tobytes()
    assert "more than" in post(rig, big)[1]["error"]
    assert post(rig, b"x" * 3)[0] == 400
    assert drain(rig.source) == []


def test_the_page_helpers_for_listening_to_a_tab():
    """player.html's tab helpers (device pick by label, capture options, the start / stop / error states), run by node."""
    node = shutil.which("node")
    if node is None:
        pytest.fail("node not found: the page's tab helpers cannot be checked")
    here = Path(__file__).resolve().parent
    r = subprocess.run([node, str(here / "tab_listen.js"), str(here.parent / "live" / "player.html")],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "checks passed" in r.stdout
