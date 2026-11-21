# SPDX-License-Identifier: GPL-3.0-only
"""jank LCD-2 add-on: the board (lcd2.kicad_pcb), arrangement A (worktrees/lcd2-addon-scratch/arr-A.png): 100 x 64 mm,
2 layers. The LCD-2 lies in landscape on the left, its USB-C at the left wall; J15 / J16 take its rows (30.48 mm apart,
Waveshare's 2D drawing); 4 M2 holes for its screws; the slot for the cell's lead in front of its J4. USB IN / OUT on
the left wall, the jacks on the top wall, the DINs (top side: JLC's Economic tier is single-sided, THT included) on the
bottom wall, the buttons on the right. Fixed parts from PLACE; the rest packed near their block's anchor (GROUP).
Then route.py routes it (Freerouting).   KiCad's python: make_board.py"""
import math
import os
import sys

import pcbnew

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import parts as P

MM = pcbnew.FromMM
LIB = os.path.join(HERE, "lib", "chip.pretty")
NAME = "lcd2"
OX = OY = 100.0
CELL = os.environ.get("LCD2_CELL", "floor")    # "floor": jank-mini-3 (mots, 2026-11-18; see MINI3 below); older
                                               # studies: "under", "top", "edge"
MINI3 = CELL == "floor"
W = float(os.environ.get("LCD2_W", 93.0))
H = float(os.environ.get("LCD2_H", 74.0 if MINI3 else 62.6))   # (mini-3: 74 = 72 for the TRS jacks + 2 so it routes, mots "a few mm"; LCD-2 centred)

# the LCD-2, landscape, its USB-C at the left wall: front coordinates (fx across its 35.01, fy along its 48.21 with the
# USB-C at fy = 48.21) -> board (LX + 48.21 - fy, LY + fx). Its P1 row at fx = 32.745, P2 at fx = 2.265 (seen from the
# screen side; Waveshare's pictures show the back), pin 1 at fy = 7.60, pitch 2.54.
LX, LY = 0.5, float(os.environ.get("LCD2_LY", (H - 35.01) / 2))   # (mots, 2026-11-18: the LCD-2 centred vertically, its USB-C at the left wall)


def lcd(fx, fy):
    return LX + 48.21 - fy, LY + fx


ROW_X = LX + 48.21 - (7.60 + 40.62) / 2         # the sockets' centre x (pin 1 at +16.51 with rotation 180)
HOLES = [lcd(fx, fy) for fx, fy in ((3.0, 3.0), (32.01, 3.0), (3.0, 45.2), (32.01, 45.2))]
HOLE_D, HOLE_KEEP = 2.2, 2.6
MOUNT = ([(W - 4.0, H - 4.0), (52.5, H - 4.0)] if CELL == "under"
         else [(W - 3.5, 30.5), (51.5, 20.0)])   # the board's own M2 holes to the case (the right part; the LCD-2's
                                               #  four screws hold the left part through the case floor)
# (mots, 2026-11-18: the cell lies in the ~11 mm gap between this board and the LCD-2's back, between its socket rows;
#  its lead plugs straight into the LCD-2's J4 in that gap: no slot through this board any more.) No parts there.
DPX = 49.0 + (W - 13.0 - 49.0) / 2           # the D-pad's centre x: between the LCD-2 and the right-wall jacks
# CELL "under": one flat cell in the gap under the LCD-2 (no parts there); "top": mots's 6 x 17 x 60 cells, 1-3
# stacked, on top along the bottom wall; "edge": 2 cells on their long edge along the bottom wall
if CELL == "under":
    CELL_KEEP = [(LX, LY, LX + 48.21, LY + 35.01)]
else:   # the stack's 17.5 x 60.5 pad (right of LINE IN) + a strip for the 3 PH2.0 sockets at its left end
    CELL_KEEP = [(W - 61.0, H - 18.0, W - 0.5, H - 0.5), (W - 23.0, H - 24.0, W - 0.5, H - 18.0)]
if CELL == "edge":   # (mots, 2026-11-18) 2 cells standing on their long edge along the bottom wall: a 12.6 x 60.6 pad
    CELL_KEEP = [(W - 61.1, H - 13.1, W - 0.5, H - 0.5)]
LCD_RECT = (LX - 0.5, LY - 0.5, LX + 48.21 + 0.5, LY + 35.01 + 0.5)
TALL = {"J18", "J19"}   # (PH2.0 sockets, 8 mm + the plug: never under the LCD-2)

