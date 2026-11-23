#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make a release's firmware assets, for the GitHub release and the Install page's browser flasher.

  python3 -I scripts/publish_firmware.py TAG [--no-build]

HEAD must be the tag TAG and the checkout clean. It builds the one release image (scripts/build.sh auto: the bare XIAO or the
jank mini 3, told apart at the start, firmware/main/board.c), takes the parts from the build's flash_args (bootloader, partition table, otadata, app) and the
beatw partition's place from the built partition table, and writes the release's assets in worktrees/jank-scratch/release/TAG/:
jank-BOARD-TAG.bin (the app image: also what a copy onto the JANK drive takes), jank-BOARD-TAG-<part>.bin for the other parts,
and the manifest jank-BOARD-TAG.json (each part's asset and address, the beatw place). The release workflow
(.github/workflows/release.yml) attaches them; the Pages workflow (scripts/site_tree.py) lays every release's parts out
under the site's firmware/ with firmware/index.json. The images hold NO network weights (core/beat/beat_weights.h): the
page adds madmom's only at the user's click, in the user's browser.

The parts are written at their own addresses (not one merged image), so the device's settings (nvs) are kept.
"""
from __future__ import annotations

import json
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARDS = {"auto": "Bare device (Seeed XIAO ESP32-S3) or jank mini 3"}
BEATW_SUBTYPE = 0x40


def git(*args: str, data: bytes | None = None) -> str:
    r = subprocess.run(["git", *args], cwd=ROOT, input=data, capture_output=True, check=False)
    if r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)}: {r.stderr.decode().strip()}")
    return r.stdout.decode().strip()


def scratch() -> Path:
    return Path(os.environ.get("JANK_SCRATCH", ROOT / "worktrees" / "jank-scratch"))


def flash_parts(build: Path) -> list[tuple[int, Path]]:
    """flash_args: a line of options, then "offset file" lines (relative to the build folder)."""
    lines = (build / "flash_args").read_text().split("\n")[1:]
    parts = []
    for line in lines:
        if line.strip():
            off, rel = line.split()
            parts.append((int(off, 16), build / rel))
    names = sorted(p.name for _, p in parts)
    if names != sorted(["bootloader.bin", "partition-table.bin", "ota_data_initial.bin", "jank.bin"]):
        raise SystemExit(f"{build}/flash_args: unexpected parts {names}")
    return parts


def beatw_place(table: bytes) -> tuple[int, int]:
    for k in range(0, len(table) - 31, 32):
        magic, ptype, sub, off, size = struct.unpack_from("<HBBII", table, k)
        if magic != 0x50AA:
            break
        label = table[k + 12:k + 28].split(b"\0", 1)[0].decode()
        if ptype == 1 and sub == BEATW_SUBTYPE and label == "beatw":
            return off, size
    raise SystemExit("the partition table has no beatw partition")


def check_release(tag: str) -> None:
    if git("status", "--porcelain"):
        raise SystemExit("the checkout is not clean: commit or stash first")
    if git("rev-parse", f"{tag}^{{commit}}") != git("rev-parse", "HEAD"):
        raise SystemExit(f"HEAD is not {tag}: check out the tag first")
    model = (ROOT / "core" / "beat" / "beat_model.c").read_text()
    if "beat_l0_w" in model:
        raise SystemExit("core/beat/beat_model.c holds network weights: they must not be in a released image")


def build_board(board: str, do_build: bool) -> dict:
    if do_build:
        subprocess.run(["bash", str(ROOT / "scripts" / "build.sh"), board], cwd=ROOT, check=True)
    build = scratch() / f"build-{board}"
    parts = flash_parts(build)
    table = next(p for _, p in parts if p.name == "partition-table.bin").read_bytes()
    off, size = beatw_place(table)
    return {"parts": [(o, p.name, p.read_bytes()) for o, p in parts], "beatw": {"offset": off, "size": size}}


def asset(board: str, tag: str, part: str) -> str:
    """the release asset of a part: the app image keeps its plain name (the drive copy), the others carry the part's"""
    return f"jank-{board}-{tag}.bin" if part == "jank.bin" else f"jank-{board}-{tag}-{part[:-4]}.bin"


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    do_build = "--no-build" not in sys.argv
    if len(args) != 1:
        raise SystemExit(__doc__)
    tag = args[0]
    if not all(c.isalnum() or c in "._-" for c in tag):
        raise SystemExit("tag: letters, digits, . _ - only")
    check_release(tag)
    rel = scratch() / "release" / tag
    rel.mkdir(parents=True, exist_ok=True)
    commit = git("rev-parse", "--short", "HEAD")
    for board, label in BOARDS.items():
        b = build_board(board, do_build)
        for _, name, data in b["parts"]:
            (rel / asset(board, tag, name)).write_bytes(data)
        manifest = {"tag": tag, "commit": commit, "board": board, "label": label,
                    "parts": [{"name": name, "asset": asset(board, tag, name), "offset": off} for off, name, _ in b["parts"]],
                    "beatw": b["beatw"]}
        (rel / f"jank-{board}-{tag}.json").write_text(json.dumps(manifest, indent=1) + "\n")
        print(f"  {board}: " + ", ".join(f"{name} @ {off:#x}" for off, name, _ in b["parts"])
              + f"; beatw @ {b['beatw']['offset']:#x} ({b['beatw']['size']} bytes)")
    print(f"release assets: {rel}/ (the release workflow attaches them; by hand: gh release upload {tag} {rel}/*)")


if __name__ == "__main__":
    main()
