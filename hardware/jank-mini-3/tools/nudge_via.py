"""jank-mini-3 helper (KiCad's python): move ONE via (and the track ends on it) by a small step in the first direction
that leaves KiCad's DRC with no violation, for a lone hole-to-hole / clearance leftover the router's fix step cannot
resolve. Tries 8 directions x the given steps; writes the board when its DRC count goes down.
Usage: python3 nudge_via.py <board> <net> <x> <y> [steps mm, default 0.05,0.08]"""
import json
import math
import os
import shutil
import subprocess
import sys

import pcbnew

KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
MM = pcbnew.FromMM


def drc_count(path):
    rpt = path + ".nudge.json"
    subprocess.run(["nice", "-n", "19", KC, "pcb", "drc", "--schematic-parity", "--severity-all", "--format", "json",
                    "-o", rpt, path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = json.load(open(rpt))
    os.remove(rpt)
    return len(d.get("violations", [])) + len(d.get("unconnected_items", [])) + len(d.get("schematic_parity", []))


def main(path, net, x, y, steps):
    start = drc_count(path)   # (written when the board's DRC count goes down, not only at zero: other items may remain)
    work = path + ".nudge.kicad_pcb"
    for base in ("kicad_pro", "kicad_sch", "kicad_dru"):    # (the schematic and rules beside the copy: parity, local rules)
        src = os.path.splitext(path)[0] + "." + base
        if os.path.exists(src):
            shutil.copy(src, os.path.splitext(work)[0] + "." + base)
    for step in steps:
        for k in range(8):
            a = 2 * math.pi * k / 8
            b = pcbnew.LoadBoard(path)
            v = next(t for t in b.GetTracks() if t.Type() == pcbnew.PCB_VIA_T and t.GetNetname() == net
                     and abs(pcbnew.ToMM(t.GetPosition().x) - x) < 0.01 and abs(pcbnew.ToMM(t.GetPosition().y) - y) < 0.01)
            p = v.GetPosition()
            q = pcbnew.VECTOR2I(p.x + MM(step * math.cos(a)), p.y + MM(step * math.sin(a)))
            for t in b.GetTracks():
                if t.Type() == pcbnew.PCB_TRACE_T and t.GetNetCode() == v.GetNetCode():
                    if t.GetStart() == p:
                        t.SetStart(q)
                    if t.GetEnd() == p:
                        t.SetEnd(q)
            v.SetPosition(q)
            b.Save(work)
            n = drc_count(work)
            print(f"step {step} mm at {round(math.degrees(a))} deg: {n} DRC items")
            if n < start:
                shutil.copy(work, path)
                print("written")
                return 0
    print("no clean spot: board unchanged")
    return 1


if __name__ == "__main__":
    st = [float(s) for s in sys.argv[5].split(",")] if len(sys.argv) > 5 else [0.05, 0.08]
    sys.exit(main(sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4]), st))
