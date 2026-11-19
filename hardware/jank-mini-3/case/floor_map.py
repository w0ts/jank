"""The floor under jank-mini-3, seen from above (board coordinates, y down): the bottom parts with their depth below
the board, the bridge strips, the cell with its rubber gap, the lid's L, the screw points and the D-pad cross. The
parts come from board_model.scad (generated from lcd2.kicad_pcb); every box number (cell, rubber gap, L, depths) is
read from box.scad itself (its "floor" echo), so the map always shows the current box. Writes floor-map-corner.svg and,
with rsvg-convert, floor-map-corner.png.
python3 floor_map.py"""
import json
import os
import re
import shutil
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
S, M = 8.0, 40.0                      # px per mm, margin


def box_numbers() -> list:
    """box.scad's echo(floor = [...]) (OpenSCAD writes it to a .echo file; temp file in ../.t, git-ignored)"""
    os.makedirs(os.path.join(HERE, "..", ".t"), exist_ok=True)
    with tempfile.TemporaryDirectory(dir=os.path.join(HERE, "..", ".t")) as tmp:
        out = os.path.join(tmp, "floor.echo")
        subprocess.run(["nice", "-n", "19", "openscad", "-o", out, os.path.join(HERE, "box.scad")], check=True,
                       capture_output=True)
        txt = open(out).read()
    m = re.search(r"ECHO: floor = (\[.*\])", txt)
    if not m:
        raise SystemExit("box.scad printed no floor echo")
    return json.loads(m.group(1))


(W, H, DIN, FREE, CELL, GAP, L_TOP, L_LEFT, CUT, (DPX, DPY, SWO), HOLES, ROW_X, STRIP_L, STRIP_W,
 STRIPS, PSW, TABS) = box_numbers()
PSW_Y, PSW_Z, PSW_D, PSW_XIN, PSW_XTAB, PSW_TAB_W, PSW_XCAP, PSW_CAP = PSW


def X(x):
    return M + x * S


def Y(y):
    return M + y * S


def rect(x0, y0, x1, y1, fill, stroke="#333", op=1.0, dash=""):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<rect x="{X(x0):.1f}" y="{Y(y0):.1f}" width="{(x1 - x0) * S:.1f}" height="{(y1 - y0) * S:.1f}" '
            f'fill="{fill}" fill-opacity="{op}" stroke="{stroke}" stroke-width="1"{d}/>')


def text(x, y, s, size=11, anchor="middle", color="#111"):
    return (f'<text x="{X(x):.1f}" y="{Y(y):.1f}" font-size="{size}" text-anchor="{anchor}" fill="{color}" '
            f'font-family="Helvetica">{s}</text>')


parts = {}
for m in re.finditer(r'\["(\w+)", "B", "(\w+)", ([-\d.]+), ([-\d.]+), ([^,]+), ([-\d.]+), ([-\d.]+), [-\d.]+\]',
                     open(os.path.join(HERE, "board_model.scad")).read()):
    ref, what, x0, y0, z0, x1, y1 = m.groups()
    z0 = -1.6 - DIN if "din_depth" in z0 else float(z0)
    depth = round(-1.6 - z0, 1)
    p = parts.setdefault(ref, [99, 99, -99, -99, 0])
    p[0], p[1] = min(p[0], float(x0)), min(p[1], float(y0))
    p[2], p[3] = max(p[2], float(x1)), max(p[3], float(y1))
    p[4] = max(p[4], depth)

out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{2 * M + W * S:.0f}" height="{2 * M + H * S + 250:.0f}">',
       '<rect width="100%" height="100%" fill="white"/>']
