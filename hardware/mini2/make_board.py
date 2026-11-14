# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2's board (mini2.kicad_pcb): mini 2's with the encoder and two RGB LEDs (README "mini 2.2"; PLACE, "mini
2.2"). jank mini 2's board: jank mini's (hardware/chip) with the LINE IN added (README "LINE IN"): an
8 mm band (DY) under the top row of connectors, everything below it moved down by DY, the jack and its front end in it.
Below: mini's own words.

jank chip's board (mini2.kicad_pcb, README "Layout"), with KiCad's own Python: the parts placed from PLACE, the nets
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
NAME = "mini2"
# the size (2026-11-13: SIZE=WxH tries a smaller board; the parts' spread and the chip's height follow it)
W, H0 = (float(v) for v in os.environ.get("SIZE", "66x60").split("x"))   # (PLACED is for 66 x 60)
# mini 2: the LINE IN band (README "LINE IN"): the top row (TOP: the USB-C, both jacks, the USB and VBUS parts under the
# USB-C) stays at the top edge; everything else moves down DY, which opens a band for the LINE IN jack (J7, on the top
# edge between MIDI OUT and the USB-C) and its front end; the DIN sockets go down with it, clear of J7's legs
DY = float(os.environ.get("DY", "8"))
H = H0 + DY
TOP = {"J1", "J2", "J3", "U5", "R3", "R4", "R12", "R13", "R23", "D3", "R5"}
GE = H - 10.0                   # the ground's bottom edge: from here to H no copper but the antenna's (YAGEO eval board: 10 mm)
S = (W - 44.6) / 2                # (the middle's shift: 44 -> 47.6 mm for the 2200 uF cans, 49.6 for the DIN pads' rings; 53.2 for RST (the right side), 54.2 and 56.2 for the left side's tracks)
SB = S - 2.0                # (the buck group's shift: it stays by C1 and J5, the chip moved 2 mm away from it)
CX, CY = 18.8 + S, GE - max(13.5, 12.1 + (H0 - 52.5) * 0.4)   # (13.5 or more: the crystal clear of J4)    # the chip's centre (turned 180: LNA_IN at the right side's bottom)
# the RF path (README "RF"): the line's centre x, its width and gap (grounded coplanar waveguide, 50 ohm: README)
XA = 27.1 + S               # the line and the antenna on this x
RF_W, RF_GAP = 1.2, 0.2
Y_LINE = CY + 2.6           # LNA_IN's y
Y_L3 = Y_LINE + 0.6         # L3 and the line's top, 0.6 under LNA_IN: room for C28 at the RF supply's pins above it
ANT_Y = GE + 2.5 + 1.58     # the antenna's centre: its feed pad (S1) 2.0 mm past the ground edge (eval board)
END_PAD = (1.8, 2.5)        # the copper beyond the antenna's end pad (S2), unconnected (eval board: 1.8 x 2.5)
DIN_D, DIN_Y = 8.6, 20.3 + DY    # the DINs' origins: their faces (8.6 from JLC's origin) on the side edges, their axes at y 20
CAN_Y = 35.5 + (CY - 30.4)               # the cans' centre y (10 x 10.5 mm, upright: under the DINs' last holes, over the ground edge)

