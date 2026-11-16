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
import collections
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
W, H0 = (float(v) for v in os.environ.get("SIZE", "72x60").split("x"))   # (mini 2.2: 66 x 68 kept. Without the cells
#  66 x 60.5 (SIZE=66x52.5) placed clean; the cells under the board need the band under the DIN bodies, y 37..58, which
#  a 60.5 mm board puts into the antenna strip)
# mini 2: the LINE IN band (README "LINE IN"): the top row (TOP: the USB-C, both jacks, the USB and VBUS parts under the
# USB-C) stays at the top edge; everything else moves down DY, which opens a band for the LINE IN jack (J7, on the top
# edge between MIDI OUT and the USB-C) and its front end; the DIN sockets go down with it, clear of J7's legs
DY = float(os.environ.get("DY", "8"))
H = H0 + DY
TOP = {"J1", "J2", "J3", "U5", "R3", "R4", "R12", "R13", "R23", "D3", "R5"}
# stage 2, 72 x 72 (the user's "one board, different screens"): the 66 mm design stretched: parts placed at fixed x
# (not from S, W or HDR) move right by DX2 (3 mm) in the middle (x 20 .. 41) and 2 x DX2 on the right (x 41 on), the
# left third stays (place(), XS_SKIP: the ones that follow S, W, HDR or BTN_C already)
DX2 = (W - 66.0) / 2
# stage 2 (the user's 66 x 64 for the CLOCK / RESET outputs): a BAND mm band at BAND_Y: everything placed below that line
# moves down BAND (the chip block with CY, the lower block's fixed spots through LOWER in place()); the top rows, the DINs
# and the LINE IN front end stay
BAND, BAND_Y = float(os.environ.get("BAND", "14")), 36.0
GE = H - 10.0 + BAND            # the ground's bottom edge: from here to H no copper but the antenna's (YAGEO eval board: 10 mm)
# stage 1e (the user's: the u.FL only): the antenna strip is gone, the board ends at the ground's edge; STRIP=10 puts the
# 10 mm strip back (for a chip antenna). Everything placed from GE and CY stays where it was
STRIP = float(os.environ.get("STRIP", "0"))
S = (W - 44.6) / 2                # (the middle's shift: 44 -> 47.6 mm for the 2200 uF cans, 49.6 for the DIN pads' rings; 53.2 for RST (the right side), 54.2 and 56.2 for the left side's tracks)
SB = S - 2.0                # (the buck group's shift: it stays by C1 and J5, the chip moved 2 mm away from it)
CX, CY = 18.8 + S, GE - max(13.5, 12.1 + (H0 - 52.5) * 0.4)   # (13.5 or more: the crystal clear of J4)    # the chip's centre (turned 180: LNA_IN at the right side's bottom)
# 2026-11-15 (the user's): U1 is an ESP32-S3-WROOM-1U-N8R2 module. MODULE: the bare chip's drawn copper (the RF line,
# the fan-out lanes, the crystal and VDD3P3 hops, the exposed pad's vias and blocks) is skipped; the module sits at
# MOD_XY turned 180 (its u.FL toward the bottom edge, its top pin row (15 .. 26: LINE_IN, I2C, BTN_UP, MIDI_TX) up
# toward the LINE IN block and the header, USB (13 / 14) at its upper right)
MODULE = True
MOD_XY = (38.6, 61.5)
# the RF path (README "RF"): the line's centre x, its width and gap (grounded coplanar waveguide, 50 ohm: README)
XA = 27.1 + S               # the line and the antenna on this x
RF_W, RF_GAP = 1.2, 0.2
Y_LINE = CY + 2.6           # LNA_IN's y
Y_L3 = Y_LINE + 0.6         # L3 and the line's top, 0.6 under LNA_IN: room for C28 at the RF supply's pins above it
ANT_Y = GE + 2.5 + 1.58     # the antenna's centre: its feed pad (S1) 2.0 mm past the ground edge (eval board)
END_PAD = (1.8, 2.5)        # the copper beyond the antenna's end pad (S2), unconnected (eval board: 1.8 x 2.5)
DIN_D, DIN_Y = 8.6, 20.3 + DY    # the DINs' origins: their faces (8.6 from JLC's origin) on the side edges, their axes at y 20
H = GE + STRIP              # (stage 1e: the board's bottom edge; the parts above were placed from GE and CY)
CAN_Y = 35.5 + (CY - 30.4)               # the cans' centre y (10 x 10.5 mm, upright: under the DINs' last holes, over the ground edge)
# stage 1e: the screen module's outline (a 0.96" 128 x 64 I2C OLED, 27.3 x 27.8 mm, on J10's header ~2.5 mm over the
# board, the header row 1.5 mm in from its top edge: *typical module, unverified per maker*): only low parts under it
HDR = (35.0 + (W - 66.0) / 2, 15.9)          # (stage 2: 1.1 lower, 1 mm right: the USB pair's way from U5 round its left end, under R4)
MODULE = (HDR[0] - 27.3 / 2, HDR[1] - 1.5, HDR[0] + 27.3 / 2, HDR[1] - 1.5 + 27.8)

BTN_C, BTN_D = (55.5 + (W - 66.0), 48.2 + BAND), 6.5     # stage 1f: the buttons' cross, its middle and each button's distance from it
PIVOT_R = 1.5                        # stage 2: the D-pad's pivot circle at BTN_C: no part, no via (freeroute.py, check_board.py)
# stage 2: where no twin goes (place_twins): the chip with its lanes and match, the drawn USB pair, the RF line and the u.FL
FORBID = [(CX - 4.6, CY - 6.0, CX + 9.0, CY + 4.6), (27.9 + DX2, 12.3, 34.6 + DX2, 14.4), (28.2 + DX2, 13.5, 29.8 + DX2, 36.8 + BAND),
          (XA - 2.4, Y_L3 - 1.2, XA + 3.0, Y_L3 + 8.6)]
if MODULE:                                   # (the module: its own courtyard keeps the auto-placed parts off it)
    FORBID = FORBID[1:3]

# 2026-11-15, the fresh floorplan (the user's "a FRESH FLOORPLAN of the whole board ... block by block"): floorplan.json
# holds every top part's place but the fixed ones (the edge connectors, the buttons, the LEDs, J10 / J10B, the cells'
# sockets, U5 under the USB-C's data pads), absolute board mm {ref: [x, y, rotation]}; it overrides PLACE, the spreads
# and AUTO. Made by the block placer (README "Floorplan"); FLOOR=none goes back to PLACE / AUTO
_FLOOR_PATH = os.environ.get("FLOOR", os.path.join(HERE, "floorplan.json"))
FLOOR = {} if _FLOOR_PATH == "none" or not os.path.exists(_FLOOR_PATH) else \
    {r: tuple(v) for r, v in __import__("json").load(open(_FLOOR_PATH)).items()}

