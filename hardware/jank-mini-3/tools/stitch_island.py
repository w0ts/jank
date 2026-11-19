"""jank-mini-3 helper (KiCad's python): one GND via inside a pour island DRC calls isolated (starved thermal), placed
on a ring round the given pad; written only when KiCad's DRC loses the item and gains none.
Usage: python3 stitch_island.py <board> <x> <y>   (board mm, as DRC prints them)"""
import json
import math
import os
import shutil
import subprocess
import sys

import pcbnew

KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
MM = pcbnew.FromMM


def drc(p):
    r = p + ".s.json"
    subprocess.run([KC, "pcb", "drc", "--schematic-parity", "--severity-all", "--format", "json", "-o", r, p],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = json.load(open(r))
    os.remove(r)
    return len(d.get("violations", [])), len(d.get("unconnected_items", []))


def main(path, x, y):
    base = drc(path)
    tmp = os.path.splitext(path)[0] + ".stitch.kicad_pcb"
    for e in ("kicad_pro", "kicad_sch", "kicad_dru"):
        src = os.path.splitext(path)[0] + "." + e
        if os.path.exists(src):
            shutil.copy(src, os.path.splitext(tmp)[0] + "." + e)
    for r in [float(v) for v in os.environ.get("STITCH_R", "2.0,2.5,3.0,3.5").split(",")]:
        for k in range(int(os.environ.get("STITCH_N", "12"))):
            a = 2 * math.pi * k / int(os.environ.get("STITCH_N", "12"))
            vx, vy = x + r * math.cos(a), y + r * math.sin(a)
            b = pcbnew.LoadBoard(path)
            v = pcbnew.PCB_VIA(b)
            v.SetPosition(pcbnew.VECTOR2I(MM(vx), MM(vy)))
            v.SetWidth(MM(0.6))
            v.SetDrill(MM(0.3))
            v.SetNet(b.FindNet("/GND"))
            b.Add(v)
            pcbnew.ZONE_FILLER(b).Fill(b.Zones())
            b.Save(tmp)
            got = drc(tmp)
            if got[0] < base[0] and got[1] <= base[1]:
                shutil.copy(tmp, path)
                print(f"via at ({vx:.2f}, {vy:.2f}): DRC {got} (was {base}); written")
                return 0
    print("no spot: board unchanged")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], float(sys.argv[2]), float(sys.argv[3])))
