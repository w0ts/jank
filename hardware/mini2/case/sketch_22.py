# SPDX-License-Identifier: GPL-3.0-only
"""mini 2.2, stage 1d / 1f: a SKETCH (not the case: mini_case.scad is stage 2). Reads the placed board (../mini2.kicad_pcb)
and draws, as SVG then PNG (renders/sketch-2.2.png):
(1) the lid seen from above (stage 1f): its outline, the USB-C's opening (the jacks' are in the base now), the four
    buttons' U-tongues (TONGUE: a pinwheel round the cross, each tongue's nub over its switch), no RST, the LED window
    over the red and the green LED, the FPC antenna's seat on the inside of the bottom wall (mini_case.scad);
(2) the underside seen from above (through the board): the DIN bodies (fitted), the two PH2.0 sockets (either way
    round: the user may solder one turned 180 degrees), and two cells standing on edge in the band between the DIN
    bodies and the ground edge, the largest the user names (GNB 550 mAh: 66 x 18 x 6.2; BT2.0 450 mAh: 62 x 15.5 x
    6.7: the sketch takes 66 long and 6.7 thick, the worst of both), with the room left;
(3) a section across the board (y, z) through its middle: the cells standing (18 and 15.5 mm), the DIN body, the
    sockets, and the base's floor (the DIN base's 19.95 mm under the board; today's no-DIN base 4.3 mm).

  .../python3.9 case/sketch_22.py
"""
import os
import subprocess

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
B = pcbnew.LoadBoard(os.path.join(HERE, "..", "mini2.kicad_pcb"))
OX = OY = 100.0
E, CLR, BT = 1.8, 0.4, 1.6      # (mini_case.scad: the lid's outer face from the board's edge; the side play; the board)
WINDOW = 4.0                    # (one 4 mm LED window)
TONGUE = dict(w=4.5, to_nub=7.5, past_nub=1.5, slot=0.8)   # (stage 1f: case/README "Buttons": width, root to the nub, the
#  free end past the nub, the U slot's width)
ANT = (3.0, 45.0)               # (mini_case.scad ant_x0, ant_x0 + ant_l: the FPC antenna's seat along the bottom wall)
CELLS = {"GNB 550 mAh": (66.0, 18.0, 6.2), "BT2.0 450 mAh": (62.0, 15.5, 6.7)}   # (length, width = height on edge, thickness)
CELL_LEN, CELL_T = 66.0, 6.7    # (the sketch's cell: the longest and the thickest of the two)
DIN_UNDER = 19.6 + 0.35         # (mini_case.scad: din_h + din_gap: the DIN base's floor under the board)
NODIN_UNDER = 3.9 + 0.4         # (mini_case.scad: tail + 0.4: today's no-DIN base)
PH_HEIGHT = 6.0                 # (B2B-PH-K-S body under the board, *estimate*; a plug adds a few mm: UNVERIFIED)
K = 8.0


def mm(v):
    return pcbnew.ToMM(v)


def fp(ref):
    return B.FindFootprintByReference(ref)


def pos(ref):
    p = fp(ref).GetPosition()
    return mm(p.x) - OX, mm(p.y) - OY


def box(ref, either_way=False):
    f = fp(ref)
    bb = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd).BBox()
    x0, y0, x1, y1 = mm(bb.GetLeft()) - OX, mm(bb.GetTop()) - OY, mm(bb.GetRight()) - OX, mm(bb.GetBottom()) - OY
    if either_way:                # (the box and its 180-degree turn about the footprint's origin)
        cx, cy = pos(ref)
        x0, y0, x1, y1 = min(x0, 2 * cx - x1), min(y0, 2 * cy - y1), max(x1, 2 * cx - x0), max(y1, 2 * cy - y0)
    return x0, y0, x1, y1


def pad_mid(*refs):
    ps = [p.GetPosition() for r in refs for p in fp(r).Pads() if p.GetNumber()]
    return sum(mm(p.x) for p in ps) / len(ps) - OX, sum(mm(p.y) for p in ps) / len(ps) - OY


