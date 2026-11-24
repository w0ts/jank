#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Makes the key for updating a jank over Wi-Fi, once, and gives it to the unit over USB (core/jank_wota.h, docs/wifi.md).

    python scripts/otakey.py            # a key is made if there is none (~/.config/jank/otakey) and sent to the stick on USB
    python scripts/otakey.py --status   # does the stick have a key? (it never says what it is)
    python scripts/otakey.py --clear    # the stick forgets its key: no Wi-Fi update until one is set again
    python scripts/otakey.py --rotate   # a new key replaces the old one, on this Mac and on the stick

The key is 32 random bytes in a file only this user can read, outside every repository; it is printed nowhere. The stick must be plugged into this
computer (not the hub) and no other MIDI client may talk to it (close the Chrome control page). Needs python-rtmidi."""
from __future__ import annotations

import argparse
import os
import secrets
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

KEY_FILE_DEFAULT = os.path.join("~", ".config", "jank", "otakey")
KEY_BYTES = 32


def read_key(path: str) -> bytes | None:
    """The key in the file, or None if there is no file; ValueError if it is not 64 hex characters."""
    full = os.path.expanduser(path)
    if not os.path.exists(full):
        return None
    with open(full, encoding="ascii") as f:
        text = f.read().strip()
    key = bytes.fromhex(text)
    if len(text) != 2 * KEY_BYTES or len(key) != KEY_BYTES:
        raise ValueError("%s does not hold %d bytes as 64 hex characters" % (path, KEY_BYTES))
    return key


def write_key(path: str, key: bytes) -> None:
    """The key as 64 hex characters in a file that only this user can open (made with those rights: it is never readable by others, not even
    for a moment); the folder around it likewise."""
    full = os.path.expanduser(path)
    os.makedirs(os.path.dirname(full), mode=0o700, exist_ok=True)
    fd = os.open(full, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="ascii") as f:
        f.write(key.hex() + "\n")
    os.chmod(full, 0o600)


def ensure_key(path: str, rotate: bool = False) -> tuple[bytes, bool]:
    """-> (the key, whether it was made now). An existing file is kept unless rotate."""
    key = None if rotate else read_key(path)
    if key is not None:
        return key, False
    key = secrets.token_bytes(KEY_BYTES)
    write_key(path, key)
    return key, True


def send(port, line: str) -> tuple[bool, list[str]]:
    """One control line to the stick; ctl.py's exchange (the line may carry the key: it is not echoed anywhere here)."""
    import ctl

    return ctl.run_line(port, line, 4.0)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="make the Wi-Fi update key and give it to the stick over USB")
    ap.add_argument("--key-file", default=os.environ.get("JANK_OTA_KEY_FILE", KEY_FILE_DEFAULT))
    ap.add_argument("--port", default="Jank", help="the MIDI port's name (default Jank)")
    ap.add_argument("--status", action="store_true", help="only ask whether the stick has a key")
    ap.add_argument("--clear", action="store_true", help="the stick forgets its key")
    ap.add_argument("--rotate", action="store_true", help="make a new key even if there is one")
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
        if args.status:
            ok, lines = send(midi, "otakey")
            sys.stdout.write("".join(line + "\n" for line in lines))
            return 0 if ok else 1
        if args.clear:
            ok, lines = send(midi, "otakey clear")
            sys.stdout.write("the stick forgot its key\n" if ok else "the stick did not answer ok: %s\n" % " ".join(lines))
            return 0 if ok else 1
        try:
            key, made = ensure_key(args.key_file, args.rotate)
        except (OSError, ValueError) as e:
            sys.stdout.write("the key file is not usable: %s\n" % e)
            return 1
        ok, lines = send(midi, "otakey " + key.hex())
        if not ok:
            sys.stdout.write("the stick did not take the key: %s\n" % " ".join(lines))
            return 1
        sys.stdout.write("%s %s; the stick has it (check: scripts/otakey.py --status)\n" % ("a new key was made in" if made else "the key in", args.key_file))
    finally:
        midi.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
