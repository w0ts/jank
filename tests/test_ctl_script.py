#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""scripts/ctl.py's line exchange against a fake port: no MIDI, no device. Usage: test_ctl_script.py"""
from __future__ import annotations

import importlib.util
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("ctl", os.path.join(here, "..", "scripts", "ctl.py"))
assert spec is not None and spec.loader is not None
ctl = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ctl)

checks = 0


def check(cond: bool, what: str) -> None:
    global checks
    checks += 1
    if not cond:
        sys.stdout.write("FAIL %s\n" % what)
        sys.exit(1)


class FakePort:
    """Answers each line sent with the scripted lines, one per recv; None once they are used up (a timeout)."""

    def __init__(self, replies: list[str]) -> None:
        self.sent: list[str] = []
        self.replies = list(replies)

    def send(self, text: str) -> None:
        self.sent.append(text)

    def recv(self, timeout: float) -> str | None:
        return self.replies.pop(0) if self.replies else None


def test_ok_collects_the_lines() -> None:
    port = FakePort(["usb=host", "source=auto", "ok"])
    ok, lines = ctl.run_line(port, "get", 0.05)
    check(ok and lines == ["usb=host", "source=auto"], "ok: the lines before the ok")
    check(port.sent == ["get"], "ok: the line was sent once, as typed")


def test_err_is_a_failure() -> None:
    ok, lines = ctl.run_line(FakePort(["err bad value"]), "set usb otg", 0.05)
    check(not ok and lines == ["err bad value"], "err: a failure, the message kept")


def test_timeout_is_a_failure() -> None:
    ok, lines = ctl.run_line(FakePort(["usb=host"]), "get", 0.05)
    check(not ok and lines == ["usb=host"], "timeout: no ok in time is a failure, what came is kept")


def test_ota_lines_are_refused() -> None:
    port = FakePort(["ok"])
    ok, lines = ctl.run_line(port, "ota begin 100 deadbeef", 0.05)
    check(not ok and port.sent == [] and "ota" in lines[0], "ota: refused before anything is sent (scripts/ota.py does that)")
    ok, _ = ctl.run_line(FakePort(["ok"]), "  OTA end", 0.05)
    check(not ok, "ota: refused whatever the case and the spaces")


def test_empty_line_is_refused() -> None:
    port = FakePort(["ok"])
    ok, _ = ctl.run_line(port, "   ", 0.05)
    check(not ok and port.sent == [], "empty: nothing is sent")


def main() -> None:
    test_ok_collects_the_lines()
    test_err_is_a_failure()
    test_timeout_is_a_failure()
    test_ota_lines_are_refused()
    test_empty_line_is_refused()
    sys.stdout.write("test_ctl_script: %d checks passed\n" % checks)


if __name__ == "__main__":
    main()