# ref: (x, y, rotation); the footprints' origins (JLC's placement origins); every part on top
PLACE = {
    # top edge: the jacks (bushings past the side edges), the USB-C between them
    # stage 1f (the user's): the jacks on the BOTTOM, DNP, soldered by the user as the DINs (flipped, turned 180: the
    # bushings still past the edges, as on top: a flipped part keeps its angle); the top keeps nothing tall but the USB-C
    # stage 2: the PJ-307C (switched TRS, 12 wide): each body's front face on its edge, the nose (3 mm) past it
    "J1": (W / 2, 4.97, 180), "J2": (6.5, 6.4, 270, "B"), "J3": (W - 6.5, 6.4, 90, "B"),   # (review pass 2, L9: 0.5 mm in)
    # the DIN sockets (DNP, hand-soldered) on the BOTTOM under each side edge, their faces on the edges (README "DIN")
    "J5": (DIN_D, DIN_Y, 90, "B"), "J6": (W - DIN_D, DIN_Y, 270, "B"),
    # between the DINs' holes (every top part keeps 0.8 mm from the DIN pads' copper: the iron's room, README "DIN"):
    # left the MIDI OUT resistors (mini 2.2 simplified: three 100 ohm 1206, R48 the third, under R14 / R26)
    # (stage 2: J2's source trio and sink in the top left band, by J2's legs: the jack's lines short)
    "R14": (2.4, 15.9, 90), "R26": (5.0, 15.9, 90), "R48": (7.6, 15.9, 90), "R15": (10.0, 15.9, 90), "RT15": (12.2, 15.9, 90),
    # stage 1f: the DINs' own source (33 ohm 0805) and sink (10 ohm) between each DIN's pads, out of their iron room
    # (stage 2: upright, each with its hand-solder twin beside it, inside the DIN's pads' iron rooms)
    # (stage 2: the DIN's source two 100 ohm 1206 side by side between its pads, its 68 ohm sink upright with its twin)
    "R54": (6.7, 27.25, 0), "R58": (6.7, 29.85, 0), "R55": (6.3, 34.1, 90), "RT55": (8.55, 34.1, 90),
    "R56": (W - 6.7, 27.25, 0), "R59": (W - 6.7, 29.85, 0), "R57": (W - 6.3, 34.1, 90), "RT57": (W - 8.55, 34.1, 90),
    # mini 2.2 simplified (README "mini 2.2 simplified"): BOOT (SW1) and RST (SW2) in one row in the middle, 10 mm apart
    # (BOOT under a big cap); the indicator (LED1, LED2: two RGB LEDs, one window) between them and the USB
    # parts, each LED's 1 k and its DNP twin beside it
    # stage 1e (the user's "LEDs and buttons, or a cheap screen and 4 buttons", both on one board): the four buttons in a
    # 2 x 2 grid at the lower right, 9.5 mm apart (8 mm caps), clear of the screen and of J6's pads (UP over DOWN on the
    # left, NO / BOOT over YES on the right, as the screen study); RST at the top right under a pinhole; the LEDs (one
    # window) at the top left, out from under the screen; the screen's header in the middle under the USB parts
    # stage 1f (the user's): no RST; the four in a cross (a D-pad), 6.5 mm from its middle (13 mm apart across): UP top,
    # DOWN bottom, NO / BOOT left, YES right; the lid's U-tongues (case/README "Buttons") fit round it inside the walls
    "SW3": (BTN_C[0], BTN_C[1] - BTN_D, 0), "SW4": (BTN_C[0], BTN_C[1] + BTN_D, 0),
    "SW1": (BTN_C[0] - BTN_D, BTN_C[1], 0), "SW5": (BTN_C[0] + BTN_D, BTN_C[1], 0),
    # (stage 2: the indicator in the top right, between the USB-C and J3's resistors, out from under the screen)
    # (2026-11-15, the user's four beat LEDs, one lid hole each: one row, beat 1 .. 4 left to right, evenly 8.17 mm
    #  apart (7.5 mm since 2026-11-15 late), turned 180 (DI on the left, DO on the right: the chain runs left to right). Clear of both screens' outlines
    #  + 1.5 mm (the 2.42" covers y 1 .. 44 over the whole width, the 1.54" x 16.8 .. 59.2, y 13.4 .. 51.4), so only the
    #  bottom band (y >= 52.9) is free across the middle; there the module (x ~31 .. 50) and the D-pad (x 51 .. 72) leave
    #  x 2 .. 28.9 (the module's escape corridor) for an even row. Spread over the full width is not possible: README)
    "LED1": (3.0, 54.6, 180), "LED2": (10.5, 54.6, 180), "LED3": (18.0, 54.6, 180), "LED4": (25.5, 54.6, 180),
    # (7.5 mm pitch since the user's check: LED4's body >= 4 mm from the module's (the lid's light hole clears its 3 mm))
    "C102": (80, 58.0, 0), "C103": (80, 59.0, 0),       # (stage 2: one over the other at the left end of the band
    #  (BAND_Y), outside the screen module's outline: the jacks took their top-edge spot; one window over both)
    "R40": (80, 8.0, 0), "C17": (80, 30.0, 0), "C16": (80, 31.0, 0), "C7": (80, 32.0, 0), "R8": (80, 33.0, 0), "C8": (49.3, 64.4, 90),
    "C32": (80, 35.0, 0), "R32": (80, 36.0, 0), "C30": (80, 37.0, 0), "C31": (80, 38.0, 0), "R30": (80, 39.0, 0),
    "R31": (80, 40.0, 0), "C33": (80, 41.0, 0), "C67": (80, 42.0, 0), "R67": (80, 43.0, 0), "C65": (80, 44.0, 0),
    "C66": (80, 45.0, 0), "R65": (80, 46.0, 0), "R66": (80, 47.0, 0), "R145": (80, 48.0, 0), "R146": (80, 49.0, 0), "R147": (80, 50.0, 0), "R148": (80, 51.0, 0), "R149": (80, 52.0, 0),
    "R150": (80, 53.0, 0), "R151": (80, 54.0, 0), "R152": (80, 55.0, 0), "R153": (80, 56.0, 0), "R154": (80, 57.0, 0),
    **{r: (80, 12.0 + 0.1 * i, 0) for i, r in enumerate(("U2B", "U19", "R50", "R134", "R135", "R136", "R131", "R137", "R138", "R139",
                                                         "R132", "R122", "R123", "R133", "R130", "R140", "R141", "R142",
                                                         "R143", "R144", "Q10", "Q11", "Q12", "Q13", "R124", "R125", "R126",
                                                         "R127", "R128", "R129"))},
    "C96": (6.6, 37.9, 90), "C97": (6.6, 40.9, 90),   # (2026-11-15: each RGB LED's 100 nF, right of it)
    "R98": (10.2, 40.9, 0), "C98": (13.6, 40.9, 0),   # (2026-11-15: VLED's filter, 4.7 ohm + 10 uF)
    # stage 2, CLOCK / RESET (parts.py "_cv"): the PJ-320A jacks under the board at the top edge, their fronts on it
    "J13": (42.3, 7.25, 270, "B"), "J14": (49.0, 7.25, 270, "B"),
    # (stage 2, CLOCK / RESET: the drivers in two rows of three SOT-23 right of the cell's protection (2N7002, AO3401A,
    #  BAV99), the outputs' 2 k pairs on the right edge, the gate pull-downs between the rows and Q2, the nodes'
    #  pull-downs over the rows; the charge pump (R83, C84, D12, D13, C85, R84) between the chip's right side and the
    #  cross, by GPIO5's pin; the gate pull-ups with their twins beside them; README "CLOCK / RESET")
    "Q3": (50.4, 39.6, 0), "Q4": (54.2, 39.6, 0), "D10": (58.1, 39.6, 0),
    "Q5": (50.4, 43.15, 0), "Q6": (54.2, 43.15, 0), "D11": (58.1, 43.15, 0),
    "R75": (62.9, 37.5, 0), "R76": (62.9, 40.1, 0), "R81": (62.9, 42.7, 0), "R82": (62.9, 45.3, 0),
    "R74": (49.0, 36.75, 0), "R80": (46.0, 35.4, 0), "R71": (47.9, 42.0, 90), "R77": (47.9, 44.0, 90),
    # (72 x 72: the pump in a row and R73 / RT73 up in the band, off the chip's decoupling (C7, R8, L2, C16, C17), which
    #  boxed VDD3P3 and CHIP_PU in: the 72 x 72 routing's first try; these are raw: + DX2 and + BAND - 6 in place())
    "RT73": (57.0, 34.6, 0), "R73": (57.0, 36.7, 0), "RT79": (62.9, 48.5, 0), "R79": (62.9, 50.6, 0),
    # (review pass 1: the nodes' 4.7 nF, C89 over D10, C90 under R80 (RST_N's pull-down): no switch-on blip)
    "C89": (58.1, 36.9, 0), "C90": (46.0, 36.8, 0),
    # (the pump at the board's lower left, by GPIO35's pin and VREG, far from LINE IN; raw: + DX2, + BAND - 6)
    # (review pass 1: U13, the pump's buffer, and its 100 nF where U10 was; R83 / C84 between it and the diodes, which
    #  moved 0.6 mm right for C3 at the LDO)
    # (2026-11-15: the pump is gone; in its place, absolute: the 5 V aux rail's boost U18, L5 and its caps, the rest of the
    #  power block by auto_place; the converters' loops >= 10 mm from LINE IN (the top middle) and J7)
    "U18": (20.0, 63.5, 0), "L5": (22.8, 63.5, 90), "C99": (17.6, 62.8, 90), "C100": (25.35, 65.6, 90), "C101": (27.75, 65.6, 90),
    "L4": (15.4, 66.4, 0),
    # (the CLK / RST sleeves' 47 ohm and their twins in the band's free left end, where U16 was; their nets run to J13 / J14)
    "R120": (23.0, 43.1, 0), "RT120": (23.0, 45.3, 0), "R121": (28.6, 43.1, 0), "RT121": (28.6, 45.3, 0),
    "C80": (36.5, 39.6, 0),                                   # (UP's debounce pad, on BTN_UP's way to UP)
    "J10": (HDR[0], HDR[1], 0, "T"), "JP1": (32.0, 19.8, 0), "JP2": (36.2, 19.8, 0),
    "C60": (42.4, 15.1, 90), "C61": (44.0, 15.1, 90),
    # (stage 2: the I2C pull-ups and their twins right of the header's caps; SCL / SDA come along the header)
    "R60": (47.5, 13.6, 0), "RT60": (47.5, 15.25, 0), "R61": (47.5, 17.6, 0), "RT61": (47.5, 19.25, 0),
    # under the USB-C: the CC resistors (each under its own pin's side: CC2 = B5 left, CC1 = A5 right); the strip under
    # U5 stays free for D+ / D- to the chip
    "R4": (W / 2 - 3.6, 11.0, 90), "R3": (W / 2 + 3.6, 11.0, 90),
    # mini 2.2, the power and the cell (README "mini 2.2 simplified"): the lower left, where the buck and C1 were, under
    # the DIN pads' iron room (y 39 on): the two PH2.0 sockets (bottom side, DNP) on one row near the ground edge, the
    # protector (U9) and the charger (U8) above them, the power path (D3, Q1) and the 3.3 V LDO (U2) towards the chip;
    # the cell's divider (R10 / R11 / C8) at its fan-out lane (GPIO4 since stage 2), above the chip's right side
    # (stage 1d: the cells stand on edge under the board in y 39..58, beside the DIN bodies: the two sockets moved out of
    #  that band to the middle, y 33, between the DIN bodies; their bodies hang under the board there)
    "J8": (62.0, 57.6, 0, "B"), "J9": (62.0, 68.25, 0, "B"),   # (buffer try, the user's: bottom right above SW3, vertical B2B   # (buffer try, the user's: bottom right, above SW3, by the cells' lead end)
    "Q2": (44.9, 38.6, 0), "R53": (47.6, 38.6, 90),          # (reverse polarity, right by the sockets: BAT_C stays short)
    "R52": (42.5, 38.9, 90),                                  # (the XB5353A's 100 ohm, by U9 and C42)
    # MIDI OUT 2's own resistors: R14 / R26 / R48 / R15 mirrored to the right side (the same rooms by J6's pads)
    # (stage 2: by J3, in the top right corner left of its legs: the jack's lines short, MIDI_TX2 the long one)
    # (stage 2: J3's source trio and sink in the top right, by J3's legs)
    # (the power switch is in the lid's right wall, its wires to J11 under it; R62 between them and J6's pads)
    # (72 x 72: the toggle on the left wall under the screen band, J11 by it: VSYS / VREG short to the power block)
    "J11": (5.0, 49.5, 0), "R62": (5.0, 52.6, 0), "J10B": (5.5, 44.6, 0), "C86": (14.0, 44.6, 0),   # (J10B: the 2.42" lead)
    # (stage 2: the spare lands and the recovery pads (parts.SPARES); R63 / C27, VBUS's ferrite place, by D3 and the
    #  charger; R39 / R65 / R66 / C67, the LINE IN's ground sensing: R39 by J7's sleeve leg, the divider by C33)
    # (stage 2: J12 left of the chip in the band, not on the bottom edge: U0TXD / U0RXD no longer cross the crystal's
    #  corner, which RESET, YES and DOWN now pass round)
    "J12": (18.0, 49.0, 0), "C72": (2.6, 48.0, 90),
    # (review pass 1, M7: each MIDI output's SRV05-4 between its jack's resistors and its DIN's, over the DIN's body,
    #  clear of the DIN pads' iron rooms)
    "U11": (8.0, 22.0, 90), "U12": (W - 8.0, 22.0, 90),   # (2026-11-15: the Hongjiacheng SRV05-4's JLC land is the old one turned 90)
    "C81": (60.4, 56.4, 90), "C82": (49.0, 51.9, 0), "C83": (46.0, 55.5, 0),
    "R63": (13.5, 38.3, 0), "C27": (20.8, 39.0, 0), "C91": (18.5, 56.4, 0),   # (+ BAND; review pass 2: C27 / C91 tantalums)
    # (review pass 2: VBUS sense by the chip's bottom-left pin 43, GPIO38; absolute, not shifted)
    "R88": (25.4, 62.5, 90), "R89": (26.6, 62.5, 90),
    # (USB host mode, battery order, absolute: the TUSB320 in the pocket right of R3 under the USB-C, by CC1 / CC2; the
    #  host signal's cluster in the free band under the screen, on CV_RAIL's way to the CLOCK / RESET drivers; Q9 in
    #  the free row left of R63, both on the VUSB -> VBUS link)
    "R101": (41.4, 10.0, 90), "R102": (30.4, 10.0, 90),   # (U17's CC links by R3 / R4, the escape hatch)
    "R94": (43.2, 10.0, 90), "R95": (42.3, 12.7, 90),
    "U17": (50.2, 55.8, 0), "C95": (50.2, 58.4, 0), "R103": (53.0, 55.0, 0), "R104": (53.0, 56.6, 0), "R105": (53.0, 58.2, 0),
    # (the band: x 30.9 .. 33 stays free for the drawn USB pair, U5 -> R24 / R25)
    "Q7": (35.2, 44.9, 0), "R90": (34.6, 47.7, 0), "R92": (36.6, 47.7, 0),
    "Q8": (38.9, 44.9, 0), "R91": (38.6, 47.7, 0),
    "R93": (43.6, 46.0, 0), "RT93": (43.6, 43.9, 0), "D16": (48.6, 44.9, 0),
    "Q9": (18.5, 52.3, 0),                # (in R63's old place; R63 moved left into the free row)
    "R39": (24.3, 5.0, 0), "RT39": (24.35, 7.1, 0),   # (72 x 72: right of J7, its twin under it)
    "R66": (31.4, 31.1, 0), "C67": (31.4, 32.5, 0), "R65": (31.4, 33.9, 0),
    "R21": (52.6, 2.8, 0), "R27": (52.6, 5.4, 0), "R49": (52.6, 8.0, 0), "R20": (52.6, 14.5, 0), "RT20": (52.6, 16.7, 0),
    # (stage 2: the XB5353A by the sockets, in the cell's minus lead: BAT_N stays 5 mm long)
    "U9": (39.9, 36.45, 0), "C42": (40.0, 38.9, 0),
    "U8": (14.0, 48.0, 0), "R51": (7.5, 46.5, 0), "RT51": (7.5, 48.45, 0), "C41": (14.0, 44.5, 0), "C40": (10.0, 44.5, 0),
    "D3": (6.0, 42.0, 0), "Q1": (19.5, 44.3, 0),    # (2026-11-15: Q1, the AO3401A load-sharing switch, back in U15's place)
    "C88": (19.5, 41.2, 0),                         # (review pass 2, L2: U15's output cap)
    "R86": (16.2, 44.5, 90),                        # (review pass 1: C41's 1 ohm, beside it)
    "D9": (13.0, 41.6, 0),                 # (review pass 2: an SS34 (SMA) beside U15, BAT_P -> VSYS)
    # the 3.3 V LDO (review pass 2: a TLV757P, WSON-6, both orders): C3 right of IN (pin 6), C5 / C6 left of OUT
    # (pin 1); D15 (OUT -> IN) under it; C72 / C92 (VSYS bulk, battery order) at the left edge
    "U2": (10.0, 54.0, 0),
    "C3": (10.0, 50.9, 0), "C5": (7.4, 53.6, 90), "C6": (5.9, 53.6, 90), "C92": (2.6, 53.75, 90),   # (2026-11-15: C3 above U2)
    # (stage 2: the cell's divider and its twins in the middle, between the header and the sockets)
    "R10": (41.7, 18.8, 0), "RT10": (41.7, 20.45, 0), "R11": (41.7, 22.8, 0), "RT11": (41.7, 24.45, 0),
    "R16": (44.0, 52.5, 90),                 # (stage 2: by SW1 / NO, off the LINE IN block and the moved u.FL)
    # the chip (turned 180: LNA_IN at the bottom of its right side) and its parts round it
    "U1": (MOD_XY[0], MOD_XY[1], 180) if MODULE else (CX, CY, 180),
    "U5": (W / 2, 11.1, 0),       # (2026-11-15: the Hongjiacheng SRV05-4's land, 90 from the ProTek's: the same pads at 0)                                         # USB ESD, right under the USB-C's data pads
    # (stage 1e: the SRV05-4's JLC footprint is the USBLC6's pattern turned 90, its pad rows 0.2 mm further out: at 270
    #  and 0.2 mm lower its top row (VBUS, D-, D+) sits where the USBLC6's did, so the drawn USB tracks and VBUS via hold)
    # USB D+ / D- 22 ohm (stage 2: upright side by side over pins 27 / 25, their chip pads down: the channel over pins
    # 21 .. 24 stays open for LIN_DET and MIDI_TX2, both on the top side)
    "R24": (CX - 2.3, CY - 6.3, 270), "R25": (CX - 1.3, CY - 6.3, 270),
    "C20": (CX + 1.1, CY - 4.85, 90),       # (stage 2: upright over pins 19 / 20: pins 15..17's channel north stays open)                                                  # VDD3P3_RTC (pin 20, top); mini 2: turned, its +3V3 pad east (the GND pad was boxed in by MIDI2_TX)
    "C21": (13.9 + S, 27.1, 270), "C22": (13.9 + S, 28.9, 270),   # (72 x 72: up by pin 29, off pins 38 .. 42's way out)                       # VDD_SPI (pin 29, left)
    "C19": (CX - 1.4, CY + 4.85, 270),      # VDD3P3_CPU (pin 46, bottom): its +3V3 pad toward the pin (review 2026-11-13)
    "C13": (CX + 2.94, CY + 4.9, 270), "C12": (CX + 3.87, CY + 4.9, 270), "C11": (CX + 5.9, CY + 4.6, 90),   # VDDA
    "C28": (CX + 5.42, CY + 2.0, 0),         # 100 nF at the RF supply's pins (2, 3), 0.6 mm out: VDD3P3's via between
    "L2": (CX + 9.1, CY - 0.2, 270), "C18": (CX + 9.8, CY + 1.5, 0),                   # VDD3P3: the 2 nH, 100 nF after it
    "C14": (30.0 + S, 32.2, 90), "C15": (31.0 + S, 32.2, 90),
    "Y1": (19.5 + S, CY + 7.9, 180), "R17": (CX + 2.0, CY + 4.85, 90), "C10": (16.45 + S, 37.7, 270), "C9": (22.55 + S, CY + 8.5, 90),
    # (review pass 1, LOW 2: C9 the mirror of C10 through Y1's centre: each load cap the same distance from its pad)
    # RF (README "RF"): the chip's pi (C23 shunt on the line at the pin, its ground end to a via; L3 series; C24 shunt),
    # the line down to the antenna's pi (C25 shunt, R18 the series 0 ohm: DNP, C26 shunt) at the ground edge; R19 (fitted) / J4
    # (fitted) the u.FL path, the default (the user's ruling 2026-11-11); the unfitted pads right against the line (their
    # stubs as short as the pads allow)
    "C23": (CX + 4.8, Y_LINE + 0.85, 270), "L3": (CX + 6.5, Y_L3, 0), "C24": (XA + 0.92, Y_L3 + 1.2, 0),
    # R19 at the line's end (stage 1e: the line ends there; the chip antenna, its pi and R18 are gone), J4 right beside it
    # (stage 2, the user's: the u.FL right after the match, not at the old antenna strip's edge: a ~4 mm line)
    # (review pass 1, M3: one straight 50 ohm line down x = XA from L3's corner: C24 and C26 across its right gap (their
    #  signal pad on the line's edge, the ground pad on the pour), R19 in it, J4's signal pad at its end)
    "R19": (XA, Y_L3 + 2.7, 270), "C26": (XA + 0.92, Y_L3 + 4.1, 0), "J4": (XA, Y_L3 + 6.66, 180),
    # mini 2, LINE IN (README "LINE IN"): J7 on the top edge (its bushing up) between J2 and the USB-C, tip / ring pads on its
    # right; D7 by them; the summer's inputs (R30 / C30 tip, R31 / C31 ring) to U6 (turned: IN- and IN+ on top, OUT and V+
    # below), R32 // C32 on its left from IN- to OUT, the bias (R33 / R34 / C33) on its right; the LDO (U7, C34 in, C35 out)
    # in the top strip by the USB-C (VBUS); R35 / C36 at the chip's GPIO9 (pin 14), the right side's top corner
    # stage 2: J7 a PJ-307C (tip pin 5 on the right, ring pin 2 on the left, the ring's switch pin 3: LIN_DET); each TVS
    # by its pin (D7 tip, upright right of J7; D8 ring, under J2 / J7's legs); the front end in a block of its own in the
    # board's empty middle-left (x 21.5 .. 29.5, y 16.5 .. 31: under the screen module, all low parts), 12 mm from the
    # chip, away from the RF corner, each resistor with its twin pads; its LDO (U7) in the block (VSYS in, AVDD short)
    "J7": (18.0, 6.5, 180, "B"),   # (review pass 2, L9: 0.5 mm in, 0.4 right of J2)
    "D7": (17.4, 15.8, 0), "D8": (17.4, 12.45, 0),
    "D14": (25.0, 12.9, 0),        # (review pass 2, L7: the sleeve's TVS, by R39; + the x shift: 28.0)
    # (stage 2, the hand-solder twins: the inputs, pull-downs and detect with their twins in a column left of the USB
    #  pair, AVDD's LDO beside them; the summer, its gain and bias with their twins right of the pair, by the chip's
    #  LINE_IN pin: only LIN_SUM and AVDD cross the pair)
    "R30": (19.95, 19.5, 0), "RT30": (19.95, 21.15, 0), "R31": (19.95, 23.5, 0), "RT31": (19.95, 25.15, 0),
    "R36": (19.95, 27.5, 0), "RT36": (19.95, 29.15, 0), "R37": (19.95, 31.5, 0), "RT37": (19.95, 33.15, 0),
    "R38": (19.95, 35.5, 0), "RT38": (19.95, 37.15, 0),
    "C30": (24.4, 19.5, 0), "C31": (24.4, 23.5, 0), "C34": (24.4, 27.6, 0), "U7": (25.0, 31.0, 0), "C35": (25.0, 34.0, 0),
    "C37": (26.3, 17.4, 0),
    # (review pass 1, LOW 1: U6 / C32 / C33 0.6 mm right: LIN_SUM's pads 1.2 mm from the drawn USB pair, was 0.65)
    "U6": (32.0, 24.8, 90), "C32": (32.0, 28.3, 0), "C33": (32.0, 29.7, 0),
    "R32": (36.6, 22.6, 0), "RT32": (36.6, 24.25, 0), "R33": (36.6, 26.6, 0), "RT33": (36.6, 28.25, 0),
    "R34": (41.7, 26.9, 0), "RT34": (41.7, 28.55, 0),
    # (review pass 1: LINE_IN's column straight: C36 turned, its GND pad to the right, R35 turned, its LINE_IN pad down
    #  next to C36's: LINE_IN no longer detours over pins 15 .. 17, whose channel north UP / SCL / SDA need)
    # (C36 / R35: auto_place by the module's LINE_IN pin, 2026-11-15)
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
FANOUT = [] if MODULE else [("CHIP_PU", CY + 1.4, CX + 7.3, CY - 4.2, True), ("BOOT", CY + 1.0, CX + 6.5, CY - 4.2, True),
          ("RST_IO", CY + 0.6, CX + 5.7, CY - 4.2, True), ("CLK_IO", CY + 0.2, CX + 4.9, CY - 4.2, True),
          ("VBAT_ADC", CY - 0.6, CX + 4.1, CY - 4.2, True)]
# (stage 2: pins 6 / 7 / 9 carry the charge pump, CLOCK and the cell's level: parts.py GPIO; YES / DOWN moved to the
#  chip's left side)
# (mini 2.2: GPIO1's lane carries the cell's level; stage 2: mini 2's VBUS_SENSE / PWR_HOLD lanes (pins 7 / 9) carry UP / DOWN)

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
PLACED = {   # (mini 2.2 simplified: only the MIDI OUT resistors and BOOT's pull-up are left of the placer's groups)
}

# the spread (72 x 65, 2026-11-12): parts placed at absolute y below the chip's top follow the chip down 7.5 mm; the
# buttons' and the start-up group's row 4 mm; the start-up group (absolute x) follows the middle 5 mm right
SPREAD_Y = {r: CY - 30.4 for r in ("C21", "C22", "C16", "C14", "C15", "C17", "C10")}   # (mini 2.2 simplified: the
#  chip block's parts only; the buck, the start-up group and MIDI OUT 2's driver are gone)
SPREAD_X = {}


# stage 2: the lower block's parts placed at fixed spots (not from CY or BTN_C): they move down the band with the chip
LOWER = {"Q1", "C27", "C91", "C88", "D15", "C92", "C3", "C40", "C41", "C5", "C6", "C72", "C81", "C82", "C83", "D3", "D9", "U15", "R86",
         "R16", "R51", "R63", "RT51", "U2", "U8"}
# (the cell's protection, U9 / C42 / R52 / Q2 / R53, stays by the sockets above the band; C80 moves by hand)


XS_SKIP = {"LED1", "LED2", "LED3", "LED4", "C8", "R120", "RT120", "R121", "RT121", "U18", "L5", "C99", "C100", "C101", "L4", "R101", "R102", "R103", "R104", "R105", "R97", "U17", "C95", "R95", "U16", "C94", "C93", "Q7", "R90", "R92", "Q8", "R91", "R93", "RT93", "D16", "R94", "Q9", "U12", "C26", "J11", "R62", "J10B", "C86", "R88", "R89", "C10", "C11", "C12", "C13", "C14", "C15", "C16", "C17", "C18", "C19", "C20", "C21", "C22", "C23", "C24",
           "C28", "C36", "C7", "C8", "C9", "J1", "J4", "L2", "L3", "R17", "R19", "R24", "R25", "R3", "R35", "R4", "R8",
           "U1", "U5", "Y1", "J3", "J6", "R56", "R57", "R59", "RT57", "J10", "SW1", "SW3", "SW4", "SW5"}
# the parts placed by hand round the chip for the 6 mm band (the pump, CLOCK / RESET, J12): with the chip down BAND - 6
# more, and right by their x shift (the pump with the chip's DX2, the drivers with the right side's 2 x DX2)
MID = {**{r: DX2 for r in ("R83", "C84", "D12", "D13", "C85", "R84", "R73", "RT73", "U13", "C87", "R87")},
       **{r: 2 * DX2 for r in ("Q3", "Q4", "D10", "Q5", "Q6", "D11", "R75", "R76", "R81", "R82", "R74", "R80", "R71", "R77",
                                "RT79", "R79", "C89", "C90")}}


def place():
    fps = {}
    for ref, _k, value, fp, _lcsc, pins in P.PARTS:
        if ref in P.TWINS and ref not in PLACE and ref not in FLOOR:   # (stage 2: the twins go beside their
            continue                                                    #  resistors, place_twins())
        x, y, rot, side = ((tuple(FLOOR[ref]) if ref in FLOOR else PLACE[ref]) + ("T",))[:4]   # (buffer try: new refs live in floorplan.json only)
        x, y = x + SPREAD_X.get(ref, 0.0), y + SPREAD_Y.get(ref, 0.0)
        if ref in LOWER:                         # (stage 2: the band, BAND_Y)
            y += BAND
        if ref in MID:                           # (stage 2: placed by hand for the 6 mm band: they keep their place
            x, y = x + MID[ref], y + BAND - 6.0  #  over the chip's block)
        elif ref not in XS_SKIP:
            x += 0.0 if x < 20.0 else (DX2 if x < 41.0 else 2 * DX2)
        if ref in PLACED and not os.environ.get("NO_PLACED"):   # (NO_PLACED=1: the spread, the placer's start)
            x, y, rot = PLACED[ref]
            y += DY                              # (mini 2: below the LINE IN band)
        if ref in FLOOR:                         # (the fresh floorplan: absolute, on top)
            x, y, rot = FLOOR[ref]
            side = "T"
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
            print(f"U1 exposed pad: paste {windowpane(fps['U1'], p) * 100:.0f} % of the pad")
    auto_place(fps)
    place_twins(fps)
    return fps


# stage 2 (the CLOCK / RESET drivers, the LED resistors moved off the top edge for the jacks): no fixed spot; each goes,
# in this order, to the free spot nearest its target (its courtyard 0.15 mm from the others', 0.35 from a twin's, out
# of the iron rooms, FORBID, the pivot circle and 0.6 mm inside the edges), its twin at once beside it (place_twins)
AUTO = [("R40", (10.0, 39.0)),   # (2026-11-15: the RGB LEDs' data resistor; no twin)
        ("R145", (20.0, 60.0)),   # (R145: U19's series resistor, review pass 3; the placer places it)
        # (experiment A, 2026-11-15: the new parts' first spots, by their partners; the placer places them)
        ("R146", (13.0, 62.5)), ("R147", (7.0, 18.0)), ("R148", (11.0, 33.0)), ("R149", (62.5, 16.0)), ("R150", (64.5, 47.0)),
        ("R151", (17.0, 46.5)), ("R152", (22.5, 53.5)),
        ("R153", (50.3, 47.5)), ("R154", (19.4, 67.3)),   # (chain list 4)
        ("C102", (18.0, 57.0)), ("C103", (25.5, 57.0)),   # (the beat LEDs 3 / 4's 100 nF)
        # (the module, 2026-11-15: its 3V3 caps by pin 2, the EN RC by pin 3, the cell ADC's cap by pin 4 (the module's
        #  right side, turned 180), LINE_IN's R35 / C36 above pin 17)
        ("C17", (49.6, 69.5)), ("C16", (49.0, 71.0)), ("C7", (50.5, 59.3)), ("R8", (50.5, 60.6)),
        # (the digital LINE IN, 2026-11-15: each node's parts by its module pin, A (LINE_IN) over pin 17, B (LINE_SENSE)
        #  right of pin 12; the bias divider R33 / R34 stays by the jack, its C33 by the nodes)
        ("C32", (43.0, 50.9)), ("R32", (41.2, 50.9)), ("C30", (39.4, 50.9)), ("C31", (37.6, 50.9)), ("R30", (39.4, 49.6)),
        ("R31", (37.6, 49.6)), ("C33", (45.3, 50.9)),
        ("C67", (49.0, 56.4)), ("R67", (49.0, 55.0)), ("C65", (50.8, 55.0)), ("C66", (50.8, 56.4)), ("R65", (52.6, 55.0)),
        ("R66", (52.6, 56.4)),
        # (2026-11-15, the power block: U2's fallback LDO land, U18 / U2's feedback, enable and mode parts, the clamp, the
        #  aux rail's group links)
        ("R50", (22.6, 58.0)), ("U19", (26.5, 69.0)), ("U2B", (44.0, 31.0)),
        ("R134", (21.0, 66.2)), ("R135", (21.0, 67.3)), ("R136", (21.0, 68.4)), ("R131", (18.0, 66.0)), ("R137", (18.0, 67.2)),
        ("R138", (26.5, 68.0)), ("R139", (26.5, 69.2)), ("R132", (27.5, 61.0)), ("R122", (24.0, 59.8)), ("R123", (24.0, 58.6)),
        ("R133", (27.5, 63.5)),
        ("R130", (7.0, 70.5)), ("R140", (7.0, 71.0)), ("R141", (8.0, 65.0)), ("R142", (8.0, 64.0)), ("R143", (5.5, 70.5)),
        ("R144", (5.5, 71.0)),
        # (the MIDI loops' FETs and their lands, by their sinks; the CLK / RST sleeve resistors by J13 / J14)
        ("Q10", (12.5, 18.5)), ("Q11", (9.5, 36.5)), ("Q12", (61.0, 17.5)), ("Q13", (62.5, 36.5)),
        ("R124", (13.0, 21.0)), ("R126", (12.0, 22.0)), ("R127", (9.5, 38.5)), ("R125", (60.0, 20.0)), ("R128", (61.0, 21.0)),
        ("R129", (62.5, 38.5)), ]
AUTO = [(r, xy) for r, xy in AUTO if r in {p[0] for p in P.PARTS}]   # (buffer try: the removed refs dropped)
AUTO_REACH = 16.0


def _fits(b, fps, rooms, skip, gap=0.15):
    if b[0] < 0.6 or b[1] < 0.6 or b[2] > W - 0.6 or b[3] > H - 0.6:
        return False
    for r, f in fps.items():
        if r == skip or f.IsFlipped():
            continue
        if _box_dist(b, top_box(f)) < (0.35 if r in P.TWINS else gap):
            return False
    if any(_box_dist(b, fb) < 0.05 for fb in FORBID):
        return False
    if math.hypot(max(b[0] - BTN_C[0], 0, BTN_C[0] - b[2]), max(b[1] - BTN_C[1], 0, BTN_C[1] - b[3])) < PIVOT_R + 0.3:
        return False
    return not any(math.hypot(max(b[0] - px, 0, px - b[2]), max(b[1] - py, 0, py - b[3])) < pr for px, py, pr in rooms)


def auto_place(fps):
    if os.environ.get("NO_AUTO"):                # (NO_AUTO=1: leave them off the board, to see the room)
        return
    rooms = iron_rooms(fps)
    for ref, (tx, ty) in AUTO:
        if ref in FLOOR:
            continue
        f = fps[ref]
        best = None
        for rot in (0, 90):
            f.SetOrientationDegrees(rot)
            f.SetPosition(pcbnew.VECTOR2I(MM(OX), MM(OY)))
            b0 = top_box(f)
            n = int(AUTO_REACH / 0.25)
            for i in range(-n, n + 1):
                for j in range(-n, n + 1):
                    d = math.hypot(i, j) * 0.25
                    if d > AUTO_REACH or (best and d >= best[0]):
                        continue
                    x, y = tx + i * 0.25, ty + j * 0.25
                    if _fits((b0[0] + x, b0[1] + y, b0[2] + x, b0[3] + y), fps, rooms, ref):
                        best = (d, x, y, rot)
        if best is None:
            print(f"AUTO: no room for {ref} within {AUTO_REACH} mm of {tx, ty}")
            continue
        _d, x, y, rot = best
        f.SetOrientationDegrees(rot)
        f.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
        twins = [t for t, (m, _w) in P.TWINS.items() if m == ref]
        if twins:
            place_twins(fps, only=set(twins))


TWIN_TRACK = 0.25           # (the two tracks joining a twin to its resistor)


def top_box(f):
    b = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd).BBox()
    return (pcbnew.ToMM(b.GetLeft()) - OX, pcbnew.ToMM(b.GetTop()) - OY, pcbnew.ToMM(b.GetRight()) - OX,
            pcbnew.ToMM(b.GetBottom()) - OY)


