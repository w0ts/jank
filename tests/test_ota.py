#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""scripts/ota.py against the firmware's own update logic (tests/ota_replay.c, built by tests/run.sh), over a pipe that
loses, damages and repeats lines. No MIDI, no device. Usage: test_ota.py <ota_replay binary> <scratch dir>"""
import importlib.util
import os
import random
import subprocess
import sys
import zlib

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("ota", os.path.join(here, "..", "scripts", "ota.py"))
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)
ota.REPLY_TIMEOUT = 0.05


class Device:
    """The replay binary behind send/recv; `faults` decides what the wire does to each line sent."""

    def __init__(self, binary, flashed, faults=None):
        self.p = subprocess.Popen([binary, flashed], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.faults, self.sent, self.pending = faults or (lambda n, text: "ok"), 0, []

    def send(self, text):
        self.sent += 1
        fault = self.faults(self.sent, text)
        if fault == "drop":
            return
        if fault == "flip":                           # one character of the base64 damaged
            i = text.rindex(" ") + 3
            text = text[:i] + ("B" if text[i] != "B" else "C") + text[i + 1:]
        for _ in range(2 if fault == "twice" else 1):
            self.p.stdin.write(text + "\n")
            self.p.stdin.flush()
            self.pending.append(self.p.stdout.readline().strip())
        if fault == "lose_answer":
            self.pending.pop()

    def recv(self, timeout):
        return self.pending.pop(0) if self.pending else None

    def close(self):
        self.p.stdin.close()
        self.p.wait()


def image(n, chip=9, seed=1):
    r = random.Random(seed)
    img = bytearray(r.getrandbits(8) for _ in range(n))
    img[0], img[1], img[12], img[13] = 0xE9, 4, chip, 0
    return bytes(img)


def run(binary, scratch, name, img, faults=None):
    out = os.path.join(scratch, name + ".flashed")
    if os.path.exists(out):
        os.remove(out)
    dev = Device(binary, out, faults)
    try:
        ota.Uploader(dev.send, dev.recv).upload(img)
    finally:
        dev.close()
    return out, dev.sent


def test_clean(binary, scratch):
    img = image(5000)
    out, sent = run(binary, scratch, "clean", img)
    assert open(out, "rb").read() == img
    assert sent == 1 + (5000 + 119) // 120 + 1        # begin, the chunks, end


def test_faults(binary, scratch):
    img = image(3001)
    plan = {3: "drop", 5: "flip", 8: "lose_answer", 11: "twice", 14: "flip", 15: "drop"}
    out, sent = run(binary, scratch, "faults", img, lambda n, t: plan.get(n, "ok"))
    assert open(out, "rb").read() == img
    assert sent > 1 + (3001 + 119) // 120 + 1


def refused(binary, scratch, name, img, why):
    try:
        run(binary, scratch, name, img)
    except ota.OtaError as e:
        assert why in str(e), str(e)
        return
    raise AssertionError(name + ": accepted")


def test_refusals(binary, scratch):
    refused(binary, scratch, "wrongchip", image(500, chip=5), "chip id")          # the sender's own check
    refused(binary, scratch, "toobig", image((1 << 20) + 1), "size")              # the slot is 1 MB in the replay
    # the device's check, past the sender's: a header the sender cannot see is wrong
    out = os.path.join(scratch, "hdr.flashed")
    dev = Device(binary, out)
    bad = bytearray(image(500)); bad[0] = 0
    dev.send("ota begin 500 %08x" % zlib.crc32(bytes(bad)))
    assert dev.recv(0) == "ok 0 120"
    dev.send(ota.data_line(0, bytes(bad[:120])))
    assert dev.recv(0).startswith("err header 0")
    dev.send("ota begin 500 00000000")                                             # a transfer is running
    assert dev.recv(0).startswith("err busy")
    dev.send("ota abort")
    assert dev.recv(0) == "ok 0"
    dev.send("ota end")
    assert dev.recv(0).startswith("err state")
    dev.close()


def test_busy_from_an_earlier_run(binary, scratch):
    out = os.path.join(scratch, "busy.flashed")
    dev = Device(binary, out)
    dev.send("ota begin 500 00000000")
    dev.pending.clear()
    img = image(500)
    ota.Uploader(dev.send, dev.recv).upload(img)
    dev.close()
    assert open(out, "rb").read() == img


def test_idle(binary, scratch):
    dev = Device(binary, os.path.join(scratch, "idle.flashed"))
    dev.send("ota begin 500 00000000")
    dev.send("idle 19999")
    dev.send("idle 2")
    assert dev.pending[1:] == ["tick", "dropped"]
    dev.close()


def test_frames():
    f = ota.frame("get\n")
    assert f == [0xF0, 0x7D, 0x4A, 0x4B, 0x67, 0x65, 0x74, 0x0A, 0xF7]
    lines = ota.Lines()
    lines.feed([0xF0, 0x7D, 0x4A, 0x4B] + list(b"ok 1") + [0xF7])
    assert lines.pop() is None
    lines.feed([0xF0, 0x7D, 0x4A, 0x4B] + list(b"20\nok") + [0xF7])
    assert lines.pop() == "ok 120" and lines.pop() is None
    lines.feed([0xF0, 0x7D, 0x00, 0x00, 0x41, 0xF7])
    assert lines.pop() is None


if __name__ == "__main__":
    binary, scratch = sys.argv[1], sys.argv[2]
    os.makedirs(scratch, exist_ok=True)
    test_frames()
    test_clean(binary, scratch)
    test_faults(binary, scratch)
    test_refusals(binary, scratch)
    test_busy_from_an_earlier_run(binary, scratch)
    test_idle(binary, scratch)
    print("ota (python): ok")
