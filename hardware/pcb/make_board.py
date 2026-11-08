# SPDX-License-Identifier: GPL-3.0-only
"""The jank board (README.md): generates jank-pcb.kicad_pcb with KiCad's own Python (pcbnew), from the schematic's
intended nets (fab/intended-nets.json, written by make_schematic.py) and the footprints of make_footprints.py.

  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 make_board.py [long|square]

long (the default): jank-pcb.kicad_pcb, 84 x 52 mm, as below. square: jank-pcb-square.kicad_pcb, 67 x 58 mm, OUT1 and
OUT2 on the front edge, OUT3 and OUT4 on the side edges, no A/B switches, no MIDI IN, no encoder (layout_square(); README
"The square variant"; it is built from the frozen intent fab/square/intended-nets.json).

Then make_fab.sh checks it (DRC, the schematic's nets) and writes the JLCPCB files.

The long board (the user's rulings, 2026-11-08): 2 layers, 84 x 52 mm. Each output is one footprint holding a DIN-5
(HOOYA DIN-504B-M10) and a 3.5 mm jack (XKB PJ-320A, right angle) at the same place, both fronts flush with the edge, one of the
two fitted, with a TRS A/B slide switch (MSS22D18) on the face side. The FRONT edge carries OUT4, OUT3, OUT2 (seen from
the top) at a 21.1 mm pitch (the DIN plate 20.9 wide), slid to the left edge. The BACK edge carries, left to right: OUT1 (the same
footprint turned 180 degrees; OUT1 doubles as a MIDI IN in software: an H11L1 behind an N-FET gate, U4 / Q3 in the front
right corner; its source from 3V3 through Q4, off in IN mode), the XIAO's own USB-C (the data port; the module on 2.54 mm
headers, its socket 1.5 mm over the edge) and J1 (power only). The bottom is the device's FACE (the box's lid): the four
A/B switches and a row of controls (centres at y 13.8): MODE (SW1) under the XIAO with the two SK6812MINI-E behind it
(reverse mount, soldered on top in their cut-outs), PLAY/STOP (SW6) and the EC11 encoder (SW7) right of the XIAO, under
the power path (whose parts sit on top round their legs). The MIDI resistors and R8 are swappable groups: an axial part
standing, a bypass solder jumper, 1206 pads in parallel (not fitted). Page coordinates: board (x, y) from the back-left
corner, y towards the front; KiCad's page = (100 + x, 100 + y).

Routing, in short: TX1..TX3 run on the bottom down the strip left of the XIAO (TX1 to OUT1's 10 ohm group, TX2 / TX3
along the band in front of the XIAO to a via on their groups), TX4 under the module; ENC_A runs under the module's
front end and ENC_B from the right row to the encoder; IN_EN runs back along the band to Q4 (between OUT1's groups) and
on the bottom to the MIDI IN corner with MIDI_RX; the ladder runs from D2 to MODE, under the module to PLAY/STOP and on
the bottom to the encoder's push; RING1 / TIP1 run along the band to the corner; the 3V3 bus feeds the front 33 ohm
groups from a line down inside the XIAO's right row; +5V comes from U2 (over the encoder) on the bottom along the back
edge to the XIAO's 5V pad and the LEDs; ground is both pours.
"""
import json
import math
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True                  # (no lib/__pycache__ in the repository)
sys.path.insert(0, os.path.join(HERE, "lib"))
sys.path.insert(0, HERE)
from stitch import stitch          # (lib/stitch.py: from optimist's SD board, after m-card's stitch(), CC0-1.0)
import make_footprints

MM = pcbnew.FromMM
OX, OY = 100.0, 100.0
PITCH = 20.5                                     # (the outputs' pitch: the DIN's 20.0 face + 0.5)
KICAD_FP = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints"
JANK = os.path.join(HERE, "lib", "jank.pretty")
SIG, PWR, BUS = 0.25, 0.5, 0.4
TOP, BOT = pcbnew.F_Cu, pcbnew.B_Cu
NAME = "jank-pcb"

make_footprints.write()
LAYOUT = sys.argv[1] if len(sys.argv) > 1 else "long"
INTENT = json.load(open(os.path.join(HERE, "fab", *(["square"] if LAYOUT == "square" else []), "intended-nets.json")))
PIN_NET = {}
for netname, pins in INTENT["nets"].items():
    for p in pins:
        ref, num = p.split(".")
        PIN_NET[(ref, num)] = netname

board = pcbnew.BOARD()
nets = {}


def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        nets[name] = n
    return nets[name]


def place(lib, name, ref, value, x, y, rot=0.0, bottom=False):
    fp = pcbnew.FootprintLoad(lib, name)
    fp.SetFPID(pcbnew.LIB_ID("jank" if lib == JANK else os.path.basename(lib)[:-len(".pretty")], name))
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
    fp.SetOrientationDegrees(rot)
    fp.Reference().SetVisible(False)            # (the silk labels below say what matters; refs crowd the silk)
    fp.Value().SetVisible(False)
    board.Add(fp)
    if bottom:                                  # (flipped once on the board: a loose footprint has no layers to flip)
        fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
    for p in fp.Pads():                         # (the nets from the schematic's intent, by pin number)
        n = PIN_NET.get((ref, p.GetNumber()))
        if n is None and ref.startswith("J") and p.GetNumber() in ("T", "R", "S"):   # (the jack inside J6..J9: J2..J5's pins)
            n = PIN_NET.get((f"J{int(ref[1:]) - 4}", p.GetNumber()))
        if n is None and ref.startswith("J") and p.GetNumber() == "E":
            n = "GND"                           # (the DIN's shield pins: GND; the DIN-5 symbol has no shield pin)
        if n:
            p.SetNet(net(n))
    return fp


def K(lib):
    return os.path.join(KICAD_FP, lib + ".pretty")


def at(fp, num, near=None):
    """a pad's place (board mm); near: of several pads with that number, the one nearest this point"""
    pads = [p for p in fp.Pads() if p.GetNumber() == str(num)]
    if not pads:
        raise KeyError(f"{fp.GetReference()} has no pad {num}")
    pts = [(round(pcbnew.ToMM(p.GetPosition().x) - OX, 4), round(pcbnew.ToMM(p.GetPosition().y) - OY, 4)) for p in pads]
    if near is None:
        thru = [pt for p, pt in zip(pads, pts) if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
        return (thru or pts)[0]
    return min(pts, key=lambda q: (q[0] - near[0]) ** 2 + (q[1] - near[1]) ** 2)


def route(name, pts, width=SIG, layer=TOP):
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pcbnew.VECTOR2I(MM(OX + ax), MM(OY + ay)))
        t.SetEnd(pcbnew.VECTOR2I(MM(OX + bx), MM(OY + by)))
        t.SetWidth(MM(width))
        t.SetLayer(layer)
        t.SetNet(net(name))
        board.Add(t)


def via(name, x, y):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
    v.SetDrill(MM(0.3))
    v.SetWidth(MM(0.6))
    v.SetNet(net(name))
    board.Add(v)


def solid(fp, num):
    """a pad joined to the pour solid (no thermal spokes): where its neighbours leave no room for the spokes"""
    for p_ in fp.Pads():
        if p_.GetNumber() == num:
            p_.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)


def label(text, x, y, size=0.8, layer=pcbnew.F_SilkS, rot=0.0):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
    t.SetTextAngleDegrees(rot)
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(MM(size), MM(size)))
    t.SetTextThickness(MM(0.12))
    if layer == pcbnew.B_SilkS:
        t.SetMirrored(True)
    board.Add(t)


R0805, R1206, R2010 = ("R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", "R_1206_3216Metric_Pad1.30x1.75mm_HandSolder",
                       "R_2010_5025Metric_Pad1.40x2.65mm_HandSolder")
C0805 = "C_0805_2012Metric_Pad1.18x1.45mm_HandSolder"
# (rot 270: an 0805/1206 standing, pad 1 at the top (towards the back); rot 90: pad 1 at the bottom)

