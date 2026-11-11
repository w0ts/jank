# SPDX-License-Identifier: GPL-3.0-only
"""jank chip's board (chip.kicad_pcb, README "Layout"), with KiCad's own Python: the parts placed from PLACE, the nets
from fab/intended-nets.json (make_schematic.py), the RF path drawn here (the 50 ohm grounded coplanar line, its via
fences, the antenna's feed and end pad), every other track by route.py, both layers poured with GND, the antenna's
ground-free strip, ground stitching (hardware/pcb/lib/stitch.py), the silkscreen.

  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 make_board.py

Board frame: x right, y down, origin the top left corner (KiCad's page = (100 + x, 100 + y)). The top edge carries the
USB-C, the left edge the MIDI OUT jack, the right edge the POWER IN jack, the bottom edge the chip antenna in a strip
with no ground (GE to H). Every part is on the top side (JLC Economic assembly: one side, SMD only).
"""
import math
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
LIB = os.path.join(HERE, "lib", "chip.pretty")
NAME = "chip"
W, H = 44.0, 51.5
GE = 41.5                   # the ground's bottom edge: from here to H no copper but the antenna's (YAGEO eval board: 10 mm)
CX, CY = 18.8, 30.4         # the chip's centre (turned 180: LNA_IN at the right side's bottom)
# the RF path (README "RF"): the line's centre x, its width and gap (grounded coplanar waveguide, 50 ohm: README)
XA = 27.1                   # the line and the antenna on this x
RF_W, RF_GAP = 1.2, 0.2
Y_LINE = CY + 2.6           # LNA_IN's y: L3 and the line's top run along it
ANT_Y = GE + 2.5 + 1.58     # the antenna's centre: its feed pad (S1) 2.0 mm past the ground edge (eval board)
END_PAD = (1.8, 2.5)
DIN_D, DIN_Y = 8.6, 20.0    # the DINs' origins: their faces (8.6 from JLC's origin) on the side edges, their axes at y 20
CAN_Y = 33.8                # the cans' centre y (their courtyards start under the DINs' last holes)        # the copper beyond the antenna's end pad (S2), unconnected (eval board: 1.8 x 2.5)

