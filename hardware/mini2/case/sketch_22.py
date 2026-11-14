# SPDX-License-Identifier: GPL-3.0-only
"""mini 2.2 simplified, stage 1c: a SKETCH (not the case: mini_case.scad is stage 2). Reads the placed board
(../mini2.kicad_pcb) and draws, as SVG then PNG (renders/sketch-2.2.png): (1) the lid seen from above: its outline, the
openings of the USB-C and the three jacks, BOOT's big cap, RST's small cap, the LED window over the red and the green LED;
(2) the underside seen from above (through the board): the DIN bodies (when fitted), the two PH2.0 sockets, and two
cells of 65 x 18 mm in the free band, with the room left; the antenna strip.

  .../python3.9 case/sketch_22.py
"""
import os
import subprocess

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
B = pcbnew.LoadBoard(os.path.join(HERE, "..", "mini2.kicad_pcb"))
OX = OY = 100.0
E = 1.8                         # (mini_case.scad: the lid's outer face from the board's edge)
BOOT_CAP, RST_CAP, WINDOW = 9.0, 3.8, 4.0      # (proposed: BOOT's cap 9 mm, RST's mini 2's 3.8, one 4 mm window)
CELL = (65.0, 18.0)             # (the largest cell the user names: 62..65 x 18 mm)
CELL_TOP = 13.0                 # (the underside is free from y 13: the jacks' and the USB-C's legs end above it)
K = 8.0


def mm(v):
    return pcbnew.ToMM(v)


def fp(ref):
    return B.FindFootprintByReference(ref)


def pos(ref):
    p = fp(ref).GetPosition()
    return mm(p.x) - OX, mm(p.y) - OY


def box(ref):
    f = fp(ref)
    bb = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd).BBox()
    return mm(bb.GetLeft()) - OX, mm(bb.GetTop()) - OY, mm(bb.GetRight()) - OX, mm(bb.GetBottom()) - OY


def pad_mid(*refs):
    ps = [p.GetPosition() for r in refs for p in fp(r).Pads() if p.GetNumber()]
    return sum(mm(p.x) for p in ps) / len(ps) - OX, sum(mm(p.y) for p in ps) / len(ps) - OY


bb = B.GetBoardEdgesBoundingBox()
W, H = mm(bb.GetWidth()), mm(bb.GetHeight())
GE = H - 10.0
out = []
M = 14.0


def view(ox):
    X = lambda x: (x + ox + M) * K
    Y = lambda y: (y + M) * K

    def rect(x0, y0, x1, y1, style):
        out.append(f'<rect x="{X(x0):.1f}" y="{Y(y0):.1f}" width="{(x1 - x0) * K:.1f}" height="{(y1 - y0) * K:.1f}" {style}/>')

    def circ(x, y, d, style):
        out.append(f'<circle cx="{X(x):.1f}" cy="{Y(y):.1f}" r="{d / 2 * K:.1f}" {style}/>')

    def text(x, y, s, size=11, fill="#222"):
        out.append(f'<text x="{X(x):.1f}" y="{Y(y):.1f}" font-family="Helvetica,Arial" font-size="{size}" text-anchor="middle" fill="{fill}">{s}</text>')
    return rect, circ, text


# ---- (1) the lid from above
rect, circ, text = view(0)
rect(-E, -E, W + E, H + E, 'fill="#e9e6df" stroke="#555" stroke-width="2" rx="14"')
rect(0, 0, W, H, 'fill="none" stroke="#999" stroke-dasharray="6,4"')
for ref in ("J1", "J2", "J3", "J7", "U1"):
    rect(*box(ref), 'fill="none" stroke="#b9b2a5"')
rect(0, GE, W, H, 'fill="#d8e6d8" opacity="0.6"')
text(W / 2, H - 4, "antenna strip (10 mm)", 11, "#476")
for ref, s, top in (("J1", "USB-C", True), ("J7", "LINE IN", True), ("J2", "MIDI OUT", False), ("J3", "MIDI OUT 2", False)):
    x, y = pos(ref)
    if top:
        rect(x - 4, -E - 0.6, x + 4, -E + 0.6, 'fill="#555"')
        text(x, -E - 2.0, s, 10)
    else:
        xx = -E if x < W / 2 else W + E
        rect(xx - 0.6, y - 3.5, xx + 0.6, y + 3.5, 'fill="#555"')
        text(10 if x < W / 2 else W - 10, y + 6.5, s, 10)