# ref: (x, y, rotation); the footprints' origins (JLC's placement origins); every part on top
PLACE = {
    # top edge: the jacks (bushings past the side edges), the USB-C between them
    "J1": (W / 2, 4.97, 180), "J2": (7.25, 5.8, 0), "J3": (W - 7.25, 5.8, 180),
    # the DIN sockets (DNP, hand-soldered) on the BOTTOM under each side edge, their faces on the edges (README "DIN")
    "J5": (DIN_D, DIN_Y, 90, "B"), "J6": (W - DIN_D, DIN_Y, 270, "B"),
    # between the DINs' holes (every top part keeps 0.8 mm from the DIN pads' copper: the iron's room, README "DIN"):
    # left the MIDI OUT resistors; right the POWER IN resistors and diodes, the detector, the reservoir's sense
    "R14": (16.9, 1.8, 0), "R26": (16.9, 4.4, 0),               # MO_R's 2 x 68 ohm 1206, between J2 and the USB-C
    "R15": (8.5, 21.0, 90),
    "R1": (W - 2.5, 18.5, 0), "R2": (W - 2.5, 21.5, 0), "D1": (W - 8.4, 15.6, 90), "D2": (W - 6.05, 15.6, 90),
    "R10": (W - 5.48, 21.2, 90),
    # the button: in the middle, 8 mm or more from every tall part (the cans, the jacks, the USB-C), so a finger reaches it
    # through the lid; the LED above it; BUCK_EN's parts beside it
    # mini 2.2 (README "mini 2.2"): the encoder in the middle of the width (its shaft at (W / 2, 27.5): JLC's origin is
    # 0.25 mm below the shaft), where BOOT, RST and the red LED were; an RGB LED 12 mm each side of the shaft, level
    # with it (LED1 the beat LED left, LED2 the status LED right), their 1 k resistors between them and the encoder;
    # the encoder's pull-ups and caps by its A (left) and B (right) pins; RST up right, under the USB-C's right side
    "ENC1": (W / 2, 27.75, 0),
    "LED1": (W / 2 - 12.0, 27.5, 0), "R40": (W / 2 - 9.0, 26.4, 0), "R41": (W / 2 - 9.0, 27.5, 0), "R42": (W / 2 - 9.0, 28.6, 0),
    "LED2": (W / 2 + 12.0, 27.5, 0), "R43": (W / 2 + 9.0, 26.4, 0), "R44": (W / 2 + 9.0, 27.5, 0), "R45": (W / 2 + 9.0, 28.6, 0),
    "R46": (W / 2 - 7.9, 33.4, 0), "C40": (W / 2 - 7.9, 34.5, 0), "R47": (W / 2 + 7.9, 33.4, 0), "C41": (W / 2 + 7.9, 34.5, 0),
    # the twins (DNP 0603 pads in parallel with R40..R45, README "mini 2.2"): a column above each resistor column, between
    # the encoder and R14 / R26 (left) or Q1 / R20 (right); RT40 / RT43 nearest their resistor rows
    "RT40": (W / 2 - 8.6, 24.9, 0), "RT41": (W / 2 - 8.6, 23.15, 0), "RT42": (W / 2 - 8.6, 21.4, 0),
    "RT43": (W / 2 + 8.6, 24.9, 0), "RT44": (W / 2 + 8.6, 23.15, 0), "RT45": (W / 2 + 8.6, 21.4, 0),
    "SW2": (W / 2 + 8.0, 15.6, 0),          # RST (CHIP_PU to ground)
    # the reservoir: one 2200 uF can down each side, under the DINs' holes
    "C1": (5.5, CAN_Y, 90), "C2": (W - 5.5, CAN_Y, 90),
    # under the USB-C: the CC resistors, the VBUS diode and its resistor, the VBUS sense and pull-down
    # (2026-11-13: each CC resistor under its own pin's side (CC2 = B5 left, CC1 = A5 right: they used to cross the data
    # lines), the VBUS parts further out, D3 up by R5; the strip under U5 stays free for D+ / D- to the chip)
    "R4": (W / 2 - 3.6, 11.0, 90), "R3": (W / 2 + 3.6, 11.0, 90),
    "R12": (W / 2 - 5.4, 11.0, 90), "R13": (W / 2 - 7.2, 11.0, 90), "R23": (W / 2 + 5.4, 11.0, 90),
    "D3": (W / 2 + 7.4, 4.6, 90), "R5": (W - 17.0, 5.4, 0),
    # the buck: lower left beside C1, its 10 uF input ceramic C3 right at VIN / GND (pins 6 / 8), L1 under it
    # the start-up group (BUCK_EN: the detector, the two OR diodes, the hold-on C4 / R6) right above the buck, so BUCK_EN
    # stays a few mm long (across the board it held the router 14+ min, 2026-11-12)
    "U3": (19.4, 25.0, 90), "D4": (22.8, 21.0, 90), "D5": (19.4, 21.0, 90), "C4": (22.1, 24.6, 90), "R6": (23.8, 24.6, 90),
    "C3": (11.2 + SB, 30.68, 180), "U2": (11.2 + SB, 34.6, 0), "L1": (11.25 + SB, 39.0, 0),
    "C5": (14.35 + SB, 39.4, 90), "C6": (14.35 + SB, 35.0, 90), "R7": (13.9 + SB, 36.7, 0), "R16": (15.8 + S, 34.85, 0),
    # the chip (turned 180: LNA_IN at the bottom of its right side) and its parts round it
    "U1": (CX, CY, 180),
    "U5": (W / 2, 10.9, 0),                                           # USB ESD, right under the USB-C's data pads
    "R24": (CX - 0.3, CY - 6.2, 180), "R25": (CX - 0.3, CY - 5.2, 180),   # USB D+ / D- 22 ohm, right of pins 26 / 25 (pin 27 free)
    "C20": (CX + 1.5, CY - 4.6, 180),                                                  # VDD3P3_RTC (pin 20, top); mini 2: turned, its +3V3 pad east (the GND pad was boxed in by MIDI2_TX)
    "U4": (23.3 + S, 24.05, 180),                                    # (DNP) a 3.0 V supervisor on CHIP_PU, README "Start-up"
    "R11": (31.0 + S, 23.85, 90), "C8": (32.0 + S, 23.85, 90), "R8": (27.85 + S, 28.1, 90),
    "C7": (CX + 8.5, CY - 4.2, 0),          # (1 uF on CHIP_PU at its lane's via: 6 mm of track from pin 4, review 2026-11-13)
    "C21": (13.9 + S, 30.5, 270), "C22": (13.9 + S, 32.3, 270),                       # VDD_SPI (pin 29, left)
    "C19": (CX - 1.4, CY + 4.85, 270),      # VDD3P3_CPU (pin 46, bottom): its +3V3 pad toward the pin (review 2026-11-13)
    "C13": (CX + 2.94, CY + 4.9, 270), "C12": (CX + 3.87, CY + 4.9, 270), "C11": (CX + 5.9, CY + 4.6, 90),   # VDDA
    "C28": (CX + 5.42, CY + 2.0, 0),         # 100 nF at the RF supply's pins (2, 3), 0.6 mm out: VDD3P3's via between
    "L2": (CX + 9.1, CY - 0.2, 270), "C18": (CX + 9.8, CY + 1.5, 0),                   # VDD3P3: the 2 nH, 100 nF after it
    "C16": (29.0 + S, 30.0, 90), "C14": (30.0 + S, 32.2, 90), "C15": (31.0 + S, 32.2, 90),
    "C17": (32.4 + S, 32.4, 90),                                                         # (10 uF: the checklist)
    "Y1": (19.5 + S, CY + 7.9, 180), "R17": (CX + 2.0, CY + 4.85, 90), "C10": (16.75 + S, 37.7, 270), "C9": (20.6 + S, CY + 10.3, 0),
    # RF (README "RF"): the chip's pi (C23 shunt on the line at the pin, its ground end to a via; L3 series; C24 shunt),
    # the line down to the antenna's pi (C25 shunt, R18 the series 0 ohm: DNP, C26 shunt) at the ground edge; R19 (fitted) / J4
    # (fitted) the u.FL path, the default (the user's ruling 2026-11-11); the unfitted pads right against the line (their
    # stubs as short as the pads allow)
    "C23": (CX + 4.8, Y_LINE + 0.85, 270), "L3": (CX + 6.5, Y_L3, 0), "C24": (XA + 1.0, Y_L3 + 1.0, 0),
    "C25": (XA + 1.0, GE - 3.2, 0), "R18": (XA, GE - 1.6, 270), "C26": (XA + 1.35, GE - 0.9, 0),
    # the selector at the line's end, one node with R18 (2026-11-12 review: R19 halfway down left the line past it an open
    # stub when the u.FL is selected), J4 right beside it
    "R19": (XA + 1.3, GE - 2.1, 0), "J4": (XA + 4.4, GE - 2.6, 270),   # (right of the line, between C25 and C26: the
    # crystal's side stays clear on a short board)
    "AE1": (XA, ANT_Y, 270),
    # MIDI OUT 2 (README "MIDI OUT 2"): the source's 33 ohm and Schottky, the sink's FET, 10 ohm and gate pull-down
    "D6": (32.15 + S, 35.15, 0), "Q1": (31.4 + S, 37.95, 0), "R21": (W - 13.43, 37.9, 0), "R27": (W - 13.43, 40.5, 0), "R20": (31.2 + S, 40.6, 0),
    "R22": (31.2 + S, 42.1, 0),
    # mini 2, LINE IN (README "LINE IN"): J7 on the top edge (its bushing up) between J2 and the USB-C, tip / ring pads on its
    # right; D7 by them; the summer's inputs (R30 / C30 tip, R31 / C31 ring) to U6 (turned: IN- and IN+ on top, OUT and V+
    # below), R32 // C32 on its left from IN- to OUT, the bias (R33 / R34 / C33) on its right; the LDO (U7, C34 in, C35 out)
    # in the top strip by the USB-C (VBUS); R35 / C36 at the chip's GPIO9 (pin 14), the right side's top corner
    "J7": (16.4, 7.25, 270), "D7": (21.9, 8.6, 180),
    "R30": (19.4, 13.4, 0), "C30": (21.2, 13.4, 0), "R31": (19.4, 14.4, 0), "C31": (21.2, 14.4, 0),
    "U6": (24.6, 15.0, 90), "R32": (22.0, 15.9, 90), "C32": (21.0, 15.9, 90),
    "R33": (27.0, 14.0, 90), "R34": (28.0, 14.0, 90), "C33": (27.5, 15.9, 0), "C37": (26.3, 17.4, 0),
    "U7": (25.6, 4.5, 0), "C34": (23.2, 2.3, 90), "C35": (27.0, 6.9, 0),
    "C36": (CX + 3.41, CY - 5.5, 90), "R35": (CX + 2.0, CY - 5.85, 0),
}
WIDTH = {"VDD3P3": 0.5, "VSYS": 0.5, "+3V3": 0.4, "VBUS": 0.4, "VUSB_D": 0.4, "SW": 0.4}
ORDER = ["UDP", "UDN", "USB_DP", "USB_DN", "XTAL_P", "XTAL_N", "XTAL_X1", "VDD_SPI", "CHIP_PU", "PWR_HOLD", "VBUS_SENSE", "VBANK_ADC", "BOOT",
         "VDD3P3", "MIDI_TX", "MIDI2_TX", "SW", "+3V3", "VSYS", "VBUS", "VUSB_D", "BUCK_EN", "DET"]