def iron_rooms(fps):
    """(x, y, r): every through-hole pad's copper + IRON (the DINs, the jacks, the sockets, the header): no top part there"""
    out = []
    for f in fps.values():
        for p in f.Pads():
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                out.append((*pad_center(p), max(pcbnew.ToMM(p.GetSize().x), pcbnew.ToMM(p.GetSize().y)) / 2 + IRON))
            elif p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:        # (stage 2: a jack's plastic peg's hole: no part on it)
                out.append((*pad_center(p), pcbnew.ToMM(p.GetDrillSize().x) / 2 + 0.5))
    return out


def _box_dist(a, b):
    return math.hypot(max(a[0] - b[2], b[0] - a[2], 0), max(a[1] - b[3], b[1] - a[3], 0))


def _seg_hits_box(p, q, b, pad=0.0):
    for k in range(21):
        x, y = p[0] + (q[0] - p[0]) * k / 20, p[1] + (q[1] - p[1]) * k / 20
        if b[0] - pad < x < b[2] + pad and b[1] - pad < y < b[3] + pad:
            return True
    return False


def _pad_box(q):
    b = q.GetBoundingBox()
    return (pcbnew.ToMM(b.GetLeft()) - OX, pcbnew.ToMM(b.GetTop()) - OY, pcbnew.ToMM(b.GetRight()) - OX,
            pcbnew.ToMM(b.GetBottom()) - OY)


