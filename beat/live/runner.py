"""Wires a source to the engine and the Link controller, and prints one line per second."""
from __future__ import annotations

import sys
import threading
import time
from typing import TextIO

from live.clockmap import ClockMap
from live.controller import LinkController
from live.engine import Engine
from live.link_port import LinkPort

TICK_S = 0.010


def format_line(stream_s: float, engine: Engine, link: LinkPort | None, controller: LinkController | None) -> str:
    snap = engine.snapshot()
    bpm = f"{snap.bpm:7.2f}" if snap.bpm else "   --- "
    peers = f"{link.peers():d}" if link is not None else "-"
    phase = controller.phase_err_ms if controller is not None else None
    err = f"{phase:+6.1f} ms" if phase is not None else "    --- "
    return f"{stream_s:6.0f} s  {snap.mode:<12} {bpm} BPM  peers {peers}  phase {err}"


class _ControlThread(threading.Thread):
    def __init__(self, engine: Engine, controller: LinkController) -> None:
        super().__init__(daemon=True)
        self.engine, self.controller = engine, controller
        self.stop_event = threading.Event()
        self.error: BaseException | None = None

    def run(self) -> None:
        try:
            while not self.stop_event.wait(TICK_S):
                self.controller.tick(self.engine.snapshot())
        except BaseException as exc:      # surfaced by run(); never lose a control failure silently
            self.error = exc


def run(source, engine: Engine, clock: ClockMap, link: LinkPort | None, controller: LinkController | None,
        out: TextIO = sys.stdout) -> None:
    control = _ControlThread(engine, controller) if controller else None
    if control:
        control.start()
    last_printed = -1
    try:
        for chunk in source.chunks():
            if chunk.link_us is not None:
                clock.report(chunk.start_s, chunk.link_us)
            engine.feed(chunk.samples)
            second = int(engine.stream_time)
            if second != last_printed:
                last_printed = second
                print(format_line(second, engine, link, controller), file=out, flush=True)
            if control and control.error:
                raise RuntimeError("Link control failed") from control.error
    finally:
        if control:
            control.stop_event.set()
            control.join(timeout=1.0)
        source.close()