# (the chip's signal pins escape first: on its right side they sit between the RF supply's pins and the rest)
RF_NETS = {"LNA_IN", "RF", "ANT_FEED", "UFL", "ANT_END"}
PRIORITY = ["UDP", "UDN", "USB_DP", "USB_DN", "XTAL_P", "XTAL_N", "XTAL_X1", "DET"]      # (routed before anything else, ground vias included)      # (drawn here, not routed)
# the chip's right-side signal pins (between the RF supply's pins and the rest): drawn fan-out, one lane each, the lowest
# pin on the outermost lane so they never cross; the router goes on from each lane's end. (net, chip pin y, lane x)
# (net, chip pin y, lane x, lane end y, a via at the end): 0.8 mm apart, each ending in a via, so the router may take
# any of them on either layer (BOOT, for one, crosses the others to reach the button on the left)
FANOUT = [("CHIP_PU", CY + 1.4, CX + 7.3, CY - 4.2, True), ("BOOT", CY + 1.0, CX + 6.5, CY - 4.2, True),
          ("VBANK_ADC", CY + 0.6, CX + 5.7, CY - 4.2, True), ("VBUS_SENSE", CY + 0.2, CX + 4.9, CY - 4.2, True),
          ("PWR_HOLD", CY - 0.6, CX + 4.1, CY - 4.2, True)]