out.append(rect(0, 0, W, H, "#e8f0e8"))
out.append(rect(*CUT, "#c9d6ef", "#4466aa"))
out.append(text((CUT[0] + CUT[2]) / 2, (CUT[1] + CUT[3]) / 2 + 2, "LCD-2 (its back parts ~3.5 deep)", 12, color="#334"))
# the cell and its rubber gap
cx0, cy0, cx1, cy1, ct = CELL
out.append(rect(cx0 - GAP, cy0 - GAP, cx1 + GAP, cy1 + GAP, "#f6e3b0", "#b07020", 0.6, "4,3"))
out.append(rect(cx0, cy0, cx1, cy1, "#f4c27a", "#b07020", 0.7))
out.append(text((cx0 + cx1) / 2, cy1 - 2.5, f"cell 34.5 x 56 x {ct}: {FREE - ct:.1f} free above it", 11, color="#7a4a10"))
out.append(text((cx0 + cx1) / 2, cy1 - 6.0, f"x {cx0:g}-{cx1:g}, y {cy0:g}-{cy1:g}; rubber {GAP:g} a side", 11,
                color="#7a4a10"))
for yc in STRIPS:
    out.append(rect(ROW_X - STRIP_L / 2, yc - STRIP_W / 2, ROW_X + STRIP_L / 2, yc + STRIP_W / 2, "#888", "#444", 0.55))
    out.append(text(ROW_X, yc + 1.2, "bridge strip (~14 deep)", 10, color="#fff"))
# the bottom parts
for ref, (x0, y0, x1, y1, d) in sorted(parts.items()):
    out.append(rect(x0, y0, x1, y1, "#d06060" if d > 10 else "#e7a0a0", "#802020", 0.8))
    out.append(text((x0 + x1) / 2, (y0 + y1) / 2 + 1.5, f"{ref} {d:g}", 11))
# the lid's screw tabs (an M3 insert each, the screw through the wall) and the right wall's hook
for i, b in enumerate(TABS):
    out.append(rect(*b, "#6a3d9a", "#3a1d6a", 0.85))
    out.append(text((b[0] + b[2]) / 2, b[3] + 3.2 if b[1] < 1 else b[1] - 1.0, "hook" if i == len(TABS) - 1 else "M3 tab", 10, color="#3a1d6a"))
# the L (lid to 0.2 under the board)
for b in (L_TOP, L_LEFT):
    out.append(rect(*b, "#1a5a1a", "#1a5a1a", 0.85))
out.append(text(L_LEFT[0] - 0.6, (L_LEFT[1] + L_LEFT[3]) / 2, "L", 13, "end", "#1a5a1a"))
# the power switch in the left wall: body + tabs inside, the cap outside (in plan)
out.append(rect(PSW_XIN, PSW_Y - PSW_D / 2, PSW_XTAB - 4.0, PSW_Y + PSW_D / 2, "#222", "#000", 0.85))
out.append(rect(PSW_XTAB - 4.0, PSW_Y - PSW_TAB_W / 2, PSW_XTAB, PSW_Y + PSW_TAB_W / 2, "#555", "#000", 0.85))
out.append(rect(PSW_XCAP, PSW_Y - PSW_CAP / 2, PSW_XCAP + 2.0, PSW_Y + PSW_CAP / 2, "#222", "#000", 0.85))
out.append(text((PSW_XIN + PSW_XTAB - 4.0) / 2, PSW_Y + 1.2, f"POWER z {PSW_Z:g}", 10, color="#fff"))


