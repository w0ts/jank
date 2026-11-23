#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Sends one control line to a jank over its USB MIDI port and prints the answer (the same line layer as the control page:
docs/firmware.md, "Control page and protocol"). Needs python-rtmidi (pip install python-rtmidi).

    python scripts/ctl.py get
    python scripts/ctl.py status
    python scripts/ctl.py set usb host

The page cannot change the USB role (it restarts the device); this can. `set usb host` is stored: on a XIAO the unit still starts as
a device, and a computer that configures it keeps it one; with no computer within 3 s it restarts as host (docs/usb.md, "Host mode").
No other MIDI client may talk to the stick at the same time (close the Chrome control page). Firmware updates are scripts/ota.py's. Exit
status 0: answered ok."""
from __future__ import annotations

import argparse
import sys
import time
from typing import Protocol


class Port(Protocol):
    """What the exchange needs of the MIDI port (scripts/ota.py's Midi has it)."""

    def send(self, text: str) -> None: ...

    def recv(self, timeout: float) -> str | None: ...


def run_line(port: Port, line: str, timeout: float = 3.0) -> tuple[bool, list[str]]:
    """Sends the line and collects the answer up to its `ok` (True) or an `err ...` (False). No end in time: False, with what came."""
    text = line.strip()
    if not text:
        return False, ["err empty line"]
    if text.split()[0].lower() == "ota":
        return False, ["err ota lines belong to scripts/ota.py"]
    port.send(text)
    lines: list[str] = []
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        got = port.recv(0.2)
        if got is None:
            time.sleep(0.01)
            continue
        if got == "ok":
            return True, lines
        lines.append(got)
        if got.startswith("err"):
            return False, lines
    return False, lines


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="send one control line to a jank over USB MIDI")
    ap.add_argument("line", nargs="+", help='the control line, e.g. get, status, set usb host')
    ap.add_argument("--port", default="Jank", help="the MIDI port's name (default Jank)")
    ap.add_argument("--timeout", type=float, default=3.0, help="seconds to wait for the answer")
    args = ap.parse_args(argv)
    try:
        from ota import Midi
    except ImportError as e:
        sys.stdout.write("python-rtmidi is missing (%s): pip install python-rtmidi\n" % e)
        return 2
    midi = Midi(args.port)
    if not midi.open():
        sys.stdout.write('no MIDI port named "%s": plug the stick into the computer (not the hub) and close other MIDI clients\n' % args.port)
        return 1
    try:
        ok, lines = run_line(midi, " ".join(args.line), args.timeout)
    finally:
        midi.close()
    for line in lines:
        sys.stdout.write(line + "\n")
    sys.stdout.write("ok\n" if ok else "no ok\n")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
