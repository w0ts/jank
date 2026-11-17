# SPDX-License-Identifier: GPL-3.0-only
"""a block-by-block start: each block's units, biggest first, at the free spot nearest the block's anchor (spiral,
0.25 mm, four turns). python3 init.py HYP OUT.json"""
import json, math, sys
import model as M, anneal as A

HYP = {
    # A: the module bottom middle (turned 180, its u.FL to the bottom wall), the power block bottom left by J11
    "A": {"U1": (38.6, 180), "module": (38, 49), "recovery": (24, 48), "linA": (46, 47), "linB": (50, 52),
          "linbias": (50, 44), "linfe": (20, 12), "usb": (36, 11), "host": (27, 19), "screen": (38, 20),
          "clk": (48, 18), "rst": (54, 18), "midi1j": (8, 15), "midi1d": (8, 30), "midi2j": (64, 15),
          "midi2d": (64, 30), "batt": (44, 39), "charger": (16, 50), "path": (8, 54), "conv3": (8, 66),
          "conv5": (20, 64), "ui": (10, 41), "lead": (12, 44), "buttons": (55, 56)},
    # W (the experiment "all MIDI on one wall"): J2 / J5 / J6 / J3 down the left wall, the LEDs, J10B and J11 on the
    #  right; MIDI 2's parts by its connectors at the lower left, the power block in from the left wall
    "W": {"U1": (38.6, 180), "module": (38, 49), "recovery": (24, 48), "linA": (46, 47), "linB": (50, 52),
          "linbias": (50, 44), "linfe": (20, 12), "usb": (36, 11), "host": (27, 19), "screen": (38, 20),
          "clk": (48, 18), "rst": (54, 18), "midi1j": (8, 15), "midi1d": (8, 26), "midi2j": (8, 60),
          "midi2d": (8, 46), "batt": (44, 39), "charger": (24, 46), "path": (62, 52), "conv3": (20, 66),
          "conv5": (28, 62), "ui": (64, 40), "lead": (62, 44), "buttons": (55, 56)},
    # B: the power block in the middle left (under the screen: low parts), between the cells, the USB-C and J11
    "B": {"U1": (38.6, 180), "module": (38, 49), "recovery": (24, 48), "linA": (46, 47), "linB": (50, 52),
          "linbias": (50, 44), "linfe": (20, 12), "usb": (36, 11), "host": (44, 22), "screen": (38, 20),
          "clk": (48, 18), "rst": (54, 18), "midi1j": (8, 15), "midi1d": (8, 30), "midi2j": (64, 15),
          "midi2d": (64, 30), "batt": (44, 39), "charger": (26, 30), "path": (12, 50), "conv3": (20, 46),
          "conv5": (28, 38), "ui": (10, 41), "lead": (12, 44), "buttons": (55, 56)},
}


def place_near(st, r, tx, ty, rots=(0, 90, 180, 270)):
    u = st.u[r]
    for k in range(0, 30000):
        d = 0.25 * math.sqrt(k)
        t = k * 2.39996
        x, y = tx + d * math.cos(t), ty + d * math.sin(t)
        for a in rots:
            b = u.gboxes(x, y, a)
            if st.legal(r, (x, y, a)):
                u.x, u.y, u.a = x, y, a
                st.gb[r] = b
                st.gp[r] = u.gpads()
                return True
    print("no room for", r)
    return False


def build(hyp):
    units, fb, c = M.load()
    st = A.State(units, fb, c)
    for r in st.mov:                         # (everything off the board first)
        st.u[r].x, st.u[r].y = 200.0, 200.0
        st.gb[r] = st.u[r].gboxes()
    h = HYP[hyp]
    x, a = h["U1"]
    st.u["U1"].x, st.u["U1"].y, st.u["U1"].a = x, M.MOD_Y, a
    st.gb["U1"] = st.u["U1"].gboxes()
    order = ["conv3", "conv5", "linfe", "clk", "rst", "midi1j", "midi1d", "midi2j", "midi2d", "usb", "screen", "host", "module", "recovery", "linA", "linB", "linbias", "batt", "charger", "path", "ui", "lead", "buttons"]
    for blk in order:
        refs = M.BLOCKS.get(blk, [])
        refs = sorted((r for r in refs if r != "U1" and r in st.u), key=lambda r: -sum((b[2] - b[0]) * (b[3] - b[1]) for b in st.u[r].boxes))
        for r in refs:
            tx, ty = h[blk]
            if r == "J11":
                tx, ty = (67.0, 49.5) if M.os.environ.get("J11_RIGHT") else (5.0, 49.5)
            place_near(st, r, tx, ty)
    st.rebuild()
    return st


if __name__ == "__main__":
    st = build(sys.argv[1])
    print(sys.argv[1], "X", st.X, "L", round(st.L), "cost", round(st.cost()))
    A.save(st, sys.argv[2])