board = pcbnew.BOARD()
nets = {}


def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        nets[name] = n
    return nets[name]


DIN_TRACK_ROOM = 0.52       # no other net's track nearer a DIN pad (the rule 0.5, +0.02 for the grid)
IRON = 0.8                  # the room round each DIN pad for a soldering iron: no top part or via nearer (README "DIN")
DIN_ROOM = []               # (x, y, radius): each DIN pad's copper plus IRON


def in_din_room(x, y):
    return any(math.hypot(x - cx, y - cy) < r + route.VIA_D / 2 for cx, cy, r in DIN_ROOM)


# the groups' places found by the placer (worktrees' placer.py, 2026-11-12: simulated annealing of the groups round the
# fixed connectors, cans, antenna and chip block, for the fewest airwire crossings, the shortest airwires, and each group
# pulled to the part it serves (the buck to C1 and the chip, the USB parts to the USB-C, the MIDI parts to their sockets);
# no group within 3 mm of the RF path), ref: (x, y, rotation); they replace PLACE (and the spread) for these parts
PLACED = {
    "C3": (15.492, 43.935, 270), "U2": (19.412, 43.935, 90), "L1": (23.812, 43.885, 90), "C5": (24.212, 40.785, 180),
    "C6": (19.812, 40.785, 180), "R7": (21.512, 41.235, 90), "U3": (12.670, 40.915, 90), "D4": (16.070, 36.915, 90),
    "D5": (12.670, 36.915, 90), "C4": (15.370, 40.515, 90), "R6": (17.070, 40.515, 90),
    "U4": (44.087, 24.652, 90),     # (mini 2.2: SW1, LED1, R9 gone, SW2 in PLACE)
    "R8": (37.922, 29.202, 0), "R10": (57.460, 27.220, 90), "R11": (38.640, 32.255, 90), "C8": (39.640, 32.255, 90),
    "R1": (63.500, 18.810, 0), "R2": (63.500, 21.810, 0), "D1": (57.600, 15.910, 90), "D2": (59.950, 15.910, 90),
    "R14": (19.560, 16.810, 180), "R26": (19.560, 14.210, 180), "R15": (18.640, 25.950, 180),
    "R16": (22.380, 26.770, 0), "Q1": (46.050, 13.540, 0), "R20": (45.850, 16.190, 0), "R22": (45.850, 17.690, 0),
    "D6": (47.720, 22.260, 270), "R21": (44.970, 31.980, 270), "R27": (42.370, 31.980, 270),
}

# the spread (72 x 65, 2026-11-12): parts placed at absolute y below the chip's top follow the chip down 7.5 mm; the
# buttons' and the start-up group's row 4 mm; the start-up group (absolute x) follows the middle 5 mm right
SPREAD_Y = {**{r: CY - 30.4 for r in ("C3", "U2", "L1", "C5", "C6", "R7", "R16", "C21", "C22", "C16", "C14", "C15", "C17", "C10",
                                "D6", "Q1", "R21", "R27", "R20", "R22", "R8")},
            **{r: (CY - 30.4) * 0.53 for r in ("U4", "R11", "C8", "U3", "D4", "D5", "C4", "R6")}}
SPREAD_X = {r: S - 8.7 for r in ("U3", "D4", "D5", "C4", "R6")}


def place():
    fps = {}
    for ref, _k, value, fp, _lcsc, pins in P.PARTS:
        x, y, rot, side = (PLACE[ref] + ("T",))[:4]
        x, y = x + SPREAD_X.get(ref, 0.0), y + SPREAD_Y.get(ref, 0.0)
        if ref in PLACED and not os.environ.get("NO_PLACED"):   # (NO_PLACED=1: the spread, the placer's start)
            x, y, rot = PLACED[ref]
            y += DY                              # (mini 2: below the LINE IN band)
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
        if not P.fitted(ref):                    # (pads only: no part, no model, out of the BOM and the CPL, no paste)
            f.SetDNP(True)
            f.SetExcludedFromBOM(True)
            f.SetExcludedFromPosFiles(True)
            for p in f.Pads():
                ls = p.GetLayerSet()
                ls.RemoveLayer(pcbnew.F_Paste)
                p.SetLayerSet(ls)
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
            windowpane(fps["U1"], p)
    return fps


EP_VIAS = [(dx, dy) for dx in (-1.3, 0.0, 1.3) for dy in (-1.3, 0.0, 1.3)]   # the exposed pad's 9 vias (Espressif: 9 or more)
# the paste (2026-11-12 review: 16 % was low): via-free strips between the via columns (x +-0.35..0.95, +-1.65..1.95),
# each cut in EP_SEGMENTS windows EP_GAP apart, 0.05 off the via rings and the pad's edge
EP_EDGE, EP_GAP, EP_SEGMENTS = 0.05, 0.25, 4