# ref: (x, y, rotation); the footprints' origins (JLC's placement origins); every part on top
PLACE = {
    # top edge: the jacks (bushings past the side edges), the USB-C between them; the cable pads under the jacks
    "J1": (W / 2, 4.97, 180), "J2": (7.25, 5.8, 0), "J3": (W - 7.25, 5.8, 180),
    # the DIN sockets (DNP, hand-soldered) on the BOTTOM under each side edge, their faces on the edges (README "DIN")
    "J5": (DIN_D, DIN_Y, 90, "B"), "J6": (W - DIN_D, DIN_Y, 270, "B"),
    # between the DINs' holes: left the MIDI OUT resistors, the button, the LED; right the POWER IN resistors and diodes,
    # the detector
    # (every top part keeps 0.8 mm from the DIN pads' copper: the iron's room, README "DIN")
    "R14": (6.3, 17.0, 90), "R15": (8.5, 21.0, 90), "LED1": (19.9, 14.9, 0), "R9": (23.4, 14.9, 0),
    "R1": (W - 2.5, 18.5, 0), "R2": (W - 2.5, 21.5, 0), "D1": (W - 8.4, 15.6, 90), "D2": (W - 6.05, 15.6, 90),
    "U3": (W - 7.7, 21.2, 90), "D5": (W - 7.7, 25.0, 90), "C4": (23.7, 16.3, 0), "R6": (26.0, 15.18, 90),
    # the button: in the middle, between the DINs, 8 mm or more from every tall part (the cans, the jacks, the USB-C) so a
    # finger reaches it through the lid; the LED just above it; the button's pull-up by the chip
    "SW1": (21.6, 19.55, 0), "R16": (15.8, 36.0, 90),
    # the reservoir: one can down each side, under the DINs' holes
    "C1": (4.6, CAN_Y, 90), "C2": (W - 4.6, CAN_Y, 90),
    # under the USB-C: the CC resistors, the VBUS diode and its resistor, the VBUS sense
    "R3": (W / 2 - 4.2, 9.0, 0), "R13": (W / 2 - 4.2, 10.8, 0), "D3": (W / 2, 10.1, 0), "R12": (W / 2 + 3.6, 9.8, 90),
    "R4": (W / 2 + 5.4, 9.8, 90), "R5": (W / 2, 12.35, 0),
    # centre: the buck, BUCK_EN's diode, the senses
    # the buck: lower left, beside C1 (VSYS) and under the chip's left side
    "U2": (11.2, 32.9, 0), "L1": (11.2, 37.4, 0), "C5": (11.2, 40.0, 0), "R7": (13.9, 35.25, 0), "C6": (14.35, 36.75, 90),
    "C3": (14.35, 39.2, 90), "D4": (27.13, 19.2, 0),
    # the chip (turned 180: LNA_IN at the bottom of its right side) and its parts round it
    "U1": (CX, CY, 180),
    "C20": (CX + 0.6, CY - 4.45, 0),                                                    # VDD3P3_RTC (pin 20, top)
    "R10": (38.52, 21.2, 90), "R11": (27.0, 22.0, 90), "C8": (28.0, 21.95, 90), "R8": (27.85, 28.1, 90), "C7": (26.9, 28.0, 90),
    "C21": (13.9, 30.5, 270), "C22": (13.9, 32.3, 270),                   # VDD_SPI (pin 29, left)
    "C19": (CX - 1.4, CY + 4.85, 90),                                                  # VDD3P3_CPU (pin 46, bottom)
    "C13": (CX + 2.94, CY + 4.9, 270), "C12": (CX + 3.87, CY + 4.9, 270), "C11": (CX + 5.9, CY + 4.6, 90),   # VDDA
    "L2": (CX + 9.1, CY - 0.2, 270), "C18": (CX + 9.8, CY + 1.5, 0),                   # VDD3P3: the 2 nH, 100 nF after it
    "C16": (29.0, 30.0, 90), "C14": (30.0, 32.2, 90), "C15": (31.0, 32.2, 90),
    "C17": (32.4, 32.4, 90),                                                    # (10 uF: the checklist)
    "Y1": (19.5, CY + 7.9, 180), "R17": (CX + 2.0, CY + 4.85, 90), "C10": (16.75, 37.7, 270), "C9": (21.0, CY + 10.3, 0),
    # RF (README "RF"): the chip's pi (C23 shunt at the pin, L3 series, C24 shunt), the line down to the antenna's pi
    # (C25 shunt, R18 the series 0 ohm, C26 shunt) at the ground edge; R19 / J4 the u.FL path (unfitted)
    "C23": (CX + 4.8, Y_LINE + 1.8, 270), "L3": (CX + 5.9, Y_LINE, 0), "C24": (XA + 1.35, Y_LINE + 1.0, 0),
    "C25": (XA + 1.35, GE - 3.2, 0), "R18": (XA, GE - 1.6, 270), "C26": (XA + 1.35, GE - 0.9, 0),
    "R19": (XA - 1.4, GE - 5.15, 180), "J4": (XA - 2.9, GE - 2.1, 0),
    "AE1": (XA, ANT_Y, 270),
    # MIDI OUT 2 (README "MIDI OUT 2"): the source's 33 ohm and Schottky, the sink's FET, 10 ohm and gate pull-down
    "D6": (32.15, 35.15, 0), "Q1": (31.4, 37.95, 0), "R21": (31.4, 40.55, 0), "R20": (34.2, 37.8, 90), "R22": (34.2, 39.9, 90),
}
WIDTH = {"VSYS": 0.5, "+3V3": 0.4, "VBUS": 0.4, "VUSB_D": 0.4, "SW": 0.4}
ORDER = ["USB_DP", "USB_DN", "XTAL_P", "XTAL_N", "XTAL_X1", "VDD_SPI", "CHIP_PU", "PWR_HOLD", "VBUS_SENSE", "VBANK_ADC", "BOOT",
         "VDD3P3", "MIDI_TX", "MIDI2_TX", "LED_DRV", "SW", "+3V3", "VSYS", "VBUS", "VUSB_D", "BUCK_EN", "DET"]