# the cell's lead, sketched along the floor (a route, not measured): red cell -> switch -> MX1.25, black cell -> MX1.25
def path(pts, color):
    d = " ".join(f"{X(x):.1f},{Y(y):.1f}" for x, y in pts)
    out.append(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="3" stroke-dasharray="8,4"/>')
    return sum(((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5 for (x0, y0), (x1, y1) in zip(pts, pts[1:]))


MX = (23.0, 41.0)                                  # the LCD-2's MX1.25: x scaled from Waveshare's back view, y NOT known
EXIT = (CELL[0] + 22.0, CELL[1] - GAP)             # the lead leaving the cell's -y end, right of the L's top arm
CHAN = (EXIT[0], (L_TOP[1] + 15.19) / 2)           # the channel between J5 (y <= 15.19) and the L's top arm
GAPX = (43.1 + L_LEFT[0]) / 2                      # the gap between the strips' right ends and the L's left arm
TABS = (PSW_XTAB + 1.0, PSW_Y)
RED_A = path([EXIT, CHAN, (GAPX, CHAN[1]), (GAPX, 65.5), (TABS[0], 65.5), TABS], "#d01010")
RED_B = path([TABS, (TABS[0], PSW_Y - 6.0), (4.0, PSW_Y - 6.0), (4.0, 50.0), MX], "#d01010")
BLACK = path([EXIT, CHAN, (GAPX, CHAN[1]), (GAPX, MX[1]), MX], "#111")
out.append(f'<circle cx="{X(MX[0]):.1f}" cy="{Y(MX[1]):.1f}" r="6" fill="#d01010"/>')
out.append(text(MX[0], MX[1] - 1.5, "MX1.25 (x ~23, y ?)", 11, color="#d01010"))
# the D-pad cross and switches (on top)
for x0, y0, x1, y1 in ((DPX - 12, DPY - 4, DPX + 12, DPY + 4), (DPX - 4, DPY - 12, DPX + 4, DPY + 12)):
    out.append(rect(x0, y0, x1, y1, "none", "#0a50c0", 1, "6,4"))
for name, dx, dy in (("SW3", 0, -SWO), ("SW4", 0, SWO), ("SW1", -SWO, 0), ("SW5", SWO, 0)):
    out.append(f'<circle cx="{X(DPX + dx):.1f}" cy="{Y(DPY + dy):.1f}" r="{2.5 * S:.1f}" fill="none" stroke="#0a50c0" '
               'stroke-width="2"/>')
    out.append(text(DPX + dx, DPY + dy + 1.2, name, 10, color="#0a50c0"))
out.append(text(DPX, DPY - 13.5, f"D-pad (on top): switches at +/-{SWO:g}", 11, color="#0a50c0"))
for i, (x, y) in enumerate(HOLES, 1):
    out.append(f'<circle cx="{X(x):.1f}" cy="{Y(y):.1f}" r="{1.6 * S:.1f}" fill="#fff" stroke="#000" stroke-width="2"/>')
    out.append(text(x, y + 4.2, f"H{i}", 10))
ly = H + 9.5
out.append(text(0, ly, f"Floor under jank-mini-3, from above (mm, board coordinates, from box.scad). Board underside to lid top = {FREE:g} mm.", 13, "start"))
out.append(text(0, ly + 4, f"Red: through-hole parts under the board, with their depth below it (DINs {DIN:g}: ASSUMED until measured).", 12, "start"))
out.append(text(0, ly + 8, "Orange: the cell on the lid, dashed: its rubber gap. Grey: the bridge strips. Blue dashed: the D-pad cross on top.", 12, "start"))
out.append(text(0, ly + 12, f"Green: the lid's L, up to 0.2 under the board: top arm x {L_TOP[0]:g}-{L_TOP[2]:g}, y {L_TOP[1]:g}-{L_TOP[3]:g}; "
                f"left arm x {L_LEFT[0]:g}-{L_LEFT[2]:g}, y {L_LEFT[1]:g}-{L_LEFT[3]:g}.", 12, "start"))
out.append(text(0, ly + 16, f"Power switch (dark grey) in the left wall, centre y {PSW_Y:g}, z {PSW_Z:g}. Dashed: the cell's lead along the floor (a sketch): "
                f"red cell -> switch {RED_A:.0f} mm,", 12, "start"))
out.append(text(0, ly + 20, f"switch -> MX1.25 {RED_B:.0f} mm (red {RED_A + RED_B:.0f} mm in all), black cell -> MX1.25 {BLACK:.0f} mm; "
                "add the slack for the plugs and bends.", 12, "start"))
out.append(text(0, ly + 24, "Purple: the lid's M3 screw tabs (side screws through the walls) and the hook on the right wall, 8 mm (hook 9) high over the lid.", 12, "start"))
out.append("</svg>")
svg = os.path.join(HERE, "floor-map-corner.svg")
open(svg, "w").write("\n".join(out))
if shutil.which("rsvg-convert"):
    subprocess.run(["rsvg-convert", "-o", svg[:-4] + ".png", svg], check=True)
print(os.path.basename(svg), len(parts), "bottom parts")