def _new_twin(t, x, y, rot, pins):
    fp = P.FP["twin_c" if t.startswith("CT") else "twin"]
    f = pcbnew.FootprintLoad(LIB, fp)
    f.SetFPID(pcbnew.LIB_ID("chip", fp))
    f.SetReference(t)
    f.SetValue("twin DNP")
    f.Reference().SetVisible(False)
    f.Value().SetVisible(False)
    f.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
    f.SetOrientationDegrees(rot)
    for q in f.Pads():
        q.SetNet(net(pins[q.GetNumber()]))
    return f


TWIN_REACH = {"RT39": 9.0}   # (mm per twin; the default 6: a longer stub, on XTAL_P / XTAL_N or the ADC pin, is worse than none)


TWINS_DONE = set()
# 72 x 72 (the coordinator's: DNP twins may go on the BOTTOM: nothing there is assembled): these sit under their part on
# the bottom, the same way round; GND by the bottom pour, the other pad by a bottom track to a via TWIN_VIA mm out
# along the part's axis past the twin's pad (no via on a twin's pad) and a top track from the via to the part's pad
BOTTOM_TWINS = {"CT8", "CT9", "CT10"}
TWIN_VIA = 2.0
BOTTOM_SHIFT = {}                  # (review pass 1: CT9 right under C9 again, C9 now C10's mirror)
# the via beside the pad instead of out along the axis (mm from the part's pad): C8's would sit on the fan-out lanes'
# vias, C10's on C19's ground pad
BOTTOM_VIA = {"CT8": (1.3, 0.0), "CT10": (-1.3, 0.0)}   # (mm: C9's own ground via, 0.7 mm right of its pad, would sit on CT9's pad)


