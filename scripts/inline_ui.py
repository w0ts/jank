#!/usr/bin/env python3
"""Expand `<!--@include ui/file-->` marker lines in a page template.

A .js file lands inside its own <script> block, any other file (.css, .html) is pasted as it is.
The marker's path is relative to the repo root. Usage: inline_ui.py TEMPLATE [-o OUTPUT]  (stdout without -o).
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MARKER = re.compile(r"^[ \t]*<!--@include (\S+)-->[ \t]*$", re.MULTILINE)


def _body(rel: str, root: Path) -> str:
    path = root / rel
    if not path.is_file():
        raise FileNotFoundError(f"include not found: {rel}")
    text = path.read_text(encoding="utf-8")
    if not text.endswith("\n"):
        text += "\n"
    return f"<script>\n{text}</script>" if path.suffix == ".js" else text.rstrip("\n")


def expand(template: str, root: Path = ROOT) -> str:
    return MARKER.sub(lambda m: _body(m.group(1), root), template)


def expand_file(template: Path, root: Path = ROOT) -> str:
    return expand(template.read_text(encoding="utf-8"), root)


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("template", type=Path)
    p.add_argument("-o", "--output", type=Path)
    args = p.parse_args(argv)
    out = expand_file(args.template)
    if args.output:
        args.output.write_text(out, encoding="utf-8")
    else:
        sys.stdout.write(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
