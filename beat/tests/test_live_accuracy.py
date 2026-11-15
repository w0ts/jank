"""The accuracy scoring of `player --xiao` (live/accuracy.py, live/bench.py): pure functions on synthetic times."""
import csv
import time
from types import SimpleNamespace

import pytest

from live.accuracy import Accuracy, Block, fold_bpm, heard_times, lock_time, match_offsets, window_metrics
from live.bench import XiaoBench, epoch_to_us, predicted_xiao_offset_ms, suggested_latency_ms
from live.clockmap import ClockMap

HOP = 441


def blocks(path, seconds, rate=1.0, heard0=1e6):
    """Output blocks of 441 samples from file position 0 at `rate`, heard from heard0 on."""
    out, pos = [], 0.0
    for k in range(int(seconds * 100)):
        out.append(Block(path, pos, rate, HOP, heard0 + k * 1e4))
        pos += rate * HOP
    return out


def test_beats_are_heard_through_position_and_rate():
    beats = [0.5, 1.0, 1.5]
    h = heard_times(beats, blocks("a", 2.0))
    assert [s for _, s in h] == beats
    assert [round(t) for t, _ in h] == [1_500_000, 2_000_000, 2_500_000]
    h = heard_times(beats, blocks("a", 2.0, rate=1.25))         # faster: file beats come sooner
    assert [round(t) for t, _ in h] == [1_400_000, 1_800_000, 2_200_000]


def test_offsets_are_estimate_minus_reference_late_positive():
    ref = [1e6, 1.5e6, 2e6, 2.5e6]
    est = [1e6 + 12_000, 1.5e6 - 3_000, 2e6 + 400_000]          # the last is nearer the next beat: matched to 2.5 s
    offs = match_offsets(est, ref)
    assert [round(o, 1) for _, o in offs] == [12.0, -3.0, -100.0]


def test_an_estimate_far_from_any_beat_is_not_matched():
    assert match_offsets([5e6], [1e6, 1.5e6]) == []


def test_window_metrics():
    offs = [(1e6 + k * 5e5, o) for k, o in enumerate([10.0, 30.0, 10.0, 30.0])]
    m = window_metrics(offs, 128.5, 128.0, start_us=1e6)
    assert m.n == 4 and m.mean_ms == pytest.approx(20.0) and m.sd_ms == pytest.approx(10.0)
    assert m.within_pct == pytest.approx(50.0) and m.bpm_err == pytest.approx(0.5)
    assert window_metrics([], None, 128.0, None).mean_ms is None


def test_lock_is_phase_lock_whatever_the_constant_offset():
    early = [(1e6 + k * 5e5, o) for k, o in enumerate([90.0, -120.0, 60.0])]          # wandering first
    steady = [(2.5e6 + k * 5e5, 35.0 + (k % 2)) for k in range(10)]                  # 35 ms late but steady
    assert lock_time(early + steady, 1e6) == pytest.approx(1.5)
    assert lock_time(early, 1e6) is None


def test_fold():
    assert fold_bpm(64.0) == 128.0 and fold_bpm(200.0) == 100.0 and fold_bpm(127.02) == 127.02
    with pytest.raises(ValueError):
        fold_bpm(0.0)


def test_accuracy_end_to_end_with_three_sources(tmp_path):
    beats = [0.25 + 0.5 * k for k in range(64)]                   # 120 BPM, in file seconds
    out = tmp_path / "acc.csv"
    acc = Accuracy({"a": (beats, 120.0)}, out)
    for b in blocks("a", 33.0):
        acc.add_block(b)
    acc.update()
    ref = acc.track.ref
    assert len(ref) == 64 and ref[0] == pytest.approx(1.25e6)
    acc.set_xiao_stats({"phase_forces": 30, "tempo_sets": 3, "gaps": 0})
    for r in ref:
        acc.add_estimate("xiao", r + 30_000, 120.1)              # the XIAO 30 ms late
        acc.add_estimate("player", r - 5_000, 120.0)
        acc.add_estimate("link", r + 8_000, 120.1)               # its session 8 ms late
    acc.set_xiao_stats({"phase_forces": 33, "tempo_sets": 3, "gaps": 2})
    m = acc.metrics()
    assert m["xiao"]["mean_ms"] == pytest.approx(30.0) and m["xiao"]["within_pct"] == 0.0
    assert m["xiao"]["bpm_err"] == pytest.approx(0.1)
    assert m["player"]["mean_ms"] == pytest.approx(-5.0) and m["player"]["within_pct"] == 100.0
    assert m["link"]["mean_ms"] == pytest.approx(8.0) and m["link"]["n"] == 16
    assert (m["xiao_phase_forces"], m["xiao_tempo_sets"], m["xiao_gaps"]) == (3, 0, 2)
    acc.maybe_log({"fifo_ms": 30.0})
    rows = list(csv.DictReader(out.open()))
    assert rows and rows[-1]["xiao_mean_ms"] == "30.0" and rows[-1]["fifo_ms"] == "30.0"
    assert rows[-1]["xiao_phase_forces_track"] == "3.0" or rows[-1]["xiao_phase_forces_track"] == "3"