# (the chip's signal pins escape first: on its right side they sit between the RF supply's pins and the rest)
RF_NETS = {"LNA_IN", "RF", "ANT_FEED", "UFL", "ANT_END"}
PRIORITY = ["USB_DP", "USB_DN", "XTAL_P", "XTAL_N", "XTAL_X1"]      # (routed before anything else, ground vias included)      # (drawn here, not routed)
# the chip's right-side signal pins (between the RF supply's pins and the rest): drawn fan-out, one lane each, the lowest
# pin on the outermost lane so they never cross; the router goes on from each lane's end. (net, chip pin y, lane x)
# (net, chip pin y, lane x, lane end y, a via at the end): 0.8 mm apart, each ending in a via, so the router may take
# any of them on either layer (BOOT, for one, crosses the others to reach the button on the left)
FANOUT = [("CHIP_PU", CY + 1.4, CX + 7.3, 26.2, True), ("BOOT", CY + 1.0, CX + 6.5, 26.2, True),
          ("VBANK_ADC", CY + 0.6, CX + 5.7, 26.2, True), ("VBUS_SENSE", CY + 0.2, CX + 4.9, 26.2, True),
          ("PWR_HOLD", CY - 0.6, CX + 4.1, 26.2, True)]

board = pcbnew.BOARD()
nets = {}


def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        nets[name] = n
    return nets[name]


IRON = 0.8                  # the room round each DIN pad for a soldering iron: no top part or via nearer (README "DIN")
DIN_ROOM = []               # (x, y, radius): each DIN pad's copper plus IRON


def in_din_room(x, y):
    return any(math.hypot(x - cx, y - cy) < r + route.VIA_D / 2 for cx, cy, r in DIN_ROOM)


