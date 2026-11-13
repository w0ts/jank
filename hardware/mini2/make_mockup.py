# SPDX-License-Identifier: GPL-3.0-only
"""A printable mockup of the board (hardware/pcb/make_mockup.py's way): the outline 1.6 mm thick, every part a block of
its courtyard and its height, top parts up and bottom parts down (a jack's courtyard takes its bushing past the edge), all one closed
solid (check_stl.py). z = 0 is the board's bottom face. Heights from the datasheets where named, else estimates
(README "Unverified").

  .../python3.9 make_mockup.py      (mockup.stl, via OpenSCAD; the DNP positions carry no part)
"""
import os
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import parts as P

NAME = "mini2"
OUT = os.path.join(HERE, "mockup.stl")
T = 1.6
BUSH_D, BUSH_OUT, BUSH_Z = 5.0, 2.0, 2.5         # (the PJ-320D's bushing: XKB drawing; 5.0 across measured on the sheet)
HEIGHT = {   # above (or below) the board face, mm
    "QFN-56_L7.0-W7.0-P0.40-TL-EP4.0": 0.9,        # (datasheet: 7 x 7 x 0.85)
    "CRYSTAL-SMD_4P-L3.2-W2.5-BL": 0.8, "ANT-SMD_L3.2-W1.6": 1.3,   # (ANT3216: 1.2 +- 0.15)
    "C0402": 0.55, "R0402": 0.45, "L0402": 0.55,
    "USB-C-SMD_TYPE-C-16PIN-2MD-073": 3.3,         # (estimate: the usual 3.26)
    "AUDIO-TH_PJ-320A-4P-DIP": 5.0,                # (its 3D model: 5.0 tall, the legs 3.5 under the board)
    "CAP-SMD_BD8.0-L8.3-W8.3-LS9.3-FD": 10.5, "CAP-SMD_BD10.0-L10.3-W10.3-LS10.9-FD": 10.5,   # (8 / 10 x 10.5 cans)
    "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-TL": 1.1,
    "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL": 1.45,
    "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5": 1.5,
    "L0806": 1.2, "HVSSOP-8_L3.0-W3.0-P0.65-LS4.9-BL-EP": 1.1, "SOD-123_L2.7-W1.6-LS3.7-RD-1": 1.2,
    "SOT-23-3_L2.9-W1.6-P1.90-LS2.8-BR": 1.1, "SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR": 1.1, "SOT-23-3_L3.0-W1.7-P0.95-LS2.9-BR": 1.1,
    "LED-SMD_L1.6-W0.8-R-RD": 0.8, "R0603": 0.55, "C0603": 0.9, "R0805": 0.6, "R1206": 0.6, "R2010": 0.6, "C0805": 1.3,
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
        if name not in HEIGHT or not P.fitted(fp.GetReference()):
            continue
        bottom = fp.IsFlipped()
        cy = fp.GetCourtyard(pcbnew.B_CrtYd if bottom else pcbnew.F_CrtYd).BBox()
        a, c, b_, d = mm(cy.GetLeft()), mm(cy.GetRight()), mm(cy.GetTop()), mm(cy.GetBottom())
        a, c, b_, d = max(a, x0), min(c, x1), max(b_, y0), min(d, y1)    # (the bodies end at the edge; bushings below)
        a, c, b_, d = a + 0.02, c - 0.02, b_ + 0.02, d - 0.02           # (courtyards that touch: no shared edge)
        h = HEIGHT[name]
        z0, z1 = (-h, 0.01) if bottom else (T - 0.01, T + h)
        cubes.append(f"translate([{a}, {-d}, {z0}]) cube([{c - a}, {d - b_}, {z1 - z0}]);  // {fp.GetReference()}")
        if name.startswith("AUDIO"):              # (the bushing: 5.0 across, 2.0 past the edge, its axis 2.5 off the board)
            py = mm(fp.GetPosition().y)
            left = mm(fp.GetPosition().x) < (x0 + x1) / 2
            zc = -BUSH_Z + 0.05 if bottom else T + BUSH_Z - 0.05   # (0.05 into the board: no edge shared with its face)
            if round(fp.GetOrientationDegrees()) % 180 == 90:      # (mini 2: J7, LINE IN, its bushing out past the TOP edge)
                px = mm(fp.GetPosition().x)
                cubes.append(f"translate([{px}, {-(y0 + 0.5)}, {zc}]) rotate([-90, 0, 0]) cylinder(d = {BUSH_D}, h = {BUSH_OUT + 0.5}, $fn = 48);")
                continue
            sx = x0 - BUSH_OUT if left else x1 - 0.5
            cubes.append(f"translate([{sx}, {-py}, {zc}]) rotate([0, 90, 0]) cylinder(d = {BUSH_D}, h = {BUSH_OUT + 0.5}, $fn = 48);")
    scad = os.path.join(HERE, ".mockup.scad")
    open(scad, "w").write("union() {\n" + "\n".join("  " + c for c in cubes) + "\n}\n")
    try:
        subprocess.run(["openscad", "--backend=manifold", "-q", "-o", OUT, scad], check=True)
    finally:
        os.remove(scad)
    print("wrote", os.path.basename(OUT))


if __name__ == "__main__":
    main()
