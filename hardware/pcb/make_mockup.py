# SPDX-License-Identifier: GPL-3.0-only
"""A mockup to 3D-print, to design and try the box: the board's outline 1.6 mm thick (its M2 holes and the two LED
cut-outs open) with every fitted part as a solid block of its courtyard and its height, and the leads under the board
as blocks too, all ONE closed manifold solid (the user's STL rule; check_stl.py checks it). Two builds: all four
outputs with the DIN (mockup-din.stl) and all four with the TRS (mockup-trs.stl). C4 (not fitted) is left out.
Heights are estimates (README "Mockup heights"), most UNVERIFIED.

  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 make_mockup.py
"""
import math
import os
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import make_footprints as F

THICK = 1.6
UP = {"U1": 7.0, "J1": 3.3, "D1": 1.1, "U2": 1.1, "U3": 1.2, "Q1": 1.1, "Q2": 1.1, "Q3": 1.1, "D2": 1.2, "U4": 4.5, "R8": 9.0,
      **{f"R1{k}": 9.0 for k in range(1, 5)}, **{f"R2{k}": 11.5 for k in range(1, 5)}}   # above the board (the floor side)
FACE = {"SW1": 9.5, "SW6": 9.5, **{f"SW{k}": 5.0 for k in range(2, 6)}}   # below it: the face side (the lid); the A/B switches with their handles
DOWN = {"U1": 3.0, "J1": 1.9, "R8": 1.5, "U4": 1.5, **{f"R{a}{k}": 1.5 for a in (1, 2) for k in range(1, 5)}}   # leads below
LEADS_UP = {"SW1": 1.5, "SW6": 1.5, "SW7": 1.5, **{f"SW{k}": 1.5 for k in range(2, 6)}}   # the face-side parts' legs on the floor side (SW1's trimmed: under the XIAO)
SMD_UP = 0.8                                     # (0805 / 1206 / 2010: about 0.6 to 0.8)
DIN_UP, DOWN_OUT = 21.1, 3.5                     # (the jack's sizes: make_footprints.py, from CUI's drawing)
XIAO_USB = (9.0, 1.5, 3.3)
ENC_BODY, ENC_SHAFT, ENC_D = (12.5, 6.5), 20.0, 6.0   # (the EC11: body 11.7 x 12.5 x 6.5, its shaft 20 mm from the board; drawing, unverified)                       # its socket overhanging the back edge: width, overhang, height

LAYOUT = sys.argv[1] if len(sys.argv) > 1 else "long"   # (long: jank-pcb.kicad_pcb; square: jank-pcb-square.kicad_pcb)
TAG = "" if LAYOUT == "long" else "-" + LAYOUT
board = pcbnew.LoadBoard(os.path.join(HERE, f"jank-pcb{TAG}.kicad_pcb"))
poly = pcbnew.SHAPE_POLY_SET()
board.GetBoardPolygonOutlines(poly, False)


def mm(v):
    return round(pcbnew.ToMM(v), 3)


def ring(r):
    return [[mm(r.CPoint(i).x), -mm(r.CPoint(i).y)] for i in range(r.PointCount())]


outer = ring(poly.Outline(0))
holes = [ring(poly.Hole(0, i)) for i in range(poly.HoleCount(0))]   # (the LED cut-outs)


def box(x0, y0, x1, y1, z0, z1, note):
    """a block from page mm (y down) to OpenSCAD (y up), slightly into the board so the union is one solid"""
    return (f"  translate([{x0:.3f}, {-y1:.3f}, {z0:.3f}]) cube([{x1 - x0:.3f}, {y1 - y0:.3f}, {z1 - z0:.3f}]);"
            f"  // {note}")


def frame_box(frame, x0, y0, x1, y1, z0, z1, note):
    """a block given in an output's own coordinates (its edge point, turned by th): its board-aligned box"""
    pts = [T(frame, x, y) for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1))]
    xs, ys = [q[0] for q in pts], [q[1] for q in pts]
    return box(min(xs), min(ys), max(xs), max(ys), z0, z1, note)


def T(frame, lx, ly):
    (ex, ey), th = frame
    c, s_ = round(math.cos(math.radians(th)), 9), round(math.sin(math.radians(th)), 9)
    return (ex + lx * c + ly * s_, ey - lx * s_ + ly * c)