def bottom_twin(fps, t, m, pins):
    fm = fps[m]
    cx, cy = pcbnew.ToMM(fm.GetPosition().x) - OX, pcbnew.ToMM(fm.GetPosition().y) - OY
    sx, sy = BOTTOM_SHIFT.get(t, (0.0, 0.0))
    f = _new_twin(t, cx + sx, cy + sy, 0, pins)
    board.Add(f)
    f.Flip(f.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
    f.SetOrientationDegrees(fm.GetOrientationDegrees())
    f.SetDNP(True)
    f.SetExcludedFromBOM(True)
    f.SetExcludedFromPosFiles(True)
    fps[t] = f
    for q in fm.Pads():
        n = pins[q.GetNumber()]
        if n == "GND":
            continue                                  # (the bottom pour)
        tq = next(z for z in f.Pads() if z.GetNumber() == q.GetNumber())
        px, py = pad_center(q)
        tx, ty = pad_center(tq)
        ux, uy = px - cx, py - cy
        L = math.hypot(ux, uy) or 1.0
        vx, vy = cx + ux / L * TWIN_VIA, cy + uy / L * TWIN_VIA
        if t in BOTTOM_VIA and not FLOOR:       # (the fresh floorplan: the via 2.0 mm out past the pad, as the placer
            vx, vy = px + BOTTOM_VIA[t][0], py + BOTTOM_VIA[t][1]   #  keeps it clear)
        add_track(n, 0, px, py, vx, vy, TWIN_TRACK)
        add_via(n, vx, vy)
        add_track(n, 1, vx, vy, tx, ty, TWIN_TRACK)


def place_twins(fps, only=None):
    """stage 2: each twin (parts.TWINS, a hand-solder land: make_footprints.twin) not placed from PLACE goes to the
    nearest free spot within 6 mm of its resistor (turned as it or 90 more): its copper 1.0 mm or more from every other
    part (their courtyards + 0.35: the twin's own courtyard is its pads + 0.5), out of every through-hole pad's iron
    room and FORBID, 0.6 mm inside the edges; its two top tracks to the resistor's pads (pad n to pad n) cross no
    other part and keep 0.45 mm from each other. Every twin then gets its two tracks (no via): parts.py, the twins"""
    rooms = iron_rooms(fps)
    boxes = {r: top_box(f) for r, f in fps.items() if not f.IsFlipped()}
    for t, (m, _why) in P.TWINS.items():
        if t in TWINS_DONE or (only is not None and t not in only):
            continue
        if t in BOTTOM_TWINS:
            TWINS_DONE.add(t)
            bottom_twin(fps, t, m, dict(next(q[5] for q in P.PARTS if q[0] == t)))
            continue
        if only is None and m in dict(AUTO) and m not in FLOOR:
            continue                             # (placed with its resistor by auto_place)
        TWINS_DONE.add(t)
        fm = fps[m]
        pins = dict(next(q[5] for q in P.PARTS if q[0] == t))
        if t not in fps:
            best = None
            why = collections.Counter()
            cx, cy = pcbnew.ToMM(fm.GetPosition().x) - OX, pcbnew.ToMM(fm.GetPosition().y) - OY
            mp = {q.GetNumber(): pad_center(q) for q in fm.Pads()}
            mpb = {q.GetNumber(): _pad_box(q) for q in fm.Pads()}
            mbox = top_box(fm)
            for rot in (fm.GetOrientationDegrees(), fm.GetOrientationDegrees() + 90):
                probe = _new_twin(t, 0, 0, rot, pins)
                pb0 = top_box(probe)
                pads0 = {q.GetNumber(): (pad_center(q), _pad_box(q)) for q in probe.Pads()}
                n = int(TWIN_REACH.get(t, 6.0) / 0.25)
                for i in range(-n, n + 1):
                    for j in range(-n, n + 1):
                        x, y = cx + i * 0.25, cy + j * 0.25
                        b = (pb0[0] + x, pb0[1] + y, pb0[2] + x, pb0[3] + y)
                        if b[0] < 0.6 or b[1] < 0.6 or b[2] > W - 0.6 or b[3] > H - 0.6:
                            why["edge"] += 1
                            continue
                        if any(r != m and _box_dist(b, o) < 0.35 for r, o in boxes.items()):
                            why["parts"] += 1
                            continue
                        if any(_box_dist(b, fb) < 0.05 for fb in FORBID):
                            why["forbid"] += 1
                            continue
                        if any(math.hypot(max(b[0] - px, 0, px - b[2]), max(b[1] - py, 0, py - b[3])) < pr for px, py, pr in rooms):
                            why["iron"] += 1
                            continue
                        tp = {n: ((c[0] + x, c[1] + y), (bb[0] + x, bb[1] + y, bb[2] + x, bb[3] + y)) for n, (c, bb) in pads0.items()}
                        if any(_box_dist(tp[n][1], mpb[k]) < 0.3 for n in tp for k in mpb if k != n):
                            why["own pads"] += 1
                            continue
                        mc = ((mbox[0] + mbox[2]) / 2, (mbox[1] + mbox[3]) / 2)
                        if any(_box_dist(tp[n][1], mbox) < 0.1 for n in tp) or (b[0] < mc[0] < b[2] and b[1] < mc[1] < b[3]):
                            why["on its resistor"] += 1      # (never over its own part: a part on the twin must sit flat)
                            continue
                        segs = [(mp[n], tp[n][0]) for n in ("1", "2")]
                        if any(_seg_hits_box(s[0], s[1], o) for s in segs for r, o in boxes.items() if r != m):
                            why["track hits a part"] += 1
                            continue
                        if any(_seg_hits_box(s[0], s[1], fb) for s in segs for fb in FORBID):
                            why["track in forbid"] += 1
                            continue
                        if any(_seg_hits_box(s[0], s[1], (px - pr, py - pr, px + pr, py + pr)) for s in segs
                               for px, py, pr in rooms):
                            why["track over a through-hole pad"] += 1     # (72 x 72: RT39's crossed J7's tip pad)
                            continue
                        if any(_seg_hits_box(segs[a][0], segs[a][1], bx, 0.33) for a, n in ((0, "2"), (1, "1"))
                               for bx in (mpb[n], tp[n][1])):
                            why["tracks cross"] += 1
                            continue
                        cost = math.hypot(i, j) * 0.25 + sum(math.dist(*s) for s in segs)
                        if best is None or cost < best[0]:
                            best = (cost, x, y, rot, b)
            if best is None:
                print("TWIN NOT PLACED", t, "beside", m, f"({cx:.1f}, {cy:.1f}):", dict(why.most_common()))
                continue
            _c, x, y, rot, b = best
            f = _new_twin(t, x, y, rot, pins)
            board.Add(f)
            f.SetDNP(True)
            f.SetExcludedFromBOM(True)
            f.SetExcludedFromPosFiles(True)
            fps[t] = f
            boxes[t] = b
        for q in fm.Pads():                          # (pad n to pad n)
            tq = next(z for z in fps[t].Pads() if z.GetNumber() == q.GetNumber())
            add_track(pins[q.GetNumber()], 0, *pad_center(q), *pad_center(tq), TWIN_TRACK)


EP_VIAS = [(dx, dy) for dx in (-1.8, 0.0, 1.8) for dy in (-1.8, 0.0, 1.8)]   # the exposed pad's 9 vias (Espressif: 9 or more;
#  2026-11-15: 1.8 mm apart on the FH4R2's 5.7 mm pad, was 1.3 on the FN8's 4.0)
# the paste (2026-11-12 review: 16 % was low; review pass 1: 35 % with the strips only, ~50 % usual): a lattice, the
# via-free rows and columns between the via rings (each ring + EP_EDGE), every strip EP_TRIM narrower on its inner sides
EP_EDGE, EP_TRIM = 0.05, 0.3          # (2026-11-15: 0.3 on the FH4R2's 5.7 mm pad: 0.1 gave 74 %)


def windowpane(f, ep):
    """the exposed pad's paste as a lattice round its vias (a whole-pad paste would float the QFN and fill the vias):
    the pad itself loses its paste layer; paste-only strips, across and down, are added to the footprint (where they
    cross the stencil opens once). Returns the paste's share of the pad"""
    ls = ep.GetLayerSet()
    ls.RemoveLayer(pcbnew.F_Paste)
    ep.SetLayerSet(ls)
    c = ep.GetPosition()
    h = pcbnew.ToMM(min(ep.GetSize().x, ep.GetSize().y)) / 2 - EP_EDGE
    keep = route.VIA_D / 2 + EP_EDGE              # (the via rings + 0.05)
    cols = sorted({dx for dx, _ in EP_VIAS})
    edges = [-h] + [e for x in cols for e in (x - keep, x + keep)] + [h]
    free = [(edges[i] + (EP_TRIM if i else 0), edges[i + 1] - (EP_TRIM if i + 2 < len(edges) else 0))
            for i in range(0, len(edges), 2)]     # (the via-free bands, trimmed on their inner sides)
    for b0, b1 in free:
        for w, hh, x, y in ((2 * h, b1 - b0, 0.0, (b0 + b1) / 2), (b1 - b0, 2 * h, (b0 + b1) / 2, 0.0)):
            a = pcbnew.PAD(f)
            a.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
            a.SetShape(pcbnew.PAD_SHAPE_RECT)
            a.SetSize(pcbnew.VECTOR2I(MM(w), MM(hh)))
            pl = pcbnew.LSET()
            pl.AddLayer(pcbnew.F_Paste)
            a.SetLayerSet(pl)
            a.SetNumber("")
            a.SetPosition(pcbnew.VECTOR2I(c.x + MM(x), c.y + MM(y)))
            f.Add(a)
    hs = sum(b1 - b0 for b0, b1 in free)
    return (2 * (2 * h) * hs - hs * hs) / pcbnew.ToMM(ep.GetSize().x) / pcbnew.ToMM(ep.GetSize().y)


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


def label(text, x, y, size=0.8, rot=0.0, layer=pcbnew.F_SilkS):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
    t.SetTextAngleDegrees(rot)
    t.SetLayer(layer)
    if layer == pcbnew.B_SilkS:                   # (bottom silk reads right way round from below)
        t.SetMirrored(True)
    size = max(size, 0.8)                         # (stage 2: JLCPCB's minimum, 0.8 mm text with a 0.15 mm stroke)
    t.SetTextSize(pcbnew.VECTOR2I(MM(size), MM(size)))
    t.SetTextThickness(MM(0.15))
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


def usb_side(fps):
    """the USB-C's D+ / D- bridge, U5's flow-through and REF via, the pair down the corridor to R24 / R25"""
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
    u5v = pad_center(pad_of(fps, "U5", "5"))      # (U5's VBUS pin sits between D+ and D-: fed through a via above it,
    vb = (u5v[0], u5v[1] - 1.0)                     # from the bottom, so VBUS never crosses the data lines on top)
    RF_SEGS.append(("+3V3", 0, vb[0], vb[1], u5v[0], u5v[1], 0.25))   # (review pass 2: U5's REF on +3V3)
    FIXED_VIAS.append(("+3V3", vb[0], vb[1]))
    u1p, u3p = pad_center(pad_of(fps, "U5", "1")), pad_center(pad_of(fps, "U5", "3"))
    RF_SEGS.append(("UDP", 0, u6[0], u6[1], u1p[0], u1p[1], 0.2))     # (U5's flow-through: pin 6 -> 1, 4 -> 3, under it)
    RF_SEGS.append(("UDN", 0, u4[0], u4[1], u3p[0], u3p[1], 0.2))
    if FLOOR:                                    # (the fresh floorplan: the router takes the pair from U5 to R24 / R25)
        FIXED_VIAS.append(("UDP", v2[0], v2[1]))
        return
    # stage 2: the pair from U5's chip-side pins (1 D+, 3 D-) to R24 / R25's pad 1, drawn as a coupled pair (0.2 mm,
    # 0.2 mm apart) on top: down under U5, west under R4 and the screen header's left end, down the corridor between
    # the LINE IN block and the header (x 28.75 / 29.15), slanting into the pads (Freerouting took it 37 .. 44 mm round
    # the LINE IN block on the bottom, 3 vias)
    r24 = pad_center(pad_of(fps, "R24", "1"))
    r25 = pad_center(pad_of(fps, "R25", "1"))
    dy = 1.9                                                    # (the slants' height: clear of R25's pad)
    xp, xn, yp, yn, yd = 28.75 + DX2, 29.15 + DX2, 13.6, 14.0, r24[1] - dy   # (72 x 72: the corridor moved with the middle)
    ydn = r25[1] - (xn - r25[0]) * dy / (xp - r24[0])       # (D-'s slant parallel to D+'s)
    for n, pts in (("UDP", [u1p, (u1p[0], yp), (xp, yp), (xp, yd), r24]),
                   ("UDN", [u3p, (u3p[0], yn), (xn, yn), (xn, ydn), r25])):
        for a, c in zip(pts, pts[1:]):
            RF_SEGS.append((n, 0, a[0], a[1], c[0], c[1], 0.2))
    FIXED_VIAS.append(("UDP", v2[0], v2[1]))


def rf_path(fps):
    """LNA_IN from the pin through C23's pad to L3; the 50 ohm line (RF, then UFL past R19) straight down to J4"""
    del RF_SEGS[:], RF_VIAS[:], COPPER[:], FAN_VIAS[:], FIXED_VIAS[:]
    if MODULE:
        usb_side(fps)
        # the module's EPAD (pin 41, nine 0.9 mm pads): four GND vias round the group, on its top-layer pour (none in a pad)
        ep = [pad_center(p) for p in fps["U1"].Pads() if p.GetNumber() == "41"]
        x0, x1 = min(x for x, _ in ep), max(x for x, _ in ep)
        y0, y1 = min(y for _, y in ep), max(y for _, y in ep)
        for x, y in ((x0 - 1.3, y0 - 1.3), (x1 + 1.3, y0 - 1.3), (x0 - 1.3, y1 + 1.3), (x1 + 1.3, y1 + 1.3)):
            RF_VIAS.append((x, y))
        return
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
    # (stage 1e: the u.FL only: the line ends at R19's stub; the chip antenna, its pi and R18 are gone)
    r19 = pad_center(pad_of(fps, "R19", "1"))
    y_turn = Y_L3
    RF_SEGS.append(("RF", 0, l3b[0], y_turn, l3b[0] + RF_W / 2, y_turn, 0.5))   # (out of L3's pad, narrow)
    RF_SEGS.append(("RF", 0, l3b[0] + RF_W / 2, y_turn, XA, y_turn, RF_W))
    NECK = 0.85                                  # (the last 0.85 mm into R19's pads 0.5 wide: a 1.2 mm end cap would reach
    RF_SEGS.append(("RF", 0, XA, y_turn, XA, r19[1] - NECK, RF_W))                #  the other pad, 0.86 mm on)
    RF_SEGS.append(("RF", 0, XA, r19[1] - NECK, XA, r19[1], 0.5))
    # (review pass 1: C24's signal pad sits on the line's edge, no stub; R19 in the line; the UFL line on at RF_W to
    #  J4's signal pad with C26's pad on its edge)
    r19b = pad_center(pad_of(fps, "R19", "2"))
    j4 = pad_center(pad_of(fps, "J4", "1"))
    RF_SEGS.append(("UFL", 0, XA, r19b[1], XA, r19b[1] + NECK, 0.5))
    RF_SEGS.append(("UFL", 0, XA, r19b[1] + NECK, XA, j4[1], RF_W))
    for ref in ("C24", "C26"):                   # (each shunt's signal pad joined to the line's edge)
        c = pad_center(pad_of(fps, ref, "1"))
        RF_SEGS.append(("RF" if ref == "C24" else "UFL", 0, XA, c[1], c[0], c[1], 0.5))
    # the fences: GND vias both sides of the line, their rings 0.35 mm from its edge (0.15 inside the pour), down to
    # J4's signal pad (its ground pads take over there)
    off = RF_W / 2 + RF_GAP + route.VIA_D / 2 + 0.15
    fence_end = min(GE - 0.6, j4[1] - 0.3)
    rf_pads = [p for ref in ("C24", "R19", "C26", "J4") for p in fps[ref].Pads()]

    def clear_of_pads(x, y):                     # (a fence via 0.2 mm or more from the RF parts' pads)
        c = pcbnew.SHAPE_CIRCLE(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)), MM(route.VIA_D / 2))
        return not any(p.GetEffectiveShape(pcbnew.F_Cu).Collide(c, MM(0.2)) for p in rf_pads)
    for x in (XA - off, XA + off):
        y = y_turn + 1.2
        while y < fence_end:
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
    usb_side(fps)
    # C28's ground pad: boxed in by the lanes, VDD3P3 and LNA_IN; its own via right of it (0.36 from the pad, 0.3 from
    # CHIP_PU's lane, 0.53 from L3)
    c28g = pad_center(pad_of(fps, "C28", "2"))
    RF_SEGS.append(("GND", 0, c28g[0], c28g[1], CX + 6.75, CY + 1.95, 0.25))
    FIXED_VIAS.append(("GND", CX + 6.75, CY + 1.95))
    # the crystal's load cap C9: its ground pad gets its own via 0.7 mm past it (the routes boxed it in)
    c9g, c9s = pad_center(pad_of(fps, "C9", "2")), pad_center(pad_of(fps, "C9", "1"))
    gx, gy = c9g[0] + 1.1, c9g[1]                 # (beside it, off CT9's bigger pad under it)
    RF_SEGS.append(("GND", 0, c9g[0], c9g[1], gx, gy, 0.25))
    FIXED_VIAS.append(("GND", gx, gy))
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
    # (stage 2) the chip's USB pins (26 D+, 25 D-, the top side) straight up and over to their 22 ohm (R24 / R25 pad 2):
    # Freerouting left these 2 mm links open beside the busier pins 15..20; D+ runs left of D-, 0.2 mm apart
    for ref, pin in (("R24", "26"), ("R25", "25")):
        px, py = pad_center(pad_of(fps, "U1", pin))
        rx, ry = pad_center(pad_of(fps, ref, "2"))
        n = "USB_DP" if pin == "26" else "USB_DN"
        RF_SEGS.append((n, 0, px, py, px, ry + 0.75, 0.2))      # (stage 2: up, then a short slant into the pad)
        RF_SEGS.append((n, 0, px, ry + 0.75, rx, ry, 0.2))
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
    if not MODULE:
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
        if n in ("RF", "UFL"):                     # (the line's gap to anything else: RF_GAP, wider than the rules)
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


