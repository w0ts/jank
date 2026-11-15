"""The play path end to end with a stand-in for the sound card: no device is opened, nothing is heard."""
import io
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import pytest
import soundfile as sf

from live.clockmap import ClockMap
from live.controller import LinkController
from live.engine import Engine
from live.link_port import SimLink
from live.runner import run
from live.sources import HOP, PlaySource

GTZAN = Path(__file__).resolve().parent.parent / "data" / "gtzan" / "genres" / "disco" / "disco.00000.wav"
OUT_LATENCY_S = 0.012


class FakeOutputStream:
    """Calls the callback as fast as it can, the way the sound card would every 10 ms."""

    def __init__(self, callback, clock, **_):
        self.callback, self.clock, self.latency = callback, clock, OUT_LATENCY_S

    def start(self):
        import sounddevice as sd
        while True:
            out = np.zeros((HOP, 2), dtype=np.float32)
            info = SimpleNamespace(currentTime=0.0, outputBufferDacTime=OUT_LATENCY_S)
            try:
                self.callback(out, HOP, info, None)
            except sd.CallbackStop:
                return
            self.clock.us += 10_000.0

    def close(self):
        pass


@pytest.mark.skipif(not GTZAN.exists(), reason="GTZAN not downloaded")
def test_play_feeds_the_tracker_and_link_follows(monkeypatch, tmp_path):
    import sounddevice as sd
    from eval.trackers import _signal
    clip = tmp_path / "clip.wav"
    sf.write(clip, np.asarray(_signal(GTZAN), dtype=np.float32)[:44100 * 25], 44100)

    class Clock:
        us = 5_000_000.0
        def __call__(self):
            return self.us
    clock = Clock()
    link = SimLink(now=clock, bpm=120.0)
    monkeypatch.setattr(sd, "OutputStream", lambda **kw: FakeOutputStream(clock=clock, **kw))
    source = PlaySource(clip, now_us=clock)
    cmap = ClockMap()
    ctl = LinkController(link, cmap)
    out = io.StringIO()

    # control ticks run in the real runner's thread; here the clock is simulated, so tick inline instead
    seen = []
    real_feed = Engine.feed

    def feed_and_tick(self, samples):
        beats = real_feed(self, samples)
        if cmap.ready:
            clock.us = cmap.to_link_us(self.tracker.time)     # the moment this audio is heard
        ctl.tick(self.snapshot())
        seen.append(ctl.phase_err_ms)
        return beats
    monkeypatch.setattr(Engine, "feed", feed_and_tick)
    run(source, Engine(90, 180), cmap, link, None, out=out)

    lines = out.getvalue().splitlines()
    assert any("steady" in line for line in lines)
    bpm = float(next(line for line in lines if "steady" in line).split()[3])
    assert 100 < bpm < 130
    assert 100 < link.tempo() < 130 and link.tempo() != 120.0       # Link got the estimator's tempo
    assert any(c[0] == "set_playing" and c[1] for c in link.calls)  # ... and started on a bar
    assert abs([e for e in seen if e is not None][-1]) <= 5.0       # ... with the phase in tolerance