bb = B.GetBoardEdgesBoundingBox()
W, H = mm(bb.GetWidth()), mm(bb.GetHeight())
GE = H                          # (stage 1e: no antenna strip: the ground and the cells' band end at the board's edge)
MODULE = (35.0 - 27.3 / 2, 15.9 - 1.5, 35.0 + 27.3 / 2, 15.9 - 1.5 + 27.8)   # (make_board.MODULE: HDR (35.0, 15.9))
INSIDE_W = W + 2 * CLR          # (the case's inside width along x)
out = []
M = 14.0


def view(ox, oy=0.0):
    X = lambda x: (x + ox + M) * K
    Y = lambda y: (y + oy + M) * K

    def rect(x0, y0, x1, y1, style):
        out.append(f'<rect x="{X(x0):.1f}" y="{Y(y0):.1f}" width="{(x1 - x0) * K:.1f}" height="{(y1 - y0) * K:.1f}" {style}/>')

    def circ(x, y, d, style):
        out.append(f'<circle cx="{X(x):.1f}" cy="{Y(y):.1f}" r="{d / 2 * K:.1f}" {style}/>')

    def text(x, y, s, size=11, fill="#222", anchor="middle"):
        out.append(f'<text x="{X(x):.1f}" y="{Y(y):.1f}" font-family="Helvetica,Arial" font-size="{size}" text-anchor="{anchor}" fill="{fill}">{s}</text>')
    return rect, circ, text


# ---- (1) the lid from above
rect, circ, text = view(0)
rect(-E, -E, W + E, H + E, 'fill="#e9e6df" stroke="#555" stroke-width="2" rx="14"')
rect(0, 0, W, H, 'fill="none" stroke="#999" stroke-dasharray="6,4"')
for ref in ("J1", "J2", "J3", "J7", "U1"):
    rect(*box(ref), 'fill="none" stroke="#b9b2a5"')
for ref, s, top in (("J1", "USB-C", True), ("J7", "LINE IN (base)", True), ("J2", "MIDI OUT (base)", False),
                    ("J3", "MIDI OUT 2 (base)", False)):
    x, y = pos(ref)
    if ref != "J1":               # (stage 1f: the jacks under the board: their openings are in the base, not the lid)
        text(x if top else (10 if x < W / 2 else W - 10), (-E - 2.0) if top else y + 6.5, s, 10, "#888")
        continue
    if top:
        rect(x - 4, -E - 0.6, x + 4, -E + 0.6, 'fill="#555"')
        text(x, -E - 2.0, s, 10)
    else:
        xx = -E if x < W / 2 else W + E
        rect(xx - 0.6, y - 3.5, xx + 0.6, y + 3.5, 'fill="#555"')
        text(10 if x < W / 2 else W - 10, y + 6.5, s, 10)
# stage 1e: the screen module (dashed: lid variant 2 has a window over its display; *the display's place on a module
# is typical, unverified*), the four button caps (8 mm, 9.5 mm apart), RST's pinhole, the LED window (lid variant 1),
# the FPC antenna's proposed seat (lower left, off the buttons and the screen)
x0, y0, x1, y1 = MODULE
rect(x0, y0, x1, y1, 'fill="none" stroke="#557" stroke-dasharray="5,3"')
wx, wy = (x0 + x1) / 2, y0 + 12.0
rect(wx - 12.25, wy - 6.5, wx + 12.25, wy + 6.5, 'fill="#1d2a3a" fill-opacity="0.85" stroke="#111"')
text(wx, wy + 0.5, "screen window 24.5 x 13 (lid variant 2)", 10, "#fff")
# stage 2: the U-tongues, all four rooted on their left (stage 1f's pinwheel looked like a hooked cross); the slot
# (dark) round three sides of each tongue (light)
t = TONGUE
for ref, s, (ux, uy) in (("SW3", "UP", (-1, 0)), ("SW1", "NO", (-1, 0)), ("SW4", "DOWN", (-1, 0)), ("SW5", "YES", (-1, 0))):
    bx, by_ = pos(ref)
    ends = [(bx - ux * t["past_nub"], by_ - uy * t["past_nub"]), (bx + ux * t["to_nub"], by_ + uy * t["to_nub"])]
    xs, ys = [e[0] for e in ends], [e[1] for e in ends]
    hw = t["w"] / 2
    x0, x1 = (min(xs), max(xs)) if ux else (bx - hw, bx + hw)
    y0, y1 = (min(ys), max(ys)) if uy else (by_ - hw, by_ + hw)
    sl = t["slot"]
    rect(x0 - sl, y0 - sl, x1 + sl, y1 + sl, 'fill="#333"')
    rx0, ry0, rx1, ry1 = x0, y0, x1, y1   # (the root: no slot on that side)
    if ux > 0:
        rx1 = x1 + sl
    elif ux < 0:
        rx0 = x0 - sl
    elif uy > 0:
        ry1 = y1 + sl
    else:
        ry0 = y0 - sl
    rect(rx0, ry0, rx1, ry1, 'fill="#e9e6df"')
    circ(bx, by_, 2.5, 'fill="#c84" stroke="#642"')
    text((x0 + x1 + ux * t["to_nub"] * 0.5) / 2, (y0 + y1 + uy * t["to_nub"] * 0.5) / 2 + 1.0, s, 8, "#222")   # (toward the root)