def place():
    fps = {}
    for ref, _k, value, fp, _lcsc, pins in P.PARTS:
        x, y, rot, side = (PLACE[ref] + ("T",))[:4]
        f = pcbnew.FootprintLoad(LIB, P.FP[fp])
        f.SetFPID(pcbnew.LIB_ID("chip", P.FP[fp]))
        f.SetReference(ref)
        f.SetValue(value)
        f.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
        f.Reference().SetVisible(False)
        f.Value().SetVisible(False)
        board.Add(f)
        if side == "B":                          # (flipped once on the board: a loose footprint has no layers to flip)
            f.Flip(f.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
        f.SetOrientationDegrees(rot)
        if not P.fitted(ref):                    # (pads only: no part, no model, out of the BOM and the CPL)
            f.SetDNP(True)
            f.SetExcludedFromBOM(True)
            f.SetExcludedFromPosFiles(True)
        for p in f.Pads():
            n = pins.get(p.GetNumber())
            if n:
                p.SetNet(net(n))
        fps[ref] = f
    for p in fps["J1"].Pads():                   # (the USB-C's shell legs: crowded, joined to the pours solid)
        if p.GetNumber() in ("13", "14"):
            p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    for ref in ("J5", "J6"):
        for p in fps[ref].Pads():
            DIN_ROOM.append((*pad_center(p), pcbnew.ToMM(p.GetSize().x) / 2 + IRON))
    for p in fps["U1"].Pads():                   # (the exposed pad: solid to the pour, its vias inside)
        if p.GetNumber() == "57":
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


def pad_of(fps, ref, num):
    return [p for p in fps[ref].Pads() if p.GetNumber() == num][0]


# ---- the RF path (fixed geometry; the router sees it as copper of its nets)
RF_SEGS = []      # (net, layer, x0, y0, x1, y1, width)
FAN_VIAS = []     # (net, x, y): the fan-out's vias
RF_VIAS = []      # GND vias: the fences along the line, the shunt capacitors' ground ends, the exposed pad's
COPPER = []       # (rect) unconnected copper: the antenna's end pad extension


def rf_path(fps):
    """the line from L3 to R18 (net RF), its stubs to C24 / C25 / R19, LNA_IN's stub to C23, the antenna feed"""
    del RF_SEGS[:], RF_VIAS[:], COPPER[:], FAN_VIAS[:]
    lx, ly = pad_center(pad_of(fps, "U1", "1"))
    c23 = pad_center(pad_of(fps, "C23", "1"))
    l3a, l3b = pad_center(pad_of(fps, "L3", "1")), pad_center(pad_of(fps, "L3", "2"))
    RF_SEGS.append(("LNA_IN", 0, lx, ly, l3a[0], l3a[1], 0.25))           # (the pin is 0.2 wide)
    RF_SEGS.append(("LNA_IN", 0, c23[0], ly, c23[0], c23[1], 0.3))        # (C23: the stub Espressif asks for)
    r18 = pad_center(pad_of(fps, "R18", "1"))
    y_turn = Y_LINE
    RF_SEGS.append(("RF", 0, l3b[0], y_turn, l3b[0] + RF_W / 2, y_turn, 0.5))   # (out of L3's pad, narrow)
    RF_SEGS.append(("RF", 0, l3b[0] + RF_W / 2, y_turn, XA, y_turn, RF_W))
    RF_SEGS.append(("RF", 0, XA, y_turn, XA, r18[1] - RF_W, RF_W))       # (its round end stops clear of R18's pad 2)
    RF_SEGS.append(("RF", 0, XA, r18[1] - RF_W, XA, r18[1], 0.5))          # (tapering into the 0402's pad)
    for ref in ("C24", "C25", "R19"):
        c = pad_center(pad_of(fps, ref, "1"))
        RF_SEGS.append(("RF", 0, XA, c[1], c[0], c[1], 0.5))
    r19b = pad_center(pad_of(fps, "R19", "2"))
    j4 = pad_center(pad_of(fps, "J4", "1"))
    RF_SEGS.append(("UFL", 0, r19b[0], r19b[1], j4[0], r19b[1], 0.4))     # (across, then down between J4's ground pads)
    RF_SEGS.append(("UFL", 0, j4[0], r19b[1], j4[0], j4[1], 0.4))
    r18b = pad_center(pad_of(fps, "R18", "2"))
    a1 = pad_center(pad_of(fps, "AE1", "1"))
    c26 = pad_center(pad_of(fps, "C26", "1"))
    RF_SEGS.append(("ANT_FEED", 0, XA, r18b[1], a1[0], a1[1], 0.5))     # (into the antenna's 1.0 mm pad)
    RF_SEGS.append(("ANT_FEED", 0, XA, c26[1], c26[0], c26[1], 0.5))
    a2 = pad_center(pad_of(fps, "AE1", "2"))     # (the end pad's extension: its own net, nothing else on it)
    RF_SEGS.append(("ANT_END", 0, a2[0], a2[1], a2[0], a2[1] + END_PAD[1] - END_PAD[0] / 2, END_PAD[0]))
    # the fences: GND vias both sides of the line, their rings 0.35 mm from its edge (0.15 inside the pour)
    off = RF_W / 2 + RF_GAP + route.VIA_D / 2 + 0.15
    for x in (XA - off, XA + off):
        y = y_turn + 1.2
        while y < GE - 0.6:
            RF_VIAS.append((x, y))
            y += 0.9
    x = l3b[0] + 1.0
    while x < XA - 0.4:
        RF_VIAS.append((x, y_turn - off))
        x += 0.9
    # the chip's right-side fan-out (FANOUT)
    for n, y, x, top, via in FANOUT:
        RF_SEGS.append((n, 0, CX + 3.41, y, x, y, 0.2))
        RF_SEGS.append((n, 0, x, y, x, top, 0.2))
        if via:
            FAN_VIAS.append((n, x, top))
    # the exposed pad: 3 x 3 vias, 1.0 mm apart (as JLC's footprint drew them)
    for dx in (-1.0, 0.0, 1.0):
        for dy in (-1.0, 0.0, 1.0):
            RF_VIAS.append((CX + dx, CY + dy))


ITEMS = []                                       # (the routing of the attempt in progress: tracks and vias)


def put(n, segs, vias, w):
    for layer, x0, y0, x1, y1 in segs:
        ITEMS.append(("t", n, layer, x0, y0, x1, y1, w))
    for x, y in vias:
        ITEMS.append(("v", n, x, y))


def entry(p, r):
    """where a track may start or end on a pad: its rectangle, or the square inside a round or oval pad"""
    if p.GetShape(pcbnew.F_Cu) in (pcbnew.PAD_SHAPE_RECT, pcbnew.PAD_SHAPE_ROUNDRECT):
        return r
    cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
    k = min(r[2] - r[0], r[3] - r[1]) / 2 * 0.68
    return (cx - k, cy - k, cx + k, cy + k)


def via_free(R, x, y):
    """a GND via fits here (nothing of another net within the via class's clearance, on both layers)"""
    ix, iy = int(round(x / route.GRID)), int(round(y / route.GRID))
    if not (0 <= ix < R.nx and 0 <= iy < R.ny):
        return False
    n = R.nid("GND")
    i = iy * R.nx + ix
    return all(R.mask["via"][layer][i] in (0, n) for layer in (0, 1))


def router(fps):
    """a fresh grid with every pad, the RF copper, the keep-outs; returns (router, {net: [(rect, layers, pad)]})"""
    R = route.Router(W, H)
    R.block((0, 1), (0.0, GE - 0.45, W, H))     # (a via's ring and the clearance stay out of the antenna strip)
    # the bottom layer under the RF path and the chip stays ground (no tracks there)
    R.block((1,), (CX + 3.0, Y_LINE - 0.45, XA + 1.8, Y_LINE + 1.0), ("sig", "pwr"))    # (LNA_IN, L3, the line's top)
    R.block((1,), (XA - 1.8, Y_LINE - 1.0, XA + 1.8, GE), ("sig", "pwr"))               # (the line down to the antenna)
    R.block((1,), (CX - 3.0, CY - 3.0, CX + 3.0, CY + 3.0), ("sig", "pwr"))
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
    for n, layer, x0, y0, x1, y1, w in RF_SEGS:
        R.seg(layer, n, (x0, y0, x1, y1), w)
        if n == "RF":                              # (the line's gap to anything else: RF_GAP, wider than the rules)
            R.seg(layer, None, (x0, y0, x1, y1), w + 2 * max(0.0, RF_GAP + 0.05 - route.CLEAR))
    for r in COPPER:
        R.rect((0,), None, r)
    for n, _y, x, top, via in FANOUT:              # (each lane's end joins its net like a pad)
        by_net.setdefault(n, []).append(((x - 0.1, top - 0.1, x + 0.1, top + 0.1), (0, 1) if via else (0,), None))
    for n, x, y in FAN_VIAS:
        R.circle((0, 1), n, x, y, route.VIA_D / 2 + route.VIA_EXTRA)
        R.drill(x, y, route.VIA_DRILL)
    for x, y, r in DIN_ROOM:                       # (no via in the DIN pads' iron room, on either layer)
        rr = r + route.VIA_D / 2
        for layer in (0, 1):
            a = R.mask["via"][layer]
            for iy in range(max(0, int((y - rr) / route.GRID)), min(R.ny, int((y + rr) / route.GRID) + 2)):
                for ix in range(max(0, int((x - rr) / route.GRID)), min(R.nx, int((x + rr) / route.GRID) + 2)):
                    if math.hypot(ix * route.GRID - x, iy * route.GRID - y) < rr:
                        a[iy * R.nx + ix] = -1
    return R, by_net


def route_all(fps, first=()):
    """one attempt: the nets in `first` before the rest; returns the pads it could not reach"""
    del ITEMS[:]
    R, by_net = router(fps)
    gv = []
    for x, y in RF_VIAS:                           # (fence and exposed-pad vias where they fit)
        if via_free(R, x, y):
            R.circle((0, 1), "GND", x, y, route.VIA_D / 2 + route.VIA_EXTRA)
            R.drill(x, y, route.VIA_DRILL)
            gv.append((x, y))
    put("GND", [], gv, 0.2)
    def ground_vias():
        # a via beside every top-side GND pad (the pours join them), after the USB pair and the crystal but before the
        # other signals, so each pad keeps a way down and the USB / crystal tracks keep the top layer
        n = 0
        # the DIN sockets' ground pins: no via may come near them (the iron's room), so each gets a track to the nearest
        # ground pad of a fitted part; the pours alone can leave them on an island
        smd = [q for q in by_net["GND"] if q[2].GetParentFootprint().GetReference() not in ("J5", "J6", "U1")
               and len(q[1]) == 1]
        for r, layers, p in by_net["GND"]:
            if p.GetParentFootprint().GetReference() not in ("J5", "J6"):
                continue
            c = pad_center(p)
            for mate in sorted(smd, key=lambda q: (q[0][0] + q[0][2] - 2 * c[0]) ** 2 + (q[0][1] + q[0][3] - 2 * c[1]) ** 2)[:4]:
                segs, vias, bad = route.route_net(R, "GND", "sig", 0.3, [(r, layers), (mate[0], mate[1])])
                if not bad:
                    put("GND", segs, vias, 0.3)
                    break
            else:
                print("a DIN ground pin with no track to ground:", p.GetParentFootprint().GetReference(), p.GetNumber())
        for r, layers, p in by_net["GND"]:
            if len(layers) != 1 or p.GetParentFootprint().GetReference() == "U1":
                continue
            res = route.via_near(R, "GND", r, layers[0])
            if not res:
                continue
            segs, (x, y) = res
            put("GND", segs, [(x, y)], 0.2)
            for layer, x0, y0, x1, y1 in segs:
                R.seg(layer, "GND", (x0, y0, x1, y1), 0.2)
            R.circle((0, 1), "GND", x, y, route.VIA_D / 2 + route.VIA_EXTRA)
            R.drill(x, y, route.VIA_DRILL)
            n += 1
        return n

    signal = [n for n in by_net if n != "GND" and n not in RF_NETS and len(by_net[n]) > 1]

    def span(n):
        xs = [q[0][0] for q in by_net[n]]
        ys = [q[0][1] for q in by_net[n]]
        return (max(xs) - min(xs)) + (max(ys) - min(ys))
    order = [n for n in ORDER if n in signal] + sorted((n for n in signal if n not in ORDER), key=span)
    order = [n for n in first if n in order] + [n for n in order if n not in first]
    order = [n for n in PRIORITY if n in order] + [n for n in order if n not in PRIORITY]
    failed = []
    gvias = None
    for n in order:
        if gvias is None and n not in PRIORITY:
            gvias = ground_vias()
        w = WIDTH.get(n, 0.2)
        cls = "pwr" if w > 0.25 else "sig"
        pads = [(q[0], q[1]) for q in by_net[n]]
        # (a pad narrower than the track (the chip's 0.2 mm pins, the buck's 0.36 mm): joined first to the nearest wide
        # pad with a 0.2 mm track; the wide route then leaves it out)
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
        if os.environ.get("VERBOSE"):
            print(f"  {n}: {len(segs)} segments, {len(vias)} vias, {len(bad)} failed", flush=True)
    print(f"routed {len(order)} nets, {len(failed)} pads not reached; {gvias} ground vias by pads, {len(gv)} RF / pad vias")
    for e in failed:
        print("  not reached:", e)
    return failed


def route_best(fps, attempts=int(os.environ.get("ATTEMPTS", "4"))):
    """rip up and retry: the nets that failed go first next time; the best attempt goes on the board"""
    first, best = [], None
    for k in range(attempts):
        failed = route_all(fps, first)
        if best is None or len(failed) < len(best[0]):
            best = (failed, list(ITEMS))
        if not failed:
            break
        bad = []
        for e in failed:
            n = e.split(":")[0]
            if n not in bad:
                bad.append(n)
        first = bad + [n for n in first if n not in bad]
    failed, items = best
    for it in items:
        if it[0] == "t":
            add_track(it[1], *it[2:])
        else:
            add_via(it[1], it[2], it[3])
    for e in failed:
        print("FAILED", e)
    return failed


def draw_rf():
    for n, layer, x0, y0, x1, y1, w in RF_SEGS:
        add_track(n, layer, x0, y0, x1, y1, w)
    for n, x, y in FAN_VIAS:                      # (a fan-out via only where the route went on on the bottom layer)
        if any(t.GetNetname() == n and t.GetLayer() == pcbnew.B_Cu and t.GetClass() == "PCB_TRACK"
               and min(abs(pcbnew.ToMM(e.x) - OX - x) + abs(pcbnew.ToMM(e.y) - OY - y) for e in (t.GetStart(), t.GetEnd())) < 0.3
               for t in board.GetTracks()):
            add_via(n, x, y)
    for x0, y0, x1, y1 in COPPER:                # (the antenna's end pad: copper with no net, as on YAGEO's eval board)
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_RECT)
        s.SetStart(pcbnew.VECTOR2I(MM(OX + x0), MM(OY + y0)))
        s.SetEnd(pcbnew.VECTOR2I(MM(OX + x1), MM(OY + y1)))
        s.SetFilled(True)
        s.SetWidth(0)
        s.SetLayer(pcbnew.F_Cu)
        board.Add(s)


