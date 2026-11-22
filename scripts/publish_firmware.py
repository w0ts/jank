#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Put a release's firmware on the site, for the Install page's browser flasher.

  python3 -I scripts/publish_firmware.py TAG [--dry-run] [--no-build]

HEAD must be the tag TAG and the checkout clean. It builds the one release image (scripts/build.sh auto: the bare XIAO or the
jank mini 3, told apart at the start, firmware/main/board.c), takes the parts from the build's flash_args (bootloader, partition table, otadata, app) and the
beatw partition's place from the built partition table, and adds them to the gh-pages branch under firmware/TAG/auto/,
with firmware/index.json listing the releases, newest first. Nothing else on gh-pages changes. The images hold NO
network weights (core/beat/beat_weights.h): the page adds madmom's only at the user's click, in the user's browser.

The parts are written at their own addresses (not one merged image), so the device's settings (nvs) are kept.
Without --dry-run it pushes gh-pages (Henri runs that). It also prints the `gh release upload` line for jank.bin.
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
REMOTE = os.environ.get("PAGES_REMOTE", "origin")
BRANCH = "gh-pages"
BEATW_SUBTYPE = 0x40


def git(*args: str, data: bytes | None = None) -> str:
    r = subprocess.run(["git", *args], cwd=ROOT, input=data, capture_output=True, check=False)
    if r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)}: {r.stderr.decode().strip()}")
    return r.stdout.decode().strip()


def blob(data: bytes) -> str:
    return git("hash-object", "-w", "--stdin", data=data)


def mktree(entries: list[tuple[str, str, str]]) -> str:
    """entries: (mode, sha, name); type from the mode."""
    lines = "".join(f"{m} {'tree' if m == '040000' else 'blob'} {s}\t{n}\n" for m, s, n in sorted(entries, key=lambda e: e[2]))
    return git("mktree", data=lines.encode())


def ls_tree(treeish: str) -> list[tuple[str, str, str]]:
    out = git("ls-tree", treeish)
    rows = []
    for line in out.splitlines():
        meta, name = line.split("\t", 1)
        mode, _, sha = meta.split()
        rows.append((mode, sha, name))
    return rows


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


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    dry, do_build = "--dry-run" in sys.argv, "--no-build" not in sys.argv
    if len(args) != 1:
        raise SystemExit(__doc__)
    tag = args[0]
    if not all(c.isalnum() or c in "._-" for c in tag):
        raise SystemExit("tag: letters, digits, . _ - only")
    check_release(tag)

    subprocess.run(["git", "fetch", "--quiet", REMOTE, BRANCH], cwd=ROOT, check=False)
    parent = subprocess.run(["git", "rev-parse", "--verify", "--quiet", f"refs/remotes/{REMOTE}/{BRANCH}"], cwd=ROOT,
                            capture_output=True, text=True).stdout.strip()
    kept = ls_tree(f"{parent}^{{tree}}") if parent else []
    fw_old = next((s for m, s, n in kept if n == "firmware" and m == "040000"), None)
    fw_rows = [r for r in (ls_tree(fw_old) if fw_old else []) if r[2] not in (tag, "index.json")]
    index = {"releases": []}
    if fw_old and any(n == "index.json" for _, _, n in ls_tree(fw_old)):
        index = json.loads(git("show", f"{fw_old}:index.json"))

    entry = {"tag": tag, "commit": git("rev-parse", "--short", "HEAD"), "boards": {}}
    board_trees = []
    for board, label in BOARDS.items():
        b = build_board(board, do_build)
        files = [("100644", blob(data), name) for _, name, data in b["parts"]]
        board_trees.append(("040000", mktree(files), board))
        entry["boards"][board] = {
            "label": label,
            "parts": [{"path": f"firmware/{tag}/{board}/{name}", "offset": off} for off, name, _ in b["parts"]],
            "beatw": b["beatw"],
        }
        rel = scratch() / "release" / tag
        rel.mkdir(parents=True, exist_ok=True)
        app = next(data for _, name, data in b["parts"] if name == "jank.bin")
        (rel / f"jank-{board}-{tag}.bin").write_bytes(app)
    index["releases"] = [entry] + [r for r in index.get("releases", []) if r.get("tag") != tag]
    tag_tree = mktree(board_trees)
    fw_tree = mktree(fw_rows + [("040000", tag_tree, tag),
                                ("100644", blob((json.dumps(index, indent=1) + "\n").encode()), "index.json")])
    tree = mktree([r for r in kept if r[2] != "firmware"] + [("040000", fw_tree, "firmware")])
    if parent and git("rev-parse", f"{parent}^{{tree}}") == tree:
        print(f"nothing to publish: {REMOTE}/{BRANCH} already holds firmware {tag}")
        return
    commit = git("commit-tree", tree, *(["-p", parent] if parent else []), "-m", f"publish: firmware {tag} for the Install page",
                 "-m", "No network weights: the page adds madmom's at the user's click. Built by scripts/publish_firmware.py.")
    print(f"firmware {tag} -> commit {commit[:10]} on {BRANCH}")
    for board, e in entry["boards"].items():
        print(f"  {board}: " + ", ".join(f"{p['path'].rsplit('/', 1)[1]} @ {p['offset']:#x}" for p in e["parts"])
              + f"; beatw @ {e['beatw']['offset']:#x} ({e['beatw']['size']} bytes)")
    rel = scratch() / "release" / tag
    print(f"release assets (the app images, for the drive copy): {rel}/")
    print(f"  gh release upload {tag} " + " ".join(str(rel / f"jank-{b}-{tag}.bin") for b in BOARDS))
    if dry:
        print("dry run: nothing pushed")
        return
    git("push", REMOTE, f"{commit}:refs/heads/{BRANCH}")
    print("published: https://w0ts.github.io/jank/install.html (GitHub takes a minute to build)")


if __name__ == "__main__":
    main()
