#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Updates a jank over its USB MIDI port: no button, no download mode (docs/usb.md, "Updating over USB MIDI").

    python scripts/ota.py [jank.bin] [--port NAME] [--check]

Finds the MIDI port named exactly "jank", reads `get`, sends the image in acknowledged chunks on the control line, waits
for the stick to restart into it and prints `get` again. Needs python-rtmidi (`pip install python-rtmidi`); --check only
looks at the image and needs nothing. The protocol is documented in core/jank_ota.h; the same code runs in the tests
against the firmware's own C logic (tests/test_ota.py).
"""
import argparse
import os
import struct
import sys
import time
import zlib

HEAD = bytes([0xF0, 0x7D, 0x4A, 0x4B])
CHUNK = 120                 # bytes per chunk; the device may ask for less
PORT = "jank"
CHIP_ESP32S3 = 9
REPLY_TIMEOUT = 2.0         # seconds for one answer
TRIES = 6                   # sends of one chunk before giving up
BACK_TIMEOUT = 60.0         # seconds for the stick to come back after the restart


class OtaError(Exception):
    pass


def frame(text):
    """One control line as a SysEx message: F0 7D 'J' 'K' <7-bit text> F7."""
    data = text.encode("ascii")
    if any(b >= 0x80 for b in data):
        raise ValueError("control lines are 7-bit text")
    return list(HEAD + data + b"\xF7")


class Lines:
    """The device's SysEx frames back into lines of text (a frame carries 30 characters, a line spans frames)."""

    def __init__(self):
        self.buf = ""
        self.lines = []

    def feed(self, msg):
        if len(msg) < 5 or bytes(msg[:4]) != HEAD or msg[-1] != 0xF7:
            return
        self.buf += bytes(msg[4:-1]).decode("ascii", "replace")
        while "\n" in self.buf:
            line, self.buf = self.buf.split("\n", 1)
            self.lines.append(line.strip())

    def pop(self):
        return self.lines.pop(0) if self.lines else None


def check_image(image):
    """The image's header as the chip's loader wants it; raises OtaError."""
    if len(image) < 24:
        raise OtaError("image too short")
    if image[0] != 0xE9 or not 1 <= image[1] <= 16:
        raise OtaError("not an ESP image (first byte %#x)" % image[0])
    chip = struct.unpack_from("<H", image, 12)[0]
    if chip != CHIP_ESP32S3:
        raise OtaError("image is for chip id %d, not the ESP32-S3 (9)" % chip)


def data_line(offset, chunk):
    import base64
    return "ota d %d %08x %s" % (offset, zlib.crc32(chunk), base64.b64encode(chunk).decode("ascii"))


class Uploader:
    """The sender's half of the protocol. send(text) writes one control line; recv(timeout) -> a line or None."""

    def __init__(self, send, recv, progress=None):
        self.send, self.recv, self.progress = send, recv, progress or (lambda done, total: None)

    def ask(self, text):
        """Sends a line and returns the answer, "ok ..." or "err ...", resending on silence."""
        for _ in range(TRIES):
            while self.recv(0) is not None:
                pass                          # old answers to sends of ours that went unanswered in time
            self.send(text)
            end = time.monotonic() + REPLY_TIMEOUT
            while time.monotonic() < end:
                line = self.recv(max(0.0, end - time.monotonic()))
                if line is not None and (line.startswith("ok") or line.startswith("err")):
                    return line
        raise OtaError("no answer to %r" % text.split(" ")[0:2])

    def begin(self, image):
        line = "ota begin %d %08x" % (len(image), zlib.crc32(image))
        answer = self.ask(line)
        if answer.startswith("err busy"):     # a transfer of an earlier run
            self.ask("ota abort")
            answer = self.ask(line)
        parts = answer.split()
        if parts[0] != "ok" or len(parts) < 3:
            raise OtaError("the stick refused: %s" % answer)
        return min(CHUNK, int(parts[2]))

    def upload(self, image):
        check_image(image)
        chunk, pos, bad = self.begin(image), 0, 0
        try:
            while pos < len(image):
                answer = self.ask(data_line(pos, image[pos:pos + chunk]))
                parts = answer.split()
                if parts[0] == "ok":
                    pos, bad = int(parts[1]), 0
                    self.progress(pos, len(image))
                elif parts[1] in ("gap", "crc") and bad < TRIES:
                    pos, bad = int(parts[2]), bad + 1     # go on from where the stick is
                else:
                    raise OtaError("the stick stopped the transfer: %s" % answer)
            answer = self.ask("ota end")
            if not answer.startswith("ok"):
                raise OtaError("the image was not accepted: %s" % answer)
        except (OtaError, KeyboardInterrupt):
            self.send("ota abort")
            raise


