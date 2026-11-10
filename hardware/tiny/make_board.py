# SPDX-License-Identifier: GPL-3.0-only
"""jank tiny's board (tiny.kicad_pcb, README "Layout"), with KiCad's own Python: the parts placed from PLACE, the nets
from fab/intended-nets.json (make_schematic.py), the tracks by route.py, both layers poured with GND, the antenna's
keep-out, ground stitching (hardware/pcb/lib/stitch.py), the silkscreen.

  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 make_board.py

Board frame: x right, y down, origin the top left corner (KiCad's page = (100 + x, 100 + y)). The top edge carries the
USB-C, the left edge the MIDI OUT jack, the right edge the POWER IN jack, the bottom edge the module's antenna. Every
part is on the top side (one-side assembly).
"""
import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "pcb", "lib"))
import make_footprints
import parts as P
import route
from stitch import stitch            # (hardware/pcb/lib/stitch.py, after m-card's stitch(), CC0-1.0)

MM = pcbnew.FromMM
OX, OY = 100.0, 100.0
LIB = os.path.join(HERE, "lib", "tiny.pretty")
W, H = 42.0, 44.0
CX, CY = 21.0, H - 0.3 - 12.78               # the module's origin (turned 180: antenna down, its end 0.3 inside the edge)
KEEP_Y = CY + 7.75 - 0.3                     # the antenna keep-out: from 0.3 above the antenna's start to the bottom edge

# ref: (x, y, rotation); the footprints' origins (JLC's placement origins)
PLACE = {
    "J1": (CX, 4.97, 180), "J2": (7.25, 5.0, 0), "J3": (W - 7.25, 5.0, 180),
    "U1": (CX, CY, 180),
    "C1": (6.3, 27.0, 270), "C2": (W - 6.3, 27.0, 270),
    "P2": (1.6, 17.0, 90), "P3": (W - 1.6, 17.0, 90),
    # left top: the MIDI OUT resistors, the button
    "R14": (6.0, 11.6, 90), "R15": (10.0, 11.6, 90), "SW1": (8.0, 16.6, 0),
    # right top: the POWER IN resistors and diodes
    "R2": (W - 10.0, 11.6, 270), "D2": (W - 10.0, 16.0, 90), "R1": (W - 6.0, 11.6, 270), "D1": (W - 6.0, 16.0, 90),
    # centre left: CC2, the enable logic, the VBUS sense
    "R4": (18.8, 9.7, 90), "D5": (15.6, 10.3, 0), "U3": (15.6, 14.0, 0), "D4": (15.6, 17.6, 0),
    "C4": (18.6, 13.6, 90), "R6": (18.6, 17.0, 90), "R12": (14.4, 21.4, 90), "R13": (16.4, 21.4, 90),
    # centre right: CC1, the USB diode and R5, the buck
    "R3": (22.9, 9.6, 90), "D3": (26.6, 9.4, 180), "R5": (26.8, 11.6, 0),
    "U2": (25.3, 15.6, 0), "C3": (28.3, 15.0, 90), "L1": (23.8, 20.75, 90), "C5": (26.6, 20.8, 90), "R7": (28.6, 18.9, 90),
    # bottom left: the module's reset RC, the LED
    "LED1": (2.2, 35.2, 0), "R9": (5.4, 35.2, 0), "C7": (8.3, 37.2, 0), "R8": (11.5, 37.2, 0),
    # bottom right: 3V3 decoupling at the module, the reservoir's sense
    "C6": (30.6, 35.5, 0), "R10": (33.9, 35.5, 0), "R11": (37.2, 35.5, 0), "C8": (40.3, 35.5, 0),
}
WIDTH = {"VSYS": 0.5, "+3V3": 0.4, "VBUS": 0.4, "VUSB_D": 0.4, "SW": 0.4}
ORDER = ["USB_DP", "USB_DN", "SW", "+3V3", "VSYS", "VBUS", "VUSB_D"]     # (the rest after these, shortest first)

board = pcbnew.BOARD()
nets = {}


def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        nets[name] = n
    return nets[name]


