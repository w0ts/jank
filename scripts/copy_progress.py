#!/usr/bin/env python3
"""Copies a file with a progress line: copy_progress.py SOURCE DESTINATION.

The jank's drive is slow to write (its flash is written as the data arrives), and `cp` says nothing meanwhile. Each chunk is
flushed to the drive before the next, so the percentage and the speed are what the stick has really taken."""
import os
import sys
import time

CHUNK = 64 * 1024


def render(done: int, total: int, started: float) -> str:
    seconds = max(time.monotonic() - started, 1e-6)
    percent = 100 * done // total if total else 100
    return "\rcopy: %3d%%  %5.2f / %.2f MB  %5.1f KB/s" % (percent, done / 1e6, total / 1e6, done / 1024 / seconds)


def copy(source: str, destination: str) -> int:
    total = os.path.getsize(source)
    done = 0
    started = time.monotonic()
    with open(source, "rb") as src, open(destination, "wb") as dst:
        while True:
            block = src.read(CHUNK)
            if not block:
                break
            dst.write(block)
            dst.flush()
            os.fsync(dst.fileno())
            done += len(block)
            sys.stdout.write(render(done, total, started))
            sys.stdout.flush()
    sys.stdout.write("\ncopied %d bytes in %.1f s\n" % (done, time.monotonic() - started))
    return 0 if done == total else 1


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        sys.stderr.write("usage: copy_progress.py SOURCE DESTINATION\n")
        return 2
    return copy(argv[1], argv[2])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