class Midi:
    """python-rtmidi, with the port named exactly PORT."""

    def __init__(self, name=PORT):
        import rtmidi
        self.rtmidi, self.name = rtmidi, name
        self.out = self.inp = None
        self.lines = Lines()

    def _find(self, port):
        names = port.get_ports()
        return names.index(self.name) if self.name in names else None

    def open(self):
        out, inp = self.rtmidi.MidiOut(), self.rtmidi.MidiIn()
        o, i = self._find(out), self._find(inp)
        if o is None or i is None:
            return False
        inp.ignore_types(sysex=False, timing=True, active_sense=True)
        out.open_port(o)
        inp.open_port(i)
        self.out, self.inp, self.lines = out, inp, Lines()
        return True

    def close(self):
        for p in (self.out, self.inp):
            if p is not None:
                p.close_port()
        self.out = self.inp = None

    def send(self, text):
        self.out.send_message(frame(text if text.endswith("\n") else text + "\n"))

    def recv(self, timeout):
        end = time.monotonic() + timeout
        while True:
            line = self.lines.pop()
            if line is not None:
                return line
            got = self.inp.get_message()
            if got:
                self.lines.feed(got[0])
            elif time.monotonic() >= end:
                return None
            else:
                time.sleep(0.0005)


def get(midi, timeout=3.0):
    """`get` -> {key: value} (ends at its "ok"); None if the stick does not answer."""
    midi.send("get")
    out, end = {}, time.monotonic() + timeout
    while time.monotonic() < end:
        line = midi.recv(0.2)
        if line == "ok":
            return out
        if line and "=" in line:
            k, v = line.split("=", 1)
            out[k] = v
    return None


def wait_back(name, timeout=BACK_TIMEOUT):
    """The stick restarts: wait for its port, then for `get`. -> (Midi, answer)."""
    time.sleep(1.5)                           # it restarts 100 ms after its answer; the old port goes away
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        m = Midi(name)
        try:
            if m.open():
                time.sleep(0.5)
                info = get(m)
                if info:
                    return m, info
            m.close()
        except Exception:                      # CoreMIDI is half-way through re-enumerating
            pass
        time.sleep(1.0)
    raise OtaError("the stick did not come back within %d s (it falls back to the old image after a failed start)" % timeout)


def show(done, total, t0=[0.0]):
    if not done:
        t0[0] = time.monotonic()
    rate = done / max(time.monotonic() - t0[0], 1e-3) / 1024
    sys.stdout.write("\r  %5.1f%%  %d / %d bytes  %.1f KB/s " % (100.0 * done / total, done, total, rate))
    sys.stdout.flush()


def default_image():
    scratch = os.environ.get("JANK_SCRATCH") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worktrees", "jank-scratch")
    return os.path.join(scratch, "build-xiao", "jank.bin")


def main(argv=None):
    ap = argparse.ArgumentParser(description="Update a jank over USB MIDI.")
    ap.add_argument("image", nargs="?", default=default_image(), help="the app image (default: build-xiao/jank.bin)")
    ap.add_argument("--port", default=PORT, help="the MIDI port's name (default: %(default)s)")
    ap.add_argument("--check", action="store_true", help="check the image and stop")
    args = ap.parse_args(argv)
    image = open(args.image, "rb").read()
    check_image(image)
    print("image %s: %d bytes, crc32 %08x" % (args.image, len(image), zlib.crc32(image)))
    if args.check:
        return 0
    m = Midi(args.port)
    if not m.open():
        print('no MIDI port named exactly "%s" (in and out); is the stick plugged in and running the app?' % args.port)
        return 1
    before = get(m)
    if before is None:
        print("the stick does not answer `get`")
        return 1
    print("now: version %s, name %s" % (before.get("version"), before.get("name")))
    up = Uploader(m.send, m.recv, show)
    show(0, len(image))
    up.upload(image)
    print("\n  accepted; the stick restarts")
    m.close()
    m, after = wait_back(args.port)
    m.close()
    print("back: version %s, name %s" % (after.get("version"), after.get("name")))
    for k in sorted(after):
        print("  %s=%s" % (k, after[k]))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except OtaError as e:
        print("\nupdate failed: %s" % e)
        sys.exit(1)