# fixed parts: ref -> (x, y, facing) with facing = the wall a connector's mouth points at ('-x', '-y', '+y'), or a
# rotation in degrees for the others
PLACE = {
    "J15": (ROW_X, LY + 32.745, 180), "J16": (ROW_X, LY + 2.265, 180),
    # (mots, 2026-11-18, by wall) LEFT: our two USB-C, OUT above and IN below the LCD-2's own USB-C
    # (edge: mots: both under the LCD-2, beside its own USB-C (LY + 17.5), between its screws; its USB-C hangs to 7 mm
    #  over our board, our receptacles are 3.2 mm tall)
    "J4": (None, 6.5 if not CELL == "edge" else LY + 12.0, "-x"), "J1": (None, H - 6.5 if not CELL == "edge" else LY + 23.0, "-x"),
    # TOP = MIDI: the two TRS MIDI jacks above the LCD-2, both DINs on the right part
    "J2": (29.8, None, "-y"), "J3": (42.6, None, "-y"),
    "J5": (W - 33.0, None, "-y"), "J6": (W - 10.9, None, "-y"),
    # RIGHT = modular: CLOCK, RESET
    "J13": (None, H / 2 - 2.0 if CELL == "under" else 24.0, "+x"), "J14": (None, H / 2 + 12.0 if CELL == "under" else 37.0, "+x"),
    # BOTTOM = audio: LINE IN, below the LCD-2's P1 row (its pins 8 / 9 are LINE IN / SENSE)
    "J7": (27.0 if CELL == "under" else 24.5, None, "+y") if not CELL == "edge" else (None, H - 6.9, "-x"),   # (edge: LINE IN on the left wall, below the LCD-2)
    # the D-pad, centred vertically in the right part, between the LCD-2 and CLOCK / RESET
    "SW3": (DPX, H / 2 - 9.0, 0), "SW4": (DPX, H / 2 + 9.0, 0), "SW1": (DPX - 9.0, H / 2, 0), "SW5": (DPX + 9.0, H / 2, 0),
    "U18": (19.0, 10.0, 0),
    "L5": (19.0, 7.4, 180),          # (the boost's switch loop: L5 right over U18, its pads drawn to SW and VIN: DRAWN)
}
# the rest: packed near an anchor (x, y); the order sets who is placed first (the parts that matter most nearest)
GROUP = [
    *([((23.0, H - 6.5), ["J18", "J19"])] if CELL == "edge" else []),   # (the cells' sockets first: the bottom-left corner)
    ((21.8, 10.0), ["C104"]),                                                    # U18's input cap at VIN / GND
    ((19.0, 10.0), ["C99", "C100", "C101", "R134", "R135", "R154", "R136", "R131", "JP10", "Q1", "R50",
                    "U19", "R145", "R138", "R139", "RT139", "JP9", "JP8", "R123"]),   # the 5 V aux rail, by P2.14
    ((11.0, 9.0) if not CELL == "edge" else (12.0, LY + 12.0), ["U5", "C28", "R93", "D16", "R160", "R161", "JP13"]),         # USB OUT (top-left) and its ESD
    ((12.0, 56.0) if not CELL == "edge" else (14.0, LY + 30.0), ["D3", "R157", "C27", "R151", "D17", "R158"]),                # VBUS_B, by P1.14 (bottom-left)
    ((14.0, 12.0), ["R88", "R89", "C29", "C60", "C61"]),                         # by P2.12 / P2.1
    ((W - 20.0, 37.0), ["U21", "C105", "R71", "R77", "R91", "D10", "D11", "R75", "R78", "R81", "R84", "C89",
                        "C90", "R120", "RT120", "R121", "RT121"]),               # CLOCK / RESET
    ((52.0, 20.0), ["U11", "R14", "R15", "RT15", "Q10", "R21", "R20", "RT20", "Q12", "R124", "R125",
                    "R126", "R128"]),                                            # MIDI TRS jacks
    ((W - 22.0, 20.0), ["U12", "R54", "R55", "RT55", "Q11", "R56", "R57", "RT57", "Q13", "R127", "R129"]),   # DINs
    ((27.0, 55.0) if not CELL == "edge" else (16.0, LY + 30.0), ["D7", "D14", "R36", "RT36", "R37", "RT37", "R38", "RT38", "R39", "RT39"]),            # LINE IN jack
    ((18.0, 56.0) if not CELL == "edge" else (24.0, LY + 26.0), ["R30", "C30", "R31", "C31", "R32", "C32", "R65", "C65", "R66", "C66", "R67", "C67",
                    "R33", "RT33", "R34", "RT34", "R153", "C33"]),               # LINE IN network, by P1.8 / P1.9
    ((DPX, H / 2), ["C80", "C81", "C82", "C83"]),                                 # button pads
    ((45.0, 57.0) if not CELL == "edge" else (60.0, 45.0), ["J17"]),
((26.0, LY + 31.0), ["R162", "R163", "U23", "U22", "R164", "C106", "R165"]),   # the cells' sockets + protector                                                     # spare pads
]
GAP = {r: 0.9 for r in ("R30", "C30", "R31", "C31", "R32", "C32", "R65", "C65", "R66", "C66", "R67", "C67", "R33",
                         "RT33", "R34", "RT34", "R153", "C33")}   # (the LINE IN network: room for its own tracks)
