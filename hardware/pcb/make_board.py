# SPDX-License-Identifier: GPL-3.0-only
"""The jank board (README.md): generates jank-pcb.kicad_pcb with KiCad's own Python (pcbnew), from the schematic's
intended nets (fab/intended-nets.json, written by make_schematic.py) and the footprints of make_footprints.py.

  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 make_board.py [long|square]

long (the default): jank-pcb.kicad_pcb, v5, 84 x 67 mm, as below. square: jank-pcb-square.kicad_pcb, 67 x 58 mm, an older
layout (layout_square(); README "The square variant"), built from the frozen intent fab/square/intended-nets.json; unused.

Then make_fab.sh checks it (DRC, the schematic's nets) and writes the JLCPCB files.

v5 (the user's rulings, 2026-11-08): 2 layers, 84 x 67 mm, the TOP is the lid side. Every output has both connectors
fitted on one footprint: the XKB PJ-320A jack on the top, its front on the edge, and the HOOYA DIN-504B-M10 on the
bottom on the same axis (its plate 1.0 mm in), with a TRS A/B slide switch (MSS22D18) on the top. The FRONT edge carries
OUT2, OUT3, OUT4 left to right (lid up, from the front too) at a 21.1 mm pitch; the BACK edge OUT1 (the same footprint
turned 180 degrees; also the MIDI IN: an H11L1 behind an N-FET gate, its source from 3V3 through Q4), the XIAO's own
USB-C (the module on 2.54 mm headers) and J1 (power only). Between them a band: the MIDI IN (left), the 0.91" OLED on
its header (middle), BACK and OK right of it and the EC11 encoder under them. The beat LED (SK6812MINI-HS) is left of
the XIAO, its driver under the module. The MIDI resistors and R8 are swappable groups: an axial part standing UNDER the
board, a bypass solder jumper and 1206 pads in parallel (not fitted) on the top. Page coordinates: board (x, y) from the
back-left corner, y towards the front; KiCad's page = (100 + x, 100 + y).

Routing, in short: TX1 (D0) on the bottom down the strip to OUT1's 10 ohm group; TXB (D1, OUT2..OUT4 on one pad) down the
strip to a bottom lane under the front groups; IN_EN / MIDI_RX (D4 / D5) on top west to the MIDI IN; the ladder, ENC_A
and ENC_B along the band in front of the XIAO to the buttons and the encoder; SCL / SDA (D8 / D9) on the bottom to the
OLED's header; 3V3 from a bottom line inside the right row to the OLED and the bus (top) to the front groups and, up the
left edge, to Q4; +5V from U2 on the bottom to the XIAO's 5V pad and along the back edge to U3 and the LED; ground is
both pours.
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
DBG = {}                                        # (pad places for the checks while laying out)


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


def cluster(refs, nets, cx, cy, orient, axial, value, dj=3.0, ds=3.2, frame=None, mirror=False, bottom=False):
    """mirror: the jumper on the other side of the axial part (and the 1206 pads); bottom: the axial part on the bottom side
    (v5: the standing parts hang under the board, beside the DINs), the jumper and the 1206 pads stay on the top"""
    rmain, jp, rpar = refs
    a, b = nets
    th = frame[1] if frame else 0.0
    xj, xs = (cx + dj, cx - ds) if mirror else (cx - dj, cx + ds)
    if orient == "v":
        r = place(K("Resistor_THT"), axial, rmain, value, *T(frame, cx, cy - 1.27), (270 + th) % 360, bottom)
        j = place(K("Jumper"), JPFP, jp, "bypass", *T(frame, xj, cy), (270 + th) % 360)
        s = place(K("Resistor_SMD"), R1206, rpar, "DNP", *T(frame, xs, cy), (270 + th) % 360)
    else:                                            # (bottom: the flip mirrors the part, so it turns the other way)
        r = place(K("Resistor_THT"), axial, rmain, value, *T(frame, cx + 1.27, cy), (th if bottom else 180 + th) % 360, bottom)
        j = place(K("Jumper"), JPFP, jp, "bypass", *T(frame, cx, cy - dj), 180 + th)
        s = place(K("Resistor_SMD"), R1206, rpar, "DNP", *T(frame, cx, cy + ds), 180 + th)
    for node, n in ((a, "1"), (b, "2")):
        j1, s1 = at(j, n), at(s, n)
        if orient == "v":
            line = cy - 1.27 if n == "1" else cy + 1.27
            route(node, [j1, T(frame, xj, line), T(frame, xs, line), s1])
        else:
            line = cx + 1.27 if n == "1" else cx - 1.27
            route(node, [j1, T(frame, line, cy - dj), T(frame, line, cy + ds), s1])
    out = {"A": at(r, 1), "B": at(r, 2), "jp": j, "par": s}
    want_a = T(frame, cx, cy - 1.27) if orient == "v" else T(frame, cx + 1.27, cy)
    assert abs(out["A"][0] - want_a[0]) < 0.01 and abs(out["A"][1] - want_a[1]) < 0.01, (rmain, out["A"], want_a)
    return out


def front_unit(k, c, H, tx_lane):
    """v5: one output on the front edge: the jack on the top and the DIN on the bottom (one footprint), the two resistor
    groups behind it (the standing parts under the board), the A/B switch on the top between them; the 10 ohm group on
    the right (the DIN's pin 5, mirrored on the bottom), the 33 ohm group on the left"""
    GY = H - 26.43                                   # (the groups' centre line)
    j = place(JANK, "MIDI_OUT_PJ-320A_top_DIN-504B-M10_bottom", f"J{5 + k}", "TRS + DIN", c, H, 0)
    ta = cluster((f"R1{k}", f"JP1{k}", f"R3{k}"), ("MIDI_TXB", f"TIP{k}"), c + 6.2, GY, "v", AX0207, "10", 2.8, 2.9,
                 mirror=True, bottom=True)
    tb = cluster((f"R2{k}", f"JP2{k}", f"R4{k}"), ("+3V3", f"RING{k}"), c - 4.3, GY, "v", AX0309, "33", 3.1, 3.3,
                 mirror=True, bottom=True)
    sw = place(JANK, "SW_Slide_DPDT_MSS22D18", f"SW{1 + k}", "TRS A/B", c + 1.7, H - 19.0, 270)   # (top)
    din5, din4 = at(j, "5"), at(j, "4")
    jt, jr = at(j, "T"), at(j, "R")
    solid(j, "S")
    s_ = {n: at(sw, n) for n in range(1, 7)}
    B = GY + 1.27
    # TIP = 10 ohm group -> DIN 5 (right), RING = 33 ohm group -> DIN 4 (left), top; the switch: 1 RING, 2 JR, 3 TIP |
    # 4 TIP, 5 JT, 6 RING
    route(f"TIP{k}", [ta["B"], (ta["B"][0], GY + 2.53), (din5[0], GY + 3.53), din5])
    route(f"TIP{k}", [(din5[0], s_[3][1]), s_[3]])
    route(f"TIP{k}", [s_[3], (c, s_[3][1] - 1.5), (c, s_[4][1] + 1.5), s_[4]], SIG, BOT)
    route(f"RING{k}", [tb["B"], (tb["B"][0], GY + 2.53), (din4[0], GY + 3.53), din4])
    route(f"RING{k}", [(din4[0], s_[6][1]), s_[6]])
    route(f"RING{k}", [(c - 1.2, B), s_[1]])
    # JT down the left of the DIN's pin 4 side to the jack's tip; JR down the right, round the jack's sleeve tab (bottom)
    route(f"JT{k}", [s_[5], (c - 3.0, s_[5][1] + 0.8), (c - 3.4, s_[5][1] + 2.0), (c - 3.4, H - 11.0), jt], SIG, BOT)
    route(f"JR{k}", [s_[2], (c + 3.4, s_[2][1] + 1.7), (c + 3.4, H - 4.6), (c - 1.0, H - 4.6), jr], SIG, BOT)
    route("+3V3", [(tb["A"][0], H - 29.2), tb["A"]], BUS)
    v = (round(ta["A"][0] + 1.4, 4), ta["A"][1])     # (the TX via on the 10 ohm group's A line, right of the axial part)
    via("MIDI_TXB", *v)
    route("MIDI_TXB", [v, at(ta["jp"], 1, v)])
    route("MIDI_TXB", [v, (v[0], tx_lane)], SIG, BOT)
    label(f"OUT{k} A|B", c - 4.1, H - 19.0, 0.7, rot=90)
    label("JP=3V3!", c + 1.0, GY, 0.6, rot=90)       # (the ring's bypass: 3V3 straight on the ring)
    return v


def layout_long():
    """v5, 84 x 67 mm (README "v5"). The top is the lid side. FRONT edge, left to right seen from the top (and from the
    front, lid up): OUT2, OUT3, OUT4, each a PJ-320A on the top and a DIN-5 on the bottom. BACK edge: OUT1 (the same,
    turned to face the back; the MIDI IN one), the strip (OUT1's A/B switch, the LED), the XIAO (USB-C over the edge), J1
    and the power path. Between: the MIDI IN circuit (left), the OLED (middle, on its header at its right end), BACK and
    OK (right of the screen) and the encoder under them."""
    W, H = 84.0, 67.0
    BUS_Y = H - 29.2                                 # (the 3V3 bus, top, behind the front groups)
    TX_LANE = H - 30.3                               # (TXB, bottom, to the three front TX vias)
    CH = [10.45 + 21.1 * i for i in range(3)]       # (OUT2, OUT3, OUT4 left to right: lid up they read 2, 3, 4 from the front)
    C1X, XC, JX = 10.75, 45.2, 64.5

    # ======== the back edge ========
    U1 = place(JANK, "XIAO-ESP32-S3-DIP_Headers", "U1", "XIAO ESP32S3", XC, 10.55, 90)
    p = {n: at(U1, n) for n in range(1, 15)}
    J1 = place(K("Connector_USB"), "USB_C_Receptacle_GCT_USB4125-xx-x-0190_6P_TopMnt_Horizontal", "J1", "USB-C power",
               JX, 3.4, 180)
    for ref, x, y in (("H1", W - 3.2, 3.2), ("H2", W - 3.2, H - 3.2), ("H3", 3.2, 26.0), ("H4", 3.2, 34.0)):
        place(K("MountingHole"), "MountingHole_2.2mm_M2", ref, "M2", x, y)

    # ---- OUT1: the footprint turned 180 (both faces on the back edge), its groups in front of it (standing parts under
    # the board), the 10 ohm one on the right (by the strip), the 33 ohm one on the left; their B sides cross to the DIN
    F1 = ((C1X, 0.0), 180)
    J6 = place(JANK, "MIDI_OUT_PJ-320A_top_DIN-504B-M10_bottom", "J6", "TRS + DIN", C1X, 0.0, 180)
    ta1 = cluster(("R11", "JP11", "R31"), ("MIDI_TX1", "TIP1"), -6.2, -20.5, "v", AX0207, "10", 2.8, 2.9, F1, bottom=True)
    tb1 = cluster(("R21", "JP21", "R41"), ("SRC1", "RING1"), 4.3, -20.5, "v", AX0309, "33", 3.1, 3.3, F1, bottom=True)
    SW2 = place(JANK, "SW_Slide_DPDT_MSS22D18", "SW2", "TRS A/B", 22.6, 9.0, 90)   # (top, in the strip)
    d5, d4 = at(J6, "5"), at(J6, "4")
    jt1, jr1 = at(J6, "T"), at(J6, "R")
    solid(J6, "S")
    s = {n: at(SW2, n) for n in range(1, 7)}
    DBG.update(d5=d5, d4=d4, jt1=jt1, jr1=jr1, sw2=s, ta1=(ta1["A"], ta1["B"]), tb1=(tb1["A"], tb1["B"]))
    route("TIP1", [ta1["B"], (ta1["B"][0], 17.9), (d5[0], 17.9), d5])                       # (top, across)
    route("RING1", [tb1["B"], (tb1["B"][0], 17.9), (d4[0], 17.9), d4], SIG, BOT)            # (bottom, across)
    route("JT1", [s[5], (25.0, 10.25), (14.0, 10.25), jt1])
    route("JR1", [s[2], (16.5, s[2][1]), (14.4, 6.9), (14.4, 4.4), jr1])
    route("RING1", [d4, (20.0, d4[1]), (22.6, 13.2), (27.6, 13.2), (27.6, s[6][1]), s[6]])
    route("RING1", [(22.6, 13.2), s[1]])
    route("TIP1", [ta1["B"], (19.5, 16.7), (24.3, 13.0), s[4]], SIG, BOT)
    route("TIP1", [s[4], (24.3, s[4][1] - 1.7), (24.3, s[3][1] + 1.7), s[3]], SIG, BOT)
    # TX1: the XIAO's D0 on the bottom down the strip to the 10 ohm group's A pad
    route("MIDI_TX1", [p[1], (35.2, p[1][1]), (35.2, 22.6), (ta1["A"][0] + 0.85, 22.6), ta1["A"]], SIG, BOT)
    via("MIDI_TX1", 35.2, 11.82)
    TP4 = place(K("TestPoint"), "TestPoint_Pad_D1.5mm", "TP4", "TX", 33.2, 11.0)
    route("MIDI_TX1", [(35.2, 11.82), (34.0, 11.82), at(TP4, 1)])

    # ---- the beat LED in the strip (top), its driver under the XIAO (top, under the module; R8 standing underneath)
    LED1 = place(JANK, "LED_SK6812MINI-HS_3.7x3.5mm_HandSolder", "LED1", "SK6812MINI-HS", 33.0, 4.2, 90)
    C6 = place(K("Capacitor_SMD"), C0805, "C6", "100nF", 29.8, 5.6, 270)
    U3 = place(K("Package_TO_SOT_SMD"), "SOT-23-5_HandSoldering", "U3", "74AHCT1G125", 47.6, 3.9, 0)
    C5 = place(K("Capacitor_SMD"), C0805, "C5", "100nF", 50.4, 2.9, 270, True)   # (bottom)
    R7 = place(K("Resistor_SMD"), R0805, "R7", "100k", 45.8, 9.4, 180, True)    # (bottom, on the data line)
    R8C = cluster(("R8", "JP8", "R9"), ("LED_BUF", "LED_DIN1"), 42.0, 4.3, "h", AX0207, "330", 2.4, 2.4, None, bottom=True)
    TP3 = place(K("TestPoint"), "TestPoint_Pad_D1.5mm", "TP3", "GND", 29.0, 1.4)
    l = {n: at(LED1, str(n)) for n in range(1, 5)}
    u3 = {n: at(U3, n) for n in range(1, 6)}
    DBG.update(led=l, u3=u3, c6=(at(C6, 1), at(C6, 2)), c5=(at(C5, 1), at(C5, 2)), r7=(at(R7, 1), at(R7, 2)),
               r8=(R8C["A"], R8C["B"]))
    route("LED_DIN1", [(R8C["B"][0], 4.2), (33.0, 4.2), l[2]])
    route("+5V", [at(C6, 1), (l[3][0], at(C6, 1)[1]), l[3]])
    via("+5V", 29.8, 3.6)
    route("+5V", [(29.8, 3.6), at(C6, 1)])
    route("+5V", [(29.8, 3.6), (29.8, 1.0)], SIG, BOT)
    for g in (l[1], at(C6, 2)):
        solid(LED1, "1")
    solid(C6, "2")
    # LED_DATA: the XIAO's D3 on the bottom, under the ladder, up again by U3's input
    route("LED_DATA", [p[4], (39.4, p[4][1]), (39.4, 9.4), (44.8, 9.4), (44.8, u3[2][1])], SIG, BOT)
    via("LED_DATA", 44.8, u3[2][1])
    route("LED_DATA", [(44.8, u3[2][1]), u3[2]])
    route("LED_BUF", [u3[4], (u3[4][0], 6.0), (R8C["A"][0], 6.0)])
    route("+5V", [u3[5], (u3[5][0], 1.0)])
    via("+5V", u3[5][0], 1.0)
    route("+5V", [at(C5, 1), (at(C5, 1)[0], 1.0)], SIG, BOT)
    solid(C5, "2")
    solid(R7, "2")
    for n in (1, 3):
        solid(U3, str(n))
    # +5V on the bottom along the back edge, from the XIAO's 5V pad to the driver and the LED
    route("+5V", [p[14], (51.9, 1.0), (29.8, 1.0)], BUS, BOT)

    # ---- under the XIAO (top): the ladder's pull-up and filter, ENC_A's pull-up and filter
    R60 = place(K("Resistor_SMD"), R0805, "R60", "10k", 41.2, 12.6, 90)
    C9 = place(K("Capacitor_SMD"), C0805, "C9", "100nF", 43.3, 12.6, 270)
    R64 = place(K("Resistor_SMD"), R0805, "R64", "10k", 40.5, 17.6, 0)
    C10 = place(K("Capacitor_SMD"), C0805, "C10", "100nF", 43.55, 16.6, 90)
    route("LADDER", [p[3], (50.8, p[3][1]), (50.8, 19.6), (75.5, 19.6), (75.5, 23.2)])
    route("LADDER", [(41.2, p[3][1]), at(R60, 2)])
    route("LADDER", [(at(C9, 1)[0], p[3][1]), at(C9, 1)])
    solid(C9, "2")
    route("GND", [at(C9, 2), (44.2, 13.9)])
    via("GND", 44.2, 13.9)
    route("+3V3", [p[12], (51.45, p[12][1] + 0.8), (51.45, 35.6)], SIG, BOT)
    route("+3V3", [(51.45, 19.15), (50.8, 19.8), (40.9, 19.8), (40.2, 19.3)], SIG, BOT)
    via("+3V3", 40.2, 19.3)
    route("+3V3", [(40.2, 19.3), at(R64, 1)])
    route("+3V3", [at(R64, 1), (39.9, 14.5), at(R60, 1)])
    route("ENC_A", [at(R64, 2), at(C10, 1)])
    route("GND", [at(C10, 2), (44.4, 15.3)])
    via("GND", 44.4, 15.3)
    # ENC_A, ENC_B, the ladder on top along the band in front of the XIAO (north to south: LADDER, ENC_A, ENC_B)
    route("ENC_A", [p[7], (38.6, 20.4), (64.35, 20.4), (64.35, 34.0)])
    route("ENC_A", [(42.4, 20.4), (42.4, 18.2), at(R64, 2)])
    route("ENC_B", [p[8], (53.5, 18.9), (53.5, 25.6)], SIG, BOT)
    via("ENC_B", 53.5, 21.2)
    route("ENC_B", [(53.5, 21.2), (63.65, 21.2), (63.65, 39.0)])
    R65 = place(K("Resistor_SMD"), R0805, "R65", "10k", 52.5, 23.0, 180, True)
    C11 = place(K("Capacitor_SMD"), C0805, "C11", "100nF", 53.5, 26.6, 270, True)
    DBG.update(r65=(at(R65, 1), at(R65, 2)), c11=(at(C11, 1), at(C11, 2)))
    solid(C11, "2")
    # the screen's I2C on the bottom (SDA = D9, SCL = D8) to its header (J10, at the module's right end)
    J10 = place(JANK, "OLED_0.91in_128x32_I2C_HS91L02W2C01", "J10", "OLED 0.91in 128x32", 62.3, 33.31, 180)
    o = {n: at(J10, n) for n in range(1, 5)}
    DBG.update(oled=o)
    route("SCL", [p[9], (54.8, p[9][1]), (54.8, o[3][1]), o[3]], SIG, BOT)
    route("SDA", [p[10], (55.5, p[10][1]), (55.5, o[4][1]), o[4]], SIG, BOT)
    R70 = place(K("Resistor_SMD"), R0805, "R70", "4.7k", 59.2, o[3][1], 0)
    R71 = place(K("Resistor_SMD"), R0805, "R71", "4.7k", 59.2, o[4][1], 0)
    route("SCL", [at(R70, 2), o[3]])
    route("SDA", [at(R71, 2), o[4]])
    route("+3V3", [(51.45, 31.5), (57.6, 31.5)], SIG, BOT)
    via("+3V3", 57.6, 31.5)
    route("+3V3", [(57.6, 31.5), (57.8, o[2][1]), o[2]])
    route("+3V3", [(57.8, o[2][1]), (57.8, o[4][1]), at(R71, 1)])
    route("+3V3", [(57.8, o[3][1]), at(R70, 1)])
    solid(J10, "1")

    # ======== the controls right of the screen (top): BACK, OK on a row, the encoder under them ========
    SW6 = place(K("Button_Switch_THT"), "SW_PUSH_6mm", "SW6", "BACK 9.5 mm", 65.9, 23.2, 0)
    SW1 = place(K("Button_Switch_THT"), "SW_PUSH_6mm", "SW1", "OK 9.5 mm", 75.5, 23.2, 0)
    SW7 = place(K("Rotary_Encoder"), "RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm", "SW7", "EC11E09244BS", 66.4, 34.0, 0)
    e = {n: at(SW7, n) for n in ("A", "B", "C", "S1", "S2")}
    DBG.update(enc=e)
    route("ENC_A", [(64.35, e["A"][1]), e["A"]])
    route("ENC_B", [(63.65, e["B"][1]), e["B"]])
    route("LADDER", [(82.0, 23.2), (65.9, 23.2)])
    route("LADDER", [(82.0, 23.2), (83.45, 24.65), (83.45, 32.9), e["S2"]])
    route("BACK_R", [(65.9, 27.7), (72.4, 27.7)])
    route("OK_R", [(75.5, 27.7), (82.0, 27.7)])
    R62 = place(K("Resistor_SMD"), R0805, "R62", "6.8k", 69.15, 25.45, 180, True)
    R61 = place(K("Resistor_SMD"), R0805, "R61", "2.2k", 78.75, 25.45, 180, True)
    R63 = place(K("Resistor_SMD"), R0805, "R63", "22k", 80.9, 41.8, 270, True)
    route("BACK_R", [(65.9, 27.7), (67.3, 26.3), at(R62, 1)], SIG, BOT)
    route("OK_R", [(75.5, 27.7), (76.9, 26.3), at(R61, 1)], SIG, BOT)
    route("PUSH_R", [e["S1"], at(R63, 1)], SIG, BOT)
    for fp_ in (R61, R62, R63):
        solid(fp_, "2")
    solid(SW7, "C")
    DBG.update(r61=(at(R61, 1), at(R61, 2)), r63=(at(R63, 1), at(R63, 2)))

    # ======== the power path (top): as in v4 ========
    D1 = place(K("Diode_SMD"), "D_SOD-123F", "D1", "SMF5.0A", 70.1, 12.7, 270)
    C1 = place(K("Capacitor_SMD"), C0805, "C1", "10uF", 72.45, 12.4, 270)
    R1 = place(K("Resistor_SMD"), R0805, "R1", "5.1k", 73.0, 4.0, 90)
    R2 = place(K("Resistor_SMD"), R0805, "R2", "5.1k", 71.0, 4.0, 90)
    Q1 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q1", "AO3401A", 75.5, 12.5, 180)
    R3 = place(K("Resistor_SMD"), R0805, "R3", "100k", 79.5, 12.3, 270)
    Q2 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q2", "AO3400A", 64.6, 15.2, 0)
    R4 = place(K("Resistor_SMD"), R0805, "R4", "100k", 63.7, 11.6, 270)
    R5 = place(K("Resistor_SMD"), R0805, "R5", "100k", 61.4, 16.3, 0, True)   # (bottom)
    C2 = place(K("Capacitor_SMD"), C0805, "C2", "100nF", 60.4, 18.5, 180)
    R6 = place(K("Resistor_SMD"), R0805, "R6", "1k", 58.55, 13.8, 270)
    U2 = place(K("Package_TO_SOT_SMD"), "SOT-363_SC-70-6", "U2", "LM66100", 75.3, 15.7, 0)
    C3 = place(K("Capacitor_SMD"), C0805, "C3", "10uF", 57.15, 4.7, 0)
    C4 = place(K("Capacitor_SMD"), "CP_Elec_5x5.4", "C4", "100uF DNP", 77.0, 7.4, 180, True)   # (bottom)
    TP1 = place(K("TestPoint"), "TestPoint_Pad_D1.5mm", "TP1", "5V", 57.0, 1.6)
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
    route("VBUS_IN", [a9, (a9[0], 9.8), (b9[0], 9.8), b9], PWR)
    NODE = 10.45
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
    route("NG", [(62.3, 14.0), (62.3, 18.5), at(C2, 1)])
    via("NG", 62.3, 16.3)
    route("NG", [(62.3, 16.3), at(R5, 1)], SIG, BOT)
    solid(R5, "2")
    solid(C2, "2")
    route("PWR_CUT", [p[11], (54.9, 11.9), (57.7, 11.9), at(R6, 1)])
    u = {n: at(U2, n) for n in range(1, 7)}
    route("+5V", [u[6], (77.3, u[6][1]), (77.3, u[3][1] + 1.0), (u[3][0], u[3][1] + 1.0), u[3]], SIG)
    via("+5V", 72.4, u[3][1] + 1.0)
    route("+5V", [(u[3][0], u[3][1] + 1.0), (72.4, u[3][1] + 1.0)], 0.4)
    route("+5V", [(72.4, u[3][1] + 1.0), (72.4, 4.6), (55.6, 4.6)], BUS, BOT)
    via("+5V", 55.6, 4.6)
    route("+5V", [(55.6, 4.6), (55.6, p[14][1]), (54.6, p[14][1])], PWR)
    route("+5V", [(55.6, p[14][1]), (55.6, 1.6), at(TP1, 1)], 0.3)
    route("+5V", [(55.6, at(C3, 1)[1]), at(C3, 1)], PWR)
    c4p = at(C4, 1)
    DBG.update(c4=(c4p, at(C4, 2)))
    route("+5V", [(72.4, c4p[1]), c4p], BUS, BOT)
    for fp_, n in ((C3, "2"), (J1, "SH"), (R5, "2"), (C1, "2")):
        solid(fp_, n)
    sh = at(J1, "SH", (JX - 4.3, 6.4))
    route("GND", [at(C3, 2), (sh[0] - 0.55, sh[1] - 0.8), sh])
    route("GND", [u[2], (73.4, u[2][1])])
    via("GND", 73.4, u[2][1])
    g = at(D1, 2)
    via("GND", g[0], g[1] + 1.1)
    route("GND", [g, (g[0], g[1] + 1.1)])
    route("GND", [at(Q2, 2), (64.9, 16.75)])
    via("GND", 64.9, 16.75)

    # ======== the MIDI IN (left, in front of OUT1): RING1 -> R50 -> U4's LED -> Q3 -> TIP1; D2 across the LED; Q4 ========
    U4 = place(K("Package_DIP"), "DIP-6_W7.62mm", "U4", "H11L1", 17.0, 26.6, 0)
    w = {n: at(U4, n) for n in (1, 2, 4, 5, 6)}
    Q4 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q4", "AO3401A", 8.0, 26.2, 0)
    R50 = place(K("Resistor_SMD"), R0805, "R50", "220", 12.3, 27.1, 90)
    D2 = place(K("Diode_SMD"), "D_SOD-123", "D2", "1N4148W", 14.6, 28.2, 270)
    Q3 = place(K("Package_TO_SOT_SMD"), "SOT-23", "Q3", "AO3400A", 21.0, 35.0, 0)
    R52 = place(K("Resistor_SMD"), R0805, "R52", "1k", 9.0, 30.6, 180, True)    # (bottom)
    R53 = place(K("Resistor_SMD"), R0805, "R53", "100k", 27.0, 25.6, 270)
    R51 = place(K("Resistor_SMD"), R0805, "R51", "4.7k", 27.5, 33.2, 90)
    C8 = place(K("Capacitor_SMD"), C0805, "C8", "100nF", 22.6, 28.0, 90, True)
    TP2 = place(K("TestPoint"), "TestPoint_Pad_D1.5mm", "TP2", "3V3", 6.8, 31.6)
    q4 = {n: at(Q4, n) for n in (1, 2, 3)}
    q3 = {n: at(Q3, n) for n in (1, 2, 3)}
    DBG.update(u4=w, q4=q4, q3=q3, d2=(at(D2, 1), at(D2, 2)), r50=(at(R50, 1), at(R50, 2)), r52=(at(R52, 1), at(R52, 2)),
               r53=(at(R53, 1), at(R53, 2)), r51=(at(R51, 1), at(R51, 2)), c8=(at(C8, 1), at(C8, 2)))
    # IN_EN (D4) and MIDI_RX (D5) on top west across the strip, then south
    route("IN_EN", [p[5], (28.6, p[5][1]), (28.6, 24.0), (q4[1][0], 24.0), q4[1]])
    route("IN_EN", [(at(R53, 1)[0], 24.0), at(R53, 1)])
    route("MIDI_RX", [p[6], (29.4, p[6][1]), (29.4, w[4][1]), w[4]])
    route("MIDI_RX", [(at(R51, 2)[0], w[4][1]), at(R51, 2)])
    solid(R53, "2")
    # SRC1 from the 33 ohm group's A pad (bottom) to Q4's drain; RING1 from its B pad (bottom) to R50
    route("SRC1", [tb1["A"], (10.0, 23.6), (10.0, 26.2)], SIG, BOT)
    via("SRC1", 10.0, 26.2)
    route("SRC1", [(10.0, 26.2), q4[3]])
    route("RING1", [tb1["B"], (10.8, 19.9), (10.8, at(R50, 1)[1])], SIG, BOT)
    via("RING1", 10.8, at(R50, 1)[1])
    route("RING1", [(10.8, at(R50, 1)[1]), at(R50, 1)])
    route("OPTO_A", [at(R50, 2), at(D2, 1), w[1]])
    route("OPTO_K", [at(D2, 2), w[2]])
    route("OPTO_K", [q3[3], (22.6, 31.0), (18.4, 29.6), w[2]])
    # TIP1 from the 10 ohm group's B pad (bottom, west of the TX1 pad) to Q3's source
    route("TIP1", [ta1["B"], (15.0, 20.8), (15.0, 36.0), (q3[2][0] - 1.0, 36.0)], SIG, BOT)
    via("TIP1", q3[2][0] - 1.0, 36.0)
    route("TIP1", [(q3[2][0] - 1.0, 36.0), q3[2]])
    # IN_G: R52 (IN_EN -> Q3's gate)
    route("IN_EN", [(q4[1][0], 24.0), (5.4, 24.6)])
    via("IN_EN", 5.4, 24.6)
    route("IN_EN", [(5.4, 24.6), (5.4, 30.6), at(R52, 1)], SIG, BOT)
    route("IN_G", [at(R52, 2), (11.2, 30.6), (11.2, 34.0)], SIG, BOT)
    via("IN_G", 11.2, 34.0)
    route("IN_G", [(11.2, 34.0), (q3[1][0], 34.0), q3[1]])
    # 3V3: a riser from the bus up the left edge to Q4's source and TP2; on the bottom up to U4 and C8; R51
    route("+3V3", [(1.5, BUS_Y), (1.5, 29.2), (4.6, 29.2), (q4[2][0], 28.0), q4[2]])
    route("+3V3", [(4.6, 29.2), at(TP2, 1)])
    route("+3V3", [(27.5, BUS_Y), at(R51, 1)])
    via("+3V3", 27.5, 35.4)
    route("+3V3", [(27.5, 35.4), (26.8, 34.7), (26.8, 27.3), w[6]], SIG, BOT)
    route("+3V3", [w[6], at(C8, 1)], SIG, BOT)
    solid(C8, "2")
    solid(U4, "5")

    # ======== the front: OUT2, OUT3, OUT4; TXB along the bottom lane, the 3V3 bus on top ========
    tx = {k: front_unit(k, c, H, TX_LANE) for k, c in zip((2, 3, 4), CH)}
    route("MIDI_TXB", [p[2], (35.85, p[2][1]), (35.85, TX_LANE)], SIG, BOT)
    route("MIDI_TXB", [(tx[2][0], TX_LANE), (tx[4][0], TX_LANE)], SIG, BOT)
    via("+3V3", 51.45, 35.6)
    route("+3V3", [(51.45, 35.6), (51.45, BUS_Y)])
    route("+3V3", [(51.45, BUS_Y), (1.5, BUS_Y)], BUS)

    # ======== silk ========
    label("OUT1 A|B", s[2][0] - 2.3, s[2][1], 0.7, rot=90)
    label("JP=SRC!", 9.55, 23.3, 0.6)
    label("MIDI IN: OUT1", 21.0, 23.3, 0.6)
    label("jank v5", 74.0, 52.0, 1.0)
    label("5V", 58.4, 2.7, 0.6)
    label("3V3", 9.0, 32.4, 0.6)
    label("GND", 29.0, 2.8, 0.6)
    label("TX", 31.4, 11.0, 0.6)
    label("PWR", JX, 1.2, 0.6)
    label("BACK", 69.15, 21.3, 0.7)
    label("OK", 78.75, 21.3, 0.7)
    label("jank v5", 74.0, 52.0, 1.0, pcbnew.B_SilkS)
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
DEBUG_OUT = os.environ.get("JANK_DEBUG")
if DEBUG_OUT:
    json.dump(DBG, open(DEBUG_OUT, "w"), indent=0, default=str)
