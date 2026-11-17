"""Traktor's GENRE tag per prepared track (name as eval.traktor makes it), from collection.nml -> genres.json.

    python genres.py PATH/collection.nml genres.json
"""
from __future__ import annotations

import hashlib
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent))


def main() -> None:
    from eval.traktor import _relative_path
    nml, out = Path(sys.argv[1]), Path(sys.argv[2])
    genres = {}
    for e in ET.parse(nml).getroot().find("COLLECTION").findall("ENTRY"):
        loc, info = e.find("LOCATION"), e.find("INFO")
        if loc is None or info is None:
            continue
        file = _relative_path(loc)
        if file is None:
            continue
        genres[f"tk_{hashlib.sha1(file.encode()).hexdigest()[:8]}"] = (info.get("GENRE") or "").strip()
    out.write_text(json.dumps(genres, indent=0))
    print(len(genres), "tracks")


if __name__ == "__main__":
    main()