def blocks(variant):
    out = []
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if ref.startswith(("TP", "H", "JP")) or ref in ("C4", "R9") or ref[:2] in ("R3", "R4") and len(ref) == 3:
            continue
        bottom = fp.GetLayer() == pcbnew.B_Cu
        court = fp.GetCourtyard(pcbnew.B_CrtYd if bottom else pcbnew.F_CrtYd)
        bb = court.BBox() if court.OutlineCount() else fp.GetBoundingBox(False)
        x0, y0, x1, y1 = mm(bb.GetLeft()), mm(bb.GetTop()), mm(bb.GetRight()), mm(bb.GetBottom())
        if ref.startswith("J") and ref != "J1":  # the outputs: the DIN or the jack, at their own sizes
            fr = ((mm(fp.GetPosition().x), mm(fp.GetPosition().y)), fp.GetOrientationDegrees() % 360)
            if variant == "din":
                out.append(frame_box(fr, -F.DIN_W / 2, -F.DIN_D, F.DIN_W / 2, 0, THICK - 0.01, THICK + DIN_UP, ref))
            else:
                jx = F.NEST[0]                       # (the SJ1-3523N: body on the board, its bushing out past the edge)
                out.append(frame_box(fr, jx - F.JACK_W / 2, -F.JACK_D, jx + F.JACK_W / 2, 0, THICK - 0.01,
                                     THICK + F.JACK_H, ref + " body"))
                bx, by = T(fr, jx, -0.01)
                out.append(f"  translate([{bx:.3f}, {-by:.3f}, {THICK + F.BUSH_Z:.3f}]) rotate([0, 0, {fr[1]:.0f}])"
                           f" rotate([90, 0, 0]) cylinder(d={F.BUSH_D}, h={F.BUSH_OUT + 0.01:.3f}, $fn=48);  // {ref} bushing")
            out.append(frame_box(fr, -8.5, -16.0, 10.0, -1.5, -DOWN_OUT, 0.01, ref + " leads"))
            continue
        if ref == "SW7":                       # (the encoder, face side: its body and its shaft through the lid)
            mp = [p for p in fp.Pads() if p.GetNumber() == "MP"]
            ex = sum(mm(q.GetPosition().x) for q in mp) / len(mp)
            ey = sum(mm(q.GetPosition().y) for q in mp) / len(mp)
            w = ENC_BODY[0] / 2
            out.append(box(ex - w, ey - w, ex + w, ey + w, -ENC_BODY[1], 0.01, "SW7 body"))
            out.append(f"  translate([{ex:.3f}, {-ey:.3f}, {-ENC_SHAFT:.3f}]) cylinder(d={ENC_D}, h={ENC_SHAFT - ENC_BODY[1] + 0.01:.3f}, $fn=48);  // SW7 shaft")
            out.append(box(x0, y0, x1, y1, THICK - 0.01, THICK + LEADS_UP[ref], ref + " legs"))
            continue
        if bottom:                               # (the face side: SW1, SW6, the A/B switches)
            out.append(box(x0, y0, x1, y1, -FACE.get(ref, 0.8), 0.01, ref + " face side"))
            if ref in LEADS_UP:
                out.append(box(x0, y0, x1, y1, THICK - 0.01, THICK + LEADS_UP[ref], ref + " legs"))
            continue
        out.append(box(x0, y0, x1, y1, THICK - 0.01, THICK + UP.get(ref, SMD_UP), ref))
        if ref in DOWN:                          # (the leads under each courtyard outline: the XIAO's two header rows)
            for i in range(max(court.OutlineCount(), 1)):
                ob = court.Outline(i).BBox() if court.OutlineCount() else bb
                out.append(box(mm(ob.GetLeft()), mm(ob.GetTop()), mm(ob.GetRight()), mm(ob.GetBottom()), -DOWN[ref], 0.01,
                               ref + " leads"))
        if ref == "U1":                          # (its USB-C over the back edge, at the module's top)
            cx = (x0 + x1) / 2
            w, over, h = XIAO_USB
            out.append(box(cx - w / 2, y0 - over, cx + w / 2, y0 + 7.0, THICK + UP["U1"] - h, THICK + UP["U1"], "U1 USB-C"))
    return out


for variant in ("din", "trs"):
    scad = os.path.join(HERE, f"mockup{TAG}-{variant}.scad")
    with open(scad, "w") as f:
        f.write(f"// generated by make_mockup.py: the jank board and its parts as blocks ({variant} outputs)\nunion() {{\n")
        f.write("  difference() {\n")
        f.write(f"    linear_extrude({THICK}) polygon({outer});\n")
        for h in holes:
            f.write(f"    translate([0, 0, -1]) linear_extrude({THICK + 2}) polygon({h});\n")
        for fp in board.GetFootprints():          # (the M2 holes stay open)
            if fp.GetReference().startswith("H"):
                x, y = mm(fp.GetPosition().x), mm(fp.GetPosition().y)
                f.write(f"    translate([{x}, {-y}, -1]) cylinder(d=2.2, h={THICK + 2}, $fn=32);\n")
        f.write("  }\n")
        f.write("\n".join(blocks(variant)) + "\n}\n")
    stl = os.path.join(HERE, f"mockup{TAG}-{variant}.stl")
    subprocess.run(["openscad", "--backend", "manifold", "-o", stl, scad], check=True, capture_output=True)
    print("wrote", stl)
subprocess.run(["python3", os.path.join(HERE, "check_stl.py"), *[os.path.join(HERE, f"mockup{TAG}-{v}.stl") for v in ("din", "trs")]],
               check=True)
