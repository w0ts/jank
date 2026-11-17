# SPDX-License-Identifier: GPL-3.0-only
"""The closed case from above for each screen size on the 72 x 72 board, to scale, each window showing the emulator's
HOME frame (case/home_frame.txt: the OLED code of live/player.html run under node by the screen study) scaled to that
panel's active area. After the screen study's closed_sketch.py (worktrees/screen-study-scratch), redrawn for 72 x 72:
J10 at (38.0, 15.9), the windows from mini_case.scad's SCREENS table, the toggle on the left wall, the D-pad at (61.5,
62.2), CLK / RST at x 48.3 / 55.0.  python3 case/screen_sizes.py   (writes case/renders/screen-sizes-closed.png)
"""
import os
import subprocess

S = 8.0                                     # px per mm
E = 1.8                                     # the case's outer face from the board's edge
PIX = "#e2ecff"
LID, EDGE, CAP, HOLE = "#9a9a9a", "#6f6f6f", "#2c2c2c", "#4a4a4a"
JACK = "#3b8f5a"

# per variant: title, board w, h, window (cx, cy, w, h), active area (x0, y0, w, h), D-pad centre, top-edge parts,
# side jacks (y), toggle (wall, y), shift of today's top parts, note
J10 = (38.0, 15.9)
TOP_JACKS = ((17.6, 3.0), (48.3, 2.0), (55.0, 2.0))      # LINE IN, CLK, RST: x, nose length


def header_variant(title, w, h, pin_row, aa):
    """a module on J10's pins (centred on them, its pin row pin_row under its top edge): window = active area + 2.76 /
    2.14 mm (mini_case.scad), centred on it"""
    x0, y0 = J10[0] - w / 2, J10[1] - pin_row
    ax, ay, aw, ah = x0 + aa[0], y0 + aa[1], aa[2], aa[3]
    return (title, 72, 72, (ax + aw / 2, ay + ah / 2, aw + 2.76, ah + 2.14), (ax, ay, aw, ah), (61.5, 62.2), 0.0,
            ("left", 54.5), f"active area {aw:.1f} x {ah:.1f} mm | case 75.6 x 75.6")


VARIANTS = [
    header_variant("1.3in SH1106 (J10)", 35.4, 33.5, 2.0, (2.95, 7.35, 29.42, 14.70)),
    header_variant("1.54in SSD1309 (J10)", 42.4, 38.0, 2.5, (3.68, 8.15, 35.05, 17.52)),
    ("2.42in SSD1309 (J10B lead)", 72, 72, (36.0, 19.86, 57.5, 29.5), (8.5, 6.11, 55.01, 27.49), (61.5, 62.2), 0.0,
     ("left", 54.5), "active area 55.0 x 27.5 mm | case 75.6 x 75.6"),
]
out = []


def R(x, y, w, h, fill, rx=0.0, stroke="none", sw=0.0):
    out.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx:.2f}" fill="{fill}" '
               f'stroke="{stroke}" stroke-width="{sw}"/>')


def T(x, y, s, size, color="#222", anchor="middle", weight="normal"):
    out.append(f'<text x="{x:.1f}" y="{y:.1f}" font-family="Helvetica, Arial" font-size="{size}" fill="{color}" '
               f'text-anchor="{anchor}" font-weight="{weight}">{s}</text>')