def place():
    fps = {}
    for ref, _k, value, fp, _lcsc, pins in P.PARTS:
        x, y, rot = PLACE[ref]
        f = pcbnew.FootprintLoad(LIB, P.FP[fp])
        f.SetFPID(pcbnew.LIB_ID("tiny", P.FP[fp]))
        f.SetReference(ref)
        f.SetValue(value)
        f.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
        f.SetOrientationDegrees(rot)
        f.Reference().SetVisible(False)
        f.Value().SetVisible(False)
        board.Add(f)
        for p in f.Pads():
            n = pins.get(p.GetNumber())
            if n:
                p.SetNet(net(n))
        fps[ref] = f
    for ref in ("P2", "P3"):                     # (the sleeve pads by the edge: no room for thermal spokes)
        for p in fps[ref].Pads():
            if p.GetNumber() == "S":
                p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    return fps


def bb(item):
    b = item.GetBoundingBox()
    return (pcbnew.ToMM(b.GetLeft()) - OX, pcbnew.ToMM(b.GetTop()) - OY, pcbnew.ToMM(b.GetRight()) - OX,
            pcbnew.ToMM(b.GetBottom()) - OY)


def pad_layers(p):
    if p.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
        return (0, 1)
    return (0,) if p.IsOnLayer(pcbnew.F_Cu) else (1,)


def add_track(name, layer, x0, y0, x1, y1, w):
    t = pcbnew.PCB_TRACK(board)
    t.SetStart(pcbnew.VECTOR2I(MM(OX + x0), MM(OY + y0)))
    t.SetEnd(pcbnew.VECTOR2I(MM(OX + x1), MM(OY + y1)))
    t.SetWidth(MM(w))
    t.SetLayer(pcbnew.F_Cu if layer == 0 else pcbnew.B_Cu)
    t.SetNet(net(name))
    board.Add(t)


def add_via(name, x, y):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
    v.SetDrill(MM(route.VIA_DRILL))
    v.SetWidth(MM(route.VIA_D))
    v.SetNet(net(name))
    board.Add(v)


def label(text, x, y, size=0.8, rot=0.0):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
    t.SetTextAngleDegrees(rot)
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(MM(size), MM(size)))
    t.SetTextThickness(MM(0.12))
    board.Add(t)


def pad_center(p):
    return pcbnew.ToMM(p.GetPosition().x) - OX, pcbnew.ToMM(p.GetPosition().y) - OY


def put(n, segs, vias, w):
    for layer, x0, y0, x1, y1 in segs:
        add_track(n, layer, x0, y0, x1, y1, w)
    for x, y in vias:
        add_via(n, x, y)


def entry(p, r):
    """where a track may start or end on a pad: its rectangle, or the square inside a round or oval pad"""
    if p.GetShape(pcbnew.F_Cu) in (pcbnew.PAD_SHAPE_RECT, pcbnew.PAD_SHAPE_ROUNDRECT):
        return r
    cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
    k = min(r[2] - r[0], r[3] - r[1]) / 2 * 0.68
    return (cx - k, cy - k, cx + k, cy + k)


