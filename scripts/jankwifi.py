#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Talks to a jank over Wi-Fi: one read-only control line in a UDP datagram, the answer in numbered chunks (core/jank_wsvc.h).

    python scripts/jankwifi.py get                      # to everyone on the network: every jank answers
    python scripts/jankwifi.py --ip 192.168.1.10 diag   # to one
    python scripts/jankwifi.py --ip 192.168.1.10 ota build/jank.bin   # update the firmware over Wi-Fi (signed: needs the key, see below)

Lines the unit runs over Wi-Fi: get, status, about, home, beat, diag, line, drive (all read-only; it refuses the rest). The unit must be
in MODE WIFI (`set net wifi` with the network's name and password set, over USB: scripts/ctl.py), so that it has an address on the
network. While it is a USB host, `diag` shows what it sees live (the `hostdiag_` lines). The network's password is never needed here.
Exit status 0: at least one complete answer.

`ota <image>` sends the firmware in signed UDP frames (core/jank_wota.h) and confirms it once the unit has restarted into it. The key is 32 random
bytes the unit has been given over USB (scripts/otakey.py makes and sets it); this script reads it from --key-file (default
~/.config/jank/otakey, 64 hex characters), never from the repository. Exit status 0: updated and confirmed."""
from __future__ import annotations

import argparse
import hmac
import hashlib
import os
import socket
import struct
import sys
import time
import zlib

PORT = 7447
LINE_MAX = 120
BROADCAST = "255.255.255.255"
POLL_S = 0.1

# the signed update (core/jank_wota.h)
MAGIC = 0x01
T_HELLO, T_CMD, T_HELLO_R, T_CMD_R = 0x01, 0x02, 0x81, 0x82
OP_BEGIN, OP_DATA, OP_END, OP_ABORT, OP_CONFIRM = 1, 2, 3, 4, 5
NONCE_LEN, MAC_LEN, KEY_LEN = 8, 16, 32
DATA_MAX = 1024
KEY_FILE_DEFAULT = os.path.join("~", ".config", "jank", "otakey")
# statuses: core/jank_ota.h enum jo_err, then core/jank_wota.h JWO_S_*
STATUS = {
    0: "ok", 1: "ok (an old chunk again)", 2: "busy (a transfer is running)", 3: "bad size", 4: "bad argument", 5: "no transfer / flash error",
    6: "a chunk from beyond the offset", 7: "CRC wrong", 8: "not an ESP32-S3 image", 9: "ended before the last byte", 10: "dropped (idle)",
    11: "playing", 0x80: "unknown op", 0x81: "not on probation (nothing to confirm)", 0x82: "too early to confirm",
}


def parse_chunk(data: bytes) -> tuple[int, bool, bytes] | None:
    """One reply datagram -> (index, last, text); None for an empty datagram (nothing a unit sends)."""
    if not data:
        return None
    return data[0] & 0x7F, bool(data[0] & 0x80), data[1:]


def assemble(parts: dict[int, bytes], last: int | None) -> str | None:
    """The chunks 0..last put together; None while the last chunk or any before it has not come."""
    if last is None or any(i not in parts for i in range(last + 1)):
        return None
    return b"".join(parts[i] for i in range(last + 1)).decode("ascii", "replace")


def check_line(line: str) -> str:
    """The line as the unit would take it, or ValueError: it answers nothing to a line it cannot read."""
    text = line.strip()
    if not text or len(text) > LINE_MAX or not all(0x20 <= ord(c) < 0x7F for c in text):
        raise ValueError("a control line is 1 to %d printable ASCII characters" % LINE_MAX)
    return text


def ask(line: str, ip: str | None = None, port: int = PORT, timeout: float = 3.0, retry_s: float | None = None) -> dict[str, str | None]:
    """Sends the line (to ip, or to everyone) and collects the answers by sender: the text, or None for an answer with a hole in it.
    One sender in particular: ends when its answer is whole. Not whole by half the time: the line is sent once more."""
    text = check_line(line)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    dst = (ip or BROADCAST, port)
    chunks: dict[str, dict[int, bytes]] = {}
    last: dict[str, int] = {}
    start = time.monotonic()
    deadline = start + timeout
    sent_at = start
    resent = False

    def send() -> None:
        """A send that fails (macOS: 'Host is down' while ARP for the address fails, 'No route to host') is a lost datagram, not the end."""
        try:
            sock.sendto(text.encode("ascii"), dst)
        except OSError:
            pass

    send()
    try:
        while time.monotonic() < deadline:
            sock.settimeout(max(0.001, min(POLL_S, deadline - time.monotonic())))
            try:
                data, addr = sock.recvfrom(2048)
            except socket.timeout:
                data, addr = b"", None
            except OSError:
                data, addr = b"", None
                time.sleep(POLL_S)
            parsed = parse_chunk(data)
            if parsed and addr:
                index, is_last, body = parsed
                chunks.setdefault(addr[0], {})[index] = body
                if is_last:
                    last[addr[0]] = index
            whole = [a for a in chunks if assemble(chunks[a], last.get(a)) is not None]
            if ip and whole:
                break
            now = time.monotonic()
            if retry_s is not None and not whole and now - sent_at >= retry_s:
                send()
                sent_at = now
            elif retry_s is None and not resent and not whole and now - start > timeout / 2:
                send()
                resent = True
    finally:
        sock.close()
    return {a: assemble(chunks[a], last.get(a)) for a in sorted(chunks)}


def load_key(path: str) -> bytes:
    """The update key: 64 hex characters in a file (whitespace around them is fine); ValueError for anything else."""
    with open(os.path.expanduser(path), encoding="ascii") as f:
        text = f.read().strip()
    key = bytes.fromhex(text)
    if len(text) != 2 * KEY_LEN or len(key) != KEY_LEN:
        raise ValueError("the key is %d bytes: 64 hex characters" % KEY_LEN)
    return key


def frame_mac(key: bytes, nonce: bytes, body: bytes) -> bytes:
    """The 16 bytes that sign `body` in the session `nonce`: HMAC-SHA-256(key, nonce || body), cut."""
    return hmac.new(key, nonce + body, hashlib.sha256).digest()[:MAC_LEN]


def build_cmd(key: bytes, nonce: bytes, counter: int, op: int, args: bytes = b"") -> bytes:
    """A signed request: magic, type, counter, op, args, mac."""
    body = bytes([MAGIC, T_CMD]) + struct.pack(">IB", counter, op) + args
    return body + frame_mac(key, nonce, body)


def begin_args(size: int, crc: int, target: int = 0) -> bytes:
    return struct.pack(">IIB", size, crc, target)


def data_args(offset: int, chunk: bytes) -> bytes:
    return struct.pack(">IIH", offset, zlib.crc32(chunk) & 0xFFFFFFFF, len(chunk)) + chunk


def check_reply(key: bytes, nonce: bytes, frame: bytes) -> tuple[int, int, int, int] | None:
    """A reply from the unit that has the key -> (counter, status, offset, chunk); None for anything else (a stranger's datagram, a damaged one)."""
    if len(frame) != 2 + 4 + 1 + 4 + 2 + MAC_LEN or frame[0] != MAGIC or frame[1] != T_CMD_R:
        return None
    body, mac = frame[:-MAC_LEN], frame[-MAC_LEN:]
    if not hmac.compare_digest(frame_mac(key, nonce, body), mac):
        return None
    counter, status, offset, chunk = struct.unpack(">IBIH", body[2:])
    return counter, status, offset, chunk


class UpdateError(Exception):
    """The update stopped: the unit answered an error, or did not answer."""


class Session:
    """One signed session with a unit: a HELLO for a nonce, then requests with rising counters, each one stop and wait. A try that gets no
    answer is sent again with a higher counter (a replay would be refused); the unit's transfer takes a chunk it has already as a duplicate."""

    def __init__(self, ip: str, key: bytes, port: int = PORT, timeout: float = 0.6, tries: int = 10) -> None:
        self.dst = (ip, port)
        self.key = key
        self.timeout = timeout
        self.tries = tries
        self.nonce = b""
        self.counter = 0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def close(self) -> None:
        self.sock.close()

    def _send(self, data: bytes) -> None:
        try:
            self.sock.sendto(data, self.dst)
        except OSError:
            pass  # a datagram lost (macOS: 'Host is down' while ARP fails): the next try goes out again

    def _recv(self, wait: float) -> bytes:
        self.sock.settimeout(max(0.001, wait))
        try:
            data, addr = self.sock.recvfrom(2048)
        except (socket.timeout, OSError):
            return b""
        return data if addr[0] == self.dst[0] else b""

    def hello(self, wait_s: float = 8.0) -> None:
        """Opens a session. A unit that is busy with another sender's session ignores a HELLO for a few seconds: this asks again until wait_s."""
        end = time.monotonic() + wait_s
        while time.monotonic() < end:
            self._send(bytes([MAGIC, T_HELLO]))
            data = self._recv(min(0.8, end - time.monotonic()))
            if len(data) == 2 + NONCE_LEN and data[0] == MAGIC and data[1] == T_HELLO_R:
                self.nonce = data[2:]
                self.counter = 0
                return
        raise UpdateError("no answer to HELLO: the unit is not in MODE WIFI, has no update key (scripts/otakey.py), or another update is running")

    def call(self, op: int, args: bytes = b"") -> tuple[int, int, int]:
        """One request -> (status, offset, chunk). Tries again, with a higher counter each time, until a reply that checks out comes."""
        first = self.counter + 1
        for _ in range(self.tries):
            self.counter += 1
            self._send(build_cmd(self.key, self.nonce, self.counter, op, args))
            end = time.monotonic() + self.timeout
            while time.monotonic() < end:
                parsed = check_reply(self.key, self.nonce, self._recv(end - time.monotonic()))
                if parsed and first <= parsed[0] <= self.counter:
                    return parsed[1], parsed[2], parsed[3]
        raise UpdateError("no answer from the unit (op %d, %d tries): out of range, or the session ended (30 s of silence)" % (op, self.tries))


def transfer(session: Session, image: bytes, log=lambda s: None, target: int = 0) -> None:
    """BEGIN, the chunks, END. Raises UpdateError with the unit's own words on an error. A lost chunk or a damaged one is sent again from the
    offset the unit names."""
    crc = zlib.crc32(image) & 0xFFFFFFFF
    status, _, chunk = session.call(OP_BEGIN, begin_args(len(image), crc, target))
    if status != 0:
        raise UpdateError("the unit refused the update: %s" % STATUS.get(status, status))
    size = max(1, min(chunk or DATA_MAX, DATA_MAX))
    offset, last_pct, bad = 0, -1, 0
    while offset < len(image):
        piece = image[offset : offset + size]
        status, nxt, _ = session.call(OP_DATA, data_args(offset, piece))
        if status in (0, 1):
            offset = nxt
            bad = 0
        elif status in (6, 7) and bad < 5:  # a gap or a damaged chunk: on from where the unit is
            offset = nxt
            bad += 1
        else:
            raise UpdateError("the unit stopped the update at %d: %s" % (offset, STATUS.get(status, status)))
        pct = offset * 100 // len(image)
        if pct != last_pct and pct % 5 == 0:
            log("sent %3d%%  (%d of %d bytes)" % (pct, offset, len(image)))
            last_pct = pct
    try:
        status, _, _ = session.call(OP_END)
    except UpdateError:
        # An accepted image restarts the unit a moment after its answer: that answer can be lost, and the END sent again finds nobody. The
        # caller's wait for the restart settles it (a unit that did not restart is found out there).
        log("no answer to END: the unit may already be restarting")
        return
    if status != 0:
        raise UpdateError("the unit did not accept the image: %s" % STATUS.get(status, status))


def uptime_of(text: str | None) -> int | None:
    """uptime_s from a unit's `about` answer."""
    for line in (text or "").splitlines():
        if line.startswith("uptime_s="):
            try:
                return int(line.split("=", 1)[1])
            except ValueError:
                return None
    return None


def read_uptime(ip: str, port: int = PORT) -> int | None:
    answers = ask("about", ip, port, 2.0)
    return uptime_of(answers.get(ip))


APP_DESC_AT = 32  # an ESP-IDF app image: 24-byte header, an 8-byte segment header, then esp_app_desc_t
APP_DESC_MAGIC = bytes.fromhex("3254cdab")  # 0xABCD5432, little-endian
APP_ELF_SHA_AT = APP_DESC_AT + 4 + 4 + 8 + 32 + 32 + 16 + 16 + 32  # magic, secure_version, reserv1[2], version, project_name, time, date, idf_ver


def image_id(image: bytes) -> str | None:
    """Which image this file is: the first 4 bytes of the ELF's SHA-256 that the app descriptor holds, as 8 hex characters (what the unit's `diag`
    reports as image=); None if the file is not an ESP-IDF app image."""
    if len(image) < APP_ELF_SHA_AT + 4 or image[APP_DESC_AT : APP_DESC_AT + 4] != APP_DESC_MAGIC:
        return None
    return image[APP_ELF_SHA_AT : APP_ELF_SHA_AT + 4].hex()


def running_image(ip: str, port: int = PORT) -> str | None:
    """The image= line of the unit's `diag`: which image runs now."""
    text = ask("diag", ip, port, 3.0).get(ip) or ""
    for line in text.splitlines():
        if line.startswith("image="):
            return line.split("=", 1)[1].strip()
    return None


def verify_image(ip: str, expected: str | None, port: int = PORT, log=lambda s: None) -> None:
    """After the update: the unit must run the image that was sent. A unit that went back to the old one (a restart while the new image was still on
    probation rolls it back) says `confirmed` or `not on probation` all the same, so this is the real answer. UpdateError if it does not."""
    if expected is None:
        log("(the file is not an ESP-IDF app image: cannot check which one runs)")
        return
    now = None
    for _ in range(5):
        now = running_image(ip, port)
        if now is not None:
            break
        time.sleep(1.0)
    if now is None:
        raise UpdateError("could not read which image the unit runs (its diag has no image= line: an older image)")
    if now != expected:
        raise UpdateError("the unit runs image %s, not the one sent (%s): the new image was rolled back" % (now, expected))
    log("the unit runs the new image (%s)" % now)


def wait_restart(ip: str, up_before: int, port: int = PORT, wait_s: float = 120.0, log=lambda s: None) -> int:
    """After END: waits for the unit to answer from a new run (its uptime below what it was) -> that uptime. UpdateError if it does not."""
    end = time.monotonic() + wait_s
    while time.monotonic() < end:
        up = read_uptime(ip, port)
        if up is not None and up < up_before:
            return up
        time.sleep(1.0)
    raise UpdateError("the unit did not come back within %d s (it rolls back to the old image by itself after 2 minutes)" % wait_s)


def confirm(ip: str, key: bytes, up_now: int, port: int = PORT, log=lambda s: None) -> None:
    """The unit has restarted into the new image, which is on probation: once it has run 20 s a signed CONFIRM makes it stay."""
    deadline = time.monotonic() + 100.0
    up = up_now
    while time.monotonic() < deadline:
        if up < 22:
            time.sleep(max(0.5, 22 - up))
            now_up = read_uptime(ip, port)
            if now_up is None:
                continue
            up = now_up
            continue
        s = Session(ip, key, port)
        try:
            s.hello()
            status, _, _ = s.call(OP_CONFIRM)
        finally:
            s.close()
        if status == 0:
            log("the new image is confirmed")
            return
        if status == 0x81:
            log("the unit says its image is not on probation (confirmed already, or rolled back: the next check says which)")
            return
        if status != 0x82:
            raise UpdateError("the unit would not confirm: %s" % STATUS.get(status, status))
        time.sleep(1.0)
        up += 1
    raise UpdateError("not confirmed in time: the unit goes back to the old image by itself")


def ota_main(ip: str, image_path: str, key_path: str, port: int, do_confirm: bool) -> int:
    """`ota <image>`: the whole update, progress on stdout. 0 updated (and confirmed), 1 failed."""
    try:
        key = load_key(key_path)
    except (OSError, ValueError) as e:
        sys.stdout.write("no usable update key at %s (%s): scripts/otakey.py makes one and sets it over USB\n" % (key_path, e))
        return 1
    try:
        with open(image_path, "rb") as f:
            image = f.read()
    except OSError as e:
        sys.stdout.write("cannot read the image: %s\n" % e)
        return 1

    def log(s: str) -> None:
        sys.stdout.write("%s\n" % s)
        sys.stdout.flush()

    up_before = read_uptime(ip, port)
    if up_before is None:
        sys.stdout.write("no answer from %s: the unit must be in MODE WIFI and on the same network (docs/wifi.md)\n" % ip)
        return 1
    s = Session(ip, key, port)
    try:
        log("updating %s with %s (%d bytes)" % (ip, image_path, len(image)))
        s.hello()
        transfer(s, image, log)
        s.close()
        log("image accepted, the unit restarts")
        up = wait_restart(ip, up_before, port, log=log)
        log("the unit is back (uptime %d s)" % up)
        if do_confirm:
            confirm(ip, key, up, port, log)
            verify_image(ip, image_id(image), port, log)
        else:
            log("not confirming (--no-confirm): the unit goes back to the old image by itself within 2 minutes")
    except UpdateError as e:
        sys.stdout.write("update failed: %s\n" % e)
        return 1
    finally:
        s.close()
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="send one read-only control line to a jank over Wi-Fi, or update its firmware (ota <image>)")
    ap.add_argument("line", nargs="+", help="the control line, e.g. get, status, diag; or: ota <image.bin>")
    ap.add_argument("--key-file", default=os.environ.get("JANK_OTA_KEY_FILE", KEY_FILE_DEFAULT), help="the update key, 64 hex characters (ota only)")
    ap.add_argument("--no-confirm", action="store_true", help="ota: do not confirm the new image (it rolls back by itself)")
    ap.add_argument("--ip", help="the unit's address (default: ask everyone on the network)")
    ap.add_argument("--port", type=int, default=PORT)
    ap.add_argument("--timeout", type=float, default=3.0, help="seconds to wait for answers")
    ap.add_argument("--wait", type=float, help="keep asking, once a second, for up to this many seconds (a unit that restarts, or joins Wi-Fi late)")
    args = ap.parse_args(argv)
    if args.line[0] == "ota":
        if len(args.line) != 2 or not args.ip:
            sys.stdout.write("usage: jankwifi.py --ip <address> ota <image.bin>\n")
            return 2
        return ota_main(args.ip, args.line[1], args.key_file, args.port, not args.no_confirm)
    try:
        if args.wait:
            answers = ask(" ".join(args.line), args.ip, args.port, args.wait, retry_s=1.0)
        else:
            answers = ask(" ".join(args.line), args.ip, args.port, args.timeout)
    except ValueError as e:
        sys.stdout.write("%s\n" % e)
        return 2
    if not answers:
        sys.stdout.write("no answer: the unit must be in MODE WIFI and on the same network (docs/wifi.md)\n")
        return 1
    ok = False
    for addr, text in answers.items():
        if text is None:
            sys.stdout.write("# %s: the answer is incomplete (a datagram was lost): ask again\n" % addr)
            continue
        ok = True
        sys.stdout.write("# %s\n%s" % (addr, text))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