GAP.update({"R162": 0.6, "R163": 0.6})   # (the two 2512s: their silk outlines touched at 0.4)
GAP.update({"R164": 1.0})   # (jank-mini-3: VBAT's 1 mm track must reach its pad 1; packed tight it could not)
BOTTOM = set()   # (mots, 2026-11-18, option 1: every part on top, Economic single-sided, the lid raised over the DINs)     # mm: parts under the LCD-2 must stay lower (its back parts hang ~3.5 mm into the 11 mm gap)
CUT = None       # the LCD-2's cut-out (jank-mini-3 only)
DPAD_KEEP = []   # the D-pad cross where no part may sit (jank-mini-3 only)

# ---- jank-mini-3 (mots, 2026-11-18): the LCD-2 drops into a cut-out, its PCB level with ours. Two bridge strips (small
# PCBs, 2 x 1x14 female sockets 3.81 mm apart) join its pins to our two 1x14 male rows, mounted under the board 3.81 mm
# outside its rows. No screws: 4 pegs under the lid press its corners down. The 2 cells (6 x 17 x 60) lie flat on the
# case floor under it. Under the board, hand-soldered by mots: DINs, jacks, the male rows, the cell sockets
# (side-entry PH2.0). On top, by JLC: the USB-Cs and every SMD part. Walls: MIDI (DINs + TRS) top, our USB-Cs left
# above / below the LCD-2's own, CLOCK / RESET right, LINE IN bottom.
if MINI3:
    D_ROW = 3.81                                   # our row -> the LCD-2's row (the bridge strip's socket spacing)
    CUT = (LX - 0.3, LY - 0.3, LX + 48.21 + 0.3, LY + 35.01 + 0.3)
    HOLES = []                                     # (its 4 M2 holes are in the cut-out: the lid pegs press there)
    MOUNT = [(W - 4.0, H / 2), (51.5, H - 4.0)]    # (our board to the case: right wall between CLOCK / RESET, bottom)
    CELL_KEEP = []                                 # (the cells are on the case floor, not on this board)
    LCD_RECT = CUT
    TALL = set()
    BOTTOM = {"J2", "J3", "J5", "J6", "J7", "J13", "J14", "J15", "J16", "J18", "J19"}
    DPX = 49.0 + (W - 15.0 - 49.0) / 2
    # (mots, 2026-11-18: NO part where the D-pad cap rests) the 24 mm cross, 8 mm arms, + 1 mm: only SW1/3/4/5 there,
    # on top; no through-hole joint from below either (check_board.py checks both)
    DPAD_KEEP = [(DPX - 13.0, H / 2 - 5.0, DPX + 13.0, H / 2 + 5.0), (DPX - 5.0, H / 2 - 13.0, DPX + 5.0, H / 2 + 13.0)]
    PLACE = {
        # our male rows under the board (pin k across from the LCD-2's pin k: main() checks pin 1's x)
        "J15": (ROW_X, LY + 32.745 + D_ROW, 180), "J16": (ROW_X, LY + 2.265 - D_ROW, 180),
        # TOP wall, left part (mots, 2026-11-18: the two USB-C side by side, away from LINE IN): J1 (IN, data only) then J4
        # (OUT + its host circuit), left of the TRS jacks, clear of the upper strip; the LCD-2's own USB-C stays at the left wall
        "J1": (6.0, None, "-y"), "J4": (17.5, None, "-y"),
        # TOP = MIDI: the TRS jacks above the LCD-2 (clear of the upper strip), the DINs on the right part
        "J2": (29.8, None, "-y"), "J3": (42.6, None, "-y"), "J5": (W - 33.0, None, "-y"), "J6": (W - 10.9, None, "-y"),
        # RIGHT = modular; BOTTOM = audio: LINE IN right of the two USB-C, under its LCD-2 pins P1.8-P1.10 (mots, 2026-11-18)
        "J13": (None, H / 2 - 10.0, "+x"), "J14": (None, H / 2 + 10.0, "+x"),
        "J7": (30.0, None, "+y"),
        # the D-pad between the LCD-2 and CLOCK / RESET: one 24 mm cross cap (8 mm arms) rocking on 4 switches at +/-8 (mots)
        "SW3": (DPX, H / 2 - 8.0, 0), "SW4": (DPX, H / 2 + 8.0, 0), "SW1": (DPX - 8.0, H / 2, 0), "SW5": (DPX + 8.0, H / 2, 0),
        # the cells' sockets under the board below the D-pad, mouths toward the cells' end (x 61)
        "J18": (DPX - 2.0, H / 2 + 16.0, 270), "J19": (DPX - 2.0, H / 2 + 23.2, 270),
        "U18": (W - 8.0, H - 14.0, 0),   # (the 5 V boost in the bottom-right corner: the top-left band was jammed)
        "L5": (W - 8.0, H - 16.6, 180),
    }
    GROUP = [
        ((W - 5.2, H - 14.0), ["C104"]),
        ((W - 10.0, H - 11.0), ["C99", "C100", "C101", "R134", "R135", "R154", "R136", "R131", "JP10", "Q1", "R50",
                        "U19", "R145", "R138", "R139", "RT139", "JP9", "JP8", "R123"]),     # the 5 V aux rail, VBAT from the protector
        ((18.0, 11.0), ["U5", "C28", "R93", "D16", "R160", "R161", "JP13"]),                # USB OUT (J4) and its ESD
        ((W - 18.0, H - 6.0), ["D3", "R157", "C27", "R151", "D17", "R158"]),                       # VBUS_B -> VIN_B, by Q1 (VBUS_B along the bottom band)
        ((40.0, 14.0), ["C60", "C61"]),                                                    # 3V3 decoupling, by P2.1 (x 41)
        ((10.0, H - 12.0), ["R88", "R89", "C29"]),                                         # VBUS sense divider, by P1.14 (VBUS_B)
        ((W - 22.0, H / 2), ["U21", "C105", "R71", "R77", "R91", "D10", "D11", "R75", "R78", "R81", "R84", "C89",
                             "C90", "R120", "RT120", "R121", "RT121"]),                    # CLOCK / RESET
        ((54.0, 21.0), ["U11", "R14", "R15", "RT15", "Q10", "R21", "R20", "RT20", "Q12", "R124", "R125",
                       "R126", "R128"]),                                                   # MIDI TRS jacks
        ((W - 22.0, 20.0), ["U12", "R54", "R55", "RT55", "Q11", "R56", "R57", "RT57", "Q13", "R127", "R129"]),   # DINs
        ((38.0, H - 15.0), ["D7", "D14", "R36", "RT36", "R37", "RT37", "R38", "RT38", "R39", "RT39"]),           # LINE IN jack
        ((24.0, H - 15.0), ["R30", "C30", "R31", "C31", "R32", "C32", "R65", "C65", "R66", "C66", "R67", "C67",
                            "R33", "RT33", "R34", "RT34", "R153", "C33"]),                 # LINE IN network, by P1.8 / P1.9
        ((DPX + 11.0, H / 2 + 11.0), ["C80", "C81", "C82", "C83"]),                        # button pads (outside the D-pad cross)
        ((53.0, H - 14.0), ["J17"]),                                                       # spare pads by P1.1 / P1.2
        ((30.0, 12.0), ["J20"]),                                                         # spare pads by P2.5 / P2.6
        ((58.0, H / 2 + 14.0), ["R162", "R163", "U23", "U22", "R164", "C106", "R165"]),   # the cells' protector, by J18 / J19 (VBAT: top row -> right, no crossing)
    ]
    # layout options for the joint placement + pin-map search (tools/layout_opt.sh); the defaults are the chosen layout
    def _anchor(first, xy):
        for i, (a, refs) in enumerate(GROUP):
            if refs[0] == first:
                GROUP[i] = (xy, refs)
    if os.environ.get("M3_USB", "top") == "bottom":     # (both USB-C on the bottom wall, left part)
        PLACE["J1"], PLACE["J4"] = (6.0, None, "+y"), (17.5, None, "+y")
        _anchor("U5", (18.0, H - 13.0))
    if float(os.environ.get("M3_LINE_X", 30.0)) > 50:  # (LINE IN on the bottom wall, right part)
        PLACE["J7"] = (70.0, None, "+y")
        _anchor("D7", (68.0, H - 12.0))
        _anchor("R30", (24.0, H - 10.0))
    sw = os.environ.get("M3_SWAP", "")                 # T: the TRS pair, D: the DIN pair, M: CLOCK / RESET
    if "T" in sw:
        PLACE["J2"], PLACE["J3"] = (42.6, None, "-y"), (29.8, None, "-y")
    if "D" in sw:
        PLACE["J5"], PLACE["J6"] = (W - 10.9, None, "-y"), (W - 33.0, None, "-y")
    if "M" in sw:
        PLACE["J13"], PLACE["J14"] = (None, H / 2 + 10.0, "+x"), (None, H / 2 - 10.0, "+x")


