# SPDX-License-Identifier: GPL-3.0-only
"""the floorplan pictures: python3 render.py PLACE.json OUT_PREFIX [title]
OUT_PREFIX-top.png: the top side, every part's courtyard coloured by block, its name, the blocks' outlines and names,
the screen's outline, the keep-outs, the ratsnest (each net's airwires, GND left out: the pour); OUT_PREFIX-bottom.png:
the bottom side seen from below (x mirrored): the jacks', DINs' and sockets' bodies, their pads, the same ratsnest."""
import json, math, subprocess, sys, zlib
import model as M, anneal as A

S = 12.0          # px per mm
PAD = 40
COL = {"usb": "#1f77b4", "host": "#17becf", "screen": "#9467bd", "midi1j": "#2ca02c", "midi1d": "#2ca02c",
       "midi2j": "#2ca02c", "midi2d": "#2ca02c", "clk": "#d62728", "rst": "#d62728", "linfe": "#ff7f0e",
       "linA": "#ff7f0e", "linB": "#ff7f0e", "linbias": "#ff7f0e", "module": "#8c564b", "recovery": "#8c564b",
       "batt": "#e377c2", "charger": "#bcbd22", "path": "#bcbd22", "conv3": "#7f7f00", "conv5": "#7f7f00",
       "lead": "#9467bd", "ui": "#7f7f7f", "buttons": "#7f7f7f", None: "#555555"}
NAME = {"usb": "USB-C CC / VBUS", "host": "USB host", "screen": "screen", "midi1j": "MIDI 1 jack", "midi1d": "MIDI 1 DIN",
        "midi2j": "MIDI 2 jack", "midi2d": "MIDI 2 DIN", "clk": "CLOCK", "rst": "RESET", "linfe": "LINE IN front",
        "linA": "LINE IN node A", "linB": "LINE IN node B", "linbias": "LINE IN bias", "module": "module",
        "recovery": "recovery", "batt": "cell protection", "charger": "charger", "path": "power path / switch",
        "conv3": "3V3 buck-boost", "conv5": "5 V boost + links", "lead": "2.42 lead", "ui": "LEDs",
        "buttons": "button caps"}
POWER = {"+3V3", "VSYS", "VREG", "V5A", "V5M", "V5C", "VBUS", "BAT_P", "VUSB", "V5H", "VLED"}


def apply(st, place):
    pins = place.get("_pins")
    if pins:
        u = st.u["U1"]
        u.pads = [(nm, n, *(u.pins[pins[nm]] if nm in pins else (dx, dy))) for nm, n, dx, dy in u.pads]
        u.pin_of = dict(pins)
    for r, v in place.items():
        if r.startswith("_"):
            continue
        x, y, a = v
        if r in st.u:
            st.u[r].x, st.u[r].y, st.u[r].a = x, y, a
    st.rebuild()


def X(x, mirror=False):
    return PAD + S * ((M.W - x) if mirror else x)


def Y(y):
    return PAD + S * y


