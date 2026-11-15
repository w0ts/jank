"""The second output to the jank XIAO (live/xiao.py), with a stand-in stream: no device is opened, nothing is heard."""
from types import SimpleNamespace

import numpy as np
import pytest

from live.tracker import HOP
from live.xiao import DriftBuffer, XiaoOutput, find_device

DEVICES = [
    {"name": "MacBook Pro Microphone", "max_output_channels": 0},
    {"name": "MacBook Pro Speakers", "max_output_channels": 2},
    {"name": "jank", "max_output_channels": 1},
]


class FakeStream:
    """Records how it was opened; the test calls its callback the way the sound card would."""

    def __init__(self, **kw):
        self.kw, self.callback, self.latency, self.started, self.closed = kw, kw["callback"], 0.008, False, False

    def start(self):
        self.started = True

    def stop(self):
        self.started = False

    def close(self):
        self.closed = True

    def tick(self, frames=HOP):
        out = np.full((frames, 1), np.nan, dtype=np.float32)
        self.callback(out, frames, SimpleNamespace(outputBufferDacTime=0.0, currentTime=0.0), None)
        return out[:, 0]


def make(devices=DEVICES):
    made = []

    def factory(**kw):
        made.append(FakeStream(**kw))
        return made[-1]

    return XiaoOutput(stream_factory=factory, devices=devices), made


def test_finds_jank_by_name_among_outputs():
    assert find_device(DEVICES) == 2
    assert find_device([{"name": "Speakers", "max_output_channels": 2}, {"name": "jank (2)", "max_output_channels": 1}]) == 1
    assert find_device([{"name": "jank", "max_output_channels": 0}]) is None      # an input only does not count
    assert find_device(DEVICES[:2]) is None


def test_no_jank_device_is_a_clear_error():
    with pytest.raises(RuntimeError, match='no audio output named "jank"'):
        make(DEVICES[:2])


def test_opens_one_mono_channel_at_44k1_in_hops():
    x, made = make()
    kw = made[0].kw
    assert (kw["device"], kw["channels"], kw["samplerate"], kw["blocksize"], kw["dtype"]) == (2, 1, 44100, HOP, "float32")
    x.start()
    assert made[0].started
    x.close()
    assert made[0].closed


def test_the_xiao_gets_the_mid_of_what_the_speakers_play():
    x, made = make()
    left = np.linspace(-1, 1, HOP, dtype=np.float32)
    right = np.linspace(1, 0, HOP, dtype=np.float32)
    for _ in range(3):                                   # fill to the target level, then one block out
        x.push(np.stack([left, right], axis=1))
    out = made[0].tick()
    assert np.allclose(out, (left + right) / 2, atol=1e-6)
    assert x.status()["callbacks"] == 1 and x.status()["underruns"] == 0


def test_steady_clocks_never_slip():
    f = DriftBuffer()
    block = np.arange(HOP, dtype=np.float32)
    f.push(block)
    f.push(block)
    for _ in range(1000):                                # 10 s at the same rate on both sides
        f.push(block)
        assert np.array_equal(f.pull(HOP), block)
    s = f.status()
    assert (s.slips_dropped, s.slips_repeated, s.underruns, s.overflows) == (0, 0, 0, 0)


@pytest.mark.parametrize("ppm", [-2000, -50, 50, 2000])
def test_drift_is_absorbed_by_one_sample_slips(ppm):
    """The jank stream's clock runs `ppm` fast (+, it takes more) or slow (-) against the speakers', for 60 s. Real
    clocks are tens of ppm apart; 2000 ppm makes the slips happen within the test."""
    f = DriftBuffer()
    rng = np.random.default_rng(1)
    pushed = taken = 0
    owed = 0.0
    f.push(np.zeros(f.target, dtype=np.float32))          # the level after each pull starts at the target
    pushed += f.target
    for _ in range(6000):
        f.push(rng.standard_normal(HOP).astype(np.float32))
        pushed += HOP
        owed += HOP * ppm * 1e-6
        n = HOP + int(owed)
        owed -= int(owed)
        f.pull(n)
        taken += n
        assert f.target - f.slack - HOP <= f.status().level <= f.target + f.slack + HOP     # held near its target
    s = f.status()
    assert s.underruns == 0 and s.overflows == 0
    assert s.level == pushed - taken + s.slips_repeated - s.slips_dropped                  # every sample accounted for
    drift = abs(taken - (pushed - f.target))
    if abs(ppm) >= 2000:                                 # the FIFO's band (~1 block) cannot hold 5292 samples of drift
        slips = s.slips_repeated if ppm > 0 else s.slips_dropped
        assert slips > drift - 3 * HOP
        assert (s.slips_dropped if ppm > 0 else s.slips_repeated) == 0
    else:                                                # 50 ppm: 132 samples in 60 s; never more slips than that
        assert s.slips_dropped + s.slips_repeated <= drift + f.slack     # the hysteresis may re-centre by up to the slack


def jittery_run(f: DriftBuffer, seconds: float, ppm: float, seed: int, checkpoint_s: float | None = None):
    """The speakers push a block every 10 ms; jank's callback comes in bursts (0, 1 or 2 pulls between two pushes,
    1 on average, as CoreAudio's two devices with different buffer sizes do) and runs `ppm` fast. Returns the status at
    checkpoint_s (or None)."""
    rng = np.random.default_rng(seed)
    owed, debt, checkpoint = 0.0, 0, None
    f.push(np.zeros(f.target + HOP, dtype=np.float32))
    for k in range(int(seconds * 100)):
        if checkpoint_s is not None and k == int(checkpoint_s * 100):
            checkpoint = f.status()
        f.push(rng.standard_normal(HOP).astype(np.float32))
        debt += 1
        pulls = int(rng.integers(0, 3)) if debt < 3 else debt     # never more than 2 blocks behind
        for _ in range(min(pulls, debt)):
            owed += HOP * ppm * 1e-6
            n = HOP + int(owed)
            owed -= int(owed)
            f.pull(n)
            debt -= 1
    return checkpoint