text(pos("SW5")[0] - 3.0, H + E + 2.4, "U-tongues: 4.5 wide, 7.5 root to nub, 0.8 slot", 8, "#333")
lx, ly = pad_mid("LED1", "LED2")
circ(lx, ly, WINDOW, 'fill="#fff" stroke="#c33" stroke-width="2"')
text(lx + 7.5, ly + 0.5, "LED window (variant 1)", 9, "#a22")
rect(ANT[0], H + CLR, ANT[1], H + CLR + 0.4, 'fill="#2a7"')
text((ANT[0] + ANT[1]) / 2, H - 1.2, "FPC antenna, upright on the bottom wall's inside", 9, "#174")
jx, jy = pos("J4")
circ(jx, jy, 1.6, 'fill="#2a7"')
text(jx, jy + 3.2, "J4 u.FL", 9, "#174")
text(W / 2, H + E + 5.5, "(1) lid from above, stage 1f (board top view, y down)", 13)

# ---- (2) the underside, seen from above through the board
ox2 = W + 2 * E + 14
rect, circ, text = view(ox2)
rect(-CLR, -CLR, W + CLR, H + CLR, 'fill="none" stroke="#999" stroke-dasharray="3,3"')
rect(0, 0, W, H, 'fill="#f4f4f4" stroke="#555" stroke-width="2"')
for ref, s in (("J2", "MIDI OUT jack"), ("J3", "MIDI OUT 2 jack"), ("J7", "LINE IN")):   # (stage 1f: under the board)
    x0, y0, x1, y1 = box(ref)
    rect(x0, y0, x1, y1, 'fill="#e8e0f8" stroke="#86a"')
    text((x0 + x1) / 2, y1 + 2.4, s, 8, "#649")
din_bottom = 0.0
for ref in ("J5", "J6"):
    x0, y0, x1, y1 = box(ref)
    din_bottom = max(din_bottom, y1)
    rect(x0, y0, x1, y1, 'fill="#ddd" stroke="#888"')
    text((x0 + x1) / 2, (y0 + y1) / 2, "DIN body", 9)
ph_bottom = 0.0
for ref, s in (("J8", "J8"), ("J9", "J9")):
    x0, y0, x1, y1 = box(ref, either_way=True)
    ph_bottom = max(ph_bottom, y1)
    rect(x0, y0, x1, y1, 'fill="#fdd" stroke="#c33"')
    text((x0 + x1) / 2, y0 - 0.8, s, 9, "#a22")
text(W / 2, ph_bottom + 2.2, "PH2.0 sockets, either way round", 9, "#a22")
band = (max(din_bottom, ph_bottom), GE)
cx0 = (W - CELL_LEN) / 2
gap = (band[1] - band[0] - 2 * CELL_T) / 3
for i in range(2):
    y0 = band[0] + gap + i * (CELL_T + gap)
    rect(cx0, y0, cx0 + CELL_LEN, y0 + CELL_T, 'fill="#cfe0ff" fill-opacity="0.7" stroke="#36c" stroke-width="2"')
    text(W / 2, y0 + CELL_T / 2 + 1.2, f"cell {i + 1} on edge: {CELL_LEN:g} x {CELL_T:g}", 10, "#246")