def _unconnected():
    """{(ref, pin): 'unconnected-(...)'} from the schematic's netlist (make_fab.sh exports it first)"""
    import xml.etree.ElementTree as ET
    path = os.path.join(HERE, "fab", f"{NAME}.net.xml")
    out = {}
    if os.path.exists(path):
        for n in ET.parse(path).getroot().iter("net"):
            if n.get("name").startswith("unconnected-"):
                for node in n.iter("node"):
                    out[(node.get("ref"), node.get("pin"))] = n.get("name")
    return out


UNCONNECTED = _unconnected()


def raw_net(board, name):
    n = board.FindNet(name)
    if n is None:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
    return n


def net(board, name):
    name = "/" + name.lstrip("/")          # (the schematic's local-label nets are "/NAME": DRC parity)
    n = board.FindNet(name)
    if n is None:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
    return n


def load(ref):
    part = next(p for p in P.PARTS if p[0] == ref)
    fp = pcbnew.FootprintLoad(LIB, P.FP[part[3]])
    if fp is None:
        sys.exit(f"no footprint {P.FP[part[3]]}")
    fp.SetReference(ref)
    fp.SetValue(part[2])
    fp.SetFPID(pcbnew.LIB_ID("chip", P.FP[part[3]]))
    return fp, part


def courtyard(fp, pads=True):
    """the courtyard's box, grown to take in every pad (the USB-A's shell pads lie outside JLC's courtyard)"""
    c = fp.GetCourtyard(pcbnew.B_CrtYd if fp.IsFlipped() else pcbnew.F_CrtYd)
    bb = c.BBox() if c.OutlineCount() else fp.GetBoundingBox(False)
    x0, y0, x1, y1 = (pcbnew.ToMM(bb.GetLeft()) - OX, pcbnew.ToMM(bb.GetTop()) - OY,
                      pcbnew.ToMM(bb.GetRight()) - OX, pcbnew.ToMM(bb.GetBottom()) - OY)
    if pads:
        for p in fp.Pads():
            pb = p.GetBoundingBox()
            x0, y0 = min(x0, pcbnew.ToMM(pb.GetLeft()) - OX - 0.2), min(y0, pcbnew.ToMM(pb.GetTop()) - OY - 0.2)
            x1, y1 = max(x1, pcbnew.ToMM(pb.GetRight()) - OX + 0.2), max(y1, pcbnew.ToMM(pb.GetBottom()) - OY + 0.2)
    return x0, y0, x1, y1


