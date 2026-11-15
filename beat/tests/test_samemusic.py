import numpy as np

from live.samemusic import BANDS, SameMusic, chroma_of, shift_chroma, timbre_of


def harmonic_spectrum(f0s, gain=1.0, rate=1.0, noise=0.0, rng=None):
    """81 log bands of a few harmonic tones (f0s in Hz, 8 harmonics each), played `rate` times faster."""
    from live.samemusic import centres
    c = centres()
    lin = np.zeros(BANDS)
    for f0 in f0s:
        for h in range(1, 9):
            f = f0 * h * rate
            lin += np.exp(-0.5 * (np.log2(c / f) * 12 / 0.4) ** 2) / h
    lin = lin * gain + 0.02
    if noise and rng is not None:
        lin = lin * np.exp(rng.normal(0, noise, BANDS))
    return np.log10(1 + 10 * lin)


def test_chroma_tells_two_chords_apart():
    a = harmonic_spectrum([261.6, 329.6, 392.0])          # C major
    ca = chroma_of(a)
    other = chroma_of(harmonic_spectrum([185.0, 233.1, 277.2]))      # F# major
    assert float(ca @ other) < 0.9
    assert abs(np.linalg.norm(ca) - 1) < 1e-6
    assert timbre_of(a).shape == (12,)


def test_a_varispeed_shift_is_undone_by_shifting_the_chroma():
    a = chroma_of(harmonic_spectrum([261.6, 329.6, 392.0]))
    b = chroma_of(harmonic_spectrum([261.6, 329.6, 392.0], rate=1.06))   # one semitone up
    plain = float(a @ b)
    back = float(shift_chroma(a, 12 * np.log2(1.06)) @ b)
    assert back > plain and back > 0.9


def feed_segments(sm, spectra, blocks_per=10):
    out = []
    for s in spectra:
        out.append(sm.feed(np.tile(s, (blocks_per, 1))))
    return out


def test_steady_music_is_not_novel_and_a_new_track_is():
    rng = np.random.default_rng(3)
    sm = SameMusic()
    a = [harmonic_spectrum([261.6, 329.6, 392.0], noise=0.1, rng=rng) for _ in range(60)]
    feed_segments(sm, a)
    steady = sm.novelty()
    b = [harmonic_spectrum([185.0, 233.1, 277.2], noise=0.1, rng=rng) * 1.3 for _ in range(6)]   # another key and sound
    feed_segments(sm, b)
    new = sm.novelty()
    assert steady["chroma"] < 0.02 and new["chroma"] > steady["chroma"] + 0.1
    assert new["timbre"] > steady["timbre"] + 1.0


def test_a_rate_change_with_the_follower_s_ratio_stays_the_same_music():
    rng = np.random.default_rng(4)
    sm = SameMusic()
    feed_segments(sm, [harmonic_spectrum([220.0, 277.2, 329.6], noise=0.1, rng=rng) for _ in range(60)])
    sm.rate = 1.05                                   # the follower's tempo ratio since the memory was laid down
    feed_segments(sm, [harmonic_spectrum([220.0, 277.2, 329.6], rate=1.05, noise=0.1, rng=rng) for _ in range(6)])
    assert sm.novelty()["chroma"] < 0.1


def test_not_enough_memory_yet_gives_none():
    sm = SameMusic()
    feed_segments(sm, [harmonic_spectrum([220.0])] * 3)
    assert sm.novelty() is None