# ---- the swappable resistors (the user's ruling, as on the SD board): each an axial part standing (hand-swappable), a
# solder jumper across it (bypass, open by default) and 1206 pads in parallel (not fitted: to lower the value).
# "v": its A net at the top (towards the back), B below; "h": A right, B left. A frame (E, th) places it in an output's
# own coordinates (that output's edge point E, turned by th degrees, KiCad's sense): see T().
AX0207, AX0309 = "R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical", "R_Axial_DIN0309_L9.0mm_D3.2mm_P2.54mm_Vertical"
JPFP = "SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm"


def T(frame, lx, ly):
    """a point in a frame's own coordinates -> board coordinates"""
    if frame is None:
        return (lx, ly)
    (ex, ey), th = frame
    c, s_ = round(math.cos(math.radians(th)), 9), round(math.sin(math.radians(th)), 9)
    return (round(ex + lx * c + ly * s_, 4), round(ey - lx * s_ + ly * c, 4))


def cluster(refs, nets, cx, cy, orient, axial, value, dj=3.0, ds=3.2, frame=None):
    rmain, jp, rpar = refs
    a, b = nets
    th = frame[1] if frame else 0.0
    if orient == "v":
        r = place(K("Resistor_THT"), axial, rmain, value, *T(frame, cx, cy - 1.27), (270 + th) % 360)
        j = place(K("Jumper"), JPFP, jp, "bypass", *T(frame, cx - dj, cy), (270 + th) % 360)
        s = place(K("Resistor_SMD"), R1206, rpar, "DNP", *T(frame, cx + ds, cy), (270 + th) % 360)
    else:
        r = place(K("Resistor_THT"), axial, rmain, value, *T(frame, cx + 1.27, cy), 180 + th)
        j = place(K("Jumper"), JPFP, jp, "bypass", *T(frame, cx, cy - dj), 180 + th)
        s = place(K("Resistor_SMD"), R1206, rpar, "DNP", *T(frame, cx, cy + ds), 180 + th)
    for node, n in ((a, "1"), (b, "2")):
        j1, s1 = at(j, n), at(s, n)
        if orient == "v":
            line = cy - 1.27 if n == "1" else cy + 1.27
            route(node, [j1, T(frame, cx - dj, line), T(frame, cx + ds, line), s1])
        else:
            line = cx + 1.27 if n == "1" else cx - 1.27
            route(node, [j1, T(frame, line, cy - dj), T(frame, line, cy + ds), s1])
    return {"A": at(r, 1), "B": at(r, 2), "jp": j, "par": s}



def front_unit(k, c, H):
    """one output on the front edge, as on the round-5 board: the combined footprint on the edge, its two resistor groups
    behind it, the A/B switch between them (face side); returns its TX via"""
    D = H - 51.0
    j = place(JANK, "MIDI_OUT_DIN-504B-M10_or_PJ-320A_AB", f"J{5 + k}", "DIN or TRS", c, H, 0)
    ta = cluster((f"R1{k}", f"JP1{k}", f"R3{k}"), (f"MIDI_TX{k}", f"TIP{k}"), c - 6.2, 25.07 + D, "v", AX0207, "10", 2.8, 2.9)
    tb = cluster((f"R2{k}", f"JP2{k}", f"R4{k}"), ("+3V3", f"RING{k}"), c + 4.3, 25.07 + D, "v", AX0309, "33", 3.1, 3.3)
    sw = place(JANK, "SW_Slide_DPDT_MSS22D18", f"SW{1 + k}", "TRS A/B", c - 1.7, 31.0 + D, 270, True)   # (face side)
    din5, din4 = at(j, "5", (c - 5, H - 15)), at(j, "4", (c + 5, H - 15))
    jt, jr = at(j, "T", (c - 2.3, H - 10.2)), at(j, "R", (c - 2.3, H - 3.2))   # (the PJ-320A: its pad column left)
    solid(j, "S")                                  # (the jack's sleeve slot: no room for spokes)
    s_ = {n: at(sw, n) for n in range(1, 7)}
    # TIP = 10 ohm group -> DIN 5, RING = 33 ohm group -> DIN 4 (top); the switch: 1 RING, 2 JR, 3 TIP | 4 TIP, 5 JT, 6 RING
    route(f"TIP{k}", [ta["B"], (ta["B"][0], 27.6 + D), (din5[0], 28.6 + D), din5])
    route(f"TIP{k}", [(din5[0], s_[3][1]), s_[3]])
    route(f"TIP{k}", [s_[3], (c, s_[3][1] - 1.5), (c, s_[4][1] + 1.5), s_[4]], SIG, BOT)
    route(f"RING{k}", [tb["B"], (tb["B"][0], 27.6 + D), (din4[0], 28.6 + D), din4])
    route(f"RING{k}", [(din4[0], s_[6][1]), s_[6]])
    route(f"RING{k}", [(c + 1.2, tb["B"][1]), s_[1]])
    # JT round the jack's sleeve pad (between it and DIN pin 4), JR down the left of the jack's pad column (bottom)
    route(f"JT{k}", [s_[5], (c + 3.0, s_[5][1] + 0.8), (c + 3.4, s_[5][1] + 2.0), (c + 3.4, H - 9.8), (jt[0] + 0.6, H - 9.8),
                     jt], SIG, BOT)
    route(f"JR{k}", [s_[2], (c - 3.4, s_[2][1] + 1.7), (c - 3.4, H - 4.6), jr], SIG, BOT)
    route("+3V3", [(tb["A"][0], 22.3 + D), tb["A"]], BUS)
    v = (ta["A"][0] - 1.4, ta["A"][1])               # (the TX via on the 10 ohm group's A line, left of the axial part)
    via(f"MIDI_TX{k}", *v)
    route(f"MIDI_TX{k}", [v, at(ta["jp"], 1, v)])
    label(f"OUT{k} A|B", c - 4.3, 31.0 + D, 0.7, pcbnew.B_SilkS, rot=90)   # (the face side, by the A/B switch)
    label("JP=3V3!", c - 1.0, 25.1 + D, 0.6, rot=90)    # (the ring's bypass: 3V3 straight on the ring)
    return v