def put(fp, x, y, rot):
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))


# each connector's mouth, as a pad on its mouth side (sign +1) or the opposite side (-1), seen from the centre of its
# numbered pads. From the datasheets: DIN-504B (HOOYA): the front face 2.5 mm past the shell pins 6 / 7; PJ-307C: the
# nose on pin 1's side; PJ-320A: the nose on pad B's side; USB-C: the mouth away from its signal pads (A6 ...).
# (Was: the courtyard's longest reach; a DIN is wider (21.3) than deep (16.6), so on the right wall a SIDE faced it.)
MOUTH = {"CONN-TH_DIN-504B-M10": ("6", 1), "AUDIO-TH_PJ307C": ("1", 1), "AUDIO-TH_PJ-320A-4P-DIP": ("B", 1),
         "USB-C-SMD_TYPE-C-16PIN-2MD-073": ("A6", -1)}
WALL_DIR = {"-x": (-1, 0), "+x": (1, 0), "-y": (0, -1), "+y": (0, 1)}


def mouth_dir(fp):
    name = str(fp.GetFPID().GetLibItemName())
    ref, sign = MOUTH[name]
    pads = [p for p in fp.Pads() if p.GetNumber()]
    cx = sum(p.GetPosition().x for p in pads) / len(pads)
    cy = sum(p.GetPosition().y for p in pads) / len(pads)
    q = next(p for p in pads if p.GetNumber() == ref).GetPosition()
    return sign * (q.x - cx), sign * (q.y - cy)