LAYERS = int(os.environ.get("LAYERS", "4"))   # (2026-11-16, the user's: 4 layers; LAYERS=2 for the old board)
LINE_IN_NETS = lambda n: n.startswith("LIN_") or n in ("LINE_IN", "LINE_SENSE")
IN2_ISLANDS = ("VSYS", "V5A", "VBUS")        # (In2's islands under their blocks; +3V3 is In2's main fill)


def plane(layer, netname, rect, priority=0, clearance=0.15, name=""):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(net(netname))
    z.SetAssignedPriority(priority)
    z.SetLocalClearance(MM(clearance))
    z.SetMinThickness(MM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)   # (as the outer pours: SMD solid, through-hole on spokes)
    z.SetThermalReliefGap(MM(0.5))
    z.SetThermalReliefSpokeWidth(MM(0.5))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    if name:
        z.SetZoneName(name)
    o = z.Outline()
    o.NewOutline()
    x0, y0, x1, y1 = rect
    for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        o.Append(MM(OX + x), MM(OY + y))
    board.Add(z)
    return z


def net_clusters(netname, link=6.0, grow=0.8):
    """the pads of a net in groups (single linkage, link mm apart); each group of 2+ pads as a box grown by grow mm"""
    pts = [pad_center(p) for f in board.GetFootprints() for p in f.Pads() if p.GetNetname() == netname]
    groups = []
    for q in pts:
        near = [g for g in groups if any(math.dist(q, r) <= link for r in g)]
        merged = [q] + [r for g in near for r in g]
        groups = [g for g in groups if g not in near] + [merged]
    out = []
    for g in groups:
        if len(g) < 2:
            continue
        xs, ys = [x for x, _ in g], [y for _, y in g]
        out.append((max(0.3, min(xs) - grow), max(0.3, min(ys) - grow), min(W - 0.3, max(xs) + grow), min(GE - 0.3, max(ys) + grow)))
    return out


