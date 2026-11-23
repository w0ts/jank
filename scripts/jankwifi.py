#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Talks to a jank over Wi-Fi: one read-only control line in a UDP datagram, the answer in numbered chunks (core/jank_wsvc.h).

    python scripts/jankwifi.py get                      # to everyone on the network: every jank answers
    python scripts/jankwifi.py --ip 192.168.1.10 diag   # to one

Lines the unit runs over Wi-Fi: get, status, about, home, beat, diag, line, drive (all read-only; it refuses the rest). The unit must be
in MODE WIFI (`set net wifi` with the network's name and password set, over USB: scripts/ctl.py), so that it has an address on the
network. While it is a USB host, `diag` shows what it sees live (the `hostdiag_` lines). The network's password is never needed here.
Exit status 0: at least one complete answer."""
from __future__ import annotations

import argparse
import socket
import sys
import time

PORT = 7447
LINE_MAX = 120
BROADCAST = "255.255.255.255"
POLL_S = 0.1


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


def ask(line: str, ip: str | None = None, port: int = PORT, timeout: float = 3.0) -> dict[str, str | None]:
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
    resent = False
    sock.sendto(text.encode("ascii"), dst)
    try:
        while time.monotonic() < deadline:
            sock.settimeout(max(0.001, min(POLL_S, deadline - time.monotonic())))
            try:
                data, addr = sock.recvfrom(2048)
            except socket.timeout:
                data, addr = b"", None
            parsed = parse_chunk(data)
            if parsed and addr:
                index, is_last, body = parsed
                chunks.setdefault(addr[0], {})[index] = body
                if is_last:
                    last[addr[0]] = index
            whole = [a for a in chunks if assemble(chunks[a], last.get(a)) is not None]
            if ip and whole:
                break
            if not resent and not whole and time.monotonic() - start > timeout / 2:
                sock.sendto(text.encode("ascii"), dst)
                resent = True
    finally:
        sock.close()
    return {a: assemble(chunks[a], last.get(a)) for a in sorted(chunks)}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="send one read-only control line to a jank over Wi-Fi")
    ap.add_argument("line", nargs="+", help="the control line, e.g. get, status, diag")
    ap.add_argument("--ip", help="the unit's address (default: ask everyone on the network)")
    ap.add_argument("--port", type=int, default=PORT)
    ap.add_argument("--timeout", type=float, default=3.0, help="seconds to wait for answers")
    args = ap.parse_args(argv)
    try:
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