def face(fp, wall, x, y, bottom=False):
    """turn a connector so its mouth (MOUTH) points at `wall`, then slide it so the courtyard's front edge sits 0.3 mm
    past that wall"""
    if bottom and not fp.IsFlipped():
        fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_TOP_BOTTOM)
    best = None
    wx, wy = WALL_DIR[wall]
    for rot in (0, 90, 180, 270):
        put(fp, 0, 0, rot)
        mx, my = mouth_dir(fp)
        score = (mx * wx + my * wy) / max(math.hypot(mx, my), 1)
        if best is None or score > best[1]:
            best = (rot, score)
    rot = best[0]
    put(fp, 0, 0, rot)
    x0, y0, x1, y1 = courtyard(fp, pads=False)
    if wall == "-x":
        x = -0.3 - x0
    elif wall == "-y":
        y = -0.3 - y0
    elif wall == "+y":
        y = H + 0.3 - y1
    elif wall == "+x":
        x = W + 0.3 - x1
    put(fp, x, y, rot)


def match_lcd_pins(fp):
    """a flipped male row: turn it so its pin 1 sits at the LCD-2's pin 1 x (the bridge joins pin k straight across)"""
    want = LX + 48.21 - 7.60
    for _ in range(2):
        p1 = next(p for p in fp.Pads() if p.GetNumber() == "1").GetPosition()
        if abs(pcbnew.ToMM(p1.x) - OX - want) < 0.05:
            return
        fp.SetOrientationDegrees(fp.GetOrientationDegrees() + 180)
    sys.exit(f"{fp.GetReference()}: pin 1 not at x {want:.2f}")


def _hit(b, o):
    return b[0] < o[2] and b[2] > o[0] and b[1] < o[3] and b[3] > o[1]


class Packer:
    def __init__(self):
        self.boxes = []          # (x0, y0, x1, y1, ref)
        self.keep = [(*k, "cell") for k in CELL_KEEP]
        self.keep += [(hx - 3.0, hy - 3.0, hx + 3.0, hy + 3.0, "hole") for hx, hy in HOLES + MOUNT]
        if CUT:
            self.keep.append((*CUT, "cut-out"))
        self.keep += [(*k, "d-pad") for k in DPAD_KEEP]

    def free(self, b, gap=0.25):
        if b[0] < 0.6 or b[1] < 0.6 or b[2] > W - 0.6 or b[3] > H - 0.6:
            return False
        for o in self.boxes + self.keep:
            if b[0] < o[2] + gap and b[2] > o[0] - gap and b[1] < o[3] + gap and b[3] > o[1] - gap:
                return False
        return True

    def add(self, fp, ref):
        b = courtyard(fp)
        self.boxes.append((*b, ref))

    def pack(self, fp, ref, ax, ay, gap=0.25):
        home = region(ax, ay)
        for r in range(0, 400):
            rad = r * 0.25
            steps = max(1, int(2 * math.pi * rad / 0.5))
            for k in range(steps):
                a = 2 * math.pi * k / steps
                x, y = ax + rad * math.cos(a), ay + rad * math.sin(a)
                for rot in (0, 90):
                    put(fp, round(x * 4) / 4, round(y * 4) / 4, rot)
                    b = courtyard(fp)
                    if (self.free(b, gap) and not (ref in TALL and _hit(b, LCD_RECT))
                            and region((b[0] + b[2]) / 2, (b[1] + b[3]) / 2) == home):
                        self.add(fp, ref)
                        return
        sys.exit(f"no room for {ref} near {ax}, {ay} (in its region {home})")