def tie_islands():
    """a GND via in every top-layer pour island that has none (an island held only by a capacitor's ground pad, cut off
    by the tracks round it): the point of it furthest from its edge"""
    vias = [v.GetPosition() for v in board.GetTracks() if v.GetClass() == "PCB_VIA" and v.GetNetname() == "GND"]
    added = 0
    for z in board.Zones():
        if z.GetIsRuleArea() or not z.IsOnLayer(pcbnew.F_Cu):
            continue
        poly = z.GetFilledPolysList(pcbnew.F_Cu)
        for i in range(poly.OutlineCount()):
            if any(poly.Contains(v, i) for v in vias):
                continue
            o = poly.Outline(i)
            b = o.BBox()
            best = None
            for ix in range(int(pcbnew.ToMM(b.GetWidth()) / 0.05) + 1):
                for iy in range(int(pcbnew.ToMM(b.GetHeight()) / 0.05) + 1):
                    p = pcbnew.VECTOR2I(b.GetLeft() + MM(ix * 0.05), b.GetTop() + MM(iy * 0.05))
                    if not poly.Contains(p, i):
                        continue
                    d = pcbnew.ToMM(o.SquaredDistance(p) ** 0.5) if hasattr(o, "SquaredDistance") else 0
                    if best is None or d > best[0]:
                        best = (d, p)
            if best and best[0] >= route.VIA_D / 2:
                if not in_din_room(pcbnew.ToMM(best[1].x) - OX, pcbnew.ToMM(best[1].y) - OY):
                    add_via("GND", pcbnew.ToMM(best[1].x) - OX, pcbnew.ToMM(best[1].y) - OY)
                added += 1
            else:
                print("a ground island with no via, too small for one at", pcbnew.ToMM(b.GetLeft()) - OX, pcbnew.ToMM(b.GetTop()) - OY)
    print(f"{added} vias tie ground islands")
    return added


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
        z.SetLocalClearance(MM(RF_GAP))         # (the RF line's gap; the rules' 0.15 elsewhere is less)
        z.SetMinThickness(MM(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)   # (SMD pads solid; through-hole pads on spokes)
        z.SetThermalReliefGap(MM(0.5))           # (the DIN sockets' ground pins: a hand iron must heat them)
        z.SetThermalReliefSpokeWidth(MM(0.5))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        o = z.Outline()
        o.NewOutline()
        for x, y in ((0, 0), (W, 0), (W, GE), (0, GE)):
            o.Append(MM(OX + x), MM(OY + y))
        board.Add(z)
    k = pcbnew.ZONE(board)                      # the antenna strip: no pour or via on either layer (its feed is a track)
    k.SetIsRuleArea(True)
    k.SetDoNotAllowTracks(False)
    k.SetDoNotAllowVias(True)
    k.SetDoNotAllowPads(False)
    k.SetDoNotAllowZoneFills(True)
    k.SetDoNotAllowFootprints(False)
    ls = pcbnew.LSET()
    ls.AddLayer(pcbnew.F_Cu)
    ls.AddLayer(pcbnew.B_Cu)
    k.SetLayerSet(ls)
    k.SetZoneName("antenna strip")
    o = k.Outline()
    o.NewOutline()
    for x, y in ((-1, GE), (W + 1, GE), (W + 1, H + 1), (-1, H + 1)):
        o.Append(MM(OX + x), MM(OY + y))
    board.Add(k)


def silk():
    label("OUT", 2.0, 22.0, 0.7, 90)            # (POWER IN's label: on the case; no room by J3)
    label("BOOT", 21.6, 23.0, 0.6)
    label("jank chip", 12.0, GE + 3.0, 1.0)
    label("antenna strip:", 12.0, GE + 4.6, 0.6)
    label("no copper", 12.0, GE + 5.5, 0.6)


def main():
    make_footprints.write()
    fps = place()
    rf_path(fps)
    failed = [] if os.environ.get("PLACE_ONLY") else route_best(fps)   # (PLACE_ONLY=1: placement only, to check it)
    if os.environ.get("PLACE_ONLY"):
        R, _ = router(fps)
        for x, y in RF_VIAS:
            if via_free(R, x, y):
                add_via("GND", x, y)
    draw_rf()
    outline()
    zones()
    silk()
    pts = [(OX + x, OY + y) for x, y in ((0, 0), (W, 0), (W, GE - 0.6), (0, GE - 0.6))]
    st = stitch(board, lambda x, y: None if in_din_room(x - OX, y - OY) else add_via("GND", x - OX, y - OY), pts)
    print(f"{len(st)} stitching vias")
    board.GetDesignSettings().SetAuxOrigin(pcbnew.VECTOR2I(MM(OX), MM(OY + H)))
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    if tie_islands():
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    out = os.path.join(HERE, f"{NAME}.kicad_pcb")
    board.Save(out)
    print("saved", out, f"{W} x {H} mm")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
