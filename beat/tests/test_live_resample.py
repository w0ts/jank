"""live/xiao.py's ResampleBuffer: the jank FIFO that follows a clock drift by resampling (a ratio near 1) instead of by
repeating or dropping one sample per block, which at its limit (0.23 %) buzzed at 100 Hz and still ran dry."""
import numpy as np
import pytest

from live.tracker import HOP
from live.xiao import DriftBuffer, ResampleBuffer, XiaoOutput

SR = 44100


def sine(n: int, start: int = 0, hz: float = 440.0) -> np.ndarray:
    return (0.5 * np.sin(2 * np.pi * hz * (np.arange(start, start + n) / SR))).astype(np.float32)


def test_steady_clocks_pass_the_samples_through_unchanged():
    f = ResampleBuffer()
    f.push(np.zeros(f.target, dtype=np.float32))                 # the preload the level aims at
    out, k = [], 0
    for _ in range(500):
        f.push(sine(HOP, k))
        out.append(f.pull(HOP))
        k += HOP
    got = np.concatenate(out)[f.target:]                          # after the preload (silence) went out
    want = sine(len(got), 0)
    assert np.max(np.abs(got - want)) < 1e-3
    s = f.status()
    assert (s.underruns, s.overflows, s.slips_dropped, s.slips_repeated) == (0, 0, 0, 0)


@pytest.mark.parametrize("ppm", [-5000, -2300, 2300, 5000])
def test_a_drift_beyond_the_old_slip_limit_is_followed_without_clicks(ppm):
    """The consumer's clock runs `ppm` fast (+) or slow (-) for 60 s: the old FIFO could follow 2270 ppm at most."""
    f = ResampleBuffer()
    f.push(np.zeros(f.target, dtype=np.float32))
    owed, k, outs = 0.0, 0, []
    for _ in range(6000):
        f.push(sine(HOP, k))
        k += HOP
        owed += HOP * ppm * 1e-6
        n = HOP + int(owed)
        owed -= int(owed)
        outs.append(f.pull(n))
    s = f.status()
    assert s.underruns == 0 and s.overflows == 0
    assert abs(s.avg_level - f.target) < 0.25 * f.target + 1.5 * HOP           # held near the target
    tail = np.concatenate(outs[-2000:])
    assert np.max(np.abs(np.diff(tail))) < 0.5 * 2 * np.pi * 440 / SR * 1.5       # a smooth sine: no step, no repeated sample
    assert abs(s.ratio - 1.0 / (1 + ppm * 1e-6)) < 0.002                        # it settled on the consumer rate


def jittery(f, seconds: float, ppm: float, seed: int):
    rng = np.random.default_rng(seed)
    owed, debt = 0.0, 0
    f.push(np.zeros(f.target + HOP, dtype=np.float32))
    for _ in range(int(seconds * 100)):
        f.push(rng.standard_normal(HOP).astype(np.float32))
        debt += 1
        pulls = int(rng.integers(0, 3)) if debt < 3 else debt
        for _ in range(min(pulls, debt)):
            owed += HOP * ppm * 1e-6
            n = HOP + int(owed)
            owed -= int(owed)
            f.pull(n)
            debt -= 1


@pytest.mark.parametrize("ppm", [-3000, 0, 3000])
def test_a_jittery_consumer_never_underruns(ppm):
    f = ResampleBuffer()
    jittery(f, 120.0, ppm, seed=7)
    s = f.status()
    assert s.underruns == 0 and s.overflows == 0 and s.target == f.target


def test_an_underrun_plays_what_is_there_then_silence_and_deepens_the_target():
    f = ResampleBuffer()
    t0 = f.target
    f.push(np.ones(100, dtype=np.float32))
    out = f.pull(HOP)
    assert out.shape == (HOP,) and np.all(out[:99] == 1) and np.all(out[200:] == 0)
    assert f.status().underruns == 1 and f.target == t0 + HOP
    for _ in range(20):
        f.pull(HOP)
    assert f.target == f.max_target


def test_a_full_fifo_keeps_the_newest():
    f = ResampleBuffer(target=HOP, capacity=4 * HOP, max_target=2 * HOP)
    for k in range(10):
        f.push(np.full(HOP, k, dtype=np.float32))
    s = f.status()
    assert s.level == 4 * HOP and s.overflows == 6
    out = f.pull(HOP)
    assert out[-1] >= 7.0 and out[0] >= 6.0


def test_bad_sizes_are_refused():
    with pytest.raises(ValueError):
        ResampleBuffer(target=0)
    with pytest.raises(ValueError):
        ResampleBuffer(target=HOP, capacity=HOP)


def test_the_xiao_output_uses_it_on_request():
    class S:
        def __init__(self, **kw):
            self.kw, self.latency = kw, 0.01

    devices = [{"name": "jank", "max_output_channels": 1}]
    assert isinstance(XiaoOutput(stream_factory=S, devices=devices).fifo, DriftBuffer)
    x = XiaoOutput(stream_factory=S, devices=devices, resample=True)
    assert isinstance(x.fifo, ResampleBuffer)
    assert "fifo_ratio" in x.status()


def test_a_bursty_producer_deepens_the_target_until_the_underruns_stop():
    """The speakers' stream with a 100 ms buffer delivers 8 blocks at once every 80 ms; the jank callback pulls one every 10 ms."""
    f = ResampleBuffer()
    k, runs = 0, []
    for burst in range(300):                              # 24 s
        for _ in range(8):
            f.push(sine(HOP, k))
            k += HOP
        for _ in range(8):
            f.pull(HOP)
        runs.append(f.status().underruns)
    assert f.target > 3 * HOP                             # it grew
    assert runs[-1] == runs[150]                          # and no underrun in the second half
    assert f.target <= f.max_target and f.status().overflows == 0