def region(x, y):
    """jank-mini-3: the top band above the cut-out, the bottom band below it, or the right part (a packed part stays in
    its anchor's region: crossing between the bands means going round the LCD-2)"""
    if not CUT:
        return "all"
    if x > CUT[2]:
        return "right"
    return "top" if y < (CUT[1] + CUT[3]) / 2 else "bottom"


# tracks drawn before routing (locked: Freerouting keeps them): (net, (ref, pad), (ref, pad), width mm)
# (V5_SW: out to the left of U18's pin column, 1.15 mm off its SW pad, then in from the side: pin 4 (GND) above it)
DRAWN = [("V5_SW", ("L5", "2"), ("U18", "5"), 0.3, -1.15), ("VIN_B", ("L5", "1"), ("U18", "3"), 0.3, None)]


def draw(board, fps):
    for name, (r0, p0), (r1, p1), w, side in DRAWN:
        a = next(p for p in fps[r0].Pads() if p.GetNumber() == p0).GetPosition()
        b = next(p for p in fps[r1].Pads() if p.GetNumber() == p1).GetPosition()
        if side is None:
            pts = [a, pcbnew.VECTOR2I(a.x, b.y), b]  # (one bend: down from the inductor, across to the pin)
        else:
            x = b.x + MM(side)
            pts = [a, pcbnew.VECTOR2I(x, a.y), pcbnew.VECTOR2I(x, b.y), b]
        for s0, s1 in zip(pts, pts[1:]):
            if s0 == s1:
                continue
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(s0)
            t.SetEnd(s1)
            t.SetWidth(MM(w))
            t.SetLayer(pcbnew.F_Cu)
            t.SetNet(net(board, name))
            t.SetLocked(True)
            board.Add(t)


def outline(board):
    def seg(x0, y0, x1, y1):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pcbnew.VECTOR2I(MM(OX + x0), MM(OY + y0)))
        s.SetEnd(pcbnew.VECTOR2I(MM(OX + x1), MM(OY + y1)))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(MM(0.1))
        board.Add(s)
    for (a, b) in (((0, 0), (W, 0)), ((W, 0), (W, H)), ((W, H), (0, H)), ((0, H), (0, 0))):
        seg(*a, *b)
    if CUT:   # (jank-mini-3: the LCD-2's cut-out)
        x0, y0, x1, y1 = CUT
        for (a, b) in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
            seg(*a, *b)


def hole(board, i, x, y):
    fp = pcbnew.FootprintLoad(LIB, "HOLE_M2_2.2")
    fp.SetReference(f"H{i}")
    fp.SetFPID(pcbnew.LIB_ID("chip", "HOLE_M2_2.2"))   # (an empty name broke KiCad's Specctra session import)
    board.Add(fp)
    fp.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))


def keepout(board, pts, name):
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetDoNotAllowTracks(True)
    z.SetDoNotAllowVias(True)
    z.SetDoNotAllowZoneFills(True)
    z.SetDoNotAllowPads(False)
    z.SetDoNotAllowFootprints(False)
    z.SetLayerSet(pcbnew.LSET.AllCuMask())
    z.SetZoneName(name)
    ol = z.Outline()
    ol.NewOutline()
    for x, y in pts:
        ol.Append(MM(OX + x), MM(OY + y))
    board.Add(z)


