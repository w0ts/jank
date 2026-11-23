#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""scripts/jankwifi.py against a fake unit on the loopback: chunks out of order, repeated and missing, garbage datagrams, a
refused line. No Wi-Fi, no device. Usage: test_jankwifi.py"""
from __future__ import annotations

import importlib.util
import os
import socket
import sys
import threading

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("jankwifi", os.path.join(here, "..", "scripts", "jankwifi.py"))
assert spec is not None and spec.loader is not None
jw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(jw)

checks = 0


def check(cond: bool, what: str) -> None:
    global checks
    checks += 1
    if not cond:
        sys.stdout.write("FAIL %s\n" % what)
        sys.exit(1)


def chunk(index: int, last: bool, text: str) -> bytes:
    return bytes([index | (0x80 if last else 0)]) + text.encode("ascii")


def test_parse_chunk() -> None:
    check(jw.parse_chunk(chunk(0, False, "abc")) == (0, False, b"abc"), "parse: a middle chunk")
    check(jw.parse_chunk(chunk(5, True, "ok\n")) == (5, True, b"ok\n"), "parse: the last chunk, bit 7")
    check(jw.parse_chunk(bytes([0x80])) == (0, True, b""), "parse: an empty last chunk")
    check(jw.parse_chunk(chunk(127, True, "x")) == (127, True, b"x"), "parse: index 127")
    check(jw.parse_chunk(b"") is None, "parse: an empty datagram is nothing")


def test_assemble() -> None:
    parts = {0: b"name=jank\n", 1: b"usb=host\n", 2: b"ok\n"}
    check(jw.assemble(parts, 2) == "name=jank\nusb=host\nok\n", "assemble: in index order")
    check(jw.assemble({0: b"a", 2: b"c"}, 2) is None, "assemble: a missing chunk is not an answer")
    check(jw.assemble(parts, None) is None, "assemble: no last chunk yet")
    check(jw.assemble({0: b"ok\n"}, 0) == "ok\n", "assemble: one chunk")
    check(jw.assemble({0: b"a", 1: b"b", 2: b"c"}, 1) == "ab", "assemble: chunks past the last are not part of it")
    check(jw.assemble({0: "caf\xe9".encode("latin-1")}, 0) == "caf�", "assemble: stray bytes do not break it")


class FakeUnit(threading.Thread):
    """A UDP server on the loopback: answers each datagram with the datagrams script(line) says (a list of bytes, in the order sent)."""

    def __init__(self, script) -> None:
        super().__init__(daemon=True)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(0.2)
        self.port: int = self.sock.getsockname()[1]
        self.script = script
        self.seen: list[str] = []
        self.stop = False

    def run(self) -> None:
        while not self.stop:
            try:
                data, addr = self.sock.recvfrom(2048)
            except socket.timeout:
                continue
            line = data.decode("ascii", "replace")
            self.seen.append(line)
            for dgram in self.script(line):
                self.sock.sendto(dgram, addr)

    def close(self) -> None:
        self.stop = True
        self.join(1.0)
        self.sock.close()


def ask(unit: FakeUnit, line: str, timeout: float = 1.0) -> dict[str, str | None]:
    return jw.ask(line, ip="127.0.0.1", port=unit.port, timeout=timeout)


def test_round_trip_out_of_order_and_repeats() -> None:
    def script(line: str) -> list[bytes]:
        if line == "get":
            return [b"\x01", chunk(1, False, "usb=host\n"), chunk(2, True, "ok\n"), chunk(0, False, "name=jank\n"), chunk(1, False, "usb=host\n"), b""]
        return [chunk(0, True, "err not allowed over wifi\n")]

    unit = FakeUnit(script)
    unit.start()
    try:
        got = ask(unit, "get")
        check(got == {"127.0.0.1": "name=jank\nusb=host\nok\n"}, "round trip: out of order, a repeat and garbage: one clean answer")
        check(unit.seen == ["get"], "round trip: the request is the line as typed, once")
        got = ask(unit, "set usb host")
        check(got == {"127.0.0.1": "err not allowed over wifi\n"}, "a refused line comes back as the unit's err")
    finally:
        unit.close()


def test_incomplete_answer_is_reported() -> None:
    unit = FakeUnit(lambda line: [chunk(0, False, "a"), chunk(2, True, "c")])
    unit.start()
    try:
        got = ask(unit, "status", timeout=0.6)
        check(got == {"127.0.0.1": None}, "incomplete: a sender whose answer has a hole is listed with no text")
        check(len(unit.seen) >= 2, "incomplete: the request was sent again once")
    finally:
        unit.close()


def test_silence() -> None:
    unit = FakeUnit(lambda line: [])
    unit.start()
    try:
        check(ask(unit, "get", timeout=0.5) == {}, "silence: nobody answered, nothing listed")
    finally:
        unit.close()


def test_bad_lines_are_not_sent() -> None:
    for bad in ("", "   ", "x" * 121, "caf\xe9", "a\x01b"):
        try:
            jw.ask(bad, ip="127.0.0.1", port=9, timeout=0.1)
        except ValueError:
            continue
        check(False, "a line the unit would not answer is refused before it is sent: %r" % bad)
    check(True, "bad lines done")


def main() -> None:
    test_parse_chunk()
    test_assemble()
    test_round_trip_out_of_order_and_repeats()
    test_incomplete_answer_is_reported()
    test_silence()
    test_bad_lines_are_not_sent()
    sys.stdout.write("test_jankwifi: %d checks passed\n" % checks)


if __name__ == "__main__":
    main()