@pytest.mark.parametrize("ppm", [-100, 0, 100])
def test_a_jittery_consumer_never_underruns(ppm):
    f = DriftBuffer()
    jittery_run(f, 120.0, ppm, seed=7)
    s = f.status()
    assert s.underruns == 0 and s.overflows == 0 and s.target == f.target
    assert abs(s.slips_dropped + s.slips_repeated - abs(ppm) * 1e-6 * 120 * 44100) <= 2 * HOP     # slips follow drift only


def test_jitter_alone_makes_no_slips():
    """After the level has settled (the first seconds trim the preload), jitter with no drift slips nothing."""
    f = DriftBuffer()
    settled = jittery_run(f, 70.0, 0.0, seed=3, checkpoint_s=10.0)
    s = f.status()
    assert s.slips_dropped + s.slips_repeated - settled.slips_dropped - settled.slips_repeated <= 2
    assert s.underruns == 0


def test_an_underrun_deepens_the_target():
    f = DriftBuffer()
    t0 = f.target
    f.pull(HOP)                                          # nothing there yet
    assert f.status().underruns == 1 and f.target == t0 + HOP
    for _ in range(20):
        f.pull(HOP)
    assert f.target == f.max_target


def test_an_empty_fifo_plays_silence_and_counts_it():
    f = DriftBuffer()
    f.push(np.ones(100, dtype=np.float32))
    out = f.pull(HOP)                                    # under the target: the first sample is repeated, then silence
    assert out.shape == (HOP,) and np.all(out[:101] == 1) and np.all(out[101:] == 0)
    assert f.status().underruns == 1 and f.status().slips_repeated == 1


def test_a_full_fifo_keeps_the_newest():
    f = DriftBuffer(target=HOP, slack=HOP // 2, capacity=4 * HOP, max_target=2 * HOP)
    for k in range(10):
        f.push(np.full(HOP, k, dtype=np.float32))
    s = f.status()
    assert s.level == 4 * HOP and s.overflows == 6
    out = f.pull(HOP)                                    # over the target: one sample of block 6 dropped first
    assert out[0] == 6.0 and out[-1] == 7.0 and f.status().slips_dropped == 1


def test_bad_sizes_are_refused():
    with pytest.raises(ValueError):
        DriftBuffer(target=0)
    with pytest.raises(ValueError):
        DriftBuffer(target=HOP, capacity=HOP)


def test_the_page_reads_the_xiao_reports():
    """player.html's parser (xiaoReport) on the firmware's frames, run by node (tests/xiao_report.js)."""
    import shutil
    import subprocess
    from pathlib import Path
    node = shutil.which("node")
    if node is None:
        pytest.fail("node not found: the page's XIAO report parser cannot be checked")
    here = Path(__file__).resolve().parent
    r = subprocess.run([node, str(here / "xiao_report.js"), str(here.parent / "live" / "player.html")],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "checks passed" in r.stdout


@pytest.mark.parametrize("script", ["xiao_m2b.js", "oled_buttons.js", "xiao_m2i.js"])
def test_the_page_follows_the_xiao_m2b_and_the_one_button(script):
    """player.html with the XIAO firmware M2b (the sound flag, outputs, home's meter, Bluetooth) and NO's clicks / the
    screenless unit, run by node."""
    import shutil
    import subprocess
    from pathlib import Path
    node = shutil.which("node")
    if node is None:
        pytest.fail("node not found: the page's helpers cannot be checked")
    here = Path(__file__).resolve().parent
    r = subprocess.run([node, str(here / script), str(here.parent / "live" / "player.html")],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "checks passed" in r.stdout


class FakeSpeakers:
    latency = 0.01

    def __init__(self, callback, **_):
        self.callback = callback

    def pull(self, frames=HOP):
        out = np.zeros((frames, 2), dtype=np.float32)
        self.callback(out, frames, SimpleNamespace(currentTime=0.0, outputBufferDacTime=0.01), None)
        return out

    def close(self):
        pass


def test_the_speakers_feed_the_xiao_through_the_tap_at_full_level(monkeypatch, tmp_path):
    import sounddevice as sd
    import soundfile as sf
    from live.clockmap import ClockMap
    from live.controller import LinkController
    from live.engine import Engine
    from live.link_port import SimLink
    from live.player import App
    from live.tracker import SAMPLE_RATE
    from live.varispeed import VarispeedSource
    monkeypatch.setattr(sd, "OutputStream", lambda **kw: FakeSpeakers(**kw))
    clip = tmp_path / "clip.wav"
    sf.write(clip, np.linspace(-0.5, 0.5, SAMPLE_RATE, dtype=np.float32), SAMPLE_RATE)
    src = VarispeedSource(now_us=lambda: 1e6)
    x, made = make()
    src.tap = x.push
    src.load(str(clip))
    src.set_volume(0.25)
    src.set_playing(True)
    heard = [src._stream.pull() for _ in range(3)]
    sent = made[0].tick()
    assert np.allclose(sent, heard[0].mean(axis=1) / 0.25, atol=1e-6)     # the full level, as a line input gets it
    link = SimLink()
    clock = ClockMap()
    app = App(src, Engine(90, 180), link, LinkController(link, clock), clock, [], (90.0, 180.0))
    assert app.status()["xiao"] is None
    app.xiao = x
    st = app.status()["xiao"]
    assert st["device"] == "jank" and st["callbacks"] == 1 and st["underruns"] == 0
