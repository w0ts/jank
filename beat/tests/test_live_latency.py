"""The XIAO's beat_latency from the two output paths (live/latency.py), its place in /api/status, and the page's Auto
latency state machine (tests/autolat.js, node). Fake stream latencies: no sound card, no browser."""
import shutil
import subprocess
from pathlib import Path
from types import SimpleNamespace

import pytest

from live.controller import LinkController
from live.clockmap import ClockMap
from live.engine import Engine
from live.latency import clamp_beat_latency, is_wireless, path_latencies
from live.link_port import SimLink
from live.player import App


def xiao(latency_ms, fifo_ms):
    return {"latency_ms": latency_ms, "fifo_ms": fifo_ms}


def test_speakers_faster_than_jank_gives_a_negative_beat_latency():
    # jank: 8 ms output + 30 ms FIFO = 38 ms; speakers 12 ms: the stick gets the audio 26 ms after the music is heard
    r = path_latencies(12.0, "MacBook Pro Speakers", xiao(8.0, 30.0))
    assert r["xiao"]["path_ms"] == 38.0 and r["speaker"]["path_ms"] == 12.0
    assert r["suggested_beat_latency_ms"] == -26


def test_slower_speakers_give_a_positive_one_and_equal_paths_zero():
    assert path_latencies(60.0, "Speakers", xiao(10.0, 20.0))["suggested_beat_latency_ms"] == 30
    assert path_latencies(40.0, "Speakers", xiao(10.0, 30.0))["suggested_beat_latency_ms"] == 0


def test_a_missing_fifo_level_counts_as_zero_and_the_value_is_rounded_and_clamped():
    assert path_latencies(20.0, "S", {"latency_ms": 7.4})["suggested_beat_latency_ms"] == 13
    assert path_latencies(1.0, "S", xiao(500.0, 0.0))["suggested_beat_latency_ms"] == -300
    assert clamp_beat_latency(999) == 300 and clamp_beat_latency(-12.5) == -12 and clamp_beat_latency(2.5) == 2


def test_wireless_outputs_are_flagged_by_name_or_by_a_large_latency():
    assert is_wireless("Mots's AirPods Pro", 10.0) and is_wireless("Bluetooth Speaker", 10.0)
    assert is_wireless("USB DAC", 180.0)
    assert not is_wireless("MacBook Pro Speakers", 14.0)
    assert path_latencies(180.0, "USB DAC", xiao(8.0, 30.0))["speaker"]["wireless"] is True


def make_app(with_xiao):
    link = SimLink(now=lambda: 0, bpm=120.0)
    clock = ClockMap()
    source = SimpleNamespace(latency_ms=12.0, state=lambda: SimpleNamespace(
        path=None, position_s=0.0, duration_s=0.0, playing=False, rate=1.0, volume=1.0), tab_active=lambda: False)
    app = App(source, Engine(90, 180), link, LinkController(link, clock), clock, [], (90.0, 180.0))
    if with_xiao:
        app.xiao = SimpleNamespace(status=lambda: {"device": "jank", "latency_ms": 8.0, "fifo_ms": 30.0})
        app.speaker_name = "MacBook Pro Speakers"
    return app


def test_the_status_carries_both_paths_and_the_suggestion():
    st = make_app(True)._latency_status()
    assert st["xiao"]["path_ms"] == 38.0 and st["xiao"]["device"] == "jank"
    assert st["speaker"]["path_ms"] == 12.0 and st["speaker"]["device"] == "MacBook Pro Speakers"
    assert st["suggested_beat_latency_ms"] == -26


def test_without_xiao_there_is_no_latency_status():
    assert make_app(False)._latency_status() == {"xiao": None, "speaker": None, "suggested_beat_latency_ms": None}


def test_the_page_auto_latency_state_machine_and_debounce():
    """player.html's Auto latency (on / manual, the 2 ms debounce, the USB gate, the Tab audio suggestion), run by node."""
    node = shutil.which("node")
    if node is None:
        pytest.fail("node not found: the page's Auto latency cannot be checked")
    here = Path(__file__).resolve().parent
    r = subprocess.run([node, str(here / "autolat.js"), str(here.parent / "live" / "player.html")],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "checks passed" in r.stdout
