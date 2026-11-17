# SPDX-License-Identifier: GPL-3.0-only
"""A printable mockup of the board (hardware/pcb/make_mockup.py's way): the outline 1.6 mm thick, every part a block of
its courtyard and its height, top parts up and bottom parts down (a jack's courtyard takes its bushing past the edge), all one closed
solid (check_stl.py). z = 0 is the board's bottom face. Heights from the datasheets where named, else estimates
(README "Unverified").

  VARIANT=2s|1s  .../python3.9 make_mockup.py      (mockup.stl / mockup-1s.stl, via OpenSCAD)
"""
import os
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
VARIANT = os.environ.get("VARIANT", "2s")
NAME = {"2s": "tiny", "1s": "tiny-1s"}[VARIANT]
OUT = os.path.join(HERE, "mockup.stl" if VARIANT == "2s" else "mockup-1s.stl")
T = 1.6
BUSH_D, BUSH_OUT, BUSH_Z = 5.0, 2.0, 2.5         # (the PJ-320D's bushing: XKB drawing; 5.0 across measured on the sheet)
HEIGHT = {   # above (or below) the board face, mm
    "BULETM-SMD_ESP32-S3-MINI-1-N8": 2.4,          # (datasheet: 15.4 x 20.5 x 2.4)
    "USB-C-SMD_TYPE-C-16PIN-2MD-073": 3.3,         # (estimate: the usual 3.26)
    "AUDIO-SMD_XKB_PJ-320D-B": 5.0,                # (XKB drawing: 5.0)
    "CAP-SMD_BD10.0-L10.3-W10.3-LS11.0-FD": 10.5,  # (ACMECON: 10 x 10.5)
    "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5": 1.5,
    "L0806": 1.2, "HVSSOP-8_L3.0-W3.0-P0.65-LS4.9-BL-EP": 1.1, "SOD-123_L2.7-W1.6-LS3.7-RD-1": 1.2,
    "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-BR": 1.1, "SOT-23-3_L3.0-W1.7-P0.95-LS2.9-BR": 1.1,
    "LED-SMD_L1.6-W0.8-R-RD": 0.8, "R0603": 0.55, "C0603": 0.9, "R0805": 0.6, "C0805": 1.3,
}


def mm(v):
    return round(pcbnew.ToMM(v), 3)


def main():
    b = pcbnew.LoadBoard(os.path.join(HERE, f"{NAME}.kicad_pcb"))
    bb = b.GetBoardEdgesBoundingBox()
    x0, y0, x1, y1 = mm(bb.GetLeft()), mm(bb.GetTop()), mm(bb.GetRight()), mm(bb.GetBottom())
    cubes = [f"translate([{x0}, {-y1}, 0]) cube([{x1 - x0}, {y1 - y0}, {T}]);"]
    for fp in b.GetFootprints():
        name = fp.GetFPID().GetLibItemName().wx_str()
        if name not in HEIGHT:
            continue
        bottom = fp.IsFlipped()
        cy = fp.GetCourtyard(pcbnew.B_CrtYd if bottom else pcbnew.F_CrtYd).BBox()
        a, c, b_, d = mm(cy.GetLeft()), mm(cy.GetRight()), mm(cy.GetTop()), mm(cy.GetBottom())
        a, c, b_, d = max(a, x0), min(c, x1), max(b_, y0), min(d, y1)    # (the bodies end at the edge; bushings below)
        h = HEIGHT[name]
        z0, z1 = (-h, 0.01) if bottom else (T - 0.01, T + h)
        cubes.append(f"translate([{a}, {-d}, {z0}]) cube([{c - a}, {d - b_}, {z1 - z0}]);  // {fp.GetReference()}")
        if name.startswith("AUDIO"):              # (the bushing: 5.0 across, 2.0 past the edge, its axis 2.5 off the board)
            py = mm(fp.GetPosition().y)
            left = mm(fp.GetPosition().x) < (x0 + x1) / 2
            zc = -BUSH_Z if bottom else T + BUSH_Z
            sx = x0 - BUSH_OUT if left else x1 - 0.5
            cubes.append(f"translate([{sx}, {-py}, {zc}]) rotate([0, 90, 0]) cylinder(d = {BUSH_D}, h = {BUSH_OUT + 0.5}, $fn = 48);")
    scad = os.path.join(HERE, f".mockup-{VARIANT}.scad")
    open(scad, "w").write("union() {\n" + "\n".join("  " + c for c in cubes) + "\n}\n")
    try:
        subprocess.run(["openscad", "--backend=manifold", "-q", "-o", OUT, scad], check=True)
    finally:
        os.remove(scad)
    print("wrote", os.path.basename(OUT))


if __name__ == "__main__":
    main()