def windowpane(f, ep):
    """the exposed pad's paste as four windows between its vias (a whole-pad paste would float the QFN and fill the
    vias): the pad itself loses its paste layer; four paste-only apertures are added to the footprint"""
    ls = ep.GetLayerSet()
    ls.RemoveLayer(pcbnew.F_Paste)
    ep.SetLayerSet(ls)
    c = ep.GetPosition()
    h = pcbnew.ToMM(min(ep.GetSize().x, ep.GetSize().y)) / 2 - EP_EDGE
    keep = route.VIA_D / 2 + EP_EDGE              # (the via rings + 0.05)
    cols = sorted({dx for dx, _ in EP_VIAS})
    edges = [-h] + [e for x in cols for e in (x - keep, x + keep)] + [h]
    lanes = [(edges[i], edges[i + 1]) for i in range(0, len(edges), 2) if edges[i + 1] - edges[i] >= 0.25]
    n = EP_SEGMENTS
    seg = (2 * h - (n - 1) * EP_GAP) / n
    for x0, x1 in lanes:                          # (via-free strips between the via columns, each cut in n windows)
        for k in range(n):
            y0 = -h + k * (seg + EP_GAP)
            a = pcbnew.PAD(f)
            a.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
            a.SetShape(pcbnew.PAD_SHAPE_RECT)
            a.SetSize(pcbnew.VECTOR2I(MM(x1 - x0), MM(seg)))
            pl = pcbnew.LSET()
            pl.AddLayer(pcbnew.F_Paste)
            a.SetLayerSet(pl)
            a.SetNumber("")
            a.SetPosition(pcbnew.VECTOR2I(c.x + MM((x0 + x1) / 2), c.y + MM(y0 + seg / 2)))
            f.Add(a)


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
FIXED_VIAS = []   # (net, x, y): drawn vias of a signal net (VDD3P3's)
RF_VIAS = []      # GND vias: the fences along the line, the shunt capacitors' ground ends, the exposed pad's
COPPER = []       # (rect) unconnected copper: the antenna's end pad extension


