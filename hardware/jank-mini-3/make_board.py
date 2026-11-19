# SPDX-License-Identifier: GPL-3.0-only
"""jank LCD-2 board (lcd2.kicad_pcb). jank-mini-3 (CELL=floor): the MINI3 block below. The older add-on, arrangement A (worktrees/lcd2-addon-scratch/arr-A.png): 100 x 64 mm,
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
W = float(os.environ.get("LCD2_W", 95.5 if os.environ.get("LCD2_CELL", "floor") == "floor" else 93.0))   # (mini-3: +2.5 for the USB-C pair 13 mm apart, mots 2026-11-18; kept for the through-hole USB4085)
H = float(os.environ.get("LCD2_H", 82.0 if MINI3 else 62.6))   # (mini-3: 82 = 74 + 4 mm each side, mots 2026-11-18: the bands beside the LCD-2; LCD-2 centred)

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
TALL = set()   # (was the PH2.0 cell sockets; mots 2026-11-19: no cells on this board)

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
((26.0, LY + 31.0), ["R162", "R163", "U23", "U22", "R164", "C106", "R165"]),   # the cells' sockets + protector                                                     # spare pads
]
GAP = {r: 0.9 for r in ("R30", "C30", "R31", "C31", "R32", "C32", "R65", "C65", "R66", "C66", "R67", "C67", "R33",
                         "RT33", "R34", "RT34", "R153", "C33")}   # (the LINE IN network: room for its own tracks)
GAP.update({"R162": 0.6, "R163": 0.6, "R166": 0.6})   # (the two 2512s: their silk outlines touched at 0.4)
GAP.update({r: 0.8 for r in ("C99", "R154", "R136", "R131", "JP10", "Q1", "R50",   # (round 3: the last
                             "U5", "C28", "R93", "D16", "R160", "R161", "JP13")})   #  opens, V5_FBL / CC1O, were inside these two groups)
GAP.update({"R164": 1.0})   # (jank-mini-3: VBAT's 1 mm track must reach its pad 1; packed tight it could not)
BOTTOM = set()   # (mots, 2026-11-18, option 1: every part on top, Economic single-sided, the lid raised over the DINs)     # mm: parts under the LCD-2 must stay lower (its back parts hang ~3.5 mm into the 11 mm gap)
CUT = None       # the LCD-2's cut-out (jank-mini-3 only)
NOTCHES = []     # every hole in the outline: the cut-out and the mid-mount USB-Cs' notches (jank-mini-3 only)
USB_NOTCH = (4.65, 6.75)  # (a mid-mount USB-C's notch: +-4.65 mm across, 6.75 deep: the maker's drawing (C2906290) asks
                          #  6.70, the body's back sits at 6.545; review mini3-3B R1. T-bone reliefs R USB_RELIEF at its
                          #  floor's corners (a milled inner corner is round): along the floor, not sideways, where a
                          #  dog-bone reached the shell legs' pads)
USB_RELIEF = 0.45
# (mots, 2026-11-19: the USB-Cs are the through-hole GCT USB4085 now, on top at the edge: no notches, no drawn fan-out.
#  USB_MID = True brings back the mid-mount C2906290 layout: notches, T-bones, usb_fan / usb_pair.)
USB_MID = False


def in_notch(x0, y0, x1, y1, m=0.0):
    """does the box (board mm) come within m of a notch?"""
    return any(x0 < c[2] + m and x1 > c[0] - m and y0 < c[3] + m and y1 > c[1] - m for c in NOTCHES)
DPAD_KEEP = []   # the D-pad cross where no part may sit (jank-mini-3 only)

# ---- jank-mini-3 (mots, 2026-11-18): the LCD-2 drops into a cut-out, its PCB level with ours. Two bridge strips (small
# PCBs, 2 x 1x14 female sockets 3.81 mm apart) join its pins to our two 1x14 male rows, mounted under the board 3.81 mm
# outside its rows. No screws; how it is held is open (mots: one side of the strips soldered; lid pegs cannot press
# its corners, the module covers its PCB to 0.21 mm, review mini3-2B N14). The cell (EEMB LP103454, on the LCD-2's own
# MX1.25, mots 2026-11-19) lies on the case floor under the right part. Under the board, hand-soldered by mots: DINs,
# jacks, the male rows. On top, by JLC: the USB-Cs and every SMD part. Walls: MIDI (DINs + TRS) top, our USB-Cs left
# above / below the LCD-2's own, CLOCK / RESET right, LINE IN bottom.
if MINI3:
    D_ROW = 3.81                                   # our row -> the LCD-2's row (the bridge strip's socket spacing)
    CUT = (0.0, LY - 0.3, LX + 48.21 + 0.3, LY + 35.01 + 0.3)   # (a notch open to the left edge: no 0.2 mm sliver, review mini3-B R1)
    HOLES = []                                     # (its 4 M2 holes lie in the notch, not ours)
    # (our board to the case: one hole in each arm beside the notch (review 2B N10), one on the bottom edge, and since
    #  round 5 one in the right part at its right edge, clear of the cell's floor area x ~50-84.5 (review 5B R3))
    MOUNT = [(3.0, 17.6), (3.0, H - 17.5), (40.5, H - 3.0), (W - 4.0, H / 2)]   # (W - 4, H / 2: the right part, between CLOCK and RESET, review 5B R3: the D-pad was ~40 mm from any hole)  # (not at x 70: with the socket column above it, it walled off the bottom band, 66-80 nets open)
    CELL_KEEP = []                                 # (the cells are on the case floor, not on this board)
    LCD_RECT = CUT
    TALL = set()   # (J18 left again, mots 2026-11-19 after review 6A R1)
    BOTTOM = {"J2", "J3", "J5", "J6", "J7", "J13", "J14", "J15", "J16"}
    DPX = 49.0 + (W - 15.0 - 49.0) / 2
    # (mots, 2026-11-18: NO part where the D-pad cap rests) the 24 mm cross, 8 mm arms, + 1 mm: only SW1/3/4/5 there,
    # on top; no through-hole joint from below either (check_board.py checks both)
    DPAD_KEEP = [(DPX - 13.0, H / 2 - 5.0, DPX + 13.0, H / 2 + 5.0), (DPX - 5.0, H / 2 - 13.0, DPX + 5.0, H / 2 + 13.0)]
    PLACE = {
        # our male rows under the board (pin k across from the LCD-2's pin k: main() checks pin 1's x)
        "J15": (ROW_X, LY + 32.745 + D_ROW, 180), "J16": (ROW_X, LY + 2.265 - D_ROW, 180),
        # TOP wall, left part (mots, 2026-11-18: the two USB-C side by side, away from LINE IN): J1 (TO ESP, data only) then J4 (TO DEVICE)
        # (OUT + its host circuit), left of the TRS jacks, clear of the upper strip; the LCD-2's own USB-C stays at the left wall
        # (mots, 2026-11-18, round 3: the USB-C pair to the RIGHT of the two TRS jacks, out of the jammed corner over the
        #  LCD-2's top row; 13 mm apart: fat plugs, review mini3-B R4; mid-mount, 2B R3)
        "J1": (31.95, None, "-y"), "J4": (44.95, None, "-y"),
        # TOP = MIDI: the TRS jacks above the LCD-2 (clear of the upper strip), the DINs on the right part
        "J2": (6.3, None, "-y"), "J3": (19.1, None, "-y"), "J5": (W - 33.0, None, "-y"), "J6": (W - 10.9, None, "-y"),
        # RIGHT = modular; BOTTOM = audio: LINE IN right of the two USB-C, under its LCD-2 pins P1.8-P1.10 (mots, 2026-11-18)
        "J13": (None, H / 2 - 10.0, "+x"), "J14": (None, H / 2 + 10.0, "+x"),
        "J7": (30.0, None, "+y"),
        # the D-pad between the LCD-2 and CLOCK / RESET: one 24 mm cross cap (8 mm arms) rocking on 4 switches at +/-8 (mots)
        "SW3": (DPX, H / 2 - 8.0, 0), "SW4": (DPX, H / 2 + 8.0, 0), "SW1": (DPX - 8.0, H / 2, 0), "SW5": (DPX + 8.0, H / 2, 0),
        "U18": (W - 8.0, H - 14.0, 90),   # (the 5 V boost in the bottom-right corner: the top-left band was jammed)
        # (review mini3-A R4 / B R2: TI wants COUT at VOUT and GND; pins 4 GND / 5 SW / 6 VOUT face down: L5 right under
        #  SW, C101 beside GND, C100 beside VOUT, no vias in the switch loop)
        "L5": (W - 8.0, H - 14.0 + 4.0, 90),
        # (2026-11-18, round 3: both output caps on VOUT's side, stacked: C101 on the left had its 5 V pad 5 mm from VOUT
        #  (review 2B R1) and blocked FB / EN / VIN's way out, 9 nets open in 2 of 2 attempts)
        "C100": (W - 8.0 + 3.6, H - 14.0 + 0.75, 0), "C101": (W - 8.0 + 3.6, H - 14.0 + 2.95, 0),   # (clear of L5's courtyard)
        # (the input cap up-left of VIN (pin 3): its track goes up and left, pins 2 (EN) / 1 (FB) escape upwards; on the
        #  right, over pins 1 / 2, it boxed EN in, 2 of 2 attempts left V5_EN / V5_FB open)
        "C104": (W - 8.0 - 2.2, H - 14.0 - 2.1, 180),
        # (U5, the USB ESD array, turned so its data pins 1 / 3 face the two USB-C above and its CC pins 4 / 6 face
        #  R160 / R161 below; at 0 the data lines wrapped round it and the router left UDP / CC1O open, round 3)
        "U5": (44.95, 13.7, 180),   # (round 4, review 4A R6: right behind J4, the port that faces the outside gear)
    }
    GROUP = [
        # (round 3: the 5 V aux group split in two, 17 parts on one anchor left V5A / V5_FB open round the boost)
        # (round 3, review 3A / 3B R3: the feedback divider's top half AT U18's FB pin (pin 1, up-right of the chip): the FB
        #  node ran 34 mm with 3 vias, partly under the switch node)
        # (round 4, review 4A R5: the whole divider chain there, R154 / R136 too: the mid node V5_FBM ran 24 mm, between
        #  L5's pads)
        ((W - 8.0 + 2.2, H - 14.0 - 2.6), ["R134", "R135", "R154", "R136"]),
        ((W - 10.0, H - 11.0), ["C99", "R131", "JP10", "Q1", "R50"]),   # the 5 V aux rail
        ((W - 8.0, H - 25.0), ["U19", "R145", "R138", "R139", "RT139", "JP9", "JP8", "R123"]),         # its clamp (U19)
        ((48.5, 15.5), ["C28", "D16", "R160", "R161", "R93", "JP13"]),   # (J4's VBUS cap / diode and Rp by J4, review 4A R6)   # (between and behind J1 / J4; 2026-11-18)                # TO DEVICE (J4) and its ESD
        ((W - 18.0, H - 6.0), ["D3", "R167", "R157", "C27", "R151", "D17", "R158"]),   # (R167: D3's anode pull-down, at it)                       # VBUS_B -> VIN_B, by Q1 (VBUS_B along the bottom band)
        ((40.0, 17.5), ["C60", "C61"]),                                                    # 3V3 decoupling, by P2.1 (x 41)
        ((10.0, H - 12.0), ["R88", "R89", "C29"]),                                         # VBUS sense divider, by P1.14 (VBUS_B)
        ((W - 22.0, H / 2), ["U21", "C105", "R71", "R77", "R91", "D10", "D11", "R75", "R78", "R81", "R84", "C89",
                             "C90", "R120", "RT120", "R121", "RT121"]),                    # CLOCK / RESET
        ((13.0, 17.5), ["U11", "R14", "R15", "RT15", "Q10", "R21", "R20", "RT20", "Q12", "R124", "R125",
                       "R126", "R128"]),   # MIDI TRS jacks: top left, by J2 / J3 and MIDI_TX / TX2 (mots's jack swap, round 3)
        ((W - 22.0, 20.0), ["U12", "R54", "R55", "RT55", "Q11", "R56", "R57", "RT57", "Q13", "R127", "R129"]),   # DINs
        ((38.0, H - 15.0), ["D7", "D14", "R36", "RT36", "R37", "RT37", "R38", "RT38", "R39", "RT39"]),           # LINE IN jack
        ((24.0, H - 15.0), ["R30", "C30", "R31", "C31", "R32", "C32", "R65", "C65", "R66", "C66", "R67", "C67",
                            "R33", "RT33", "R34", "RT34", "R153", "C33"]),                 # LINE IN network, by P1.8 / P1.9
        ((DPX + 11.0, H / 2 + 11.0), ["C80", "C81", "C82", "C83"]),                        # button pads (outside the D-pad cross)
        ((30.0, 18.0), ["J20"]),                                                         # spare pads by P2.5 / P2.6
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
        PLACE["J2"], PLACE["J3"] = (19.1, None, "-y"), (6.3, None, "-y")
    if "D" in sw:
        PLACE["J5"], PLACE["J6"] = (W - 10.9, None, "-y"), (W - 33.0, None, "-y")
    if "M" in sw:
        PLACE["J13"], PLACE["J14"] = (None, H / 2 + 10.0, "+x"), (None, H / 2 - 10.0, "+x")
    assert all(PLACE[r][2] == "-y" for r in ("J1", "J4")), "the USB-C notches are drawn in the top edge only"
    NOTCHES = [CUT] + ([(PLACE[r][0] - USB_NOTCH[0], 0.0, PLACE[r][0] + USB_NOTCH[0], USB_NOTCH[1]) for r in ("J1", "J4")]
                       if USB_MID else [])


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
         "USB-C-SMD_TYPE-C-16PIN-2MD-073": ("A6", -1), "USB-C-SMD_SHOU_TYPE-C-16P-CB1.6": ("A6", -1),
         "USB-C-TH_GCT_USB4085-GF-A": ("B6+B7", 1)}   # (the B row's middle pair: on the centre line, the mouth side)
# the courtyard's front edge past the wall: 0.3 mm, except a mid-mount USB-C, whose notch ends at the board's edge
FRONT = {"USB-C-SMD_SHOU_TYPE-C-16P-CB1.6": 0.0}
WALL_DIR = {"-x": (-1, 0), "+x": (1, 0), "-y": (0, -1), "+y": (0, 1)}


def mouth_dir(fp):
    name = str(fp.GetFPID().GetLibItemName())
    ref, sign = MOUTH[name]
    pads = [p for p in fp.Pads() if p.GetNumber()]
    cx = sum(p.GetPosition().x for p in pads) / len(pads)
    cy = sum(p.GetPosition().y for p in pads) / len(pads)
    qs = [next(p for p in pads if p.GetNumber() == n).GetPosition() for n in ref.split("+")]   # ("A+B": their middle)
    qx, qy = sum(q.x for q in qs) / len(qs), sum(q.y for q in qs) / len(qs)
    return sign * (qx - cx), sign * (qy - cy)


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
    f = FRONT.get(str(fp.GetFPID().GetLibItemName()), 0.3)
    if wall == "-x":
        x = -f - x0
    elif wall == "-y":
        y = -f - y0
    elif wall == "+y":
        y = H + f - y1
    elif wall == "+x":
        x = W + f - x1
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
        # (behind each mid-mount USB-C: its drawn data fan-out, usb_fan(): +-2 mm, to 3.1 mm past the notch's floor)
        self.keep += [((n[0] + n[2]) / 2 - 3.0, 0.0, (n[0] + n[2]) / 2 + 3.0, n[3] + 4.6, "usb fan") for n in NOTCHES[1:]]
        if MINI3:   # (and up-left of U18's pin row: vin_cap()'s track from pin 3 up and left to C104; pins 1 / 2's way up)
            self.keep.append((W - 8.0 - 3.5, H - 14.0 - 2.7, W - 8.0 + 0.8, H - 14.0 - 1.0, "vin cap"))
            # (boost_ground()'s vias: beside U18's GND pin, right of the output caps)
            self.keep.append((W - 8.0 - 2.0, H - 14.0 + 0.2, W - 8.0 - 0.9, H - 14.0 + 2.2, "boost gnd"))
            self.keep.append((W - 8.0 + 5.2, H - 14.0 + 0.2, W - 8.0 + 6.2, H - 14.0 + 3.5, "boost gnd"))

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
if MINI3:   # (U18 turned 90: L5 straight under the SW pin; VIN_B left to the router)
    DRAWN = [("V5_SW", ("L5", "2"), ("U18", "5"), 0.3, None)]


def boost_ground(board, fps, bx, by):
    """the boost's output loop in fixed copper (review mini3-2-A R1, 2B R1): VOUT (pin 6) to C100's 5 V pad on top
    (1.75 mm), C101 stacked under C100 (5 V pads joined, ground pads joined); U18's GND (pin 4) to two vias beside it,
    the caps' ground to two vias right of them, a 1.2 mm bottom bar joining the four under U18 and L5. The DRL
    pinout puts GND and VOUT at the two ends of a row with SW between, so on 2 layers the return runs underneath.
    Locked; the pours join it too."""
    pad = {(r, p.GetNumber()): p for r in ("U18", "C100", "C101") for p in fps[r].Pads()}
    gnd, v5 = pad[("U18", "4")].GetNet(), pad[("U18", "6")].GetNet()
    xy = {k: (pcbnew.ToMM(p.GetPosition().x) - OX, pcbnew.ToMM(p.GetPosition().y) - OY) for k, p in pad.items()}
    want = {("U18", "4"): (bx - 0.5, by + 0.75), ("U18", "6"): (bx + 0.5, by + 0.75), ("C100", "1"): (bx + 2.6, by + 0.75),
            ("C100", "2"): (bx + 4.6, by + 0.75), ("C101", "1"): (bx + 2.6, by + 2.95), ("C101", "2"): (bx + 4.6, by + 2.95)}
    for k, (x, y) in want.items():
        assert abs(xy[k][0] - x) < 0.01 and abs(xy[k][1] - y) < 0.01, f"boost_ground: {k} at {xy[k]}, not {(x, y)}"

    def seg(a, b, w, net, layer=pcbnew.F_Cu):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pcbnew.VECTOR2I(MM(OX + a[0]), MM(OY + a[1])))
        t.SetEnd(pcbnew.VECTOR2I(MM(OX + b[0]), MM(OY + b[1])))
        t.SetWidth(MM(w))
        t.SetLayer(layer)
        t.SetNet(net)
        t.SetLocked(True)
        board.Add(t)

    def via(x, y):
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
        v.SetWidth(MM(0.6))
        v.SetDrill(MM(0.3))
        v.SetNet(gnd)
        v.SetLocked(True)
        board.Add(v)
    u6 = xy[("U18", "6")]                                              # (VOUT to C100, straight; 0.3 mm out of the pin:
    seg(u6, (u6[0] + 0.4, u6[1]), 0.3, v5)                             #  the SW track runs down 0.5 mm to its left)
    seg((u6[0] + 0.4, u6[1]), xy[("C100", "1")], 0.6, v5)
    seg(xy[("C100", "1")], xy[("C101", "1")], 0.8, v5)                 # (the two caps' 5 V pads)
    seg(xy[("C100", "2")], xy[("C101", "2")], 0.8, gnd)                # (and their ground pads)
    u4 = xy[("U18", "4")]
    gl = [(bx - 1.45, by + 0.75), (bx - 1.45, by + 1.65)]              # (beside pin 4: 0.5 mm from its pad)
    gr = [(bx + 5.65, by + 0.75), (bx + 5.65, by + 2.95)]              # (right of the caps' ground pads)
    seg(u4, gl[0], 0.3, gnd)                                           # (0.3 mm out of the pin: pin 3 above, 0.2 mm)
    seg(gl[0], gl[1], 0.6, gnd)
    seg(xy[("C100", "2")], gr[0], 0.8, gnd)
    seg(xy[("C101", "2")], gr[1], 0.8, gnd)
    for x, y in gl + gr:
        via(x, y)
    seg(gl[0], gl[1], 1.2, gnd, pcbnew.B_Cu)
    seg(gl[1], (gr[0][0], gl[1][1]), 1.2, gnd, pcbnew.B_Cu)
    seg(gr[0], gr[1], 1.2, gnd, pcbnew.B_Cu)


def vin_cap(board, fps):
    """U18's VIN pin (3) to its input cap C104 (PLACE: up-left of it): up from the pin, left into C104's pad 1. The
    router left pin 3 open in 2 of 2 attempts (boxed in by pin 2, pin 4 and C101). Locked; the router joins the rest
    of VIN_B to C104."""
    a = next(p for p in fps["U18"].Pads() if p.GetNumber() == "3")
    b = next(p for p in fps["C104"].Pads() if p.GetNumber() == "1")
    assert a.GetNetname() == b.GetNetname() and b.GetPosition().y < a.GetPosition().y, "vin_cap: C104 above U18's pin row"
    pts = [a.GetPosition(), pcbnew.VECTOR2I(a.GetPosition().x, b.GetPosition().y), b.GetPosition()]
    for s0, s1 in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(s0)
        t.SetEnd(s1)
        t.SetWidth(MM(0.3))
        t.SetLayer(pcbnew.F_Cu)
        t.SetNet(a.GetNet())
        t.SetLocked(True)
        board.Add(t)


def usb_fan(board, fps, ref):
    """a mid-mount USB-C's pins behind its notch (no room on the mouth side as the top-mount part had), drawn and
    locked so Freerouting routes on from their ends (it left this crossing, then CC1 and VBUS, open). Offsets across
    the row (rel), away from the mouth (down): B5 CC2 -1.75, A8 SBU -1.25, B6 D+ -0.75, A7 D- -0.25, A6 D+ +0.25,
    B7 D- +0.75, A5 CC1 +1.25, B8 SBU +1.75, VBUS pairs at -+2.25 / 2.55.
    D-: A7 1.0 down to a via, B7 2.6 down to a via, the two joined on the bottom. D+: B6 and A6 1.7 down, joined on
    top. With VBUS / CC (J4): CC1 down past the lower D- via and out to +1.75; each VBUS pair 3.4 down to a via, a
    bottom bar between them under everything. 0.15 mm tracks between the pads; >= 0.175 mm everywhere (KiCad 0.15)."""
    pads = {p.GetNumber(): p for p in fps[ref].Pads()}
    py = pads["A7"].GetPosition().y
    inward = 1 if py > fps[ref].GetPosition().y else -1   # (away from the mouth)
    cx = (pads["A7"].GetPosition().x + pads["A6"].GetPosition().x) / 2
    sx = 1 if pads["A6"].GetPosition().x > cx else -1    # (the side A6 is on: +rel)
    assert all(pads[k].GetPosition().y == py for k in ("B6", "A6", "B7", "A5", "B5")), "usb_fan: the pads in one row"

    def at(rel, dy):
        return pcbnew.VECTOR2I(int(cx + sx * MM(rel)), py + inward * MM(dy))

    def seg(a, b, net, w, layer=pcbnew.F_Cu):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(a)
        t.SetEnd(b)
        t.SetWidth(MM(w))
        t.SetLayer(layer)
        t.SetNet(net)
        t.SetLocked(True)
        board.Add(t)

    def via(p, net):
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(p)
        v.SetWidth(MM(0.5))
        v.SetDrill(MM(0.3))
        v.SetNet(net)
        v.SetLocked(True)
        board.Add(v)
    dn, dp = pads["A7"].GetNet(), pads["A6"].GetNet()
    # (0.1 mm between the pads and past the vias: 0.2 mm from a 0.5 mm via at 0.5 mm pitch, Freerouting's USB clearance;
    #  at 0.15 the 0.175 mm gaps read as 17 "pre-existing violations" and it would not connect into the fan-out, round 3)
    NARROW = 0.1
    seg(at(-0.25, 0), at(-0.25, 1.0), dn, NARROW)
    via(at(-0.25, 1.0), dn)
    seg(at(0.75, 0), at(0.75, 2.6), dn, NARROW)
    via(at(0.75, 2.6), dn)
    seg(at(-0.25, 1.0), at(-0.25, 1.9), dn, 0.2, pcbnew.B_Cu)
    seg(at(-0.25, 1.9), at(0.75, 2.6), dn, 0.2, pcbnew.B_Cu)
    seg(at(-0.75, 0), at(-0.75, 1.7), dp, NARROW)
    seg(at(0.25, 0), at(0.25, 1.7), dp, NARROW)
    seg(at(-0.75, 1.7), at(0.25, 1.7), dp, 0.2)
    if pads["A5"].GetNetname().lstrip("/").startswith("CC"):   # (J4: CC1 out past the lower D- via)
        cc = pads["A5"].GetNet()
        seg(at(1.25, 0), at(1.25, 1.9), cc, NARROW)
        seg(at(1.25, 1.9), at(1.75, 2.4), cc, NARROW)
        seg(at(1.75, 2.4), at(1.75, 3.2), cc, NARROW)
    # (J4's VBUS: only the pair on CC1's side (A9 / B4, rel +2.4) carries VUSB; left to the router. Was: both pairs
    #  dropped and joined by a bottom bar, and the left drop sat in the D+ path, round 3)


def usb_pair(board, fps):
    """the data pair's run J1 -> U5 -> J4, drawn and locked: the geometry Freerouting found on the clean board of
    2026-11-19 (round 4), taken from the fan-out's ends. (Round 5: Freerouting routes GND first, and in 5 of 5 attempts
    its GND tracks from J1's ground pins walled J1's D+ / D- fan-out in, UDP / UDN left open.)
    D+: J1's join down to a via, under J1's lower D- via on the bottom, up again past it, along the top to J4's join,
    then down to a via, on the bottom to U5.1. D-: J1's lower D- via along the top to U5.3; J4's lower D- via to it."""
    pad = lambda r, n: next(p for p in fps[r].Pads() if p.GetNumber() == n).GetPosition()
    py = pad("J1", "A7").y
    assert pad("J4", "A7").y == py, "usb_pair: J1 / J4 in one row"
    dp, dn = (next(p for p in fps["J1"].Pads() if p.GetNumber() == n).GetNet() for n in ("A6", "A7"))
    # (the D+ joins run B6 -> A6 at 1.7 down; each lower D- via sits under B7, 2.6 down: usb_fan)
    j1b6, j1b7, j4b6, j4b7 = pad("J1", "B6").x, pad("J1", "B7").x, pad("J4", "B6").x, pad("J4", "B7").x
    u1, u3 = pad("U5", "1"), pad("U5", "3")
    assert j1b6 < j1b7 < j4b6 < j4b7 and pad("J1", "B6").x < pad("J1", "A6").x and pad("J4", "B6").x < pad("J4", "A6").x, \
        "usb_pair: the drawn order"
    v = lambda x, dy: pcbnew.VECTOR2I(int(x), int(py + MM(dy)))
    xy = lambda x, y: pcbnew.VECTOR2I(int(x), int(y))

    def seg(a, b, net, layer=pcbnew.F_Cu):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(a)
        t.SetEnd(b)
        t.SetWidth(MM(0.2))
        t.SetLayer(layer)
        t.SetNet(net)
        t.SetLocked(True)
        board.Add(t)

    def via(p, net):
        t = pcbnew.PCB_VIA(board)
        t.SetPosition(p)
        t.SetWidth(MM(0.5))
        t.SetDrill(MM(0.3))
        t.SetNet(net)
        t.SetLocked(True)
        board.Add(t)
    # D+
    b = j1b6
    v1, v2 = v(b, 2.215), v(b + MM(2.35), 1.975)
    seg(v(b, 1.7), v1, dp)
    via(v1, dp)
    for a_, c_ in ((v1, v(b + MM(0.94), 3.155)), (v(b + MM(0.94), 3.155), v(b + MM(1.8), 3.155)),
                   (v(b + MM(1.8), 3.155), v(b + MM(2.35), 2.565)), (v(b + MM(2.35), 2.565), v2)):
        seg(a_, c_, dp, pcbnew.B_Cu)
    via(v2, dp)
    v3 = v(j4b6, 2.255)
    seg(v2, v(j4b6, 1.975), dp)
    seg(v(j4b6, 1.7), v3, dp)
    via(v3, dp)
    v4 = xy(u1.x, u1.y - MM(1.12))
    seg(v3, v4, dp, pcbnew.B_Cu)
    via(v4, dp)
    seg(v4, u1, dp)
    # D-
    k = xy(u3.x, u3.y - MM(0.9))
    seg(v(j1b7, 2.6), xy(k.x - MM(1.57), py + MM(2.6)), dn)
    seg(xy(k.x - MM(1.57), py + MM(2.6)), k, dn)
    seg(v(j4b7, 2.6), xy(k.x + MM(0.13), k.y), dn)
    seg(xy(k.x + MM(0.13), k.y), k, dn)
    seg(k, u3, dn)


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

    def arc(a, m, b):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_ARC)
        s.SetArcGeometry(*(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)) for x, y in (a, m, b)))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(MM(0.1))
        board.Add(s)
    if CUT and CUT[0] <= 0:   # (jank-mini-3: the LCD-2's cut-out as a notch open to the left edge, review mini3-B R1)
        _x0, y0, x1, y1 = CUT
        r = USB_RELIEF
        last = (0, 0)
        for nx0, _ny0, nx1, d in sorted(NOTCHES[1:]):   # (the mid-mount USB-Cs' notches in the top edge, T-boned)
            seg(*last, nx0, 0)
            seg(nx0, 0, nx0, d)
            arc((nx0, d), (nx0 + r, d + r), (nx0 + 2 * r, d))
            seg(nx0 + 2 * r, d, nx1 - 2 * r, d)
            arc((nx1 - 2 * r, d), (nx1 - r, d + r), (nx1, d))
            seg(nx1, d, nx1, 0)
            last = (nx1, 0)
        pts = [last, (W, 0), (W, H), (0, H), (0, y1), (x1, y1), (x1, y0), (0, y0), (0, 0)]
        for a, b in zip(pts, pts[1:]):
            seg(*a, *b)
        return
    for (a, b) in (((0, 0), (W, 0)), ((W, 0), (W, H)), ((W, H), (0, H)), ((0, H), (0, 0))):
        seg(*a, *b)
    if CUT:   # (an inner cut-out)
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


def keepout_tracks(board, pts, layer, name):
    """a rule area on one layer that keeps tracks out but lets vias and the pour in (a solid ground plane under the boost)"""
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetDoNotAllowTracks(True)
    z.SetDoNotAllowVias(False)
    z.SetDoNotAllowZoneFills(False)
    z.SetDoNotAllowPads(False)
    z.SetDoNotAllowFootprints(False)
    z.SetLayer(layer)
    z.SetZoneName(name)
    ol = z.Outline()
    ol.NewOutline()
    for x, y in pts:
        ol.Append(MM(OX + x), MM(OY + y))
    board.Add(z)


def gnd_vias(board, fps, spots):
    """fixed GND vias at power ground pads: (ref, pad, count); each 1.0-1.6 mm out from the pad, away from its part's
    centre, kept only where no other net's pad or the board edge is near (review mini3-2-A R1 / R2: the boost's output
    cap return and the cells' return current at U23 pin 3 went through one 0.3 mm via)"""
    gnd = net(board, "GND")
    others = [p for f in fps.values() for p in f.Pads() if p.GetNetname().lstrip("/") != "GND"]
    gnd_pads = [p for f in fps.values() for p in f.Pads() if p.GetNetname().lstrip("/") == "GND"]
    placed = []
    for ref, num, count in spots:
        f = fps[ref]
        pad = next(p for p in f.Pads() if p.GetNumber() == num)
        px, py = pcbnew.ToMM(pad.GetPosition().x) - OX, pcbnew.ToMM(pad.GetPosition().y) - OY
        cx, cy = pcbnew.ToMM(f.GetPosition().x) - OX, pcbnew.ToMM(f.GetPosition().y) - OY
        ux, uy = px - cx, py - cy
        d = math.hypot(ux, uy) or 1.0
        ux, uy = ux / d, uy / d
        n_ok = 0
        for dist in (1.0, 1.3, 1.6, 1.9):
            for side in (0.0, 0.9, -0.9, 1.8, -1.8):
                if n_ok >= count:
                    break
                x, y = px + ux * dist - uy * side, py + uy * dist + ux * side
                if not (1.0 < x < W - 1.0 and 1.0 < y < H - 1.0) or in_notch(x, y, x, y, 1.0):
                    continue
                if any(math.hypot(x - a, y - b) < 0.95 for a, b in placed):
                    continue
                v = pcbnew.VECTOR2I(MM(OX + x), MM(OY + y))
                def gap(o):   # (point to pad box, mm; KiCad's BOX2I.Distance() gave 0 for every pad here)
                    bb = o.GetBoundingBox()
                    gx = max(pcbnew.ToMM(bb.GetLeft()) - OX - x, 0, x - (pcbnew.ToMM(bb.GetRight()) - OX))
                    gy = max(pcbnew.ToMM(bb.GetTop()) - OY - y, 0, y - (pcbnew.ToMM(bb.GetBottom()) - OY))
                    return math.hypot(gx, gy)
                if any(gap(o) < 0.3 + 0.25 for o in others):   # (via radius + 0.25 mm)
                    continue
                if any(gap(o) < 0.3 + 0.1 for o in gnd_pads):   # (never in a pad: it would wick the joint's solder)
                    continue
                via = pcbnew.PCB_VIA(board)
                via.SetPosition(v)
                via.SetWidth(MM(0.6))
                via.SetDrill(MM(0.3))
                via.SetNet(gnd)
                via.SetLocked(True)
                board.Add(via)
                placed.append((x, y))
                n_ok += 1
    return len(placed)


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
        # (jank-mini-3: mots hand-solders the through-hole parts: thermal reliefs on THT pads, solid on SMD pads, review
        #  mini3-B R3; was solid everywhere when JLC soldered the THT parts)
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL if MINI3 else pcbnew.ZONE_CONNECTION_FULL)
        z.SetThermalReliefGap(MM(0.25))
        z.SetThermalReliefSpokeWidth(MM(0.4 if MINI3 else 0.3))   # (0.4: review mini3-B R3 for hand soldering)
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
    ds.SetAuxOrigin(pcbnew.VECTOR2I(MM(OX), MM(OY + H)))   # (the lower left: Gerbers, drill and the CPL share it)
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
    for anchor, refs in GROUP:
        if isinstance(anchor, str):   # (a part's own position: its cap right next to it)
            p0 = fps[anchor].GetPosition()
            anchor = (pcbnew.ToMM(p0.x) - OX, pcbnew.ToMM(p0.y) - OY)
        ax, ay = anchor
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
        if MINI3 and ref in ("J1", "J4"):        # (the USB-C shell legs: JLC solders them, solid to the pours; with
            for pad in fp.Pads():                 #  thermal reliefs DRC found them starved, review round 1 re-route)
                if pad.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                    pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
        fp.SetField("LCSC", lcsc)                 # (DRC parity: the symbols carry it, even empty)
        fld = fp.GetField("LCSC")
        if fld is not None:
            fld.SetVisible(False)
        fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)
        if ref in P.DNP:
            fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_DNP | pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
            # (no paste on a part that is not fitted, review mini3-4B N3: parts.py gives them the R0402_NP / C0603_NP
            #  footprints, which have none; editing the pads here made KiCad call them library mismatches)
    draw(board, fps)
    if MINI3 and USB_MID:   # (the mid-mount USB-C's drawn fan-out; the through-hole USB4085 is routed as it is)
        for r in ("J1", "J4"):
            usb_fan(board, fps, r)
        usb_pair(board, fps)
    if MINI3:   # (the boost: a solid bottom ground under it, vias at its ground pins; the cells' return into ground)
        bx, by = W - 8.0, H - 14.0
        # (the boost's ground: boost_ground(), a drawn bar, not a track-free area: the keep-out under the boost left
        #  16-17 nets open after round 1 in 4 of 4 attempts, 5 without it, the A/B run of 2026-11-18)
        boost_ground(board, fps, bx, by)
        vin_cap(board, fps)
        # (no bottom track under L5 and the switch node: review 3B R3; below the drawn ground bar, which ends at by + 2.25)
        keepout_tracks(board, [(bx - 1.6, by + 2.6), (bx + 1.6, by + 2.6), (bx + 1.6, by + 6.2), (bx - 1.6, by + 6.2)],
                       pcbnew.B_Cu, "under L5")
        # (the GND vias at U23's source pin went with the cells' protector, mots 2026-11-19)
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
            if pts[1][0] - pts[0][0] > 0.01:   # (no left band when the cut-out is a notch open to the edge)
                keepout(board, list(pts), "cut-out band")
    for nx0, _ny0, nx1, ny1 in NOTCHES[1:]:   # (and round the USB-C notches: their sides and floor, reliefs included)
        e = max(0.45, USB_RELIEF)   # (the sides; the T-bones reach USB_RELIEF below the floor at the corners)
        # (the floor's band only 0.25 deep: the signal pads start 0.2 below the floor and the drawn fan-out leaves them
        #  0.6 below it; at 0.45 the band came 0.15 from those tracks, inside Freerouting's 0.2, and it would not use them,
        #  round 3. KiCad's copper-to-edge rule still guards the edge itself)
        # (round 5, review 5B R1: the pads now start 0.075 below the floor; a 0.25 band overlapped them by 0.175 and
        #  Freerouting left UDP / UDN open in 3 of 3 attempts: the band 0.05 deep, lcd2.kicad_dru's 0.07 guards the edge)
        f = 0.05
        for pts in (((nx0 - e, 0), (nx0, 0), (nx0, ny1 + e), (nx0 - e, ny1 + e)), ((nx1, 0), (nx1 + e, 0), (nx1 + e, ny1 + e), (nx1, ny1 + e)),
                    ((nx0 + 2 * USB_RELIEF, ny1), (nx1 - 2 * USB_RELIEF, ny1), (nx1 - 2 * USB_RELIEF, ny1 + f), (nx0 + 2 * USB_RELIEF, ny1 + f))):
            keepout(board, list(pts), "USB notch band")
        # (the T-bone reliefs reach USB_RELIEF below the floor at its corners: their own boxes, the floor band no
         #  longer covers them; round 5: GND tracks 0.20-0.23 mm from the arcs, the edge rule is 0.25)
        rd = USB_RELIEF + 0.15
        for x0, x1 in ((nx0, nx0 + 2 * USB_RELIEF), (nx1 - 2 * USB_RELIEF, nx1)):
            keepout(board, [(x0, ny1), (x1, ny1), (x1, ny1 + rd), (x0, ny1 + rd)], "USB relief")
    # (the GND pours are added by route.py --pours after the routing: GND is routed as a net first)
    board.Save(board.GetFileName())
    print(f"placed {len(fps)} parts + {len(HOLES + MOUNT)} holes on {W:.1f} x {H:.1f} mm")


if __name__ == "__main__":
    main()