def test_a_new_track_starts_over():
    acc = Accuracy({"a": ([0.5, 1.0], 120.0), "b": ([0.5], 100.0)}, None)
    for b in blocks("a", 2.0):
        acc.add_block(b)
    acc.update()
    acc.add_estimate("xiao", acc.track.ref[0])
    for b in blocks("b", 1.0, heard0=5e6):
        acc.add_block(b)
    acc.update()
    assert acc.track.path == "b" and acc.track.est["xiao"] == [] and len(acc.track.ref) == 1


def test_the_bench_ties_the_sources_together(tmp_path):
    """XiaoBench with stand-ins: the page's XIAO beats (epoch ms, with the session offset), the engine's beats."""
    source = SimpleNamespace(latency_ms=15.0, on_block=None)
    engine = SimpleNamespace(beats=[], status=lambda: SimpleNamespace(bpm=120.0))
    clock = ClockMap()
    clock.report(0.0, 1e6)                                       # stream time 0 is heard at 1 s
    xiao = SimpleNamespace(status=lambda: {"latency_ms": 20.0, "fifo_ms": 30.0, "fifo_target": 1323, "dropped": 0,
                                           "repeated": 0, "underruns": 0})
    beats = [0.25 + 0.5 * k for k in range(70)]
    bench = XiaoBench(lambda: 40e6, source, engine, clock, xiao, {"a": (beats, 120.0)}, tmp_path / "a.csv", (90.0, 180.0))
    for b in blocks("a", 35.0):
        source.on_block(b.path, b.pos, b.rate, b.frames, b.heard_us)
    bench.tick()
    ref = bench.acc.track.ref
    engine.beats.extend(b + 0.002 for b in beats[:64])          # the player's tracker 2 ms late
    now_epoch = time.time()
    for r in ref:                                                # the XIAO 25 ms late, its session 4.3 ms before that
        bench.command("xiao_beat", {"epoch_ms": (r + 25_000 - 40e6) / 1000.0 + now_epoch * 1000.0, "bpm": 120.0,
                                    "link_ms": -4.3})
    bench.command("xiao_stats", {"latency_us": 25000, "phase_forces": 3, "mode": "steady"})
    bench.tick()
    st = bench.status()
    assert st["player"]["mean_ms"] == pytest.approx(2.0, abs=0.01)
    assert st["xiao"]["mean_ms"] == pytest.approx(25.0, abs=2.0)          # the two clocks were read a moment apart
    assert st["link"]["mean_ms"] == pytest.approx(20.7, abs=2.0)
    assert st["link"]["mean_ms"] - st["xiao"]["mean_ms"] == pytest.approx(-4.3, abs=0.01)
    assert st["model"]["beat_latency_ms"] == 25.0 and st["model"]["fifo_ms"] == 30.0
    assert st["model"]["predicted_xiao_ms"] == pytest.approx(2.0 + 30.0 + 20.0 - 15.0 + 25.0, abs=0.01)
    assert st["model"]["suggested_latency_ms"] == round(25.0 - st["link"]["mean_ms"])
    with pytest.raises(KeyError):
        bench.command("set_tempo", {})


def test_bench_helpers():
    assert epoch_to_us(1_000.5, now_us=5e6, now_epoch_s=2.0) == pytest.approx(1_000_500 + 5e6 - 2e6)
    assert suggested_latency_ms(25.0, 40.0) == -15                 # Link 40 ms late with 25 ms: take it 15 ms early
    assert suggested_latency_ms(25.0, -500.0) == 300 and suggested_latency_ms(None, 3.0) is None
    assert predicted_xiao_offset_ms(-5.0, 30.0, 20.0, 15.0, 25.0) == pytest.approx(55.0)
    assert predicted_xiao_offset_ms(None, 30.0, 20.0, 15.0, 25.0) is None


def test_xiao_mode_never_constructs_real_link(monkeypatch):
    """player --xiao: only the XIAO is on Link. main() with every device stood in: RealLink must never be built."""
    import live.player as player

    def no_link(*a, **k):
        raise AssertionError("--xiao constructed RealLink")

    class FakeSource:
        latency_ms = 10.0

        def __init__(self, now_us, *a, **k):
            self.now_us, self.tap, self.on_block = now_us, None, None

        def chunks(self):
            return iter(())

        def close(self):
            pass

    class FakeXiao:
        device_name, latency_ms = "jank", 8.0

        def push(self, block):
            pass

        def start(self):
            pass

        def close(self):
            pass

        def status(self):
            return {"latency_ms": 8.0, "fifo_ms": 30.0, "fifo_target": 1323, "dropped": 0, "repeated": 0, "underruns": 0}

    class FakeServer:
        server_address = ("127.0.0.1", 0)

        def serve_forever(self):
            pass

        def shutdown(self):
            pass

    monkeypatch.setattr(player, "RealLink", no_link)
    monkeypatch.setattr(player, "VarispeedSource", FakeSource)
    monkeypatch.setattr(player, "make_xiao", lambda: FakeXiao())
    monkeypatch.setattr(player, "speaker_device_name", lambda source: "Fake Speakers")
    monkeypatch.setattr(player, "serve", lambda port, app: FakeServer())          # port 8791 is never touched
    monkeypatch.setattr(player, "run", lambda *a, **k: None)
    monkeypatch.setattr(player, "Engine", lambda *a, **k: SimpleNamespace(beats=[], status=lambda: SimpleNamespace(bpm=None)))
    monkeypatch.setattr(player, "ACCURACY_DIR", __import__("pathlib").Path("/nonexistent-never-written"))
    assert player.main(["--xiao", "--no-browser"]) == 0
