"""jank-mini-3 helper (KiCad's python): remove the track stubs KiCad's DRC calls dangling (route.py --clean skips locked
tracks, and the repair round locks every track), one at a time, keeping a removal only when DRC's unconnected count
does not rise. Usage: python3 drop_dangling.py <board>"""
import json
import os
import shutil
import subprocess
import sys

import pcbnew

KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"


def drc(p):
    r = p + ".d.json"
    subprocess.run([KC, "pcb", "drc", "--schematic-parity", "--severity-all", "--format", "json", "-o", r, p],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = json.load(open(r))
    os.remove(r)
    return d


def main(path):
    """DRC's dangling track ends, removed one at a time while the unconnected count does not rise; repeated, since a
    removed stub can leave the next piece of its dead-end branch dangling"""
    import re
    tmp = os.path.splitext(path)[0] + ".drop.kicad_pcb"
    for e in ("kicad_pro", "kicad_sch", "kicad_dru"):
        src = os.path.splitext(path)[0] + "." + e
        if os.path.exists(src):
            shutil.copy(src, os.path.splitext(tmp)[0] + "." + e)
    d = drc(path)
    base_u = len(d["unconnected_items"])
    tried = set()
    for _round in range(int(os.environ.get("DROP_ROUNDS", "1"))):
        spots = [(it["pos"]["x"], it["pos"]["y"], it["description"]) for v in d["violations"]
                 if v["type"] == "track_dangling" for it in v["items"]]
        spots = [sp for sp in spots if sp not in tried]
        if not spots:
            break
        x, y, desc = spots[0]
        tried.add(spots[0])
        want = float(re.search(r"length ([\d.]+) mm", desc).group(1))
        b = pcbnew.LoadBoard(path)
        hit = [t for t in b.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T and abs(pcbnew.ToMM(t.GetLength()) - want) < 0.002
               and t.HitTest(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))]
        if len(hit) > 1:   # (two of that length there: the one with an END at the dangling spot)
            q = pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))
            hit = [t for t in hit if min((t.GetStart() - q).EuclideanNorm(), (t.GetEnd() - q).EuclideanNorm()) < pcbnew.FromMM(0.002)]
        if len(hit) != 1:
            print(f"{desc}: {len(hit)} candidates, skipped")
            continue
        b.Remove(hit[0])
        pcbnew.ZONE_FILLER(b).Fill(b.Zones())
        b.Save(tmp)
        n = drc(tmp)
        if len(n["unconnected_items"]) <= base_u:
            shutil.copy(tmp, path)
            d = n
            print(f"removed {desc}")
        else:
            print(f"kept {desc}: it carries a connection")
    print(f"now {len(d['violations'])} violations, {len(d['unconnected_items'])} unconnected")


if __name__ == "__main__":
    main(sys.argv[1])