def inner_planes():
    """4 layers (JLC's 1.6 mm default stack): In1 a solid GND plane; In2 +3V3 with VSYS / V5A / VBUS islands under their
    blocks; nothing of In2 under LINE IN's front end (the analog parts see In1's GND only)"""
    plane(pcbnew.In1_Cu, "GND", (0, 0, W, GE), name="In1 GND")
    plane(pcbnew.In2_Cu, "+3V3", (0, 0, W, GE), name="In2 +3V3")
    for n in IN2_ISLANDS:
        for k, r in enumerate(net_clusters(n)):
            plane(pcbnew.In2_Cu, n, r, priority=1, clearance=0.3, name=f"In2 {n} {k}")
    pts = [pad_center(p) for f in board.GetFootprints() for p in f.Pads()
           if LINE_IN_NETS(p.GetNetname()) and p.GetNetname() != "LIN_DET" and f.GetReference() != "U1"]
    groups = []                                  # (LINE IN's analog parts in groups 5 mm apart: the front end, the nodes)
    for q in pts:
        near = [g for g in groups if any(math.dist(q, r) <= 5.0 for r in g)]
        groups = [g for g in groups if g not in near] + [[q] + [r for g in near for r in g]]
    boxes = []
    for g in groups:
        xs, ys = [x for x, _ in g], [y for _, y in g]
        x0, y0, x1, y1 = min(xs) - 1.5, min(ys) - 1.5, max(xs) + 1.5, max(ys) + 1.5
        boxes.append((round(x0, 1), round(y0, 1), round(x1, 1), round(y1, 1)))
        k = pcbnew.ZONE(board)
        k.SetIsRuleArea(True)
        k.SetDoNotAllowTracks(False)
        k.SetDoNotAllowVias(False)
        k.SetDoNotAllowPads(False)
        k.SetDoNotAllowZoneFills(True)
        k.SetDoNotAllowFootprints(False)
        k.SetLayer(pcbnew.In2_Cu)
        k.SetZoneName("no power under LINE IN")
        o = k.Outline()
        o.NewOutline()
        for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
            o.Append(MM(OX + x), MM(OY + y))
        board.Add(k)
    print(f"In2: +3V3, islands {[z.GetZoneName() for z in board.Zones() if z.GetZoneName().startswith('In2 ') and z.GetZoneName() != 'In2 +3V3']}, "
          f"no fill under LINE IN: {boxes}")


