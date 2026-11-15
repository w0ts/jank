"""The XIAO's `beat_latency`, worked out from the two output paths of the Mac (`python -m live.player --xiao`).

`beat_latency` (jank docs/beat.md): the music is heard that many ms AFTER its samples reach the XIAO (negative when the
XIAO gets the audio after the speakers play it). Both paths start at the same instant, the speakers' callback that renders
a block and hands it to the jank FIFO (live/xiao.py):

    speaker path = the speaker stream's output latency (PortAudio: callback to the DAC)
    jank path    = the FIFO's smoothed level + the jank stream's output latency (callback to the USB packet)

so the music is heard `speaker path` after that instant and the XIAO has it `jank path` after it:
    beat_latency = speaker path - jank path.
The block size does not enter: both streams use 441-sample blocks, and a sample's place in its block is the same on both
paths."""
from __future__ import annotations

from typing import Any

MIN_BEAT_LATENCY_MS, MAX_BEAT_LATENCY_MS = -300, 300     # the XIAO's range (jank core/jank_core.h)
WIRELESS_LATENCY_MS = 60.0       # a speaker stream reporting more than this is not a wired / built-in output
WIRELESS_WORDS = ("airpods", "bluetooth", "beats", "wireless")


def clamp_beat_latency(ms: float) -> int:
    return int(max(MIN_BEAT_LATENCY_MS, min(MAX_BEAT_LATENCY_MS, round(ms))))


def is_wireless(name: str, latency_ms: float) -> bool:
    """A Bluetooth-like speaker output: its name says so, or CoreAudio reports an unusually large latency (the report of
    a wireless link is rarely the real delay, so the suggestion may be off)."""
    n = name.lower()
    return any(w in n for w in WIRELESS_WORDS) or latency_ms > WIRELESS_LATENCY_MS


def path_latencies(speaker_latency_ms: float, speaker_name: str, xiao: dict[str, Any]) -> dict[str, Any]:
    """The two paths in ms and the beat_latency they suggest. `xiao` is XiaoOutput.status(): latency_ms and fifo_ms."""
    jank_ms = float(xiao["latency_ms"]) + float(xiao.get("fifo_ms") or 0.0)
    speaker_ms = float(speaker_latency_ms)
    return {
        "speaker": {"device": speaker_name, "latency_ms": round(speaker_ms, 2), "path_ms": round(speaker_ms, 2),
                    "wireless": is_wireless(speaker_name, speaker_ms)},
        "xiao": {"path_ms": round(jank_ms, 2)},
        "suggested_beat_latency_ms": clamp_beat_latency(speaker_ms - jank_ms),
    }
