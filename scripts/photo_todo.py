#!/usr/bin/env python3
"""List the photos still to come on a page: its `<div class="msg PHOTO-TODO">Photo to come: ...</div>` boxes.

`make page` runs it on page/build.html so the build says which photos are missing. Usage: photo_todo.py PAGE.html
Prints a count line, then one "  - what" line per placeholder, in page order. Exit 0 either way (a note, not an error).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

PHOTO_TODO = "PHOTO-TODO"
_BOX = re.compile(rf'class="msg {PHOTO_TODO}"[^>]*>Photo to come: ([^<]*)<')


def todo_photos(html: str) -> list[str]:
    """the texts of the photos still to come, in page order"""
    return _BOX.findall(html)


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: photo_todo.py PAGE.html", file=sys.stderr)
        return 2
    page = Path(argv[1])
    todo = todo_photos(page.read_text(encoding="utf-8"))
    print(f"{page.name}: {len(todo)} photo(s) to come")
    for what in todo:
        print(f"  - {what}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