def route_all(fps):
    R = route.Router(W, H)
    R.block((0, 1), (0.0, KEEP_Y, W, H))
    by_net = {}
    for f in fps.values():
        for p in f.Pads():
            n = p.GetNetname() or None
            r = bb(p)
            if p.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                R.drill(*pad_center(p), pcbnew.ToMM(max(p.GetDrillSize().x, p.GetDrillSize().y)))
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                R.rect((0, 1), None, r, 0.05)          # (hole clearance 0.2)
                continue
            R.rect(pad_layers(p), n, r)
            if n:
                by_net.setdefault(n, []).append((entry(p, r), pad_layers(p), p))
    signal = [n for n in by_net if n != "GND" and len(by_net[n]) > 1]

    def span(n):
        xs = [q[0][0] for q in by_net[n]]
        ys = [q[0][1] for q in by_net[n]]
        return (max(xs) - min(xs)) + (max(ys) - min(ys))
    order = [n for n in ORDER if n in signal] + sorted((n for n in signal if n not in ORDER), key=span)
    failed = []
    for n in order:
        w = WIDTH.get(n, 0.2)
        cls = "pwr" if w > 0.25 else "sig"
        pads = [(q[0], q[1]) for q in by_net[n]]
        # (a pad narrower than the track, the buck's 0.36 mm pins: joined first to the nearest wide pad with a 0.2 mm
        # track; the wide route then leaves it out)
        narrow = [q for q in pads if min(q[0][2] - q[0][0], q[0][3] - q[0][1]) < w]
        wide = [q for q in pads if q not in narrow]
        for q in narrow if wide else []:
            c = ((q[0][0] + q[0][2]) / 2, (q[0][1] + q[0][3]) / 2)
            mate = min(wide, key=lambda o: (o[0][0] + o[0][2] - 2 * c[0]) ** 2 + (o[0][1] + o[0][3] - 2 * c[1]) ** 2)
            segs, vias, bad = route.route_net(R, n, "sig", 0.2, [mate, q])
            put(n, segs, vias, 0.2)
            failed += bad
        segs, vias, bad = route.route_net(R, n, cls, w, wide if wide else pads)
        put(n, segs, vias, w)
        failed += bad
    # ground: a via beside every top-side GND pad (the pours join them; this makes sure)
    gvias = 0
    for r, layers, p in by_net["GND"]:
        if layers != (0,):
            continue
        res = route.via_near(R, "GND", r, 0)
        if not res:
            continue
        segs, (x, y) = res
        for layer, x0, y0, x1, y1 in segs:
            add_track("GND", layer, x0, y0, x1, y1, 0.2)
            R.seg(layer, "GND", (x0, y0, x1, y1), 0.2)
        add_via("GND", x, y)
        R.circle((0, 1), "GND", x, y, route.VIA_D / 2 + route.VIA_EXTRA)
        R.drill(x, y, route.VIA_DRILL)
        gvias += 1
    print(f"routed {len(order)} nets, {len(failed)} pads not reached; {gvias} ground vias by pads")
    for e in failed:
        print("FAILED", e)
    return failed


def outline():
    pts = [(0, 0), (W, 0), (W, H), (0, H)]
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pcbnew.VECTOR2I(MM(OX + ax), MM(OY + ay)))
        s.SetEnd(pcbnew.VECTOR2I(MM(OX + bx), MM(OY + by)))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(MM(0.1))
        board.Add(s)


def zones():
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(net("GND"))
        z.SetLocalClearance(MM(0.25))
        z.SetMinThickness(MM(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)   # (SMD pads solid)
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        o = z.Outline()
        o.NewOutline()
        for x, y in ((0, 0), (W, 0), (W, H), (0, H)):
            o.Append(MM(OX + x), MM(OY + y))
        board.Add(z)
    k = pcbnew.ZONE(board)                      # the antenna: no copper, tracks, vias or pour on either layer
    k.SetIsRuleArea(True)
    k.SetDoNotAllowTracks(True)
    k.SetDoNotAllowVias(True)
    k.SetDoNotAllowPads(False)
    k.SetDoNotAllowZoneFills(True)
    k.SetDoNotAllowFootprints(False)
    ls = pcbnew.LSET()
    ls.AddLayer(pcbnew.F_Cu)
    ls.AddLayer(pcbnew.B_Cu)
    k.SetLayerSet(ls)
    k.SetZoneName("antenna keep-out")
    o = k.Outline()
    o.NewOutline()
    for x, y in ((-1, KEEP_Y), (W + 1, KEEP_Y), (W + 1, H + 1), (-1, H + 1)):
        o.Append(MM(OX + x), MM(OY + y))
    board.Add(k)


def silk():
    label("MIDI OUT", 7.0, 9.1, 0.8)
    label("POWER IN", W - 7.0, 9.1, 0.8)
    label("BOOT", 8.0, 20.0, 0.7)
    label("jank tiny", CX, KEEP_Y + 2.6, 1.0)
    label("antenna: no copper", CX, KEEP_Y + 4.0, 0.6)


def main():
    make_footprints.write()
    fps = place()
    failed = route_all(fps)
    outline()
    zones()
    silk()
    pts = [(OX + x, OY + y) for x, y in ((0, 0), (W, 0), (W, KEEP_Y - 0.6), (0, KEEP_Y - 0.6))]
    st = stitch(board, lambda x, y: add_via("GND", x - OX, y - OY), pts)
    print(f"{len(st)} stitching vias")
    board.GetDesignSettings().SetAuxOrigin(pcbnew.VECTOR2I(MM(OX), MM(OY + H)))
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    out = os.path.join(HERE, "tiny.kicad_pcb")
    board.Save(out)
    print("saved", out, f"{W} x {H} mm")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