spare = band[1] - band[0] - 2 * CELL_T
text(W / 2, H + E + 5.5, f"(2) underside: band y {band[0]:.1f}..{band[1]:.1f} = {band[1] - band[0]:.1f} mm; two cells on edge:"
     f" {spare:.1f} mm spare (6.7 thick), {band[1] - band[0] - 12.4:.1f} mm (6.2 thick)", 12)
text(W / 2, H + E + 9.5, f"case inside width {INSIDE_W:.1f} mm: GNB 66 long leaves {INSIDE_W - 66:.1f}, BT2.0 62 leaves {INSIDE_W - 62:.1f}"
     " (the lead leaves one end)", 12)

# ---- (3) a section across the board through its middle (y along, z down from the board)
oy3 = H + 2 * E + 22
rect, circ, text = view(ox2, oy3)
rect(0, -BT, W, 0, 'fill="#2f5d3a"')                           # (the board, its bottom face at 0; y along x here)
Z = lambda z: z                                                 # (z down = the sheet's y down)
rect(min(box("J5")[1], box("J6")[1]), 0, din_bottom, 19.6, 'fill="#ddd" stroke="#888" stroke-dasharray="4,3"')
text((din_bottom + min(box("J5")[1], box("J6")[1])) / 2, 10, "DIN body (at the side edges)", 9)
x0, y0, x1, y1 = box("J8", either_way=True)
rect(y0, 0, y1, PH_HEIGHT, 'fill="#fdd" stroke="#c33"')
text((y0 + y1) / 2, PH_HEIGHT + 3, "PH2.0", 9, "#a22")
for i, (name, (ln, wd, th)) in enumerate(CELLS.items()):
    y = band[0] + gap + i * (CELL_T + gap)
    rect(y, 0.3, y + th, 0.3 + wd, 'fill="#cfe0ff" fill-opacity="0.7" stroke="#36c" stroke-width="2"')
    text(y + th + 1.0, 4.0 + 4.0 * i, f"{name}: {wd:g} tall, {th:g} thick", 9, "#246", "start")
rect(-E, DIN_UNDER, W + E, DIN_UNDER + 1.4, 'fill="#999"')
text(W + E + 1, DIN_UNDER + 1, f"DIN base floor {DIN_UNDER:.2f}", 10, anchor="start")
rect(-E, NODIN_UNDER, W + E, NODIN_UNDER + 0.3, 'fill="#c33" opacity="0.6"')
text(W + E + 1, NODIN_UNDER + 0.5, f"today's no-DIN floor {NODIN_UNDER:.1f}", 10, "#a22", "start")
text(W / 2, DIN_UNDER + 6, f"(3) section through the middle (y across, mm under the board): cells 18 / 15.5 tall:"
     f" {DIN_UNDER - 0.3 - 18:.1f} / {DIN_UNDER - 0.3 - 15.5:.1f} mm under them in the DIN base", 12)

sheet_w = (ox2 + W + 2 * M + 30) * K
sheet_h = (oy3 + DIN_UNDER + 12 + 2 * M) * K
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{sheet_w:.0f}" height="{sheet_h:.0f}">'
       f'<rect width="100%" height="100%" fill="#fff"/>' + "".join(out) + "</svg>")
os.makedirs(os.path.join(HERE, "renders"), exist_ok=True)
sv = os.path.join(HERE, "renders", "sketch-2.2.svg")
open(sv, "w").write(svg)
subprocess.run(["rsvg-convert", "-o", os.path.join(HERE, "renders", "sketch-2.2.png"), sv], check=True)
print(f"band {band[0]:.2f}..{band[1]:.2f} ({band[1] - band[0]:.2f}); spare 2 x 6.7: {spare:.2f}, 2 x 6.2: {band[1] - band[0] - 12.4:.2f};"
      f" inside width {INSIDE_W:.2f}; under the cells in the DIN base: {DIN_UNDER - 0.3 - 18:.2f} / {DIN_UNDER - 0.3 - 15.5:.2f}")
