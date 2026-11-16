# SPDX-License-Identifier: GPL-3.0-only
"""python3 report.py PLACE.json: the blocks' boxes, the power and signal airwire lengths, the LINE IN to converter
distance, the module's pin map (if swapped)"""
import json, math, sys
import model as M, anneal as A, render as R

units, fb, c = M.load()
st = A.State(units, fb, c)
pl = json.load(open(sys.argv[1]))
R.apply(st, pl)
print(f"crossings {st.X}, airwires {sum(math.dist(a, b) for es in st.edges.values() for a, b, _ in es):.0f} mm")
for blk, refs in M.BLOCKS.items():
    bs = [b for r in refs if r in st.gb for b in st.gb[r]]
    x0, y0 = min(b[0] for b in bs), min(b[1] for b in bs)
    x1, y1 = max(b[2] for b in bs), max(b[3] for b in bs)
    print(f"  {blk:9s} x {x0:5.1f} .. {x1:5.1f}  y {y0:5.1f} .. {y1:5.1f}")
for n in ("VBUS", "VSYS", "VREG", "BAT_P", "+3V3", "V5A", "V5M", "V5C", "V5H", "VLED", "V3_L1", "V3_L2", "V5_SW",
          "UDP", "UDN", "USB_DP", "USB_DN", "LINE_IN", "LINE_SENSE", "LIN_T", "LIN_R", "LIN_S", "CLK_IO", "RST_IO",
          "MIDI_TX", "MIDI_TX2", "SCL", "SDA"):
    print(f"  {n:10s} {sum(math.dist(a, b) for a, b, _ in st.edges.get(n, [])):6.1f} mm")
d = min(M.box_dist(a, b) for r in M.LINE_IN if r in st.gb for o in M.CONVERTERS if o in st.gb
        for a in st.gb[r] for b in st.gb[o])
print(f"LINE IN parts to the converters (the U2 / U18 templates): {d:.1f} mm at the nearest")
u = st.u["U1"]
print(f"U1 at ({u.x:.1f}, {u.y:.1f}) rot {u.a:.0f}")
if "_pins" in pl:
    orig = {f"U1.{p}": p for p in M.PIN_GPIO}
    for nm, pin in sorted(pl["_pins"].items(), key=lambda kv: kv[1]):
        net = next(n for m, n, _x, _y in u.pads if m == nm)
        was = int(nm.split(".")[1])
        if was != pin:
            print(f"  {net:11s} pin {was} (GPIO{M.PIN_GPIO[was]}) -> pin {pin} (GPIO{M.PIN_GPIO[pin]})")
