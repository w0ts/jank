# SPDX-License-Identifier: GPL-3.0-only
"""mini 2.2, stage 1: a SKETCH of the lid (not the case: mini_case.scad is stage 2). Reads the placed board
(../mini2.kicad_pcb) and draws, as SVG, (1) the lid seen from above: its outline, the knob (14 mm, the big box's), the
shaft's hole, the two LED windows, RST's cap hole, the openings of the USB-C and the three jacks, the parts under the
lid as outlines; (2) a section through the shaft: board, can, encoder, lid, knob, with heights. Then rsvg-convert makes
renders/sketch-2.2.png.

  .../python3.9 case/sketch_22.py

Heights marked "UNVERIFIED" come from the ALPS EC11 drawing as read here (shaft top 24.5 mm over the board, a 7 mm
bushing over the body), not from a part in hand.
"""
import os
import subprocess

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
B = pcbnew.LoadBoard(os.path.join(HERE, "..", "mini2.kicad_pcb"))
OX = OY = 100.0
E = 1.8                     # (mini_case.scad: the lid's outer face from the board's edge)
LID_TOP, ROOF_IN = 12.4, 11.0   # (mini_case.scad: zt 14.0 - bt 1.6; z_in - bt) over the board's top face
KNOB_D, KNOB_H, BORE = 14.0, 12.0, 10.0
SHAFT_TOP, BUSH_D, BUSH_TOP, BODY_TOP = 24.5, 7.0, 11.5, 4.5   # (ALPS EC11E drawing, UNVERIFIED)
K = 8.0                     # px per mm


def mm(v):
    return pcbnew.ToMM(v)


def fp(ref):
    return B.FindFootprintByReference(ref)


def pos(ref):
    p = fp(ref).GetPosition()
    return mm(p.x) - OX, mm(p.y) - OY


def court(ref):
    f = fp(ref)
    bb = f.GetCourtyard(pcbnew.F_CrtYd).BBox()
    return mm(bb.GetLeft()) - OX, mm(bb.GetTop()) - OY, mm(bb.GetRight()) - OX, mm(bb.GetBottom()) - OY


def pad_mid(ref):
    ps = [p.GetPosition() for p in fp(ref).Pads() if p.GetNumber()]
    return sum(mm(p.x) for p in ps) / len(ps) - OX, sum(mm(p.y) for p in ps) / len(ps) - OY


bb = B.GetBoardEdgesBoundingBox()
W, H = mm(bb.GetWidth()), mm(bb.GetHeight())
sx, sy = pos("ENC1")
shaft = (sx, sy - 0.25)       # (JLC's origin is 0.25 mm below the shaft: the tabs' and the body's centre line)
leds = {"BEAT": pad_mid("LED1"), "STATUS": pad_mid("LED2")}
rst = pos("SW2")

M = 14.0                       # margin, mm
out = []
X = lambda x: (x + M) * K
Y = lambda y: (y + M) * K


def rect(x0, y0, x1, y1, style):
    out.append(f'<rect x="{X(x0):.1f}" y="{Y(y0):.1f}" width="{(x1 - x0) * K:.1f}" height="{(y1 - y0) * K:.1f}" {style}/>')


def circ(x, y, d, style):
    out.append(f'<circle cx="{X(x):.1f}" cy="{Y(y):.1f}" r="{d / 2 * K:.1f}" {style}/>')


def text(x, y, s, size=11, anchor="middle", fill="#222"):
    out.append(f'<text x="{X(x):.1f}" y="{Y(y):.1f}" font-family="Helvetica,Arial" font-size="{size}" text-anchor="{anchor}" fill="{fill}">{s}</text>')


# ---- (1) the lid from above
rect(-E, -E, W + E, H + E, 'fill="#e9e6df" stroke="#555" stroke-width="2" rx="14"')
rect(0, 0, W, H, 'fill="none" stroke="#999" stroke-dasharray="6,4"')
for ref in ("C1", "C2", "J1", "J2", "J3", "J7", "U1", "ENC1"):
    x0, y0, x1, y1 = court(ref)
    rect(x0, y0, x1, y1, 'fill="none" stroke="#b9b2a5" stroke-width="1"')
rect(0, H - 10, W, H, 'fill="#d8e6d8" stroke="none" opacity="0.6"')
text(W / 2, H - 4, "antenna strip (10 mm): no knob, no metal", 11, fill="#476")
# the openings in the walls (USB-C top, LINE IN top, MIDI OUT left, PWR IN right): marks only
for ref, side in (("J1", "top"), ("J7", "top"), ("J2", "left"), ("J3", "right")):
    x, y = pos(ref)
    if side == "top":
        rect(x - 4, -E - 0.6, x + 4, -E + 0.6, 'fill="#555"')
    else:
        xx = -E if side == "left" else W + E
        rect(xx - 0.6, y - 3.5, xx + 0.6, y + 3.5, 'fill="#555"')
labels = {"J1": "USB-C", "J7": "LINE IN", "J2": "MIDI OUT", "J3": "PWR IN / OUT 2"}
for ref, s in labels.items():
    x, y = pos(ref)
    text(x if ref in ("J1", "J7") else (10 if ref == "J2" else W - 10), -E - 2.0 if ref in ("J1", "J7") else y + 6.5, s, 10)