def rf_path(fps):
    """the line from L3 to R18 (net RF), its stubs to C24 / C25 / R19, LNA_IN's stub to C23, the antenna feed"""
    del RF_SEGS[:], RF_VIAS[:], COPPER[:], FAN_VIAS[:], FIXED_VIAS[:]
    lx, ly = pad_center(pad_of(fps, "U1", "1"))
    c23 = pad_center(pad_of(fps, "C23", "1"))
    l3a, l3b = pad_center(pad_of(fps, "L3", "1")), pad_center(pad_of(fps, "L3", "2"))
    # LNA_IN runs through C23's pad (no stub on the signal side; C23's ground end gets its via: Espressif's stub) to L3
    # (0.2 as the pin, 1.4 mm; 0.2 from pin 2 and VDD3P3's via, then 0.05 lower under C28's pad: 0.2 from it too)
    RF_SEGS.append(("LNA_IN", 0, lx, ly, CX + 4.5, ly, 0.2))
    RF_SEGS.append(("LNA_IN", 0, CX + 4.5, ly, CX + 4.6, ly + 0.05, 0.2))
    RF_SEGS.append(("LNA_IN", 0, CX + 4.6, ly + 0.05, c23[0], ly + 0.05, 0.2))
    RF_SEGS.append(("LNA_IN", 0, c23[0], ly + 0.05, c23[0], c23[1], 0.3))
    RF_SEGS.append(("LNA_IN", 0, c23[0], c23[1], l3a[0], l3a[1], 0.3))
    r18 = pad_center(pad_of(fps, "R18", "1"))
    y_turn = Y_L3
    RF_SEGS.append(("RF", 0, l3b[0], y_turn, l3b[0] + RF_W / 2, y_turn, 0.5))   # (out of L3's pad, narrow)
    RF_SEGS.append(("RF", 0, l3b[0] + RF_W / 2, y_turn, XA, y_turn, RF_W))
    RF_SEGS.append(("RF", 0, XA, y_turn, XA, r18[1] - RF_W, RF_W))       # (its round end stops clear of R18's pad 2)
    RF_SEGS.append(("RF", 0, XA, r18[1] - RF_W, XA, r18[1], 0.5))          # (tapering into the 0402's pad)
    for ref in ("C24", "C25", "R19"):
        c = pad_center(pad_of(fps, ref, "1"))
        RF_SEGS.append(("RF", 0, XA, c[1], c[0], c[1], 0.5))
    r19b = pad_center(pad_of(fps, "R19", "2"))
    j4 = pad_center(pad_of(fps, "J4", "1"))
    RF_SEGS.append(("UFL", 0, r19b[0], r19b[1], j4[0], j4[1], 0.5))         # (the selector straight to J4's signal pad)
    r18b = pad_center(pad_of(fps, "R18", "2"))
    a1 = pad_center(pad_of(fps, "AE1", "1"))
    c26 = pad_center(pad_of(fps, "C26", "1"))
    RF_SEGS.append(("ANT_FEED", 0, XA, r18b[1], a1[0], a1[1], 0.5))     # (into the antenna's 1.0 mm pad)
    RF_SEGS.append(("ANT_FEED", 0, XA, c26[1], c26[0], c26[1], 0.5))
    a2 = pad_center(pad_of(fps, "AE1", "2"))     # (the end pad's extension: its own net, nothing else on it)
    RF_SEGS.append(("ANT_END", 0, a2[0], a2[1], a2[0], a2[1] + END_PAD[1] - END_PAD[0] / 2, END_PAD[0]))
    # the fences: GND vias both sides of the line, their rings 0.35 mm from its edge (0.15 inside the pour)
    off = RF_W / 2 + RF_GAP + route.VIA_D / 2 + 0.15
    rf_pads = [p for ref in ("C24", "C25", "C26", "R18", "R19", "J4") for p in fps[ref].Pads()]

    def clear_of_pads(x, y):                     # (a fence via 0.2 mm or more from the RF parts' pads)
        c = pcbnew.SHAPE_CIRCLE(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)), MM(route.VIA_D / 2))
        return not any(p.GetEffectiveShape(pcbnew.F_Cu).Collide(c, MM(0.2)) for p in rf_pads)
    for x in (XA - off, XA + off):
        y = y_turn + 1.2
        while y < GE - 0.6:
            if clear_of_pads(x, y):
                RF_VIAS.append((x, y))
            y += 0.9
    x = l3b[0] + 1.0
    while x < XA - 0.4:
        RF_VIAS.append((x, y_turn - off))
        x += 0.9
    # the chip's right-side fan-out (FANOUT)
    for n, y, x, top, via in ([] if os.environ.get("NO_LANES") else FANOUT):   # (NO_LANES=1: the outside router fans out)
        RF_SEGS.append((n, 0, CX + 3.41, y, x, y, 0.2))
        RF_SEGS.append((n, 0, x, y, x, top, 0.2))
        if via:
            FAN_VIAS.append((n, x, top))
    # USB (2026-11-12 review: the router looped D+ 21 mm, 4 vias): the USB-C's D+ / D- pads alternate (B6 D+, A7 D-,
    # A6 D+, B7 D-); D- joins A7 / B7 by a bridge 0.2 under A6 and goes on to U5's pin 4; D+ from B6 through a via to U5's
    # pin 6; A6 reaches D+ through a via above its pad (under the connector's body) and a hop on the bottom to that via
    a6, a7 = pad_center(pad_of(fps, "J1", "A6")), pad_center(pad_of(fps, "J1", "A7"))
    b6, b7 = pad_center(pad_of(fps, "J1", "B6")), pad_center(pad_of(fps, "J1", "B7"))
    u4, u6 = pad_center(pad_of(fps, "U5", "4")), pad_center(pad_of(fps, "U5", "6"))
    yb = a6[1] + 0.85                            # (the pads 1.1 long: their ends + 0.2, the track's half)
    RF_SEGS.append(("UDN", 0, a7[0], a7[1], a7[0], yb, 0.2))
    RF_SEGS.append(("UDN", 0, b7[0], b7[1], b7[0], yb, 0.2))
    RF_SEGS.append(("UDN", 0, a7[0], yb, b7[0], yb, 0.2))
    RF_SEGS.append(("UDN", 0, b7[0], yb, u4[0], u4[1], 0.2))   # (from B7's end of the bridge: clear of U5's VBUS pin)
    v2 = (b6[0] - 0.95, b6[1] + 1.05)              # (clear of SBU A8 left of B6, of both VBUS pads and of U5's pin 5)
    RF_SEGS.append(("UDP", 0, b6[0], b6[1], b6[0], yb, 0.2))
    RF_SEGS.append(("UDP", 0, b6[0], yb, v2[0], v2[1], 0.2))
    RF_SEGS.append(("UDP", 0, v2[0], v2[1], u6[0], u6[1], 0.2))
    v1 = (a6[0], a6[1] - 1.75)
    RF_SEGS.append(("UDP", 0, a6[0], a6[1], v1[0], v1[1], 0.2))
    RF_SEGS.append(("UDP", 1, v1[0], v1[1], v2[0], v2[1], 0.2))
    FIXED_VIAS.append(("UDP", v1[0], v1[1]))
    # C28's ground pad: boxed in by the lanes, VDD3P3 and LNA_IN; its own via right of it (0.36 from the pad, 0.3 from
    # CHIP_PU's lane, 0.53 from L3)
    c28g = pad_center(pad_of(fps, "C28", "2"))
    RF_SEGS.append(("GND", 0, c28g[0], c28g[1], CX + 6.75, CY + 1.95, 0.25))
    FIXED_VIAS.append(("GND", CX + 6.75, CY + 1.95))
    # the crystal's load cap C9: its ground pad gets its own via 0.7 mm right of it (the routes boxed it in)
    c9g = pad_center(pad_of(fps, "C9", "2"))
    RF_SEGS.append(("GND", 0, c9g[0], c9g[1], c9g[0] + 0.7, c9g[1], 0.25))
    FIXED_VIAS.append(("GND", c9g[0] + 0.7, c9g[1]))
    # VDD3P3 on from its via at the pins: under the fan-out lanes on the bottom (y CY+1.3, clear of C28's ground via) to a
    # via beside C18, then C18 and L2's pad on top (review 2026-11-13: the router took 22.8 mm for this 6 mm hop)
    l2b, c18 = pad_center(pad_of(fps, "L2", "2")), pad_center(pad_of(fps, "C18", "1"))
    vdv = (CX + 8.4, CY + 1.5)
    for p0, p1 in (((CX + 4.2, CY + 2.0), (CX + 4.9, CY + 1.3)), ((CX + 4.9, CY + 1.3), (CX + 8.4, CY + 1.3)),
                   ((CX + 8.4, CY + 1.3), vdv)):
        RF_SEGS.append(("VDD3P3", 1, p0[0], p0[1], p1[0], p1[1], 0.25))
    FIXED_VIAS.append(("VDD3P3", vdv[0], vdv[1]))
    RF_SEGS.append(("VDD3P3", 0, vdv[0], vdv[1], c18[0], c18[1], 0.25))
    RF_SEGS.append(("VDD3P3", 0, c18[0], c18[1], l2b[0], l2b[1], 0.25))
    # C12 / C13's ground pads: their own via under them (the C12 -> C11 link below cuts their pour off otherwise)
    c12g, c13g = pad_center(pad_of(fps, "C12", "2")), pad_center(pad_of(fps, "C13", "2"))
    gv = (CX + 3.4, CY + 6.2)
    RF_SEGS.append(("GND", 0, c12g[0], c12g[1], gv[0], gv[1], 0.25))
    RF_SEGS.append(("GND", 0, c13g[0], c13g[1], gv[0], gv[1], 0.25))
    FIXED_VIAS.append(("GND", gv[0], gv[1]))
    # C7 (CHIP_PU's 1 uF) to the CHIP_PU lane's via
    c7p = pad_center(pad_of(fps, "C7", "1"))
    RF_SEGS.append(("CHIP_PU", 0, CX + 7.3, CY - 4.2, c7p[0], c7p[1], 0.2))
    # the analog supply's caps: C12's +3V3 pad straight to C11's (the router left C11..C13 cut off from pins 55 / 56)
    c12p, c11p = pad_center(pad_of(fps, "C12", "1")), pad_center(pad_of(fps, "C11", "1"))
    RF_SEGS.append(("+3V3", 0, c12p[0], c12p[1], c11p[0], c11p[1], 0.25))
    u5v = pad_center(pad_of(fps, "U5", "5"))      # (U5's VBUS pin sits between D+ and D-: fed through a via above it,
    vb = (u5v[0], u5v[1] - 1.0)                     # from the bottom, so VBUS never crosses the data lines on top)
    RF_SEGS.append(("VBUS", 0, vb[0], vb[1], u5v[0], u5v[1], 0.25))
    FIXED_VIAS.append(("VBUS", vb[0], vb[1]))
    u1p, u3p = pad_center(pad_of(fps, "U5", "1")), pad_center(pad_of(fps, "U5", "3"))
    RF_SEGS.append(("UDP", 0, u6[0], u6[1], u1p[0], u1p[1], 0.2))     # (U5's flow-through: pin 6 -> 1, 4 -> 3, under it)
    RF_SEGS.append(("UDN", 0, u4[0], u4[1], u3p[0], u3p[1], 0.2))
    FIXED_VIAS.append(("UDP", v2[0], v2[1]))
    # VDD3P3's two pins (2 and 3) joined on top, at their pads' centre line
    RF_SEGS.append(("VDD3P3", 0, CX + 3.41, CY + 1.8, CX + 3.41, CY + 2.2, 0.2))
    # ... on to C28 through a via between the pins and C28: the only way out of the corner (the fan-out lanes above, LNA_IN
    # below, C28's ground pad right); the router takes VDD3P3 on from the via on the bottom to L2 / C18
    c28 = pad_center(pad_of(fps, "C28", "1"))
    RF_SEGS.append(("VDD3P3", 0, CX + 3.41, CY + 2.0, c28[0], c28[1], 0.2))
    FIXED_VIAS.append(("VDD3P3", CX + 4.2, CY + 2.0))
    # mini 2: LINE_IN (GPIO9, pin 14: the right side's top pin) straight up out of the corner, past pin 15, to C36 (100 nF
    # at the pin) and R35: the fan-out lanes (PWR_HOLD's at CX + 4.1) close every other ADC1 pin's way out
    c36 = pad_center(pad_of(fps, "C36", "1"))
    RF_SEGS.append(("LINE_IN", 0, CX + 3.41, CY - 2.6, CX + 3.41, c36[1], 0.2))
    # the exposed pad's vias (EP_VIAS: a cross, clear of the paste windows)
    for dx, dy in EP_VIAS:
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