def layout_long():
    """84 x 52 mm. FRONT edge: OUT2, OUT3, OUT4. BACK edge, left to right: OUT1 (the MIDI IN one, the same footprint turned
    to face the back), a strip (OUT1's A/B switch, R8, the LED driver), the XIAO (its USB-C over the edge), J1 and the
    power path. The face side's row of controls (centres at y 13.8): MODE under the XIAO (the two LEDs behind it),
    PLAY/STOP and the encoder under the power path. The front right corner: the MIDI IN (U4 and its parts)."""
    W, H = 84.0, 52.0
    D = H - 51.0
    BUS_Y = 22.3 + D                                 # (the 3V3 bus to the front 33 ohm groups, top)
    CH = [10.45 + 21.1 * i for i in range(3)]       # (the front DINs' axes, left to right seen from the top: OUT4, OUT3,
                                                     #  OUT2, so that from the face they read 2, 3, 4; pitch 21.1: the HOOYA DIN plate is
                                                     #  20.9 +/-0.1 wide; OUT4's plate flush with the left edge)
    C1X = 10.75                                      # (OUT1 on the back edge, behind OUT4)
    XC = 45.2                                        # (the XIAO's axis)
    JX = 64.5                                        # (J1's axis)
    ROW = 13.8                                       # (the controls' row: their centres' y)

    # ======== the back edge ========
    U1 = place(JANK, "XIAO-ESP32-S3-DIP_Headers", "U1", "XIAO ESP32S3", XC, 10.55, 90)   # (on 2.54 mm headers, USB-C over the edge)
    p = {n: at(U1, n) for n in range(1, 15)}
    J1 = place(K("Connector_USB"), "USB_C_Receptacle_GCT_USB4125-xx-x-0190_6P_TopMnt_Horizontal", "J1", "USB-C power",
               JX, 3.4, 180)                         # (its "PCB Edge" line, 3.4 mm in front of its origin, on the edge)
    H1 = place(K("MountingHole"), "MountingHole_2.2mm_M2", "H1", "M2", W - 3.2, 25.0)
    H2 = place(K("MountingHole"), "MountingHole_2.2mm_M2", "H2", "M2", W - 3.2, H - 3.2)

    # ---- OUT1: the combined footprint turned 180 (its faces on the back edge), its groups in front of it
    F1 = ((C1X, 0.0), 180)
    J6 = place(JANK, "MIDI_OUT_DIN-504B-M10_or_PJ-320A_AB", "J6", "DIN or TRS", C1X, 0.0, 180)
    ta1 = cluster(("R11", "JP11", "R31"), ("MIDI_TX1", "TIP1"), -6.2, -19.0, "v", AX0207, "10", 2.8, 2.9, F1)
    tb1 = cluster(("R21", "JP21", "R41"), ("SRC1", "RING1"), 4.3, -19.0, "v", AX0309, "33", 3.1, 3.3, F1)
    SW2 = place(JANK, "SW_Slide_DPDT_MSS22D18", "SW2", "TRS A/B", 23.5, 10.5, 270, True)   # (face side, in the strip)
    d5, d4 = at(J6, "5", (C1X + 5, 15.0)), at(J6, "4", (C1X - 5, 15.0))
    jt1, jr1 = at(J6, "T", (C1X + 2.3, 10.2)), at(J6, "R", (C1X + 2.3, 3.2))
    solid(J6, "S")
    s = {n: at(SW2, n) for n in range(1, 7)}
    X, Y = s[2]
    route("TIP1", [ta1["B"], d5])
    route("TIP1", [d5, (20.95, d5[1]), s[3]])                                   # (top: to the switch)
    route("TIP1", [s[3], (X + 1.7, s[3][1] - 1.25), (X + 1.7, s[4][1] + 1.25), s[4]], SIG, BOT)
    route("RING1", [tb1["B"], d4])
    route("RING1", [d4, (d4[0] + 1.6, 16.6), (s[6][0], 16.6), s[6]], SIG, BOT)   # (bottom: under the groups' B side)
    route("RING1", [s[1], (X, 6.4), (X + 5.0, 6.4), (X + 5.0, s[6][1]), s[6]])   # (top: round the switch's right row)
    route("JR1", [s[2], (16.5, s[2][1]), (14.4, 8.4), (14.4, 4.4), jr1], SIG, BOT)
    route("JT1", [s[5], (X + 1.7, 9.25), (X - 1.5, 9.25), (X - 2.5, 10.5), (jt1[0] + 0.6, 10.5), jt1])
    # RING1 and TIP1 on to the MIDI IN (the front right corner): down the strip's left side, then along y 22.15 / 22.65
    via("RING1", 21.6, 16.6)
    route("RING1", [(21.6, 16.6), (21.6, 22.15), (62.9, 22.15)])
    route("TIP1", [(20.95, d5[1]), (20.95, 22.65), (61.6, 22.65)])
    # TX1: the XIAO's D0 (pad 1) on the bottom down the strip's right side to the 10 ohm group's A pad
    route("MIDI_TX1", [p[1], (35.4, p[1][1]), (35.4, ta1["A"][1]), ta1["A"]], SIG, BOT)
    # OUT1's source: Q4 (face side, between OUT1's groups), 3V3 to SRC1, its gate on IN_EN: IN_EN high = source off
    Q4 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q4", "AO3401A", 11.9, 19.3, 90, True)
    q4 = {n: at(Q4, n) for n in (1, 2, 3)}
    route("SRC1", [q4[3], (9.6, q4[3][1]), (8.0, 19.6), tb1["A"]], SIG, BOT)
    route("IN_EN", [(54.0, 21.3), (53.65, 21.65), (23.0, 21.65), (22.5, 21.2)])       # (top, from the XIAO's D8 line)
    via("IN_EN", 22.5, 21.2)
    route("IN_EN", [(22.5, 21.2), (22.05, 21.65), (q4[1][0] + 1.2, 21.65), q4[1]], SIG, BOT)
    route("+3V3", [(CH[0] + 4.3, BUS_Y), (2.0, BUS_Y)], BUS)                         # (the bus on to the left edge)
    via("+3V3", 2.0, BUS_Y)
    route("+3V3", [(2.0, BUS_Y), (2.0, 22.1), (q4[2][0], 22.1), q4[2]], SIG, BOT)

    # ---- the strip's right side (top): R8's group at the back (its B side towards the LEDs), the LED driver U3 in front
    U3 = place(K("Package_TO_SOT_SMD"), "SOT-23-5_HandSoldering", "U3", "74AHCT1G125", 31.9, 13.0, 90)
    R7 = place(K("Resistor_SMD"), R0805, "R7", "100k", 34.3, 17.4, 270)
    C5 = place(K("Capacitor_SMD"), C0805, "C5", "100nF", 29.4, 17.4, 270)
    R8C = cluster(("R8", "JP8", "R9"), ("LED_BUF", "LED_DIN1"), 31.6, 16.4, "h", AX0207, "330", 3.0, 3.2,
                  ((63.2, 21.9), 180))
    TP = {n: place(K("TestPoint"), "TestPoint_Pad_D1.5mm", n, v, x, y)
          for n, v, x, y in (("TP1", "5V", 57.0, 1.6), ("TP2", "3V3", 78.4, 21.4), ("TP3", "GND", 23.6, 3.6),
                             ("TP4", "TX", 32.0, 19.7))}
    via("MIDI_TX1", 32.0, ta1["A"][1])
    route("MIDI_TX1", [(32.0, ta1["A"][1]), at(TP["TP4"], 1)])
    u3 = {n: at(U3, n) for n in range(1, 6)}
    route("LED_DATA", [p[6], (u3[2][0], p[6][1]), u3[2]])
    route("LED_DATA", [(at(R7, 1)[0], p[6][1]), at(R7, 1)])
    route("GND", [at(R7, 2), (34.3, 19.5)])
    via("GND", 34.3, 19.5)
    jp8a = at(R8C["jp"], 1)
    route("LED_BUF", [u3[4], (u3[4][0], 10.2), (jp8a[0], 10.2), jp8a])
    route("LED_DIN1", [R8C["B"], (34.4, 6.74), (43.85, 6.74)])
    via("+5V", 29.5, 12.6)
    route("+5V", [(29.5, 12.6), u3[5]])
    route("+5V", [(29.5, 12.6), at(C5, 1)])
    solid(C5, "2")
    # +5V from the XIAO's 5V pad (pad 14) on the bottom along the back edge: to U3 and, by vias, to the LEDs
    route("+5V", [p[14], (51.9, 1.0), (28.9, 1.0), (28.9, 12.0), (29.5, 12.6)], BUS, BOT)

    # ---- under the XIAO, face side: MODE (THT, from the bottom) and behind it the two LEDs (reverse mount, cut-outs)
    SW1 = place(K("Button_Switch_THT"), "SW_PUSH_6mm", "SW1", "MODE 9.5 mm", 45.5, ROW - 3.25, 270, True)
    LED1 = place(K("LED_SMD"), "LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount", "LED1", "SK6812MINI-E", 45.5, 4.0, 90)
    LED2 = place(K("LED_SMD"), "LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount", "LED2", "SK6812MINI-E", 49.85, 4.0, 90)
    C6 = place(K("Capacitor_SMD"), C0805, "C6", "100nF", 42.4, 3.0, 270)
    C7 = place(K("Capacitor_SMD"), C0805, "C7", "100nF", 40.4, 3.0, 270)
    l1 = {n: at(LED1, n) for n in range(1, 5)}
    l2 = {n: at(LED2, n) for n in range(1, 5)}
    m1b, m1f = at(SW1, 1, (45.5, ROW - 3.25)), at(SW1, 1, (45.5, ROW + 3.25))
    m2b, m2f = at(SW1, 2, (50.0, ROW - 3.25)), at(SW1, 2, (50.0, ROW + 3.25))
    route("LED_DIN1", [(43.85, 6.74), l1[2]])
    route("LED_DIN2", [l1[4], (47.5, l1[4][1]), (47.5, 6.3), l2[2]])
    via("+5V", 43.6, 1.0)
    route("+5V", [(43.6, 1.0), l1[3]], 0.3)
    route("+5V", [(43.6, 1.0), (43.0, 1.6), (at(C6, 1)[0], 1.6), at(C6, 1)], 0.3)
    route("+5V", [(at(C6, 1)[0], 1.6), (at(C7, 1)[0], 1.6), at(C7, 1)], 0.3)
    via("+5V", 48.3, 0.8)
    route("+5V", [(48.3, 0.8), l2[3]], 0.3)
    for g in (l1[1], l2[1]):                         # (a via in front of each LED's ground pad)
        route("GND", [g, (g[0], 8.0)])
        via("GND", g[0], 8.0)
    for c in (C6, C7):
        g = at(c, 2)
        route("GND", [g, (g[0], 5.2)])
        via("GND", g[0], 5.2)
    # the ladder: the XIAO's D2 (pad 3) to MODE's pin-1 pair, on under the module (y 9.15) to PLAY/STOP and the encoder
    route("LADDER", [p[3], (44.3, p[3][1]), m1b])
    route("LADDER", [m1b, m1f], SIG, BOT)
    route("LADDER", [m1b, (46.7, 9.15), (55.0, 9.15)])
    route("MODE_R", [m2b, m2f], SIG, BOT)
    R61 = place(K("Resistor_SMD"), R0805, "R61", "2.2k", 47.75, ROW, 270)       # (between MODE's legs, under the module)
    route("MODE_R", [m2b, (48.6, 11.6), at(R61, 1)])
    route("GND", [at(R61, 2), (47.75, 16.1)])
    via("GND", 47.75, 16.1)
    # the ladder's pull-up R60 and filter C9, the encoder lines' pull-ups R64 / R65 and filters C10 / C11 (top, under
    # the module); their 3V3 from the module's 3V3 line by a bottom lane along y 19.15
    R60 = place(K("Resistor_SMD"), R0805, "R60", "10k", 41.2, 12.6, 90)
    C9 = place(K("Capacitor_SMD"), C0805, "C9", "100nF", 43.3, 12.6, 270)
    R64 = place(K("Resistor_SMD"), R0805, "R64", "10k", 40.5, 17.6, 0)
    C10 = place(K("Capacitor_SMD"), C0805, "C10", "100nF", 43.55, 16.6, 90)
    R65 = place(K("Resistor_SMD"), R0805, "R65", "10k", 48.8, 19.15, 0)
    C11 = place(K("Capacitor_SMD"), C0805, "C11", "100nF", 64.4, 12.6, 90, True)    # (face side, by the encoder)
    route("LADDER", [(41.2, p[3][1]), at(R60, 2)])
    route("LADDER", [(at(C9, 1)[0], p[3][1]), at(C9, 1)])
    route("GND", [at(C9, 2), (44.2, 13.9)])
    via("GND", 44.2, 13.9)
    route("+3V3", [p[12], (51.45, p[12][1] + 0.8), (51.45, 21.6), (52.6, 22.8), (52.6, BUS_Y)], SIG, BOT)
    route("+3V3", [(51.45, 19.15), (50.8, 19.8), (40.9, 19.8), (40.2, 19.3)], SIG, BOT)
    route("+3V3", [(47.1, 19.8), (46.9, 19.6)], SIG, BOT)
    via("+3V3", 40.2, 19.3)
    route("+3V3", [(40.2, 19.3), at(R64, 1)])
    route("+3V3", [at(R64, 1), (39.9, 14.5), at(R60, 1)])
    via("+3V3", 46.9, 19.6)
    route("+3V3", [(46.9, 19.6), at(R65, 1)])
    route("ENC_A", [at(R64, 2), at(C10, 1)])
    route("GND", [at(C10, 2), (44.4, 15.3)])
    via("GND", 44.4, 15.3)
    route("ENC_B", [p[8], (50.9, 19.15), at(R65, 2)])

    # ---- the XIAO's front-row lines: TX4 (D3) and TX3 (D4) on the bottom down the strip's right side to OUT4 (left)
    # and OUT3; TX2 (D1) under the module to OUT2 (right)
    tx = {k: front_unit(k, c, H) for k, c in zip((4, 3, 2), CH)}
    route("MIDI_TX4", [p[4], (35.85, p[4][1]), (35.85, 22.6), (tx[4][0], 22.6), tx[4]], SIG, BOT)
    route("MIDI_TX3", [p[5], (36.3, p[5][1]), (36.3, 23.1), (tx[3][0], 23.1), tx[3]], SIG, BOT)
    route("MIDI_TX2", [p[2], (39.3, p[2][1]), (39.3, 21.8), (41.6, 23.6), (tx[2][0], 23.6), tx[2]], SIG, BOT)
    via("+3V3", 52.6, BUS_Y)
    route("+3V3", [(CH[0] + 4.3, BUS_Y), (57.5, BUS_Y)], BUS)
    # ENC_A: the XIAO's D6 (pad 7) along y 20.4 under the module's front to the encoder; ENC_B: D7 (pad 8) along 18.6
    route("ENC_A", [p[7], (39.0, 20.4), (60.8, 20.4), (61.0, 20.2)])
    route("ENC_A", [(42.4, 20.4), (42.4, 18.2), at(R64, 2)])
    via("ENC_A", 61.0, 20.2)
    route("ENC_B", [p[8], (54.6, 18.6), (61.5, 18.6)])
    via("ENC_B", 61.5, 18.6)
    # IN_EN, MIDI_RX: the XIAO's D8, D9 (pads 9, 10) on the bottom, right, down to y 21.5 / 22.0, on to the corner
    route("IN_EN", [p[9], (54.0, p[9][1]), (54.0, 22.5), (69.0, 22.5)], SIG, BOT)
    via("IN_EN", 54.0, 21.3)
    route("MIDI_RX", [p[10], (54.75, p[10][1]), (54.75, 22.0), (75.0, 22.0)], SIG, BOT)

    # ======== the controls right of the XIAO (face side): PLAY/STOP and the encoder, on the row ========
    SW6 = place(K("Button_Switch_THT"), "SW_PUSH_6mm", "SW6", "PLAY/STOP 9.5 mm", 56.3, ROW - 3.25, 270, True)
    SW7 = place(K("Rotary_Encoder"), "RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm", "SW7", "EC11E09244BS",
                68.0, ROW + 3.2, 180, True)
    e = {n: at(SW7, n) for n in ("A", "B", "C", "S1", "S2")}
    p1b, p1f = at(SW6, 1, (56.3, ROW - 3.25)), at(SW6, 1, (56.3, ROW + 3.25))
    p2b, p2f = at(SW6, 2, (60.8, ROW - 3.25)), at(SW6, 2, (60.8, ROW + 3.25))
    route("LADDER", [(55.0, 9.15), p1b])
    route("LADDER", [p1b, p1f], SIG, BOT)
    route("LADDER", [p1f, (p1f[0], 21.5), (e["S2"][0], 21.5), e["S2"]], SIG, BOT)
    route("PLAY_R", [p2b, p2f], SIG, BOT)
    R62 = place(K("Resistor_SMD"), R0805, "R62", "6.8k", 58.55, 19.8, 0, True)
    route("PLAY_R", [p2f, at(R62, 1, (59.55, 19.8))], SIG, BOT)
    R63 = place(K("Resistor_SMD"), R0805, "R63", "22k", e["S1"][0], 4.3, 90, True)
    route("PUSH_R", [e["S1"], at(R63, 1, (e["S1"][0], 5.3))], SIG, BOT)
    route("ENC_A", [(61.0, 20.2), (64.9, 20.2), (e["A"][0] - 1.0, e["A"][1] + 1.0), e["A"]], SIG, BOT)
    route("ENC_B", [(61.5, 18.6), (e["B"][0] - 1.4, e["B"][1] + 1.0), e["B"]], SIG, BOT)
    route("ENC_B", [at(C11, 1), (e["B"][0] - 1.4, e["B"][1] + 1.0)], SIG, BOT)

    # ======== the power path (top): J1 -> D1, C1 -> Q1 -> U2 -> +5V over the encoder; the cut network beside it ========
    D1 = place(K("Diode_SMD"), "D_SOD-123F", "D1", "SMF5.0A", 70.1, 12.7, 270)
    C1 = place(K("Capacitor_SMD"), C0805, "C1", "10uF", 72.45, 12.4, 270)
    R1 = place(K("Resistor_SMD"), R0805, "R1", "5.1k", 73.0, 4.0, 90)
    R2 = place(K("Resistor_SMD"), R0805, "R2", "5.1k", 71.0, 4.0, 90)
    Q1 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q1", "AO3401A", 75.5, 12.5, 180)
    R3 = place(K("Resistor_SMD"), R0805, "R3", "100k", 79.5, 12.3, 270)
    Q2 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q2", "AO3400A", 64.6, 15.2, 0)
    R4 = place(K("Resistor_SMD"), R0805, "R4", "100k", 63.7, 11.6, 270)
    R5 = place(K("Resistor_SMD"), R0805, "R5", "100k", 64.3, 18.0, 0)
    C2 = place(K("Capacitor_SMD"), C0805, "C2", "100nF", 64.3, 20.2, 0)
    R6 = place(K("Resistor_SMD"), R0805, "R6", "1k", 58.55, ROW, 270)           # (between PLAY/STOP's legs)
    U2 = place(K("Package_TO_SOT_SMD"), "SOT-363_SC-70-6", "U2", "LM66100", 75.3, 15.7, 0)
    C3 = place(K("Capacitor_SMD"), C0805, "C3", "10uF", 57.15, 4.7, 0)
    C4 = place(K("Capacitor_SMD"), "CP_Elec_5x5.4", "C4", "100uF DNP", 79.0, 3.6, 0)
    # J1: CC on the bottom to R1/R2 right of it; VBUS both pads joined below them and on across the encoder
    a5, b5, a9, b9 = at(J1, "A5"), at(J1, "B5"), at(J1, "A9"), at(J1, "B9")
    via("CC2", b5[0], 7.75)
    via("CC1", a5[0], 8.45)
    route("CC2", [b5, (b5[0], 7.75)])
    route("CC1", [a5, (a5[0], 8.45)])
    route("CC2", [(b5[0], 7.75), (71.0, 7.75)], SIG, BOT)
    route("CC1", [(a5[0], 8.45), (71.6, 8.45)], SIG, BOT)
    via("CC2", 71.0, 7.75)
    via("CC1", 71.6, 8.45)
    route("CC2", [(71.0, 7.75), at(R2, 1)])
    route("CC1", [(71.6, 8.45), (73.0, 7.0), at(R1, 1)])
    NODE = 9.8                                       # (the VBUS_IN line under J1, on over the encoder)
    route("VBUS_IN", [a9, (a9[0], NODE), (b9[0], NODE), b9], PWR)
    NODE = 10.45                                     # (over the encoder: between its B pad and its rear mounting tab)
    route("VBUS_IN", [(b9[0], 9.8), (b9[0] + 0.65, NODE)], PWR)
    q1s, q1g, q1d = at(Q1, 2), at(Q1, 1), at(Q1, 3)
    route("VBUS_IN", [(b9[0] + 0.65, NODE), (at(R3, 1)[0], NODE), at(R3, 1)], PWR)
    for pad in (at(D1, 1), at(C1, 1), q1s):
        route("VBUS_IN", [(pad[0], NODE), pad], PWR)
    route("VBUS_IN", [(at(R4, 1)[0], 9.8), at(R4, 1)])
    route("VIN_SW", [q1d, at(U2, 1)], 0.4)
    route("PG", [q1g, (77.2, at(R3, 2)[1]), at(R3, 2)])
    q2g, q2d = at(Q2, 1), at(Q2, 3)
    route("PG", [q2d, (66.6, 16.0), (66.6, 18.6), (80.4, 18.6), (80.4, 13.7), at(R3, 2)])
    route("NG", [at(R4, 2), q2g])
    route("NG", [at(R6, 2), (59.6, 14.0), (62.3, 14.0), q2g])
    route("NG", [(62.3, 14.0), (62.3, 19.9), at(C2, 1)])
    route("NG", [(62.3, at(R5, 1)[1]), at(R5, 1)])
    # PWR_CUT: the XIAO's D10 (pad 11) right to R6, top
    route("PWR_CUT", [p[11], (54.9, 11.9), (57.7, 11.9), at(R6, 1)])
    # U2: VIN from Q1, CE onto VOUT; +5V by a via onto the bottom lane (y 4.6) back to the XIAO's 5V pad, C3, TP1, C4
    u = {n: at(U2, n) for n in range(1, 7)}
    route("+5V", [u[6], (77.3, u[6][1]), (77.3, u[3][1] + 1.0), (u[3][0], u[3][1] + 1.0), u[3]], SIG)
    via("+5V", 72.4, u[3][1] + 1.0)
    route("+5V", [(u[3][0], u[3][1] + 1.0), (72.4, u[3][1] + 1.0)], 0.4)
    route("+5V", [(72.4, u[3][1] + 1.0), (72.4, 4.6), (55.6, 4.6)], BUS, BOT)
    via("+5V", 55.6, 4.6)
    route("+5V", [(55.6, 4.6), (55.6, p[14][1]), (54.6, p[14][1])], PWR)
    route("+5V", [(55.6, p[14][1]), (55.6, 1.6), at(TP["TP1"], 1)], 0.3)
    route("+5V", [(55.6, at(C3, 1)[1]), at(C3, 1)], PWR)
    solid(C3, "2")
    solid(J1, "SH")
    solid(R62, "2")
    solid(C9, "2")
    solid(R5, "2")
    solid(C1, "2")
    sh = at(J1, "SH", (JX - 4.3, 6.4))
    route("GND", [at(C3, 2), (sh[0] - 0.55, sh[1] - 0.8), sh])
    route("GND", [u[2], (73.4, u[2][1])])
    via("GND", 73.4, u[2][1])
    c4p = at(C4, 1)
    route("+5V", [(72.4, 4.6), (c4p[0] - 1.6, 4.6)], BUS, BOT)
    via("+5V", c4p[0] - 1.6, 4.6)
    route("+5V", [(c4p[0] - 1.6, 4.6), c4p], BUS)
    for fp_, n in ((D1, 2),):
        g = at(fp_, n)
        via("GND", g[0], g[1] + 1.1)
        route("GND", [g, (g[0], g[1] + 1.1)])
    route("GND", [at(Q2, 2), (64.9, 16.75)])
    via("GND", 64.9, 16.75)

    # ======== OUT1 as MIDI IN (front right corner): RING1 -> R50 -> U4's LED -> Q3 -> TIP1; D2 across the LED ========
    U4 = place(K("Package_DIP"), "DIP-6_W7.62mm", "U4", "H11L1", 65.9, 29.5, 0)
    R50 = place(K("Resistor_SMD"), R0805, "R50", "220", 65.0, 26.2, 0)
    D2 = place(K("Diode_SMD"), "D_SOD-123", "D2", "1N4148W", 63.4, 30.6, 270)
    Q3 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q3", "AO3400A", 65.4, 38.8, 0)
    R52 = place(K("Resistor_SMD"), R0805, "R52", "1k", 63.4, 35.1, 270)
    R53 = place(K("Resistor_SMD"), R0805, "R53", "100k", 69.0, 38.9, 270)
    R51 = place(K("Resistor_SMD"), R0805, "R51", "4.7k", 74.9, 25.4, 270)
    C8 = place(K("Capacitor_SMD"), C0805, "C8", "100nF", 71.4, 26.3, 180)
    w = {n: at(U4, n) for n in (1, 2, 4, 5, 6)}
    route("RING1", [(62.9, 22.15), (at(R50, 1)[0], 23.6), at(R50, 1)])
    route("OPTO_A", [at(R50, 2), w[1]])
    route("OPTO_A", [at(D2, 1), w[1]])
    route("OPTO_K", [at(D2, 2), w[2]])
    route("OPTO_K", [w[2], (67.2, 33.0), (67.2, 38.0), at(Q3, 3)])
    route("TIP1", [(61.6, 22.65), (61.6, at(Q3, 2)[1]), at(Q3, 2)])
    route("IN_G", [at(R52, 2), at(Q3, 1)])
    route("IN_EN", [(69.0, 22.5), (69.0, 33.3), (62.4, 33.3)], SIG, BOT)
    via("IN_EN", 62.4, 33.3)
    route("IN_EN", [(62.4, 33.3), at(R52, 1)])
    route("IN_EN", [(69.0, 33.3), (69.0, 36.9)], SIG, BOT)
    via("IN_EN", 69.0, 36.9)
    route("IN_EN", [(69.0, 36.9), at(R53, 1)])
    route("MIDI_RX", [(75.0, 22.0), (75.0, w[4][1]), w[4]], SIG, BOT)
    via("MIDI_RX", 75.0, 27.3)
    route("MIDI_RX", [at(R51, 2), (75.0, 27.3)])
    route("GND", [at(R53, 2), (69.0, 40.9)])
    via("GND", 69.0, 40.9)
    route("GND", [w[5], (72.2, 32.04)])
    via("GND", 72.2, 32.04)
    c8g = at(C8, 2)
    route("GND", [c8g, (c8g[0], 27.6)])
    via("GND", c8g[0], 27.6)
    # 3V3: the bus on into the corner on the bottom (under RING1 / TIP1), up again before IN_EN
    via("+3V3", 57.5, BUS_Y)
    route("+3V3", [(57.5, BUS_Y), (67.5, BUS_Y)], SIG, BOT)
    via("+3V3", 67.5, BUS_Y)
    c8p = at(C8, 1)
    route("+3V3", [(67.5, BUS_Y), (at(TP["TP2"], 1)[0], BUS_Y), at(TP["TP2"], 1)])
    route("+3V3", [(at(R51, 1)[0], BUS_Y), at(R51, 1)])
    route("+3V3", [(c8p[0], BUS_Y), c8p, (w[6][0], c8p[1] + 1.1), w[6]])

    # ======== silk ========
    label("OUT1 A|B", X - 2.4, Y, 0.7, pcbnew.B_SilkS, rot=90)
    label("JP=SRC!", 12.4, 19.0, 0.6, rot=90)
    label("MIDI IN: OUT1", 69.5, 42.5, 0.6)
    label("jank", 74.0, 46.0, 1.0)
    label("5V", 58.4, 2.7, 0.6)
    label("3V3", 78.4, 19.9, 0.6)
    label("GND", 23.6, 2.0, 0.6)
    label("TX", 32.0, 18.3, 0.6)
    label("PWR", JX, 1.2, 0.6)
    label("MODE", 47.75, ROW - 4.3, 0.7, pcbnew.B_SilkS)
    label("PLAY", 58.55, ROW - 4.3, 0.7, pcbnew.B_SilkS)
    label("jank", 74.0, 46.0, 1.0, pcbnew.B_SilkS)
    return W, H