circ(*shaft, KNOB_D, 'fill="#3a3a3a" fill-opacity="0.85" stroke="#111" stroke-width="2"')
circ(*shaft, BUSH_D + 0.6, 'fill="none" stroke="#ddd" stroke-dasharray="3,3"')
text(shaft[0], shaft[1] + 0.9, "knob 14", 11, fill="#fff")
text(shaft[0], shaft[1] + KNOB_D / 2 + 3.2, f"shaft ({shaft[0]:.1f}, {shaft[1]:.1f}); push = BOOT", 10)
for name, (x, y) in leds.items():
    circ(x, y, 3.2, 'fill="#fff" stroke="#c33" stroke-width="2"')
    d = ((x - shaft[0]) ** 2 + (y - shaft[1]) ** 2) ** 0.5
    text(x, y - 3.0, f"{name} LED", 11, fill="#a22")
    text(x + (-2.0 if x < shaft[0] else 2.0), y + 4.6, f"{d:.1f} mm from the shaft", 9)
circ(*rst, 3.8, 'fill="#fff" stroke="#36c" stroke-width="2"')
text(rst[0], rst[1] - 3.0, "RST (button cap, as mini 2)", 10, fill="#246")
text(W / 2, H + E + 5.5, "jank mini 2.2: lid from above (stage 1 sketch, board top view, y down)", 13)

# ---- (2) the section through the shaft (x along the board, z up), drawn right of the top view
ox = W + 2 * E + 22.0          # its left, mm in the sheet
base = 45.0                    # the board's top face at this sheet y (mm)
Z = lambda z: base - z


def srect(x0, z0, x1, z1, style):
    rect(ox + x0, Z(z1), ox + x1, Z(z0), style)


srect(-E, -1.6, W + E, 0, 'fill="#2f5d3a"')                                       # the board
srect(-E, ROOF_IN, W + E, LID_TOP, 'fill="#e9e6df" stroke="#555"')              # the lid's roof
c1x0, _a, c1x1, _b = court("C1")
srect(c1x0, 0, c1x1, 10.5, 'fill="#bbb" stroke="#777"')                          # a can, the tallest part
text(ox + (c1x0 + c1x1) / 2, Z(5), "can 10.5", 9)
e0, _a, e1, _b = court("ENC1")
srect(shaft[0] - 5.9, 0, shaft[0] + 5.9, BODY_TOP, 'fill="#aaa" stroke="#555"')   # the encoder's body
srect(shaft[0] - BUSH_D / 2, BODY_TOP, shaft[0] + BUSH_D / 2, BUSH_TOP, 'fill="#999" stroke="#555"')
srect(shaft[0] - 3.0, BUSH_TOP, shaft[0] + 3.0, SHAFT_TOP, 'fill="#ccc" stroke="#555"')
srect(shaft[0] - KNOB_D / 2, SHAFT_TOP - BORE, shaft[0] + KNOB_D / 2, SHAFT_TOP - BORE + KNOB_H,
      'fill="#3a3a3a" fill-opacity="0.85" stroke="#111"')
srect(shaft[0] - (BUSH_D + 0.6) / 2, ROOF_IN, shaft[0] + (BUSH_D + 0.6) / 2, LID_TOP, 'fill="#fff" stroke="none"')
for name, (x, _y) in leds.items():
    srect(x - 1.6, 0.6, x + 1.6, LID_TOP, 'fill="#fde" stroke="#c33" stroke-dasharray="3,2"')
for z, s in ((0, "board top 0"), (BODY_TOP, f"body {BODY_TOP:g} (UNVERIFIED)"), (ROOF_IN, f"lid roof {ROOF_IN:g}..{LID_TOP:g}"),
             (BUSH_TOP, f"bushing top {BUSH_TOP:g} (UNVERIFIED)"), (SHAFT_TOP - BORE, f"knob bottom {SHAFT_TOP - BORE:g}"),
             (SHAFT_TOP, f"shaft top {SHAFT_TOP:g} (UNVERIFIED)"), (SHAFT_TOP - BORE + KNOB_H, f"knob top {SHAFT_TOP - BORE + KNOB_H:g}")):
    out.append(f'<line x1="{X(ox + W + E):.1f}" y1="{Y(Z(z)):.1f}" x2="{X(ox + W + E + 2):.1f}" y2="{Y(Z(z)):.1f}" stroke="#333"/>')
    dz = {BUSH_TOP: -1.2, ROOF_IN: 1.4}.get(z, 0.0)          # (the two labels 0.5 mm apart: spread)
    text(ox + W + E + 2.5, Z(z) + 0.4 + dz, s, 9, "start")
text(ox + W / 2, Z(-6), "section through the shaft (z up, mm over the board's top face; LED light pipes dashed)", 11)

sheet_w = (ox + W + E + 30 + 2 * M) * K
sheet_h = (H + 2 * E + 12 + 2 * M) * K
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{sheet_w:.0f}" height="{sheet_h:.0f}">'
       f'<rect width="100%" height="100%" fill="#fff"/>' + "".join(out) + "</svg>")
os.makedirs(os.path.join(HERE, "renders"), exist_ok=True)
sv = os.path.join(HERE, "renders", "sketch-2.2.svg")
open(sv, "w").write(svg)
subprocess.run(["rsvg-convert", "-o", os.path.join(HERE, "renders", "sketch-2.2.png"), sv], check=True)
print("shaft", shaft, "leds", leds, "rst", rst, "board", W, H)
