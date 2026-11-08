# SPDX-License-Identifier: GPL-3.0-only
"""A mockup to 3D-print, to design and try the box (v5): the board's outline 1.6 mm thick (its M2 holes open), every
fitted part as a solid block of its courtyard and its height, the top side's parts (the lid side: z up) and the bottom
side's (the DINs, the standing resistors, a few 0805: z down), the leads as blocks too, all ONE closed manifold solid
(the user's STL rule; check_stl.py checks it): mockup.stl. Both connectors of every output are in it (v5 fits both).
Not fitted parts (C4, the 1206 pads, the jumpers, the test points) are left out. Heights are estimates (README "v5:
heights"), most UNVERIFIED. z = 0 is the board's bottom face, z = 1.6 its top.

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
# above the board (top, the lid side), mm from its top face
UP = {"U1": 7.0, "J1": 3.3, "D1": 1.1, "U2": 1.1, "U3": 1.2, "Q1": 1.1, "Q2": 1.1, "Q3": 1.1, "Q4": 1.1, "D2": 1.2,
      "U4": 4.5, "LED1": 1.95, "SW1": 9.5, "SW6": 9.5, **{f"SW{k}": 5.0 for k in range(2, 6)}}
# below the board (bottom), mm from its bottom face: the standing resistors (body + bent lead), the bottom 0805s
DOWN = {"R8": 9.0, **{f"R1{k}": 9.0 for k in range(1, 5)}, **{f"R2{k}": 11.5 for k in range(1, 5)}}
LEADS = {"U1": 3.0, "J1": 1.9, "U4": 1.5, "SW1": 1.5, "SW6": 1.5, "SW7": 1.5, "J10": 1.5,
         **{f"SW{k}": 1.5 for k in range(2, 6)}}        # (a top part's legs under the board)
LEADS_UP = 1.5                                   # (a bottom part's legs over the board: the standing resistors)
SMD = 0.8                                        # (0805 / SOT: about 0.6 to 1.1)
XIAO_USB = (9.0, 1.5, 3.3)                       # its socket over the back edge: width, overhang, height
ENC_BODY, ENC_SHAFT, ENC_D = (12.5, 6.5), 20.0, 6.0   # (the EC11: body 11.7 x 12.5 x 6.5, shaft 20 mm from the board)
OLED_UP, OLED_T = 2.5, 2.6                       # (the module on a 2.54 header's 2.5 mm plastic; module + glass 2.6 thick)
SKIP = ("TP", "H", "JP")

board = pcbnew.LoadBoard(os.path.join(HERE, "jank-pcb.kicad_pcb"))
poly = pcbnew.SHAPE_POLY_SET()
board.GetBoardPolygonOutlines(poly, False)


def mm(v):
    return round(pcbnew.ToMM(v), 3)


def ring(r):
    return [[mm(r.CPoint(i).x), -mm(r.CPoint(i).y)] for i in range(r.PointCount())]


outer = ring(poly.Outline(0))


def box(x0, y0, x1, y1, z0, z1, note):
    """a block from page mm (y down) to OpenSCAD (y up)"""
    return (f"  translate([{x0:.3f}, {-y1:.3f}, {z0:.3f}]) cube([{x1 - x0:.3f}, {y1 - y0:.3f}, {z1 - z0:.3f}]);"
            f"  // {note}")


def T(frame, lx, ly):
    (ex, ey), th = frame
    c, s_ = round(math.cos(math.radians(th)), 9), round(math.sin(math.radians(th)), 9)
    return (ex + lx * c + ly * s_, ey - lx * s_ + ly * c)


def frame_box(frame, x0, y0, x1, y1, z0, z1, note):
    pts = [T(frame, x, y) for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1))]
    xs, ys = [q[0] for q in pts], [q[1] for q in pts]
    return box(min(xs), min(ys), max(xs), max(ys), z0, z1, note)


def output(fp, ref):
    """the jack on the top (body, bushing past the edge) and the DIN on the bottom (plate + body, 1.0 behind the edge)"""
    fr = ((mm(fp.GetPosition().x), mm(fp.GetPosition().y)), fp.GetOrientationDegrees() % 360)
    out = [frame_box(fr, -F.PJ_W / 2, -F.PJ_D, F.PJ_W / 2, 0, THICK - 0.01, THICK + F.PJ_H, ref + " jack body")]
    bx, by = T(fr, 0.0, -0.01)
    out.append(f"  translate([{bx:.3f}, {-by:.3f}, {THICK + F.PJ_BUSH_Z:.3f}]) rotate([0, 0, {fr[1]:.0f}])"
               f" rotate([90, 0, 0]) cylinder(d={F.PJ_BUSH_D}, h={F.PJ_BUSH_OUT + 0.01:.3f}, $fn=48);  // {ref} bushing")
    f0 = -F.DIN_BACK
    out.append(frame_box(fr, -F.HY_W / 2, f0 - F.HY_D, F.HY_W / 2, f0, -F.HY_H, 0.01, ref + " DIN (plate and body)"))
    out.append(frame_box(fr, -8.6, -16.9, 8.6, -1.5, THICK - 0.01, THICK + LEADS_UP, ref + " DIN leads"))
    return out


def blocks():
    out = []
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if ref.startswith(SKIP) or ref in ("C4", "R9") or (ref[:2] in ("R3", "R4") and len(ref) == 3):
            continue
        bottom = fp.GetLayer() == pcbnew.B_Cu
        court = fp.GetCourtyard(pcbnew.B_CrtYd if bottom else pcbnew.F_CrtYd)
        bb = court.BBox() if court.OutlineCount() else fp.GetBoundingBox(False)
        x0, y0, x1, y1 = mm(bb.GetLeft()), mm(bb.GetTop()), mm(bb.GetRight()), mm(bb.GetBottom())
        if ref.startswith("J") and ref not in ("J1", "J10"):
            out += output(fp, ref)
            continue
        if ref == "SW7":                         # (the encoder: its body and its shaft up through the lid)
            mp = [q for q in fp.Pads() if q.GetNumber() == "MP"]
            ex = sum(mm(q.GetPosition().x) for q in mp) / len(mp)
            ey = sum(mm(q.GetPosition().y) for q in mp) / len(mp)
            w = ENC_BODY[0] / 2
            out.append(box(ex - w, ey - w, ex + w, ey + w, THICK - 0.01, THICK + ENC_BODY[1], "SW7 body"))
            out.append(f"  translate([{ex:.3f}, {-ey:.3f}, {THICK + ENC_BODY[1] - 0.01:.3f}]) cylinder(d={ENC_D},"
                       f" h={ENC_SHAFT - ENC_BODY[1] + 0.01:.3f}, $fn=48);  // SW7 shaft")
            out.append(box(x0, y0, x1, y1, -LEADS[ref], 0.01, ref + " legs"))
            continue
        if ref == "J10":                         # (the OLED: the header, the module over it, the glass on top)
            out.append(box(x0, y0, x1, y1, THICK - 0.01, THICK + OLED_UP + 0.01, "J10 header"))
            hx, hy = mm(fp.GetPosition().x), mm(fp.GetPosition().y)
            out.append(box(hx - F.OLED_L + F.OLED_PIN_X, hy - F.OLED_W + F.OLED_PIN_Y, hx + F.OLED_PIN_X, hy + F.OLED_PIN_Y,
                           THICK + OLED_UP, THICK + OLED_UP + OLED_T, "J10 OLED module (rot 180: along -x)"))
            out.append(box(x0, y0, x1, y1, -LEADS[ref], 0.01, ref + " legs"))
            continue
        if bottom:
            out.append(box(x0, y0, x1, y1, -DOWN.get(ref, SMD), 0.01, ref + " (bottom)"))
            if ref in DOWN:
                out.append(box(x0, y0, x1, y1, THICK - 0.01, THICK + LEADS_UP, ref + " leads (top)"))
            continue
        out.append(box(x0, y0, x1, y1, THICK - 0.01, THICK + UP.get(ref, SMD), ref))
        if ref in LEADS:
            for i in range(max(court.OutlineCount(), 1)):
                ob = court.Outline(i).BBox() if court.OutlineCount() else bb
                out.append(box(mm(ob.GetLeft()), mm(ob.GetTop()), mm(ob.GetRight()), mm(ob.GetBottom()), -LEADS[ref], 0.01,
                               ref + " leads"))
        if ref == "U1":
            cx = (x0 + x1) / 2
            w, over, h = XIAO_USB
            out.append(box(cx - w / 2, y0 - over, cx + w / 2, y0 + 7.0, THICK + UP["U1"] - h, THICK + UP["U1"], "U1 USB-C"))
    return out


scad = os.path.join(HERE, "mockup.scad")
with open(scad, "w") as f:
    f.write("// generated by make_mockup.py: the jank board v5 and its parts as blocks (both connectors on every output)\n")
    f.write("union() {\n  difference() {\n")
    f.write(f"    linear_extrude({THICK}) polygon({outer});\n")
    for fp in board.GetFootprints():              # (the M2 holes stay open)
        if fp.GetReference().startswith("H"):
            x, y = mm(fp.GetPosition().x), mm(fp.GetPosition().y)
            f.write(f"    translate([{x}, {-y}, -1]) cylinder(d=2.2, h={THICK + 2}, $fn=32);\n")
    f.write("  }\n")
    f.write("\n".join(blocks()) + "\n}\n")
stl = os.path.join(HERE, "mockup.stl")
subprocess.run(["openscad", "--backend", "manifold", "-o", stl, scad], check=True, capture_output=True)
print("wrote", stl)
for old in ("mockup-din.stl", "mockup-trs.stl"):  # (v4's two builds: v5 fits both connectors)
    if os.path.exists(os.path.join(HERE, old)):
        os.remove(os.path.join(HERE, old))
subprocess.run(["python3", os.path.join(HERE, "..", "check_stl.py"), stl], check=True)
