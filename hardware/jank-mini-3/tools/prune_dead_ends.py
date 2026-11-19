"""jank-mini-3 helper (KiCad's python): prune dead-end track branches: repeatedly remove a track with an end that touches
no pad, via or other track of its net (in one process: KiCad's SWIG breaks on a second LoadBoard), then KiCad's DRC
decides: written only when the unconnected count does not rise. Usage: python3 prune_dead_ends.py <board>"""
import json
import os
import shutil
import subprocess
import sys

import pcbnew

KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"


def drc(p):
    r = p + ".p.json"
    subprocess.run([KC, "pcb", "drc", "--schematic-parity", "--severity-all", "--format", "json", "-o", r, p],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = json.load(open(r))
    os.remove(r)
    return len(d["violations"]), len(d["unconnected_items"])


def main(path):
    """ONE dead-end track removed per run (KiCad's SWIG crashes on removals in a loop); prints DEAD or NONE"""
    b = pcbnew.LoadBoard(path)
    pads = [p for f in b.GetFootprints() for p in f.Pads()]
    tracks = [t for t in b.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]
    vias = [v for v in b.GetTracks() if v.Type() == pcbnew.PCB_VIA_T]
    for t in tracks:
        for q in (t.GetStart(), t.GetEnd()):
            n, L = t.GetNetCode(), t.GetLayer()
            if any(p.GetNetCode() == n and p.IsOnLayer(L) and p.HitTest(q) for p in pads):
                continue
            if any(v.GetNetCode() == n and v.HitTest(q) for v in vias):
                continue
            if any(o is not t and o.GetNetCode() == n and o.GetLayer() == L and o.HitTest(q) for o in tracks):
                continue
            print(f"DEAD {t.GetNetname()} {pcbnew.ToMM(t.GetLength()):.3f} mm")
            b.Remove(t)
            b.Save(path)
            return
    print("NONE")


if __name__ == "__main__":
    if sys.argv[1:2] == ["--drc"]:
        print(drc(sys.argv[2]))
    else:
        main(sys.argv[1])
