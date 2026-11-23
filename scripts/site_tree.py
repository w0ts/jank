#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Lay out the site https://w0ts.github.io/jank/ in a folder, for the Pages workflow (.github/workflows/pages.yml).

  python3 -I scripts/site_tree.py OUT [--no-firmware]

The pages from page/ (committed: `make page` builds them), the device page with the w0ts project switcher (the copy the
firmware embeds stays without it: it loads nothing from outside, tests/test_page.js), page/img/, .nojekyll, and
firmware/ for the Install page's browser flasher: every release's flash parts at firmware/TAG/BOARD/NAME and
firmware/index.json, newest first, made from the GitHub releases' assets (scripts/publish_firmware.py writes them: the
manifest jank-BOARD-TAG.json and one asset a part). The Install page reads them from the site itself: a page cannot
fetch a release file (GitHub sends it with no CORS header). Needs gh (GH_TOKEN in the workflow)."""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = os.environ.get("GITHUB_REPOSITORY", "w0ts/jank")
# published name: file in the repository
PAGES = {
    "index.html": "page/index.html",
    "install.html": "page/install.html",
    "device.html": "page/jank.html",
    "build.html": "page/build.html",
}
SWITCHER = '<script src="https://w0ts.github.io/site-style/switcher.js" defer></script>'


def gh(*args: str) -> bytes:
    r = subprocess.run(["gh", *args], capture_output=True, check=False)
    if r.returncode != 0:
        raise SystemExit(f"gh {' '.join(args)}: {r.stderr.decode().strip()}")
    return r.stdout


def asset_bytes(asset_id: int) -> bytes:
    return gh("api", "-H", "Accept: application/octet-stream", f"repos/{REPO}/releases/assets/{asset_id}")


def firmware(out: Path) -> list[str]:
    """firmware/TAG/BOARD/NAME and firmware/index.json from the releases; returns the tags carried"""
    releases = json.loads(gh("api", f"repos/{REPO}/releases?per_page=100"))
    releases = sorted((r for r in releases if not r.get("draft")), key=lambda r: r.get("published_at") or "", reverse=True)
    index, carried = {"releases": []}, []
    for rel in releases:
        tag = rel["tag_name"]
        assets = {a["name"]: a["id"] for a in rel.get("assets", [])}
        manifests = sorted(n for n in assets if re.fullmatch(rf"jank-[a-z0-9]+-{re.escape(tag)}\.json", n))
        if not manifests:
            print(f"site_tree: release {tag} has no firmware manifest (jank-BOARD-{tag}.json): not on the Install page")
            continue
        entry = {"tag": tag, "commit": None, "boards": {}}
        for name in manifests:
            m = json.loads(asset_bytes(assets[name]))
            board = m["board"]
            entry["commit"] = m["commit"]
            parts = []
            for p in m["parts"]:
                if p["asset"] not in assets:
                    raise SystemExit(f"release {tag}: {name} names {p['asset']}, which is not attached")
                dst = out / "firmware" / tag / board / p["name"]
                dst.parent.mkdir(parents=True, exist_ok=True)
                dst.write_bytes(asset_bytes(assets[p["asset"]]))
                parts.append({"path": f"firmware/{tag}/{board}/{p['name']}", "offset": p["offset"]})
            entry["boards"][board] = {"label": m["label"], "parts": parts, "beatw": m["beatw"]}
        index["releases"].append(entry)
        carried.append(tag)
    (out / "firmware").mkdir(parents=True, exist_ok=True)
    (out / "firmware" / "index.json").write_text(json.dumps(index, indent=1) + "\n")
    return carried


def pages(out: Path) -> None:
    for pub, src in PAGES.items():
        html = (ROOT / src).read_text(encoding="utf-8")
        if pub == "device.html":
            if html.count("</head>") != 1:
                raise SystemExit(f"{src}: expected one </head>")
            html = html.replace("</head>", SWITCHER + "</head>")
        (out / pub).write_text(html, encoding="utf-8")
    shutil.copytree(ROOT / "page" / "img", out / "img")
    (out / ".nojekyll").write_text("")


def main(argv: list[str]) -> int:
    args = [a for a in argv if not a.startswith("--")]
    if len(args) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    out = Path(args[0])
    if out.exists() and any(out.iterdir()):
        raise SystemExit(f"{out}: not empty")
    out.mkdir(parents=True, exist_ok=True)
    pages(out)
    tags = [] if "--no-firmware" in argv else firmware(out)
    print(f"site_tree: {out}: {', '.join(PAGES)}, img/, firmware for {', '.join(tags) or 'no release'}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
