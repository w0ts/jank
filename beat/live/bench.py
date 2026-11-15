"""The XIAO bench of `player --xiao`: scores the XIAO, this program's tracker and the Link session (what Live follows)
against Traktor's beats as heard (live/accuracy.py), and models where the XIAO's offset comes from.

This program is never on Link (only the XIAO is). The XIAO reports, with each tracked beat, how long ago it took that
beat to be heard (the lag) and where its Link session's nearest beat is from that instant (jank docs/beat.md). The page
dates each report with the browser's clock (Web MIDI timeStamp, epoch ms, minus the lag) and posts it; `epoch_to_us`
maps that to this program's clock (`now_us`, the local model's) with the offset between the two read as it comes in.
The Link column is then the reported instant plus the session offset."""
from __future__ import annotations

import threading
import time
from pathlib import Path
from typing import Callable

from live.accuracy import Accuracy, Block

TICK_S = 0.02
MIN_LATENCY_MS, MAX_LATENCY_MS = -300, 300      # the XIAO's beat_latency range (jank core/jank_core.h)


def epoch_to_us(epoch_ms: float, now_us: float, now_epoch_s: float) -> float:
    """An epoch time (ms) on this program's clock, given one simultaneous reading of both clocks."""
    return epoch_ms * 1000.0 + (now_us - now_epoch_s * 1e6)


def suggested_latency_ms(current_ms: float | None, link_mean_ms: float | None) -> int | None:
    """The beat_latency that would put the Link session's beat on the heard beat: the session is late by link_mean_ms
    (+), so the XIAO should take the beat as heard that much earlier. None without both numbers."""
    if current_ms is None or link_mean_ms is None:
        return None
    return int(max(MIN_LATENCY_MS, min(MAX_LATENCY_MS, round(current_ms - link_mean_ms))))


def predicted_xiao_offset_ms(player_mean_ms: float | None, fifo_ms: float, jank_latency_ms: float,
                             speakers_latency_ms: float, beat_latency_ms: float) -> float | None:
    """What the XIAO's offset should be from the latencies alone: the tracker's own bias (the player's tracker runs
    the same network on the same audio), plus how much later the XIAO gets a sample than the speakers play it (the
    FIFO and jank's output latency against the speakers' latency), plus the beat_latency it adds."""
    if player_mean_ms is None:
        return None
    return player_mean_ms + fifo_ms + jank_latency_ms - speakers_latency_ms + beat_latency_ms


class XiaoBench:
    def __init__(self, now_us: Callable[[], float], source, engine, clock, xiao, beats_by_path: dict,
                 csv_path: Path | None, bpm_range: tuple[float, float]) -> None:
        self.now_us, self.source, self.engine, self.clock, self.xiao = now_us, source, engine, clock, xiao
        self.acc = Accuracy(beats_by_path, csv_path, bpm_range)
        self.xiao_stats: dict = {}
        self._last_player_beat = -1.0
        self._stop = threading.Event()
        source.on_block = lambda path, pos, rate, frames, heard: self.acc.add_block(Block(path, pos, rate, frames, heard))

    def start(self) -> None:
        threading.Thread(target=self._loop, daemon=True, name="xiao-bench").start()

    def close(self) -> None:
        self._stop.set()

    def _loop(self) -> None:
        while not self._stop.wait(TICK_S):
            try:
                self.tick()
            except Exception as exc:          # the bench must not take the player down; say why, keep going
                print(f"xiao bench: {exc!r}", flush=True)

    def tick(self) -> None:
        self.acc.update()
        if self.clock.ready:
            for s in list(self.engine.beats):
                if s > self._last_player_beat:
                    self.acc.add_estimate("player", self.clock.to_link_us(s))
                    self._last_player_beat = s
        self.acc.set_bpm("player", self.engine.status().bpm)
        self.acc.maybe_log(self._log_extra())

    def _log_extra(self) -> dict:
        x = self.xiao.status()
        return {"fifo_ms": x.get("fifo_ms"), "fifo_target": x.get("fifo_target"), "slips": x["dropped"] + x["repeated"],
                "underruns": x["underruns"], "jank_latency_ms": round(x["latency_ms"], 2),
                "speakers_latency_ms": round(self.source.latency_ms, 2)}

    # ---- the page's inputs ----
    def command(self, name: str, body: dict) -> None:
        if name == "xiao_beat":
            heard = epoch_to_us(float(body["epoch_ms"]), self.now_us(), time.time())
            bpm = float(body["bpm"]) if body.get("bpm") else None
            self.acc.add_estimate("xiao", heard, bpm)
            if body.get("link_ms") is not None:              # the XIAO's Link session beat near that instant
                self.acc.add_estimate("link", heard + float(body["link_ms"]) * 1000.0, bpm)
        elif name == "xiao_stats":
            stats = {k: float(v) for k, v in body.items() if isinstance(v, (int, float))}
            self.xiao_stats = stats
            self.acc.set_xiao_stats(stats)
        else:
            raise KeyError(name)

    def status(self) -> dict:
        m = self.acc.metrics()
        x = self.xiao.status()
        latency_ms = self.xiao_stats.get("latency_us")
        latency_ms = latency_ms / 1000.0 if latency_ms is not None else None
        player_mean = (m.get("player") or {}).get("mean_ms")
        link_mean = (m.get("link") or {}).get("mean_ms")
        pred = predicted_xiao_offset_ms(player_mean, x.get("fifo_ms") or 0.0, x["latency_ms"], self.source.latency_ms,
                                        latency_ms) if latency_ms is not None else None
        m["model"] = {"speakers_latency_ms": round(self.source.latency_ms, 2), "jank_latency_ms": round(x["latency_ms"], 2),
                      "fifo_ms": x.get("fifo_ms"), "beat_latency_ms": latency_ms,
                      "predicted_xiao_ms": None if pred is None else round(pred, 2),
                      "suggested_latency_ms": suggested_latency_ms(latency_ms, link_mean)}
        return m