def zones():
    if LAYERS == 4:
        inner_planes()
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
    if STRIP <= 0:                               # (stage 1e: no antenna strip, no keep-out)
        return
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
    # stage 2: the D-pad's pivot (case/README "Buttons"): a 3 mm circle at the cross's middle kept free of parts and vias,
    # a silk ring round it
    r = pcbnew.PCB_SHAPE(board)
    r.SetShape(pcbnew.SHAPE_T_CIRCLE)
    r.SetCenter(pcbnew.VECTOR2I(MM(OX + BTN_C[0]), MM(OY + BTN_C[1])))
    r.SetEnd(pcbnew.VECTOR2I(MM(OX + BTN_C[0] + PIVOT_R + 0.2), MM(OY + BTN_C[1])))
    r.SetWidth(MM(0.15))
    r.SetLayer(pcbnew.F_SilkS)
    board.Add(r)
    label("MIDI OUT", 6.0, 19.6, 0.7)            # (J2 and J5)
    label("MIDI OUT 2", W - 6.1, 19.6, 0.7)      # (J3 and J6: mini 2.2 simplified, a copy of MIDI OUT)
    for ref, text, dx, dy in (("SW1", "NO / BOOT", 0, 3.4), ("SW3", "UP", 0, 3.4), ("SW5", "YES", 0, 3.4),
                              ("SW4", "DOWN", -5.9, 0)):   # (under the buttons; DOWN, at the bottom edge, on its left)
        p = board.FindFootprintByReference(ref).GetPosition()
        label(text, pcbnew.ToMM(p.x) - OX + dx, pcbnew.ToMM(p.y) - OY + dy, 0.6)
    # the screen (stage 1e): the module's outline on F.Fab (not on the silk: it crosses pads); the header's pin names; the
    # jumpers' choices, each pad's net under it (modules differ: GND-VCC-SCL-SDA or VCC-GND-SCL-SDA)
    x0, y0, x1, y1 = MODULE
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_RECT)
    s.SetStart(pcbnew.VECTOR2I(MM(OX + x0), MM(OY + y0)))
    s.SetEnd(pcbnew.VECTOR2I(MM(OX + x1), MM(OY + y1)))
    s.SetWidth(MM(0.1))
    s.SetLayer(pcbnew.F_Fab)
    board.Add(s)
    h = board.FindFootprintByReference("J10")
    for p in h.Pads():
        x, y = pad_center(p)
        label({"1": "P1", "2": "P2", "3": "SCL", "4": "SDA"}[p.GetNumber()], x, y + 1.9, 0.6)
    for ref, text in (("JP1", "P1 G|V"), ("JP2", "P2 V|G")):
        p = board.FindFootprintByReference(ref).GetPosition()
        label(text, pcbnew.ToMM(p.x) - OX, pcbnew.ToMM(p.y) - OY + 1.8, 0.6)
    label("LINE IN", 14.7, 18.3, 0.6)            # (mini 2: J7)
    # the cells' PH2.0 sockets (mini 2.2): "+" and "-" beside each pad on both sides (the user solders from the top, the
    # socket and the cell are under the board); pin 1 (square) = +, pin 2 = - (README: the LOLIN32 Lite reasoning)
    for ref in ("J8", "J9"):
        f = board.FindFootprintByReference(ref)
        for p in f.Pads():
            x, y = pad_center(p)
            text = "+" if p.GetNetname() == "BAT_C" else "-"
            label(text, x, y - 1.9, 1.0)
            label(text, x, y - 1.9, 1.0, layer=pcbnew.B_SilkS)
        label("CELL " + {"J8": "1", "J9": "2"}[ref], pcbnew.ToMM(f.GetPosition().x) - OX, pcbnew.ToMM(f.GetPosition().y) - OY - 3.3, 0.6)   # (stage 2: above the signs: U9 is under them)
    # (stage 2: the bottom's name, date, wordmark and cheat sheet: silk.py, on the routed board)
    if STRIP > 0:
        label("antenna strip:", 12.0, GE + 4.6, 0.6)
        label("no copper", 12.0, GE + 5.5, 0.6)


def main():
    make_footprints.write()
    board.GetDesignSettings().SetCopperLayerCount(LAYERS)
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
    ds.SetCopperLayerCount(LAYERS)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    if tie_islands():
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    out = os.path.join(HERE, f"{NAME}.kicad_pcb")
    board.Save(out)
    print("saved", out, f"{W} x {H} mm")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