def svg(st, mirror, title):
    w, h = int(2 * PAD + S * M.W), int(2 * PAD + S * M.H + 60)
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" font-family="Helvetica">',
         f'<rect width="{w}" height="{h}" fill="white"/>',
         f'<rect x="{X(0)}" y="{Y(0)}" width="{S * M.W}" height="{S * M.H}" fill="#f4f1e8" stroke="black" stroke-width="2"/>']
    sx0 = M.SCREEN[2] if mirror else M.SCREEN[0]
    o.append(f'<rect x="{X(sx0, mirror)}" y="{Y(M.SCREEN[1])}" width="{S * (M.SCREEN[2] - M.SCREEN[0])}" '
             f'height="{S * (M.SCREEN[3] - M.SCREEN[1])}" fill="none" stroke="#3060ff" stroke-width="2" stroke-dasharray="10,6"/>')
    o.append(f'<text x="{X((M.SCREEN[0] + M.SCREEN[2]) / 2, mirror)}" y="{Y(M.SCREEN[3]) - 6}" font-size="13" fill="#3060ff" '
             f'text-anchor="middle">1.54" screen outline: parts under 2.0 mm only</text>')
    s2 = (0.0, 1.0, 72.0, 44.0)                   # (the 2.42" module, hung from the roof: case/mini_case.scad "s242")
    o.append(f'<rect x="{X(s2[2] if mirror else s2[0], mirror)}" y="{Y(s2[1])}" width="{S * (s2[2] - s2[0])}" '
             f'height="{S * (s2[3] - s2[1])}" fill="none" stroke="#c03090" stroke-width="2" stroke-dasharray="4,4"/>')
    o.append(f'<text x="{X(36.0, mirror)}" y="{Y(s2[3]) - 6}" font-size="13" fill="#c03090" text-anchor="middle">'
             f'2.42" screen outline (y 1 .. 44, the whole width)</text>')
    # the blocks' outlines (top)
    if not mirror:
        for blk, refs in M.BLOCKS.items():
            bs = [b for r in refs if r in st.gb for b in st.gb[r]]
            if not bs:
                continue
            x0, y0 = min(b[0] for b in bs) - 0.4, min(b[1] for b in bs) - 0.4
            x1, y1 = max(b[2] for b in bs) + 0.4, max(b[3] for b in bs) + 0.4
            o.append(f'<rect x="{X(x0)}" y="{Y(y0)}" width="{S * (x1 - x0)}" height="{S * (y1 - y0)}" fill="{COL[blk]}" '
                     f'fill-opacity="0.07" stroke="{COL[blk]}" stroke-width="2.5" rx="6"/>')
    # parts
    for r, u in st.u.items():
        if mirror and u.side != "B":
            continue
        if not mirror and u.side == "B":
            bb = u.bbox_bottom
            o.append(f'<rect x="{X(bb[0])}" y="{Y(bb[1])}" width="{S * (bb[2] - bb[0])}" height="{S * (bb[3] - bb[1])}" '
                     f'fill="none" stroke="#aaaaaa" stroke-width="1" stroke-dasharray="3,3"/>')
            continue
        boxes = [u.bbox_bottom] if u.side == "B" else st.gb[r]
        col = COL[u.block] if not u.fixed else "#444444"
        for i, b in enumerate(boxes):
            x0 = b[2] if mirror else b[0]
            o.append(f'<rect x="{X(x0, mirror)}" y="{Y(b[1])}" width="{S * (b[2] - b[0])}" height="{S * (b[3] - b[1])}" '
                     f'fill="{col}" fill-opacity="{0.10 if i else 0.30}" stroke="{col}" stroke-width="1.2"/>')
        b = boxes[0]
        fs = 15 if u.fixed or (b[2] - b[0]) * (b[3] - b[1]) > 20 else 9.5
        o.append(f'<text x="{X((b[0] + b[2]) / 2, mirror)}" y="{Y((b[1] + b[3]) / 2) + fs * 0.35}" font-size="{fs}" '
                 f'text-anchor="middle" fill="black">{r}</text>')
    # the keep-outs
    for cx, cy, cr in st.circles:
        o.append(f'<circle cx="{X(cx, mirror)}" cy="{Y(cy)}" r="{S * cr}" fill="none" stroke="#c0c0c0" stroke-width="0.8"/>')
    # pads
    for r, ps in st.gp.items():
        for _nm, n, x, y in ps:
            o.append(f'<circle cx="{X(x, mirror)}" cy="{Y(y)}" r="2.2" fill="{"#d00" if n in POWER else "#222"}"/>')
    # the ratsnest
    for n, es in st.edges.items():
        hue = zlib.crc32(n.encode()) % 360
        col = "#e00000" if n in POWER else f"hsl({hue},70%,40%)"
        wdt = 1.8 if n in POWER else 1.1
        for a, b, _ in es:
            o.append(f'<line x1="{X(a[0], mirror)}" y1="{Y(a[1])}" x2="{X(b[0], mirror)}" y2="{Y(b[1])}" stroke="{col}" '
                     f'stroke-width="{wdt}" stroke-opacity="0.75"/>')
    # block names last (on top)
    if not mirror:
        for blk, refs in M.BLOCKS.items():
            bs = [b for r in refs if r in st.gb for b in st.gb[r]]
            if not bs:
                continue
            x0, y0 = min(b[0] for b in bs) - 0.4, min(b[1] for b in bs) - 0.4
            o.append(f'<text x="{X(x0) + 3}" y="{Y(y0) - 3}" font-size="14" font-weight="bold" fill="{COL[blk]}" '
                     f'stroke="white" stroke-width="3" paint-order="stroke">{NAME[blk]}</text>')
    side = "BOTTOM (seen from below, x mirrored)" if mirror else "TOP"
    o.append(f'<text x="{PAD}" y="{h - 22}" font-size="15">{title} - {side} - crossings {st.X}, airwires '
             f'{sum(math.dist(a, b) for es in st.edges.values() for a, b, _ in es):.0f} mm (no GND)</text>')
    o.append("</svg>")
    return "\n".join(o)


if __name__ == "__main__":
    units, fb, c = M.load()
    st = A.State(units, fb, c)
    apply(st, json.load(open(sys.argv[1])))
    title = sys.argv[3] if len(sys.argv) > 3 else sys.argv[1]
    for mirror, tag in ((False, "top"), (True, "bottom")):
        open(f"{sys.argv[2]}-{tag}.svg", "w").write(svg(st, mirror, title))
        subprocess.run(["rsvg-convert", "-o", f"{sys.argv[2]}-{tag}.png", f"{sys.argv[2]}-{tag}.svg"], check=True)
    print("X", st.X)