def out_unit(k, frame, groups=True, tx_via=True):
    """one output in its own frame: the combined DIN / jack footprint at the frame's edge point and, with groups, its
    two resistor groups behind it, wired as on the long board; returns its pads and groups"""
    (ex, ey), th = frame
    j = place(JANK, "MIDI_OUT_SDS-50J_or_SJ1-3523N", f"J{5 + k}", "DIN or TRS", ex, ey, th)
    L = lambda x, y: T(frame, x, y)
    din5, din4 = at(j, "5", L(-5, -15)), at(j, "4", L(5, -15))
    t_, r_ = at(j, "5", L(5, -8.5)), at(j, "4", L(-5, -8.5))
    route(f"TIP{k}", [din5, L(-5, -10.5), L(3.2, -10.5), t_], SIG, BOT)       # (inside the footprint, as on the long board)
    route(f"RING{k}", [din4, L(5, -10.5), L(-3.2, -10.5), r_])
    u = {"j": j, "din5": din5, "din4": din4, "T": t_, "R": r_}
    if groups:
        ta = cluster((f"R1{k}", f"JP1{k}", f"R3{k}"), (f"MIDI_TX{k}", f"TIP{k}"), -6.2, -19.93, "v", AX0207, "10", 2.8, 2.9,
                     frame)
        tb = cluster((f"R2{k}", f"JP2{k}", f"R4{k}"), ("+3V3", f"RING{k}"), 4.3, -19.93, "v", AX0309, "33", 3.1, 3.3, frame)
        route(f"TIP{k}", [ta["B"], L(-6.2, -17.4), L(-5, -16.4), din5])
        route(f"RING{k}", [tb["B"], L(4.3, -17.4), L(5, -16.4), din4])
        if tx_via:                                   # (none where the TX line arrives on top)
            via(f"MIDI_TX{k}", *L(-7.6, -21.2))
        u.update(ta=ta, tb=tb, tx_via=L(-7.6, -21.2), r_smd1=at(tb["par"], 1))
    return u