def case_view(ox, oy, v, rows):
    title, w, h, win, aa, cross, dx, toggle, note = v
    p = lambda x, y: (ox + (x + E) * S, oy + (y + E) * S)   # board mm -> px (the case's outer corner at ox, oy)
    # the jacks' noses and the USB-C first (they stick out of the walls), then the lid over them
    for y in (6.4,):                                                   # MIDI OUT 1 / 2: PJ-307C noses out the side walls
        a, b = p(-E - 3.0, y - 2.5)
        R(a, b, 3.0 * S, 5.0 * S, JACK)
        a, b = p(w + E, y - 2.5)
        R(a, b, 3.0 * S, 5.0 * S, JACK)
    for x, l in TOP_JACKS:                                             # LINE IN, CLK, RST: noses out the top wall
        a, b = p(x - 2.5, -E - l)
        R(a, b, 5.0 * S, l * S, JACK)
    side, ty = toggle                                                  # the toggle's lever and nut
    tx = -E - 9.0 if side == "left" else w + E
    a, b = p(tx, ty - 2.0)
    R(a, b, 9.0 * S, 4.0 * S, "#555", rx=1.5 * S)
    a, b = p(-E, -E)
    R(a, b, (w + 2 * E) * S, (h + 2 * E) * S, LID, rx=1.8 * S, stroke=EDGE, sw=2)
    ux = w / 2                                                         # the USB-C's mouth in the top wall
    a, b = p(ux - 4.5, -E)
    R(a, b, 9.0 * S, 1.0 * S, "#333", rx=0.5 * S)
    cx, cy, ww, wh = win                                               # the window and its bezel pocket
    a, b = p(cx - ww / 2 - 2.0, cy - wh / 2 - 2.0)
    R(a, b, (ww + 4.0) * S, (wh + 4.0) * S, "#8a8a8a", rx=0.6 * S)
    a, b = p(cx - ww / 2, cy - wh / 2)
    R(a, b, ww * S, wh * S, "#000")
    ax, ay, aw, ah = aa                                                # the pixels on the active area
    pw, ph = aw / 128.0, ah / 64.0
    for j, row in enumerate(rows):
        for i, c in enumerate(row):
            if c == "1":
                a, b = p(ax + i * pw, ay + j * ph)
                R(a, b, pw * 0.93 * S, ph * 0.93 * S, PIX)
    gx, gy = cross                                                     # the D-pad: the hole, then the four arms
    for horiz in (True, False):
        hl, hw = 8.5 + 0.9, 2.5 + 0.9
        a, b = p(gx - hl, gy - hw) if horiz else p(gx - hw, gy - hl)
        R(a, b, (2 * hl if horiz else 2 * hw) * S, (2 * hw if horiz else 2 * hl) * S, HOLE)
    for horiz in (True, False):
        a, b = p(gx - 8.5, gy - 2.5) if horiz else p(gx - 2.5, gy - 8.5)
        R(a, b, (17.0 if horiz else 5.0) * S, (5.0 if horiz else 17.0) * S, CAP, rx=0.8 * S)
    for lab, lx, ly in (("UP", 0, -5.6), ("DOWN", 0, 6.4), ("NO", -5.8, 0.4), ("YES", 5.9, 0.4)):
        a, b = p(gx + lx, gy + ly)
        T(a, b + 4, lab, 11, "#bbb", "middle", "bold")
    cx0 = ox + (w + 2 * E) * S / 2
    T(cx0, oy - 72, title, 26, "#111", "middle", "bold")
    T(cx0, oy - 44, note, 17, "#444")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    rows = [r.strip() for r in open(os.path.join(here, "home_frame.txt")).read().split("\n") if r.strip()]
    if len(rows) != 64 or any(len(r) != 128 for r in rows):
        raise SystemExit("home.txt: expected 64 rows of 128 pixels")
    widths = [(v[1] + 2 * E) * S for v in VARIANTS]
    gap = 14 * S
    W = int(sum(widths) + gap * (len(VARIANTS) - 1) + 2 * 12 * S)
    H = int(155 + (72 + 2 * E) * S + 120)
    out.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
    out.append(f'<rect width="{W}" height="{H}" fill="#f6f6f6"/>')
    T(12 * S, 46, "jank mini 2.2 (72 x 72), closed, from above, to scale: the HOME screen at each size", 30, "#111", "start", "bold")
    x = 12 * S
    for v, wpx in zip(VARIANTS, widths):
        case_view(x, 175, v, rows)
        x += wpx + gap
    T(12 * S, H - 52, "Screen: the emulator's own HOME drawing (live/player.html OLED code, run as is): 128.4 BPM, CF 87 %, "
      "IN -18 dB, 2 peers, playing.", 17, "#444", "start")
    T(12 * S, H - 26, "Green: the jacks' noses (MIDI OUT left / right, LINE IN, CLK, RST at the top); dark bar: USB-C; grey "
      "lever: power toggle (left wall). One board; one lid per size.", 17, "#444", "start")
    out.append("</svg>")
    png = os.path.join(here, "renders", "screen-sizes-closed.png")
    subprocess.run(["rsvg-convert", "-o", png], input="\n".join(out).encode(), check=True)
    print("wrote", png)


if __name__ == "__main__":
    main()