bx, by_ = pos("SW1")
circ(bx, by_, BOOT_CAP, 'fill="#3a3a3a" fill-opacity="0.85" stroke="#111" stroke-width="2"')
text(bx, by_ + 0.5, "BOOT", 11, "#fff")
rx, ry = pos("SW2")
circ(rx, ry, RST_CAP, 'fill="#fff" stroke="#36c" stroke-width="2"')
text(rx, ry + 4.2, "RST", 10, "#246")
lx, ly = pad_mid("LED1", "LED2")
circ(lx, ly, WINDOW, 'fill="#fff" stroke="#c33" stroke-width="2"')
text(lx, ly - 3.0, "LED window (red + green)", 10, "#a22")
text(W / 2, H + E + 5.5, "lid from above (board top view, y down)", 13)

# ---- (2) the underside, seen from above through the board
ox2 = W + 2 * E + 14
rect, circ, text = view(ox2)
rect(0, 0, W, H, 'fill="#f4f4f4" stroke="#555" stroke-width="2"')
rect(0, GE, W, H, 'fill="#d8e6d8" opacity="0.6"')
text(W / 2, H - 4, "antenna strip: no cell, no metal", 11, "#476")
for ref, s in (("J5", "DIN (if fitted)"), ("J6", "DIN (if fitted)")):
    x0, y0, x1, y1 = box(ref)
    rect(x0, y0, x1, y1, 'fill="#ddd" stroke="#888" stroke-dasharray="4,3"')
    text((x0 + x1) / 2, y1 + 3, s, 9)
ph_top = min(box("J8")[1], box("J9")[1])
for ref, s in (("J8", "PH2.0 1"), ("J9", "PH2.0 2")):
    x0, y0, x1, y1 = box(ref)
    rect(x0, y0, x1, y1, 'fill="#fdd" stroke="#c33"')
    text((x0 + x1) / 2, y0 - 0.8, s, 9, "#a22")
cx0 = (W - CELL[0]) / 2
for i in range(2):
    y0 = CELL_TOP + i * CELL[1]
    rect(cx0, y0, cx0 + CELL[0], y0 + CELL[1], 'fill="#cfe0ff" fill-opacity="0.55" stroke="#36c" stroke-width="2"')
    text(W / 2, y0 + CELL[1] / 2 + 1, f"cell {i + 1}: {CELL[0]:g} x {CELL[1]:g}", 11, "#246")
left = ph_top - (CELL_TOP + 2 * CELL[1])
text(W / 2, H + E + 5.5, f"underside: two cells in y {CELL_TOP:g}..{CELL_TOP + 2 * CELL[1]:g}, {left:.1f} mm left above the sockets (no DINs)", 13)
din_bottom = max(box("J5")[3], box("J6")[3])
text(W / 2, H + E + 9.5, f"with the DINs fitted: y {din_bottom:.1f}..{ph_top:.1f} = {ph_top - din_bottom:.1f} mm free at full width", 12)

sheet_w = (ox2 + W + 2 * M) * K
sheet_h = (H + 2 * E + 16 + 2 * M) * K
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{sheet_w:.0f}" height="{sheet_h:.0f}">'
       f'<rect width="100%" height="100%" fill="#fff"/>' + "".join(out) + "</svg>")
os.makedirs(os.path.join(HERE, "renders"), exist_ok=True)
sv = os.path.join(HERE, "renders", "sketch-2.2.svg")
open(sv, "w").write(svg)
subprocess.run(["rsvg-convert", "-o", os.path.join(HERE, "renders", "sketch-2.2.png"), sv], check=True)
print("board", W, H, "cells up to", CELL_TOP + 2 * CELL[1], "sockets from", round(ph_top, 2), "room", round(left, 2),
      "DIN band", round(din_bottom, 2), "..", round(ph_top, 2))
