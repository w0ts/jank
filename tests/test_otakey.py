#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""scripts/otakey.py's key file and the line it sends: a key is made once, kept, readable by its owner only, never overwritten except on rotate,
and the line to the stick carries it without anything echoing it. No MIDI, no device. Usage: test_otakey.py"""
from __future__ import annotations

import importlib.util
import os
import stat
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("otakey", os.path.join(here, "..", "scripts", "otakey.py"))
assert spec is not None and spec.loader is not None
ok = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ok)

checks = 0


def check(cond: bool, what: str) -> None:
    global checks
    checks += 1
    if not cond:
        sys.stdout.write("FAIL %s\n" % what)
        sys.exit(1)


def test_a_key_is_made_once_and_kept() -> None:
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "sub", "otakey")
        key, made = ok.ensure_key(path)
        check(made and len(key) == 32, "made: 32 bytes")
        check(open(path, encoding="ascii").read().strip() == key.hex(), "the file holds the key as 64 hex characters")
        check(stat.S_IMODE(os.stat(path).st_mode) == 0o600, "the file is for its owner only")
        check(stat.S_IMODE(os.stat(os.path.dirname(path)).st_mode) == 0o700, "so is its folder")
        again, made_again = ok.ensure_key(path)
        check(again == key and not made_again, "an existing key is kept")
        new, made_new = ok.ensure_key(path, rotate=True)
        check(made_new and new != key and ok.read_key(path) == new, "rotate: a new key replaces it")
        check(stat.S_IMODE(os.stat(path).st_mode) == 0o600, "and the file stays private")


def test_a_bad_file_is_not_used() -> None:
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "otakey")
        check(ok.read_key(path) is None, "no file: no key")
        for text in ("abcd\n", "zz" * 32, "00" * 31, "00" * 33):
            with open(path, "w", encoding="ascii") as f:
                f.write(text)
            try:
                ok.read_key(path)
                check(False, "a bad file must raise: %r" % text[:8])
            except ValueError:
                check(True, "bad file refused")
        try:
            ok.ensure_key(path)
            check(False, "a bad key file is not silently replaced")
        except ValueError:
            check(True, "a bad file is reported, not overwritten")
        check(open(path, encoding="ascii").read() == "00" * 33, "and it is left as it was")


def test_the_line_to_the_stick() -> None:
    sent: list[str] = []

    class Fake:
        def send(self, text: str) -> None:
            sent.append(text)

        def recv(self, timeout: float) -> str | None:
            return "ok" if sent else None

    import ctl  # the real exchange, with a fake port

    result = ctl.run_line(Fake(), "otakey " + "ab" * 32, 1.0)
    check(result == (True, []) and sent == ["otakey " + "ab" * 32], "the line goes out as it is and ok comes back")
    check(ctl.run_line(Fake(), "ota key abcd", 0.2)[0] is False, "'ota key' is not a way in: ctl.py leaves ota lines to ota.py (hence the one word)")


def main() -> int:
    test_a_key_is_made_once_and_kept()
    test_a_bad_file_is_not_used()
    test_the_line_to_the_stick()
    sys.stdout.write("test_otakey: %d checks passed\n" % checks)
    return 0


if __name__ == "__main__":
    sys.path.insert(0, os.path.join(here, "..", "scripts"))
    sys.exit(main())