def layout_square():
    """OUT1 and OUT2 on the front edge, OUT3 on the left edge, OUT4 on the right edge; both USB-Cs on the back edge"""
    W, H, YC = 67.0, 58.0, 30.5
    BUSY = H - 16.75                                 # (the 3V3 bus, top, between the side groups and the front DINs)
    U1 = place(JANK, "XIAO-ESP32-S3-DIP", "U1", "XIAO ESP32S3", W / 2, 10.55, 90)
    J1 = place(K("Connector_USB"), "USB_C_Receptacle_GCT_USB4125-xx-x-0190_6P_TopMnt_Horizontal", "J1", "USB-C power",
               60.0, 3.4, 180)
    place(K("MountingHole"), "MountingHole_2.2mm_M2", "H1", "M2", 2.6, YC - 13.7)
    place(K("MountingHole"), "MountingHole_2.2mm_M2", "H2", "M2", W / 2 + 6.5, H - 22.0)
    p = {n: at(U1, n) for n in range(1, 15)}

    # ---- the four outputs: the side ones with their groups behind them, the front ones with theirs in the corners
    o3 = out_unit(3, ((0.0, YC), 270))
    o4 = out_unit(4, ((W, YC), 90), tx_via=False)
    c1, c2 = W / 2 - PITCH / 2, W / 2 + PITCH / 2
    o1 = out_unit(1, ((c1, H), 0), groups=False)
    o2 = out_unit(2, ((c2, H), 0), groups=False)
    g1t = cluster(("R11", "JP11", "R31"), ("MIDI_TX1", "TIP1"), 6.5, H - 14.1, "v", AX0207, "10", 2.8, 2.9)
    g1r = cluster(("R21", "JP21", "R41"), ("+3V3", "RING1"), 6.5, H - 8.1, "v", AX0309, "33", 3.1, 3.3)
    g2r = cluster(("R22", "JP22", "R42"), ("+3V3", "RING2"), W - 6.5, H - 14.1, "v", AX0309, "33", 3.1, 3.3)
    g2t = cluster(("R12", "JP12", "R32"), ("MIDI_TX2", "TIP2"), W - 6.5, H - 8.1, "v", AX0207, "10", 2.8, 2.9)
    CORR = H - 5.4                                   # (the corridor below the front DINs' pegs, to the jacks' near pins)
    b = g1t["B"]                                     # OUT1: 10 ohm group -> DIN 5 (bottom); 33 ohm group -> jack ring (top)
    route("TIP1", [b, (11.0, b[1]), (13.5, o1["din5"][1] + 0.27), o1["din5"]], SIG, BOT)
    b = g1r["B"]
    route("RING1", [b, (b[0] + 1.8, CORR), (o1["R"][0] - 1.25, CORR), o1["R"]])
    b = g2r["B"]                                     # OUT2: 33 ohm group -> DIN 4 (top); 10 ohm group -> jack tip (top)
    route("RING2", [b, (W - 11.0, b[1]), (W - 13.5, o2["din4"][1] + 0.27), o2["din4"]])
    b = g2t["B"]
    route("TIP2", [b, (b[0] - 1.8, CORR), (o2["T"][0] + 1.25, CORR), o2["T"]])

    # ---- TX: OUT1 and OUT2 on the bottom under the module, then out along LOW; OUT3 on the bottom to its group; OUT4 on
    # top under the module (the XIAO on headers), out between the right row's pads and down the right lane
    v1 = (g1t["A"][0] - 1.4, g1t["A"][1])
    v2 = (g2t["A"][0] - 1.4, g2t["A"][1])
    via("MIDI_TX1", *v1)
    via("MIDI_TX2", *v2)
    LOW = BUSY - 0.25
    XA, XB = p[1][0] + 2.1, p[1][0] + 1.3            # (TX1 down at XA, TX2 at XB; TX2 hops over TX1 on top near LOW)
    route("MIDI_TX1", [p[1], (XA, p[1][1]), (XA, LOW), (v1[0], LOW), v1], SIG, BOT)
    HOP = LOW - 1.0
    route("MIDI_TX2", [p[2], (XB, p[2][1]), (XB, HOP)], SIG, BOT)
    via("MIDI_TX2", XB, HOP)
    route("MIDI_TX2", [(XB, HOP), (XA + 0.82, HOP)])
    via("MIDI_TX2", XA + 0.82, HOP)
    route("MIDI_TX2", [(XA + 0.82, HOP), (XA + 0.82, LOW), (v2[0] - 1.6, LOW), (v2[0] - 1.6, v2[1]), v2], SIG, BOT)
    tv3 = o3["tx_via"]
    route("MIDI_TX3", [p[5], (p[5][0] - 1.8, p[5][1] + 1.8), (p[5][0] - 1.8, tv3[1]), tv3], SIG, BOT)
    RL = W / 2 + 10.6                                # (the right lane, top, between the module and OUT4's groups)
    tv4 = o4["tx_via"]
    route("MIDI_TX4", [p[4], (p[4][0] + 1.27, p[4][1] + 1.27), (RL, p[4][1] + 1.27), (RL, tv4[1] - 0.7), tv4])
    TP4 = place(K("TestPoint"), "TestPoint_Pad_D1.5mm", "TP4", "TX", XA - 2.0, 30.0)
    via("MIDI_TX1", XA, 30.0)
    route("MIDI_TX1", [at(TP4, 1), (XA, 30.0)])

    # ---- 3V3: the XIAO's pad to OUT4's 33 ohm group (top, right); on the bottom under the module down to the bus
    route("+3V3", [p[12], (o4["r_smd1"][0], p[12][1]), o4["r_smd1"]], BUS)
    X3 = W / 2 - 0.5
    route("+3V3", [p[12], (X3, p[12][1]), (X3, 22.0)], BUS, BOT)
    via("+3V3", X3, 22.0)
    route("+3V3", [(X3, 22.0), (X3, BUSY)], BUS)
    route("+3V3", [(11.5, BUSY), (W - 3.5, BUSY)], BUS)
    route("+3V3", [(o3["r_smd1"][0], BUSY), o3["r_smd1"]], BUS)
    s1 = at(g1r["par"], 1)
    route("+3V3", [(11.5, BUSY), (11.5, s1[1]), s1], BUS)
    s2 = at(g2r["par"], 1)
    route("+3V3", [(W - 3.5, BUSY), (W - 3.5, s2[1]), s2], BUS)
    TP2 = place(K("TestPoint"), "TestPoint_Pad_D1.5mm", "TP2", "3V3", X3, 30.0)
    TP3 = place(K("TestPoint"), "TestPoint_Pad_D1.5mm", "TP3", "GND", X3 + 4.0, 30.0)

    # ---- the power path (top, the back right corner): J1 -> D1, C1 -> Q1 -> U2 -> +5V, as on the long board, 12 mm left
    D1 = place(K("Diode_SMD"), "D_SOD-123F", "D1", "SMF5.0A", 60.0, 12.2, 270)
    C1 = place(K("Capacitor_SMD"), C0805, "C1", "10uF", 62.5, 11.6, 270)
    R1 = place(K("Resistor_SMD"), R0805, "R1", "5.1k", 65.5, 10.2, 270)
    R2 = place(K("Resistor_SMD"), R0805, "R2", "5.1k", 65.5, 14.0, 270)
    Q1 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q1", "AO3401A", 54.0, 11.0, 180)
    R3 = place(K("Resistor_SMD"), R0805, "R3", "100k", 57.0, 12.8, 270)
    Q2 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q2", "AO3400A", 54.0, 16.0, 0)
    R4 = place(K("Resistor_SMD"), R0805, "R4", "100k", 58.5, 17.0, 270)
    R5 = place(K("Resistor_SMD"), R0805, "R5", "100k", 46.8, 17.0, 90)
    C2 = place(K("Capacitor_SMD"), C0805, "C2", "100nF", 48.8, 17.0, 90)
    R6 = place(K("Resistor_SMD"), R0805, "R6", "1k", 50.8, 17.0, 270)
    U2 = place(K("Package_TO_SOT_SMD"), "SOT-363_SC-70-6", "U2", "LM66100", 49.0, 10.0, 180)
    C3 = place(K("Capacitor_SMD"), C0805, "C3", "10uF", 45.0, 4.8, 270)
    C4 = place(K("Capacitor_SMD"), "CP_Elec_5x5.4", "C4", "100uF DNP", 50.4, 4.0, 0)
    TP1 = place(K("TestPoint"), "TestPoint_Pad_D1.5mm", "TP1", "5V", 45.0, 1.5)
    a5, b5, a9, b9 = at(J1, "A5"), at(J1, "B5"), at(J1, "A9"), at(J1, "B9")
    via("CC1", a5[0], 8.9)
    via("CC2", b5[0], 8.0)
    route("CC1", [a5, (a5[0], 8.9)])
    route("CC2", [b5, (b5[0], 8.0)])
    route("CC1", [(a5[0], 8.9), (64.3, 8.9)], SIG, BOT)
    via("CC1", 64.3, 8.9)
    route("CC1", [(64.3, 8.9), at(R1, 1)])
    route("CC2", [(b5[0], 8.0), (66.3, 8.0), (66.3, 13.4)], SIG, BOT)
    via("CC2", 66.3, 13.4)
    route("CC2", [(66.3, 13.4), at(R2, 1)])
    NODE = 9.8
    route("VBUS_IN", [a9, (a9[0], NODE), (b9[0], NODE), b9], PWR)
    route("VBUS_IN", [at(D1, 1), (at(D1, 1)[0], NODE)], PWR)
    route("VBUS_IN", [at(C1, 1), (at(C1, 1)[0], NODE), (b9[0], NODE)], PWR)
    q1s, q1g, q1d = at(Q1, 2), at(Q1, 1), at(Q1, 3)
    route("VBUS_IN", [(a9[0], NODE), (55.6, NODE), q1s], PWR)
    route("VBUS_IN", [(at(R3, 1)[0], NODE), at(R3, 1)])
    route("VBUS_IN", [(a9[0], NODE), at(R4, 1)], SIG)
    route("PG", [q1g, (q1g[0], 12.9), (at(R3, 2)[0], 12.9), at(R3, 2)])
    q2g, q2d = at(Q2, 1), at(Q2, 3)
    route("PG", [q2d, (at(R3, 2)[0], q2d[1]), at(R3, 2)])
    NG = at(R4, 2)[1]
    route("NG", [at(R4, 2), (51.95, NG), at(R6, 2), at(C2, 1), at(R5, 1)])
    route("NG", [q2g, (51.95, q2g[1]), (51.95, NG)])
    r6p = at(R6, 1)
    route("PWR_CUT", [p[11], (r6p[0], p[11][1]), (r6p[0], r6p[1] - 1.1)], SIG, BOT)
    via("PWR_CUT", r6p[0], r6p[1] - 1.1)
    route("PWR_CUT", [(r6p[0], r6p[1] - 1.1), r6p])
    r1g, c1g = at(R1, 2), at(C1, 2)                  # (R1's ground pad: its pour connection is starved)
    route("GND", [r1g, (r1g[0] - 1.5, r1g[1]), c1g])
    u = {n: at(U2, n) for n in range(1, 7)}
    X5 = u[6][0] - 1.06
    route("VIN_SW", [q1d, (u[1][0] + 0.96, q1d[1]), (u[1][0] + 0.4, u[1][1]), u[1]], 0.4)
    route("+5V", [u[3], (u[3][0], 8.5), (X5, 8.5), (X5, u[6][1]), u[6]], 0.3)
    route("GND", [u[2], (u[2][0] + 0.8, u[2][1]), (u[2][0] + 1.66, 9.3)])
    via("GND", u[2][0] + 1.66, 9.3)
    route("+5V", [(X5, 8.5), (X5, p[14][1]), p[14]], PWR)
    route("+5V", [at(C3, 1), (at(C3, 1)[0], p[14][1])], PWR)
    route("+5V", [at(C4, 1), (X5, at(C4, 1)[1])], PWR)
    route("+5V", [at(TP1, 1), (at(TP1, 1)[0], p[14][1])], PWR)
    V5 = (p[14][0] + 2.5, p[14][1])                  # (to the left on the bottom, along the back edge)
    via("+5V", *V5)
    route("+5V", [V5, (V5[0] - 1.3, 1.2), (10.0, 1.2)], BUS, BOT)

    # ---- the face side (bottom) in the back left corner: SW1 and the two LEDs; their driver below them
    SW1 = place(K("Button_Switch_THT"), "SW_PUSH_6mm", "SW1", "button 9.5 mm", 19.0, 3.2, 0, True)
    LED1 = place(K("LED_SMD"), "LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount", "LED1", "SK6812MINI-E", 5.0, 6.6, 0)
    LED2 = place(K("LED_SMD"), "LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount", "LED2", "SK6812MINI-E", 5.0, 2.2, 180)
    route("BTN", [p[3], (21.5, p[3][1]), (21.5, 3.2), at(SW1, 1, (19.0, 3.2)), at(SW1, 1, (12.5, 3.2))])
    d1o, d2i = at(LED1, 4), at(LED2, 2)
    route("LED_DIN2", [d1o, (9.1, d1o[1]), (9.1, d2i[1]), d2i])
    for led in (LED1, LED2):                         # (each VDD through a via to the +5V branch on the bottom)
        v = at(led, 3)
        via("+5V", v[0], 4.4)
        route("+5V", [v, (v[0], 4.4)], BUS)
    route("+5V", [(at(LED2, 3)[0], 4.4), (10.0, 4.4), (10.0, 1.2)], 0.3, BOT)
    g = at(LED1, 1)                                  # (a via by each LED's ground pad: starved in the pour)
    route("GND", [g, (g[0], g[1] + 1.55)])
    via("GND", g[0], g[1] + 1.55)
    g = at(LED2, 1)
    route("GND", [g, (g[0] + 0.78, 0.7)])
    via("GND", g[0] + 0.78, 0.7)
    for p_ in LED2.Pads():                           # (LED2's ground pad by the edge: no room for a 2-spoke relief, solid)
        if p_.GetNumber() == "1":
            p_.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    U3 = place(K("Package_TO_SOT_SMD"), "SOT-23-5_HandSoldering", "U3", "74AHCT1G125", 16.0, 15.0, 180)
    R7 = place(K("Resistor_SMD"), R0805, "R7", "100k", 20.5, 17.0, 270)
    C5 = place(K("Capacitor_SMD"), C0805, "C5", "100nF", 16.5, 18.6, 0)
    C6 = place(K("Capacitor_SMD"), C0805, "C6", "100nF", 15.75, 5.5, 0)
    C7 = place(K("Capacitor_SMD"), C0805, "C7", "100nF", 15.75, 9.0, 0)
    R8C = cluster(("R8", "JP8", "R9"), ("LED_BUF", "LED_DIN1"), 10.5, 14.5, "h", AX0207, "330")
    a_ = at(U3, 2)
    route("LED_DATA", [p[6], (19.5, p[6][1]), (18.9, a_[1]), a_])
    route("LED_DATA", [(at(R7, 1)[0], p[6][1]), at(R7, 1)])
    route("LED_BUF", [at(U3, 4), (R8C["A"][0], at(U3, 4)[1])])
    jb = at(R8C["jp"], 2)
    d1i = at(LED1, 2)
    route("LED_DIN1", [(R8C["B"][0], jb[1]), (1.0, jb[1]), (1.0, d1i[1]), d1i])
    route("GND", [at(U3, 3), (18.6, 13.3)])
    via("GND", 18.6, 13.3)
    route("GND", [at(U3, 1), (18.6, 16.9)])
    via("GND", 18.6, 16.9)
    route("+5V", [(14.0, 1.2), (14.0, 17.2)], BUS, BOT)               # (the driver's and the caps' +5V, on the bottom)
    for y in (17.2, at(C6, 1)[1], at(C7, 1)[1]):
        via("+5V", 14.0, y)
    route("+5V", [(14.0, 17.2), (14.0, 16.4), at(U3, 5)])
    route("+5V", [(14.0, 17.2), at(C5, 1)])
    route("+5V", [(14.0, at(C6, 1)[1]), at(C6, 1)])
    route("+5V", [(14.0, at(C7, 1)[1]), at(C7, 1)])

    # ---- silk
    label("OUT1 TRS|DIN", c1, H - 17.0, 0.7)
    label("OUT2 TRS|DIN", c2 - 3.0, H - 17.0, 0.7)
    label("OUT3", 17.4, YC, 0.7, rot=270)
    label("OUT4", W - 17.4, YC, 0.7, rot=90)
    label("jank", W / 2, 24.0, 1.0)
    label("3V3", X3, 28.4, 0.6)
    label("GND", X3 + 4.0, 28.4, 0.6)
    label("5V", 47.0, 1.5, 0.6)
    label("TX", XA - 2.0, 28.6, 0.6)
    label("PWR", 60.0, 1.2, 0.6)
    label("BUTTON", 15.75, 11.0, 0.7, pcbnew.B_SilkS)
    for jp, frame, lx, ly in (("JP21", None, 2.0, H - 8.1), ("JP22", None, W - 11.1, H - 14.1),
                              ("JP23", ((0.0, YC), 270), -0.2, -19.93), ("JP24", ((W, YC), 90), -0.2, -19.93)):
        label("JP=3V3!", *T(frame, lx, ly), 0.6, rot=90 + (frame[1] if frame else 0))   # (the ring's bypass: 3V3 on the ring)
    return W, H