def circle_pts(x, y, r, n=16):
    return [(x + r * math.cos(2 * math.pi * k / n), y + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def pours(board):
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(net(board, "GND"))
        z.SetZoneName(f"GND {'top' if layer == pcbnew.F_Cu else 'bottom'}")
        z.SetLocalClearance(MM(0.25))
        z.SetMinThickness(MM(0.2))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)   # (solid: JLC solders the THT parts too; thin spokes starved)
        z.SetThermalReliefGap(MM(0.25))
        z.SetThermalReliefSpokeWidth(MM(0.3))
        ol = z.Outline()
        ol.NewOutline()
        for x, y in ((0.3, 0.3), (W - 0.3, 0.3), (W - 0.3, H - 0.3), (0.3, H - 0.3)):
            ol.Append(MM(OX + x), MM(OY + y))
        board.Add(z)


def main():
    board = pcbnew.BOARD()
    board.SetFileName(os.environ.get("BOARD", os.path.join(HERE, f"{NAME}.kicad_pcb")))   # (make_route.sh: a try's file)
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    outline(board)
    packer = Packer()
    fps = {}
    # fixed parts
    for ref, (x, y, f) in PLACE.items():
        fp, part = load(ref)
        board.Add(fp)
        if isinstance(f, str):
            face(fp, f, x if x is not None else 0, y if y is not None else 0, bottom=ref in BOTTOM)
        else:
            put(fp, x, y, f)
            if ref in BOTTOM:
                fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_TOP_BOTTOM)
                if ref in ("J15", "J16"):
                    match_lcd_pins(fp)
        fps[ref] = fp
        if ref in BOTTOM:          # (a bottom part blocks the top only with its through-hole pads)
            for p in fp.Pads():
                pb = p.GetBoundingBox()
                packer.boxes.append((pcbnew.ToMM(pb.GetLeft()) - OX - 0.3, pcbnew.ToMM(pb.GetTop()) - OY - 0.3,
                                     pcbnew.ToMM(pb.GetRight()) - OX + 0.3, pcbnew.ToMM(pb.GetBottom()) - OY + 0.3, ref))
        else:
            packer.add(fp, ref)
    # the LCD-2's socket bodies are in the packer through their courtyards; the area under the LCD-2 is open to low parts
    placed = set(fps)
    for (ax, ay), refs in GROUP:
        for ref in refs:
            fp, part = load(ref)
            board.Add(fp)
            packer.pack(fp, ref, ax, ay, gap=GAP.get(ref, 0.4))
            fps[ref] = fp
            placed.add(ref)
    # twins (and anything not named): beside their resistor
    for part in P.PARTS:
        ref = part[0]
        if ref in placed:
            continue
        fp, _ = load(ref)
        board.Add(fp)
        main = P.TWINS.get(ref, (None,))[0]
        if main in fps:
            m = fps[main].GetPosition()
            ax, ay = pcbnew.ToMM(m.x) - OX, pcbnew.ToMM(m.y) - OY
        else:
            ax, ay = W / 2, H / 2
            print(f"note: {ref} packed in the middle (no anchor)")
        packer.pack(fp, ref, ax, ay)
        fps[ref] = fp
        placed.add(ref)
    # nets and DNP flags
    for ref, kind, value, fpn, lcsc, pins in P.PARTS:
        fp = fps[ref]
        for pad in fp.Pads():
            n = pins.get(pad.GetNumber())
            if n:
                pad.SetNet(net(board, n))
            elif (ref, pad.GetNumber()) in UNCONNECTED:   # (the schematic's own name for an open pin: DRC parity)
                pad.SetNet(raw_net(board, UNCONNECTED[(ref, pad.GetNumber())]))
        fp.SetField("LCSC", lcsc)                 # (DRC parity: the symbols carry it, even empty)
        fld = fp.GetField("LCSC")
        if fld is not None:
            fld.SetVisible(False)
        fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)
        if ref in P.DNP:
            fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_DNP | pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
    draw(board, fps)
    for i, (hx, hy) in enumerate(HOLES + MOUNT, 1):
        hole(board, i, hx, hy)
        keepout(board, circle_pts(hx, hy, HOLE_KEEP), f"hole {i}")
    e = 0.45
    for pts in (((0, 0), (W, 0), (W, e), (0, e)), ((0, H - e), (W, H - e), (W, H), (0, H)),
                ((0, 0), (e, 0), (e, H), (0, H)), ((W - e, 0), (W, 0), (W, H), (W - e, H))):
        keepout(board, list(pts), "edge band")
    if CUT:   # (the same band round the LCD-2's cut-out: Freerouting sees only the outer edge's)
        cx0, cy0, cx1, cy1 = CUT
        for pts in (((cx0 - e, cy0 - e), (cx1 + e, cy0 - e), (cx1 + e, cy0), (cx0 - e, cy0)),
                    ((cx0 - e, cy1), (cx1 + e, cy1), (cx1 + e, cy1 + e), (cx0 - e, cy1 + e)),
                    ((cx1, cy0), (cx1 + e, cy0), (cx1 + e, cy1), (cx1, cy1)),
                    ((max(cx0 - e, 0), cy0), (cx0, cy0), (cx0, cy1), (max(cx0 - e, 0), cy1))):
            keepout(board, list(pts), "cut-out band")
    # (the GND pours are added by route.py --pours after the routing: GND is routed as a net first)
    board.Save(board.GetFileName())
    print(f"placed {len(fps)} parts + {len(HOLES + MOUNT)} holes on {W:.1f} x {H:.1f} mm")


if __name__ == "__main__":
    main()
