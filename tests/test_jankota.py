#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The signed Wi-Fi update's client (scripts/jankwifi.py `ota`) against a mock unit on the loopback that speaks core/jank_wota.h: the frames
byte for byte (the same vectors as tests/test_wota.c), a transfer with lost replies and a damaged chunk, a wrong key, a refused update, the
confirm. No Wi-Fi, no device. Usage: test_jankota.py"""
from __future__ import annotations

import hmac
import hashlib
import importlib.util
import itertools
import os
import socket
import struct
import sys
import tempfile
import threading
import zlib

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("jankwifi", os.path.join(here, "..", "scripts", "jankwifi.py"))
assert spec is not None and spec.loader is not None
jw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(jw)

KEY = bytes(range(32))
NONCE = bytes(range(0xA0, 0xA8))
checks = 0


def check(cond: bool, what: str) -> None:
    global checks
    checks += 1
    if not cond:
        sys.stdout.write("FAIL %s\n" % what)
        sys.exit(1)


def test_vectors_match_the_c_implementation() -> None:
    begin = jw.build_cmd(KEY, NONCE, 1, jw.OP_BEGIN, jw.begin_args(1514240, 0x12345678, 0))
    check(begin.hex() == "0102000000010100171b00123456780080dc27e23e1ddf3a060bebbf2fcab849", "vector: BEGIN")
    data = jw.build_cmd(KEY, NONCE, 2, jw.OP_DATA, struct.pack(">IIH", 0, 0xCAFEBABE, 5) + b"hello")
    check(data.hex() == "0102000000020200000000cafebabe000568656c6c6f0e3dece4f6e38e99d8462556b0c6ec34", "vector: DATA")
    check(jw.build_cmd(KEY, NONCE, 3, jw.OP_CONFIRM).hex() == "010200000003057b8c2c6da2b5ffc6809c9c65443a6d6f", "vector: CONFIRM")
    reply = bytes.fromhex("018200000002000000000504001e8bdb57ade9afac3579c3921e99d360")
    check(jw.check_reply(KEY, NONCE, reply) == (2, 0, 5, 1024), "vector: REPLY checks out")
    check(jw.data_args(0, b"hello") == struct.pack(">IIH", 0, zlib.crc32(b"hello"), 5) + b"hello", "data_args carries the chunk's own CRC")


def test_reply_checks() -> None:
    reply = bytearray.fromhex("018200000002000000000504001e8bdb57ade9afac3579c3921e99d360")
    check(jw.check_reply(bytes(range(1, 33)), NONCE, bytes(reply)) is None, "reply: another key's is not believed")
    check(jw.check_reply(KEY, NONCE[::-1], bytes(reply)) is None, "reply: another session's is not believed")
    for i in range(len(reply)):
        bad = bytearray(reply)
        bad[i] ^= 0x40
        check(jw.check_reply(KEY, NONCE, bytes(bad)) is None, "reply: an altered byte %d" % i)
    check(jw.check_reply(KEY, NONCE, bytes(reply[:-1])) is None and jw.check_reply(KEY, NONCE, b"") is None, "reply: short or empty")


def test_load_key() -> None:
    with tempfile.TemporaryDirectory() as d:
        good, short, junk = (os.path.join(d, n) for n in ("good", "short", "junk"))
        with open(good, "w", encoding="ascii") as f:
            f.write(KEY.hex() + "\n")
        with open(short, "w", encoding="ascii") as f:
            f.write("abcd\n")
        with open(junk, "w", encoding="ascii") as f:
            f.write("zz" * 32)
        check(jw.load_key(good) == KEY, "key: 64 hex characters")
        for p in (short, junk):
            try:
                jw.load_key(p)
                check(False, "key: a bad file must raise")
            except ValueError:
                check(True, "key: bad file")


class MockUnit:
    """The unit's side of core/jank_wota.h and the transfer's rules (core/jank_ota.h), on the loopback."""

    def __init__(
        self, key: bytes, drop_every: int = 0, damage_chunk_once: bool = False, busy: bool = False, slot: int = 1 << 20, drop_end_reply: bool = False
    ) -> None:
        self.key = key
        self.restarted = False  # after an accepted END the real unit restarts: it answers nothing more
        self.drop_end_reply = drop_end_reply
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.port = self.sock.getsockname()[1]
        self.nonce = b""
        self.last_ctr = 0
        self.drop_every = drop_every
        self.sent = 0
        self.damage = damage_chunk_once
        self.busy = busy
        self.slot = slot
        self.size = self.crc = self.offset = 0
        self.active = False
        self.image = bytearray()
        self.ended = False
        self.confirms = 0
        self.confirm_early = 2  # CONFIRM answers "too early" this many times first
        self.counters: list[int] = []
        self.stop = False
        self.t = threading.Thread(target=self.run, daemon=True)
        self.t.start()

    def reply(self, addr: tuple[str, int], counter: int, status: int, offset: int, chunk: int) -> None:
        self.sent += 1
        if self.drop_every and self.sent % self.drop_every == 0:
            return  # a reply lost on the way
        body = bytes([jw.MAGIC, jw.T_CMD_R]) + struct.pack(">IBIH", counter, status, offset, chunk)
        self.sock.sendto(body + jw.frame_mac(self.key, self.nonce, body), addr)

    def handle(self, f: bytes, addr: tuple[str, int]) -> None:
        if self.restarted:
            return
        if len(f) == 2 and f[0] == jw.MAGIC and f[1] == jw.T_HELLO:
            self.nonce = os.urandom(jw.NONCE_LEN)
            self.last_ctr = 0
            self.sock.sendto(bytes([jw.MAGIC, jw.T_HELLO_R]) + self.nonce, addr)
            return
        if len(f) < 7 + jw.MAC_LEN or f[0] != jw.MAGIC or f[1] != jw.T_CMD or not self.nonce:
            return
        body, mac = f[: -jw.MAC_LEN], f[-jw.MAC_LEN :]
        if not hmac.compare_digest(jw.frame_mac(self.key, self.nonce, body), mac):
            return  # a wrong mac: no answer
        counter, op = struct.unpack(">IB", body[2:7])
        if counter <= self.last_ctr:
            return  # a replay: no answer
        self.last_ctr = counter
        self.counters.append(counter)
        args = body[7:]
        if op == jw.OP_BEGIN:
            size, crc, _ = struct.unpack(">IIB", args)
            if self.busy:
                self.reply(addr, counter, 2, 0, 0)
            elif size < 24 or size > self.slot:
                self.reply(addr, counter, 3, 0, 0)
            else:
                self.active, self.size, self.crc, self.offset, self.image = True, size, crc, 0, bytearray()
                self.reply(addr, counter, 0, 0, jw.DATA_MAX)
        elif op == jw.OP_DATA:
            off, crc, n = struct.unpack(">IIH", args[:10])
            data = args[10:]
            if not self.active:
                self.reply(addr, counter, 5, 0, 0)
            elif off < self.offset:
                self.reply(addr, counter, 1, self.offset, 0)  # an old chunk again: ok, next offset
            elif off > self.offset:
                self.reply(addr, counter, 6, self.offset, 0)
            else:
                if self.damage and off == 2 * jw.DATA_MAX:
                    self.damage = False
                    data = bytes([data[0] ^ 1]) + data[1:]  # damaged in transit, once: its CRC no longer fits
                if zlib.crc32(data) & 0xFFFFFFFF != crc or len(data) != n:
                    self.reply(addr, counter, 7, self.offset, 0)
                else:
                    self.image += data
                    self.offset += n
                    self.reply(addr, counter, 0, self.offset, 0)
        elif op == jw.OP_END:
            if self.active and self.offset == self.size and zlib.crc32(bytes(self.image)) & 0xFFFFFFFF == self.crc:
                self.active, self.ended = False, True
                if not self.drop_end_reply:
                    self.reply(addr, counter, 0, self.offset, 0)
                self.restarted = True
            else:
                self.reply(addr, counter, 9 if self.offset != self.size else 7, self.offset, 0)
        elif op == jw.OP_CONFIRM:
            if self.confirm_early > 0:
                self.confirm_early -= 1
                self.reply(addr, counter, 0x82, 0, 0)
            else:
                self.confirms += 1
                self.reply(addr, counter, 0, 0, 0)
        else:
            self.reply(addr, counter, 0x80, 0, 0)

    def run(self) -> None:
        self.sock.settimeout(0.05)
        while not self.stop:
            try:
                f, addr = self.sock.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                return
            self.handle(f, addr)

    def close(self) -> None:
        self.stop = True
        self.t.join(timeout=1)
        self.sock.close()


def image_of(n: int) -> bytes:
    return bytes([0xE9, 0x04, 0x02, 0x10] + [(i * 7 + 3) & 0xFF for i in range(n - 4)])  # an ESP image's first bytes, then anything


def session(u: MockUnit, key: bytes = KEY) -> "jw.Session":
    return jw.Session("127.0.0.1", key, u.port, timeout=0.15, tries=8)


def test_a_transfer() -> None:
    u = MockUnit(KEY)
    s = session(u)
    img = image_of(5000)
    try:
        s.hello()
        jw.transfer(s, img)
        check(u.ended and bytes(u.image) == img, "transfer: the unit has the image")
        check(u.counters == sorted(set(u.counters)), "transfer: counters only rise")
        check(len(u.counters) == 1 + (len(img) + jw.DATA_MAX - 1) // jw.DATA_MAX + 1, "transfer: begin, the chunks, end: nothing sent twice")
    finally:
        s.close()
        u.close()


def test_a_transfer_with_lost_replies() -> None:
    u = MockUnit(KEY, drop_every=3)  # every third reply lost
    s = session(u)
    img = image_of(9000)
    try:
        s.hello()
        jw.transfer(s, img)
        check(u.ended and bytes(u.image) == img, "loss: still the whole image")
        check(len(set(u.counters)) == len(u.counters) and u.counters == sorted(u.counters), "loss: every try had a higher counter")
        check(len(u.counters) > 1 + 9 + 1, "loss: some chunks were sent again")
    finally:
        s.close()
        u.close()


def test_the_end_answer_lost_is_not_a_failure() -> None:
    u = MockUnit(KEY, drop_end_reply=True)  # the image is accepted and the unit restarts, but its answer never arrives
    s = jw.Session("127.0.0.1", KEY, u.port, timeout=0.05, tries=3)
    img = image_of(3000)
    logs: list[str] = []
    try:
        s.hello()
        jw.transfer(s, img, logs.append)  # must not raise: the wait for the restart decides
        check(u.ended and bytes(u.image) == img, "end lost: the unit has the whole image")
        check(any("END" in line for line in logs), "end lost: the client says so")
    finally:
        s.close()
        u.close()


def test_a_damaged_chunk_is_sent_again() -> None:
    u = MockUnit(KEY, damage_chunk_once=True)
    s = session(u)
    img = image_of(6000)
    try:
        s.hello()
        jw.transfer(s, img)
        check(u.ended and bytes(u.image) == img, "damage: sent again from the offset, the image is whole")
    finally:
        s.close()
        u.close()


def test_a_refused_update() -> None:
    for kwargs, words in (({"busy": True}, "busy"), ({"slot": 100}, "size")):
        u = MockUnit(KEY, **kwargs)
        s = session(u)
        try:
            s.hello()
            try:
                jw.transfer(s, image_of(5000))
                check(False, "refused: must raise")
            except jw.UpdateError as e:
                check(words in str(e), "refused: the unit's own words (%s)" % words)
        finally:
            s.close()
            u.close()


def test_the_wrong_key() -> None:
    u = MockUnit(KEY)
    s = jw.Session("127.0.0.1", bytes(range(1, 33)), u.port, timeout=0.05, tries=3)
    try:
        s.hello()  # the nonce is not secret: the HELLO works
        try:
            s.call(jw.OP_END)
            check(False, "wrong key: must raise")
        except jw.UpdateError as e:
            check("no answer" in str(e), "wrong key: no answer, so an error here")
        check(u.counters == [], "wrong key: the unit did nothing")
    finally:
        s.close()
        u.close()


def test_no_unit() -> None:
    s = jw.Session("127.0.0.1", KEY, 9, timeout=0.05, tries=2)  # nobody on port 9
    try:
        try:
            s.hello(wait_s=0.3)
            check(False, "no unit: HELLO must raise")
        except jw.UpdateError as e:
            check("HELLO" in str(e), "no unit: the error says HELLO went unanswered")
    finally:
        s.close()


def test_confirm() -> None:
    u = MockUnit(KEY)
    try:
        jw.confirm("127.0.0.1", KEY, 30, port=u.port, log=lambda s: None)  # answers "too early" twice, then ok
        check(u.confirms == 1, "confirm: it went through after the early answers")
    finally:
        u.close()


def test_restart_detection() -> None:
    ups = iter([None, 120, 119, 4])
    real = jw.read_uptime
    jw.read_uptime = lambda ip, port=jw.PORT: next(ups)  # type: ignore[assignment]
    real_sleep = jw.time.sleep
    jw.time.sleep = lambda s: None  # type: ignore[assignment]
    try:
        check(jw.wait_restart("x", 118, wait_s=5.0) == 4, "restart: the first uptime below the old one")
        ups = itertools.repeat(200)  # a unit that answers, again and again, with an uptime that never went down
        try:
            jw.wait_restart("x", 100, wait_s=0.05)
            check(False, "restart: a unit that never restarts must raise")
        except jw.UpdateError:
            check(True, "restart: timeout")
    finally:
        jw.read_uptime = real  # type: ignore[assignment]
        jw.time.sleep = real_sleep  # type: ignore[assignment]
    check(jw.uptime_of("a=1\nuptime_s=57\nreset=SW\n") == 57 and jw.uptime_of("nothing") is None and jw.uptime_of(None) is None, "uptime_of")


def main() -> int:
    test_vectors_match_the_c_implementation()
    test_reply_checks()
    test_load_key()
    test_a_transfer()
    test_a_transfer_with_lost_replies()
    test_the_end_answer_lost_is_not_a_failure()
    test_a_damaged_chunk_is_sent_again()
    test_a_refused_update()
    test_the_wrong_key()
    test_no_unit()
    test_confirm()
    test_restart_detection()
    sys.stdout.write("test_jankota: %d checks passed\n" % checks)
    return 0


if __name__ == "__main__":
    sys.exit(main())