W, H = {"long": layout_long, "square": layout_square}[LAYOUT]()
# ======== the outline, the pours ========
OUTLINE = [(0, 0), (W, 0), (W, H), (0, H)]
for (ax, ay), (bx, by) in zip(OUTLINE, OUTLINE[1:] + OUTLINE[:1]):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(pcbnew.VECTOR2I(MM(OX + ax), MM(OY + ay)))
    s.SetEnd(pcbnew.VECTOR2I(MM(OX + bx), MM(OY + by)))
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(MM(0.1))
    board.Add(s)
for layer in (TOP, BOT):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(net("GND"))
    z.SetLocalClearance(MM(0.25))
    z.SetMinThickness(MM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    o = z.Outline()
    o.NewOutline()
    for x, y in OUTLINE:
        o.Append(MM(OX + x), MM(OY + y))
    board.Add(z)

pts = [(OX + x, OY + y) for x, y in OUTLINE]
STITCHED = stitch(board, lambda x, y: via("GND", x - OX, y - OY), pts)
print(f"{len(STITCHED)} stitching vias added")
board.GetDesignSettings().SetAuxOrigin(pcbnew.VECTOR2I(MM(OX), MM(OY + H)))   # (the lower left: Gerbers, drill)
pcbnew.ZONE_FILLER(board).Fill(board.Zones())
out = os.path.join(HERE, f"{NAME}{'' if LAYOUT == 'long' else '-' + LAYOUT}.kicad_pcb")
board.Save(out)
print("saved", out, f"{W} x {H} mm")