FAN_KEEP = set()                                 # (grid cells near the fan-out stubs: no ground via there)


def router(fps):
    """a fresh grid with every pad, the RF copper, the keep-outs; returns (router, {net: [(rect, layers, pad)]})"""
    R = route.Router(W, H)
    FAN_KEEP.clear()
    R.block((0, 1), (0.0, GE - 0.45, W, H))     # (a via's ring and the clearance stay out of the antenna strip)
    # the bottom layer under the RF path and the chip stays ground (no tracks there)
    R.block((1,), (CX + 3.0, Y_LINE + 0.3, XA + 1.8, Y_L3 + 1.0), ("sig", "mid", "pwr"))       # (LNA_IN, L3, the line's top)
    R.block((1,), (XA - 1.8, Y_L3 - 1.0, XA + 1.8, GE), ("sig", "mid", "pwr"))                 # (the line down to the antenna)
    R.block((1,), (CX - 3.8, CY - 3.8, CX + 3.8, CY + 3.8), ("sig", "mid", "pwr"))             # (the chip)
    y1 = PLACE["Y1"]
    R.block((1,), (y1[0] - 2.0, y1[1] - 1.7, y1[0] + 2.0, y1[1] + 1.7), ("sig", "mid", "pwr"))  # (the crystal)
    by_net = {}
    for f in fps.values():
        for p in f.Pads():
            if not (p.IsOnLayer(pcbnew.F_Cu) or p.IsOnLayer(pcbnew.B_Cu)):
                continue                           # (the exposed pad's paste-only windows: no copper)
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
    # the DIN sockets' hand-soldered pads: no other net's track within DIN_TRACK_ROOM of their copper (both layers)
    for ref in ("J5", "J6"):
        for p in fps[ref].Pads():
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                cx, cy = pad_center(p)
                R.circle((0, 1), p.GetNetname() or None, cx, cy, pcbnew.ToMM(p.GetSize().x) / 2 + DIN_TRACK_ROOM - route.CLEAR)
    # the drawn fan-out stubs are their net's copper: their centre lines stay passable for it even where a neighbour's
    # clearance margin (the router's 0.17 against the rules' 0.15) reaches over them
    for n, y, x, top, via in FANOUT:
        k = R.nid(n)
        pts = [(CX + 3.41 + i * route.GRID, y) for i in range(int((x - CX - 3.41) / route.GRID) + 1)]
        pts += [(x, y - i * route.GRID) for i in range(int((y - top) / route.GRID) + 1)]
        for px, py in pts:
            ix, iy = int(round(px / route.GRID)), int(round(py / route.GRID))
            R.mask["sig"][0][iy * R.nx + ix] = k
            # (and no ground via within 0.75 of it (FAN_KEEP, for ground_vias): a via's mark on the grid, its ring +
            # 0.12 drill room + the clearance, would otherwise close the stub although the copper keeps the rules)
            rr = int(0.75 / route.GRID) + 1
            for jy in range(iy - rr, iy + rr + 1):
                for jx in range(ix - rr, ix + rr + 1):
                    if math.hypot(jx - ix, jy - iy) * route.GRID < 0.75:
                        FAN_KEEP.add(jy * R.nx + jx)
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
            res = route.via_near(R, "GND", r, layers[0], avoid=FAN_KEEP)
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
            wn = 0.3 if min(q[0][2] - q[0][0], q[0][3] - q[0][1]) >= 0.3 else 0.2   # (the buck's 0.36 mm pins: 0.3)
            segs, vias, bad = route.route_net(R, n, "mid" if wn > 0.25 else "sig", wn, [mate, q])
            put(n, segs, vias, wn)
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
    for n, x, y in FIXED_VIAS:
        add_via(n, x, y)
    for n, x, y in FAN_VIAS:                      # (a fan-out via only where the route went on on the bottom layer)
        if os.environ.get("LANE_VIAS") or any(t.GetNetname() == n and t.GetLayer() == pcbnew.B_Cu and t.GetClass() == "PCB_TRACK"
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


def prune_dangling(longest=0.3):
    """remove the router's leftovers: a track shorter than `longest` with an end that touches no other track, via or pad"""
    tracks = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK"]
    vias = [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    pads = [p for f in board.GetFootprints() for p in f.Pads()]
    gone = 0
    for t in tracks:
        if pcbnew.ToMM(t.GetLength()) >= longest:
            continue
        hw = t.GetWidth() // 2                  # (an end touches what its round cap reaches: a pad's rounded corner too)
        for e in (t.GetStart(), t.GetEnd()):
            hit = any(o is not t and o.GetLayer() == t.GetLayer() and o.GetNetCode() == t.GetNetCode()
                      and (o.HitTest(e, hw)) for o in tracks)
            hit = hit or any(v.GetNetCode() == t.GetNetCode() and v.HitTest(e, hw) for v in vias)
            hit = hit or any(p.GetNetCode() == t.GetNetCode() and p.IsOnLayer(t.GetLayer()) and p.HitTest(e, hw) for p in pads)
            if not hit:
                board.Remove(t)
                gone += 1
                break
    print(f"{gone} dangling stubs removed")


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
    label("MIDI OUT", 6.0, 12.3, 0.7)            # (J2 and J5)
    label("PWR IN / OUT 2", W - 6.1, 12.3, 0.6)  # (J3 and J6: the power input, or MIDI OUT 2 on USB power)
    for ref, text, dy in (("SW2", "RST", 3.6), ("LED1", "BEAT", 2.4), ("LED2", "STATUS", 2.4)):   # (mini 2.2)
        p = board.FindFootprintByReference(ref).GetPosition()
        label(text, pcbnew.ToMM(p.x) - OX, pcbnew.ToMM(p.y) - OY + dy, 0.6)
    label("PUSH = BOOT", W / 2, 18.6, 0.6)       # (mini 2.2: above the encoder's push pins)
    label("LINE IN", 14.6, 13.6, 0.6)            # (mini 2: J7)
    label("jank mini 2.2", 12.0, GE + 3.0, 1.0)
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
    prune_dangling()
    outline()
    zones()
    silk()
    pts = [(OX + x, OY + y) for x, y in ((0, 0), (W, 0), (W, GE - 0.6), (0, GE - 0.6))]
    st = []                                       # (NO_STITCH=1: a placement for an outside router, freeroute.py)
    if not os.environ.get("NO_STITCH"):
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
