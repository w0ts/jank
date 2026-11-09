# SPDX-License-Identifier: GPL-3.0-only
"""The jack footprints, drawn from the makers' drawings (README "The layout"): lib/jank.pretty/
  Jack_3.5mm_CUI_SJ1-3523N_RightAngle      the 3.5 mm stereo jack alone
  DIN-5_180deg_CUI_SDS-50J                 the DIN alone
  MIDI_OUT_SDS-50J_or_SJ1-3523N            both in one (the square board): the jack nested in the DIN, both fronts on the
                                           board edge, both plugs from the front; the jack's pads are numbered as the
                                           DIN pins on the same net (T = 5, R = 4, S = 2), so one symbol carries it
  MIDI_OUT_SDS-50J_or_SJ1-3523N_AB         the same for the long board: the jack's pads named T, R, S (their own nets,
                                           through the A/B switch)
  SW_Slide_DPDT_MSS22D18                   the TRS A/B slide switch (SHOU HAN MSS-22D18, LCSC C431542)
  XIAO-ESP32-S3-DIP_Headers                Seeed's XIAO footprint with the courtyard on its pin rows only (on headers)
  MIDI_OUT_DIN-504B-M10_or_PJ-320A_AB      the long board's output since 2026-11-08: the HOOYA DIN-504B-M10 or the XKB
                                           PJ-320A, nested, both fronts on the edge; the jack's pads T, R, S on their own
                                           nets (through the A/B switch), its switch pin (pad "SW") left open
  MIDI_OUT_PJ-320A_top_DIN-504B-M10_bottom v5 (the board since 2026-11-08, later the same day): BOTH fitted, the
                                           DIN-504B-M10 on the BOTTOM on the footprint's axis, its plate DIN_BACK in, and
                                           the PJ-320A on the top BESIDE it, JACK_DX along the edge (between two DINs at
                                           the 21.1 pitch), its front JACK_OUT past the edge (the user, 2026-11-08 night:
                                           "put the midi jack between the DIN plugs ... DIN on one side, jack on the
                                           other", both faces at the box wall); the DIN's pins mirrored in x as seen from
                                           the top; F.CrtYd the jack's, B.CrtYd the DIN's
  LED_SK6812MINI-HS_3.7x3.5mm_HandSolder   the beat LED (OPSCO SK6812MINI-HS), its datasheet's pads lengthened outward
  OLED_0.91in_128x32_I2C_HS91L02W2C01      the OLED module's 4-pin header (GND VCC SCL SDA), the module drawn on F.Fab
Origins on the board edge, on the connector's axis, the board inward towards -y. Run by make_board.py (or alone:
python3 make_footprints.py).

Sources (both read 2026-11-08):
- SDS-50J: CUI "SDS-J" datasheet 08/05/2022 page 3, "Recommended PCB Layout, top view" and the side view: 7 holes
  1.3 mm (pins 1-5 and two E shield pins), 2 locating holes 2.4 mm; face 20.0 wide, 15.8 deep at most; the E row 2.5 mm
  behind the face, the pegs 5.0 behind the E row, pins 1-3 10.0 and pins 4-5 12.5 behind it.
- SJ1-3523N: CUI "SJ1-352XN" datasheet 10/05/2022 page 2, "Recommended PCB Layout, top view": from the connector edge
  (the body's front), 1.2 mm holes at (+/-5.0, 1.0), (+/-5.0, 3.5) and (0, 8.5), plated slots 1.5 x 0.8 for pin 1
  (sleeve, 0, 3.5, across), pin 2 (tip) and pin 3 (ring) at (+/-5.0, 8.5) (10 and 11 are not fitted on the -3523N).
  Body 12.0 x 11.0 x 5.0; bushing 6.0 mm, 3.3 mm out past the body's front, its axis 2.5 mm above the board (so it
  hangs 0.5 mm below the board's top: it must be past the edge). Tip on the right seen from above with the edge at the
  front (the drawing's view turned; KiCad's own Jack_3.5mm_CUI_SJ1-3523N_Horizontal agrees: T +5, R -5). The 1.2 mm
  holes are taken as plastic pegs, not plated (UNVERIFIED); KiCad's footprint uses 0.4 x 1.4 slots, here CUI's
  1.5 x 0.8 (JLCPCB's plated slots start at 0.5 mm wide).
- DIN-504B-M10 (HOOYA, LCSC C2939346): its drawing "DIN JACK, MODEL DIN-504B-M10", rev A1 2019-07-10, read
  2026-11-08: plate 20.9 +/-0.1 along the edge x 19.6 +/-0.1 high, 1.8 thick; 7 holes 1.5 mm ("7-o1.5"): the 3-hole row
  (15 mm span, centre pin) 12.3 +/-0.3 behind the plate front, the 2-hole row (10 mm span) 14.8 +/-0.3 behind, two
  shield pins 2.5 +/-0.3 behind and 10 mm apart (x +/-5.0); no locating pegs; the body 11.8 deep behind the plate (so
  13.6 overall); the socket's axis 9.7 above the plate's board-side edge. Its pin numbers are not on the sheet: the
  leg pattern is the SDS-50J's (pins 1-3 in the 12.3 row, 4-5 in the 14.8 row), the same numbering is taken
  (UNVERIFIED: check pins 4 / 5 with a meter before soldering; the A/B switch does not swap the DIN).
- PJ-320A (XKB, LCSC C2884926): its drawing "3.5 PHONE JACK, PJ-320A", rev A1 2025-03-03, read 2026-11-08 (its
  "PCB LAYOUT" view, top view, the opening up; turned 180 degrees here, the opening at the edge): body 12.2 deep x
  6.2 wide x 5.0 high, its front on the edge, a round bushing (5.0 mm across, measured on the sheet, no figure) 2.0 mm
  out past it, plug hole 3.6, axis 2.5 above the board; two 1.4 mm pegs on the axis 1.6 and 8.6 behind the body's
  front; one column of three tabs 4.6 mm beside the lone tab: 3.2, 6.2, 10.2 behind the front, the lone one 11.4
  behind (the tabs drawn 0.6 x 1.2: plated slots that size here). Pin 1 sleeve, 2 ring, 3 tip, 4 the switch (the
  sheet's schematic); which tab is which pin is NOT on the sheet: taken as the lone tab = 1 (sleeve, the "contact
  piece"), the column 2 (ring), 4 (switch), 3 (tip) from the front (UNVERIFIED: check with a meter; a tip / ring swap
  is undone by the A/B switch, a sleeve or switch mix-up is not).
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
LIB = os.path.join(HERE, "lib", "jank.pretty")

# the CUI SJ1-3523N, in the board's frame: the body's front on the edge (y = 0), the board inward towards -y, x across
JACK_PINS = {"S": (0.0, -3.5, "h"), "T": (5.0, -8.5, "v"), "R": (-5.0, -8.5, "v")}   # (pin 1, 2, 3; slot direction)
JACK_PEGS = [(-5.0, -1.0), (5.0, -1.0), (-5.0, -3.5), (5.0, -3.5), (0.0, -8.5)]          # (the 5 x 1.2 mm holes)
JACK_SLOT, JACK_PAD, JACK_PEG = (1.5, 0.8), (2.2, 1.5), 1.2   # plated slot (CUI), copper round it, the pegs' holes
JACK_W, JACK_D, JACK_H = 12.0, 11.0, 5.0         # body: width, depth behind the edge, height (CUI drawing)
BUSH_D, BUSH_OUT, BUSH_Z = 6.0, 3.3, 2.5         # bushing: diameter, out past the body's front, axis above the board
DIN_W, DIN_D, DIN_FLANGE = 20.0, 15.8, 1.8
DIN_PINS = {"1": (7.5, -12.5), "2": (0.0, -12.5), "3": (-7.5, -12.5), "4": (5.0, -15.0), "5": (-5.0, -15.0)}
DIN_E = [(-2.5, -2.5), (2.5, -2.5)]
DIN_PEGS = [(-7.5, -7.5), (7.5, -7.5)]
DIN_HOLE, DIN_PAD, PEG = 1.3, 2.0, 2.4
NEST = (0.0, 0.0)         # the jack in the combined footprint: on the DIN's axis, its body's front on the same edge
CY = 0.25                 # courtyard margin
SILK_BACK = 16.3          # (the silk's back line behind pins 4 and 5: their pads reach 16.0)
NUM = {"T": "5", "R": "4", "S": "2"}
# the HOOYA DIN-504B-M10 (the board's frame, as above)
HY_PINS = {"1": (7.5, -12.3), "2": (0.0, -12.3), "3": (-7.5, -12.3), "4": (5.0, -14.8), "5": (-5.0, -14.8)}
HY_E = [(-5.0, -2.5), (5.0, -2.5)]
HY_HOLE, HY_PAD = 1.5, 2.2
HY_W, HY_D, HY_H, HY_AXIS, HY_PLATE = 20.9, 13.6, 19.6, 9.7, 1.8   # plate width, depth with the body, height, axis, plate
# the XKB PJ-320A (the board's frame: its body's front on the edge, turned 180 degrees from the sheet's layout view)
PJ_PINS = {"R": (-2.3, -3.2), "SW": (-2.3, -6.2), "T": (-2.3, -10.2), "S": (2.3, -11.4)}
PJ_PEGS = [(0.0, -1.6), (0.0, -8.6)]
PJ_SLOT, PJ_PAD, PJ_PEG = (0.6, 1.5), (1.2, 2.1), 1.4   # (slots 1.5 long: JLC's own footprint for C2884926; the review 2026-11-09)
PJ_W, PJ_D, PJ_H = 6.2, 12.2, 5.0                # body: width, depth behind the edge, height
PJ_BUSH_D, PJ_BUSH_OUT, PJ_BUSH_Z = 5.0, 2.0, 2.5   # bushing: diameter (measured), out past the body's front, axis   # (the jack's pads named after the DIN pin on the same net)
# the A/B slide switch, SHOU HAN MSS-22D18G2 (LCSC C431542; its drawing, read 2026-11-08): pins 1-2-3 and 4-5-6, 2.5 mm apart,
# rows 3.4 mm apart, 0.8 mm holes; body 9.1 x 3.5 mm, 3.5 mm tall, handle 1.5 mm, travel 2.0 mm
SW_PINS = {"1": (-2.5, 0.0), "2": (0.0, 0.0), "3": (2.5, 0.0), "4": (-2.5, 3.4), "5": (0.0, 3.4), "6": (2.5, 3.4)}
SW_PAD, SW_HOLE, SW_BODY = 1.6, 0.9, (9.1, 3.5)
# the surface-mount DPDT slide switch, XKB SS-3235S-L2 (LCSC C500056; its drawing X-SS-3235S-L2 rev A1 2025-06-21, read
# 2026-11-09, "PCB LAYOUT"): 6 tabs 0.7 x 1.2, 2.5 mm apart, the rows 6.5 mm apart; body 7.7 x 3.55 x 3.2, knob 3.0 (L2)
# high, 1.5 wide, travel 1.5 along the body; pins 1-2-3 one row, 4-5-6 the other, 2 and 5 the commons (its schematic)
SWS_PINS = {"1": (-2.5, -3.25), "2": (0.0, -3.25), "3": (2.5, -3.25), "4": (-2.5, 3.25), "5": (0.0, 3.25), "6": (2.5, 3.25)}
SWS_PAD, SWS_BODY, SWS_SPAN = (0.7, 1.2), (7.7, 3.55), 3.85   # (pad across x along, body, the pads' outer edge from the axis)


def stroke(w, layer):
    return f'(stroke (width {w}) (type solid)) (fill none) (layer "{layer}")'


def rect(x0, y0, x1, y1, layer, w):
    return f"  (fp_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) {stroke(w, layer)})"


def line(x0, y0, x1, y1, layer, w):
    return f"  (fp_line (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f}) {stroke(w, layer)})"


def circle(x, y, r, layer, w):
    return f"  (fp_circle (center {x:.3f} {y:.3f}) (end {x + r:.3f} {y:.3f}) {stroke(w, layer)})"


def text(kind, value, x, y, layer, hide=False):
    h = " (hide yes)" if hide else ""
    return (f'  (property "{kind}" "{value}" (at {x:.3f} {y:.3f} 0) (layer "{layer}"){h}'
            f" (effects (font (size 1 1) (thickness 0.15))))")


def pth(num, x, y, size, drill, shape="circle"):
    if isinstance(size, tuple):
        return (f'  (pad "{num}" thru_hole oval (at {x:.3f} {y:.3f}) (size {size[0]} {size[1]})'
                f' (drill oval {drill[0]} {drill[1]}) (layers "*.Cu" "*.Mask"))')
    return f'  (pad "{num}" thru_hole {shape} (at {x:.3f} {y:.3f}) (size {size} {size}) (drill {drill}) (layers "*.Cu" "*.Mask"))'


def npth(x, y, d):
    return f'  (pad "" np_thru_hole circle (at {x:.3f} {y:.3f}) (size {d} {d}) (drill {d}) (layers "*.Cu" "*.Mask"))'


def footprint(name, descr, body, attr="through_hole"):
    return "\n".join([f'(footprint "{name}" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      f'  (descr "{descr}")', f"  (attr {attr})", *body, ")"]) + "\n"


def jack_items(cx, names):
    """the SJ1-3523N's pads, pegs and drawing, its axis at x = cx, its body's front on y = 0"""
    out = []
    for pin, (x, y, d) in JACK_PINS.items():
        pad, slot = (JACK_PAD, JACK_SLOT) if d == "h" else (JACK_PAD[::-1], JACK_SLOT[::-1])
        out.append(pth(names[pin], cx + x, y, pad, slot))
    out += [npth(cx + x, y, JACK_PEG) for x, y in JACK_PEGS]
    out += [rect(cx - JACK_W / 2, -JACK_D, cx + JACK_W / 2, 0, "F.Fab", 0.1),
            rect(cx - BUSH_D / 2, 0, cx + BUSH_D / 2, BUSH_OUT, "F.Fab", 0.1)]   # (the bushing, past the edge)
    return out


def courtyard(w, d, cx):
    """a body w x d behind the edge with the jack's bushing out in front, one outline"""
    b = BUSH_D / 2 + CY
    pts = [(-w - CY, -d - CY), (w + CY, -d - CY), (w + CY, 0), (cx + b, 0), (cx + b, BUSH_OUT + CY), (cx - b, BUSH_OUT + CY),
           (cx - b, 0), (-w - CY, 0)]
    xy = " ".join(f"(xy {x:.3f} {y:.3f})" for x, y in pts)
    return f"  (fp_poly (pts {xy}) {stroke(0.05, 'F.CrtYd')})"


def din_items():
    out = [pth(n, x, y, DIN_PAD, DIN_HOLE) for n, (x, y) in DIN_PINS.items()]
    out += [pth("E", x, y, DIN_PAD, DIN_HOLE) for x, y in DIN_E]
    out += [npth(x, y, PEG) for x, y in DIN_PEGS]
    w = DIN_W / 2
    out += [rect(-w, -DIN_D, w, 0, "F.Fab", 0.1), line(-w, -DIN_FLANGE, w, -DIN_FLANGE, "F.Fab", 0.1),
            line(-w, -SILK_BACK, w, -SILK_BACK, "F.SilkS", 0.12), line(-w, -SILK_BACK, -w, -1.0, "F.SilkS", 0.12),
            line(w, -SILK_BACK, w, -1.0, "F.SilkS", 0.12)]
    return out


def din_pj_items():
    """the HOOYA DIN-504B-M10 and, nested on its axis, the XKB PJ-320A (README "MIDI out")"""
    out = [pth(n, x, y, HY_PAD, HY_HOLE) for n, (x, y) in HY_PINS.items()]
    out += [pth("E", x, y, HY_PAD, HY_HOLE) for x, y in HY_E]
    out += [pth(n, x, y, PJ_PAD, PJ_SLOT) for n, (x, y) in PJ_PINS.items()]
    out += [npth(x, y, PJ_PEG) for x, y in PJ_PEGS]
    w = HY_W / 2
    out += [rect(-w, -HY_D, w, 0, "F.Fab", 0.1), line(-w, -HY_PLATE, w, -HY_PLATE, "F.Fab", 0.1),
            rect(-PJ_W / 2, -PJ_D, PJ_W / 2, 0, "F.Fab", 0.1),
            rect(-PJ_BUSH_D / 2, 0, PJ_BUSH_D / 2, PJ_BUSH_OUT, "F.Fab", 0.1),
            line(-w + 0.3, -HY_D, -w + 0.3, -1.0, "F.SilkS", 0.12), line(w - 0.3, -HY_D, w - 0.3, -1.0, "F.SilkS", 0.12),
            line(-w + 0.3, -HY_D, -6.6, -HY_D, "F.SilkS", 0.12), line(6.6, -HY_D, w - 0.3, -HY_D, "F.SilkS", 0.12)]
    a, b, c = w, -HY_D - CY, 5.0 + HY_PAD / 2 + CY        # (the sides on the plate itself: the pitch is 21.1)
    d, e = -14.8 - HY_PAD / 2 - CY, PJ_BUSH_D / 2 + CY
    pts = [(-a, b), (-c, b), (-c, d), (c, d), (c, b), (a, b), (a, 0), (e, 0), (e, PJ_BUSH_OUT + CY), (-e, PJ_BUSH_OUT + CY),
           (-e, 0), (-a, 0)]
    xy = " ".join(f"(xy {x:.3f} {y:.3f})" for x, y in pts)
    out.append(f"  (fp_poly (pts {xy}) {stroke(0.05, 'F.CrtYd')})")
    return out


# v5 (2026-11-08 night): the DIN under the board on the footprint's axis, the jack on the top BESIDE it. The user: both
# faces at the box wall, which the box does by bringing the board edge to DIN_BACK inside the outside face (README "v5").
DIN_BACK = 1.0           # the DIN's plate behind the edge: its face flush with the box's outside, the board edge 1.0 in
JACK_DX = 11.5           # the jack's axis along the edge from the DIN's: half the 23.0 pitch, between two DINs
JACK_OUT = 0.5           # the jack's body front past the edge: its face 0.3 mm inside the box's outside, bushing out
# The outputs' pitch is 23.0 (make_board.py CH): the DIN plates are 20.9 wide, so the 2.1 mm between two of them takes the
# jack's two pegs (1.4 mm holes, 0.35 mm from each DIN body) and nothing of the jack goes THROUGH the board under a DIN
# body (its contacts are surface-mount tabs on the top: see PJ_SMD). The jack's rear pads keep >= 0.3 mm from the
# neighbouring DINs' pin 1 / 3 pads, and the front peg's hole stays 0.4 mm inside the board edge.


def v5_din_pins():
    """the HOOYA DIN-504B-M10 mounted on the BOTTOM, seen from the top: x mirrored, DIN_BACK further in"""
    pins = {n: (-x, y - DIN_BACK) for n, (x, y) in HY_PINS.items()}
    return pins, [(-x, y - DIN_BACK) for x, y in HY_E]


# the XKB PJ-320D-B-SMT (LCSC C2884940), the surface-mount twin of the PJ-320A: its drawing X-PJ-320D-B-SMT rev A1
# 2025-06-26, read 2026-11-08 ("PCB Layout", top view, the opening to the left; turned 90 degrees here, the opening at the
# edge): body 12.0 deep x 6.0 wide x 5.0 high, bushing 5.0 across and 2.0 out, plug hole 3.6, axis 2.5 above the board;
# four tabs "1.5x2.5" on the body's sides, their centres 7.0 apart across: terminals 2, 3, 4 on one side 3.0, 6.0 and
# 10.0 behind the body's front, terminal 1 on the other side 11.5 behind it (= 10.0 + the drawing's 1.50); two 1.3 mm
# pegs on the axis 1.5 and 8.5 behind the front. Terminal 4 = tip (deepest), 3 = ring, 2 = the second ring (a TRS plug's
# sleeve also touches it; left open here like the PJ-320A's switch pad), 1 = sleeve. Named here after the PJ-320A's pads so
# the nets and routes stay: T = 4, R = 3, SW = 2, S = 1. Seen from the top with the opening at the edge (+y), tabs 2/3/4
# sit on -x and tab 1 on +x: the drawing's "PCB Layout" is a top view (opening left, 2/3/4 at the top), turned 90
# degrees without mirroring. (v5c had them mirrored; found 2026-11-09 by the review against JLC's own footprint for
# C2884940, which fits these pads at rotation 90 with 0.00 mm error.)
PJ_SMD_PADS = {"SW": (-3.5, -3.0), "R": (-3.5, -6.0), "T": (-3.5, -10.0), "S": (3.5, -11.5)}
PJ_SMD_PAD = (2.5, 1.5)                          # (across, along the depth)
PJ_SMD_PEGS, PJ_SMD_PEG = [(0.0, -1.5), (0.0, -8.5)], 1.3
PJ_SMD_SPAN = 4.75                               # the pads' outer edges from the axis (the tabs reach 4.0)


def v5_jack(x, y):
    """a point of the jack (its own frame: axis x = 0, body front y = 0) in the combined footprint's frame"""
    return x + JACK_DX, y + JACK_OUT


def smd(num, x, y, size):
    return (f'  (pad "{num}" smd rect (at {x:.3f} {y:.3f}) (size {size[0]} {size[1]})'
            f' (layers "F.Cu" "F.Paste" "F.Mask"))')


def v5_items():
    """v5 (README "v5"): the HOOYA DIN-504B-M10 on the BOTTOM on the footprint's axis, its plate DIN_BACK behind the edge,
    and the XKB PJ-320A on the top, JACK_DX along the edge, its front JACK_OUT past it; both fitted, on the same nets (the
    jack through the A/B switch)"""
    pins, e = v5_din_pins()
    out = [pth(n, x, y, HY_PAD, HY_HOLE) for n, (x, y) in pins.items()]
    out += [pth("E", x, y, HY_PAD, HY_HOLE) for x, y in e]
    out += [smd(n, *v5_jack(x, y), PJ_SMD_PAD) for n, (x, y) in PJ_SMD_PADS.items()]
    out += [npth(*v5_jack(x, y), PJ_SMD_PEG) for x, y in PJ_SMD_PEGS]
    w, f, b = HY_W / 2, -DIN_BACK, -DIN_BACK - HY_D
    jx0, jx1 = JACK_DX - PJ_W / 2, JACK_DX + PJ_W / 2
    out += [rect(-w, b, w, f, "B.Fab", 0.1), line(-w, f - HY_PLATE, w, f - HY_PLATE, "B.Fab", 0.1),
            line(-w + 0.3, b, -w + 0.3, f, "B.SilkS", 0.12), line(w - 0.3, b, w - 0.3, f, "B.SilkS", 0.12),
            line(-w + 0.3, b, -6.6, b, "B.SilkS", 0.12), line(6.6, b, w - 0.3, b, "B.SilkS", 0.12),
            rect(-w, 0, w, b - CY, "B.CrtYd", 0.05),
            rect(jx0, JACK_OUT - PJ_D, jx1, JACK_OUT, "F.Fab", 0.1),
            rect(JACK_DX - PJ_BUSH_D / 2, JACK_OUT, JACK_DX + PJ_BUSH_D / 2, JACK_OUT + PJ_BUSH_OUT, "F.Fab", 0.1),
            line(jx0 - 0.15, JACK_OUT - PJ_D, jx0 - 0.15, JACK_OUT - PJ_D + 1.0, "F.SilkS", 0.12),
            line(jx1 + 0.15, JACK_OUT - PJ_D, jx1 + 0.15, JACK_OUT - PJ_D + 1.0, "F.SilkS", 0.12),
            line(jx0 - 0.15, -1.6, jx0 - 0.15, -1.0, "F.SilkS", 0.12), line(jx1 + 0.15, -1.6, jx1 + 0.15, -1.0, "F.SilkS", 0.12)]
    a, e_ = PJ_SMD_SPAN + CY, PJ_BUSH_D / 2 + CY   # (the courtyard takes the tabs' pads, wider than the body)
    pts = [v5_jack(x, y) for x, y in ((-a, -PJ_D - CY), (a, -PJ_D - CY), (a, 0), (e_, 0), (e_, PJ_BUSH_OUT + CY),
                                      (-e_, PJ_BUSH_OUT + CY), (-e_, 0), (-a, 0))]
    xy = " ".join(f"(xy {x:.3f} {y:.3f})" for x, y in pts)
    out.append(f"  (fp_poly (pts {xy}) {stroke(0.05, 'F.CrtYd')})")
    return out


# the SK6812MINI-HS (OPSCO, LCSC C2922787; its datasheet SK6812MINI-HS-012 rev B/0, read 2026-11-08, "PCB recommended pad
# size", TOP VIEW): body 3.7 x 3.5 x 1.95, 4 pads 1.5 x 1.0, 1.8 apart across, rows 1.8 apart; 3 VDD top left, 2 DIN top
# right, 4 DOUT bottom left, 1 GND bottom right. Here the pads 0.6 longer outward (hand soldering), their inner edges kept.
LED_PADS = {"3": (-1.9, -0.9), "2": (1.9, -0.9), "4": (-1.9, 0.9), "1": (1.9, 0.9)}
LED_PAD, LED_BODY = (2.1, 1.0), (3.7, 3.5)
# the 0.91" 128 x 32 OLED module HS91L02W2C01 (HS, LCSC C5248081; its outline drawing rev A1 2020-04-11, read 2026-11-08):
# PCB 38.00 x 12.00, 4 holes 1.00 mm at 2.54 (GND, VCC, SCL, SDA) 1.50 from one short edge, GND 2.19 from the long edge;
# the panel 30.00 x 11.56 from 5.50, the viewing area 24.38 x 7.58 from 6.60, the active area 22.38 x 5.58 from 7.62
# (along the length, from the pin edge); across, measured on the sheet (no figure): VA 3.2 .. 10.7, AA 4.2 .. 9.7 from the
# GND side. 2.60 thick (glass 1.20). No mounting holes. Origin: pin 1 (GND), the module along +x, pins along +y.
OLED_L, OLED_W, OLED_PIN_X, OLED_PIN_Y = 38.0, 12.0, 1.5, 2.19
OLED_AA = (7.62, 4.2, 30.0, 9.7)                 # (x0, y0, x1, y1 from the module's pin edge and GND side)


# the 0.96" 128 x 64 OLED module HS96L03W2C03 (Hansheng, LCSC C5248080; its datasheet "1.4 Mechanical Drawing", 2018/07/23,
# read 2026-11-09): PCB 27.30 x 27.80 x 1.0, 4 holes at 2.54 (GND VCC SCL SDA) centred on the 27.30 edge, the row 1.44 in;
# the glass (LCD) 24.74 x 16.90 from 4.87, the viewing area 22.74 x 11.86, the active area 21.744 x 10.864 from 6.37
# (from the pin edge); four 2.5 mm mounting holes 2.0 from the edges; SSD1315 (SSD1306 commands), I2C 0x3C. Seen from
# the front with the pins at the top, GND is on the left. Origin: pin 1 (GND), the pins along +x, the module along +y.
O96_W, O96_L, O96_ROW = 27.30, 27.80, 1.44
O96_X0 = -(O96_W - 3 * 2.54) / 2                 # (the module's left edge from pin 1: -9.84)
O96_LCD, O96_AA = (24.74, 16.90, 4.87), (21.744, 10.864, 6.37)


def oled96_items():
    out = [pth(str(i + 1), 2.54 * i, 0.0, 1.7, 1.0, "rect" if i == 0 else "circle") for i in range(4)]
    x0, y0, cx = O96_X0, -O96_ROW, 3 * 2.54 / 2
    out += [rect(x0, y0, x0 + O96_W, y0 + O96_L, "F.Fab", 0.1),
            rect(cx - O96_LCD[0] / 2, y0 + O96_LCD[2], cx + O96_LCD[0] / 2, y0 + O96_LCD[2] + O96_LCD[1], "F.Fab", 0.1),
            rect(cx - O96_AA[0] / 2, y0 + O96_AA[2], cx + O96_AA[0] / 2, y0 + O96_AA[2] + O96_AA[1], "F.Fab", 0.1),
            rect(x0, y0, x0 + O96_W, y0 + O96_L, "F.SilkS", 0.12),
            rect(-1.25, -1.25, 3 * 2.54 + 1.25, 1.25, "F.CrtYd", 0.05)]   # (the courtyard: the header only)
    out += [f"  (fp_circle (center {x:.3f} {y:.3f}) (end {x + 1.25:.3f} {y:.3f}) {stroke(0.1, 'F.Fab')})"
            for x in (x0 + 2.0, x0 + O96_W - 2.0) for y in (y0 + 2.0, y0 + O96_L - 2.0)]
    return out


def led_items():
    out = [f'  (pad "{n}" smd rect (at {x:.3f} {y:.3f}) (size {LED_PAD[0]} {LED_PAD[1]}) (layers "F.Cu" "F.Paste" "F.Mask"))'
           for n, (x, y) in LED_PADS.items()]
    bx, by = LED_BODY[0] / 2, LED_BODY[1] / 2
    out += [rect(-bx, -by, bx, by, "F.Fab", 0.1), rect(-3.2, -by - 0.3, 3.2, by + 0.3, "F.CrtYd", 0.05),
            line(-bx, -by - 0.2, bx, -by - 0.2, "F.SilkS", 0.12), line(-bx, by + 0.2, 0.6, by + 0.2, "F.SilkS", 0.12),
            line(0.6, by + 0.2, bx, by - 0.5, "F.SilkS", 0.12)]   # (the GND corner cut, as on the sheet)
    return out


def oled_items():
    out = [pth(str(i + 1), 0.0, 2.54 * i, 1.7, 1.0, "rect" if i == 0 else "circle") for i in range(4)]
    x0, y0 = -OLED_PIN_X, -OLED_PIN_Y
    a = OLED_AA
    out += [rect(x0, y0, x0 + OLED_L, y0 + OLED_W, "F.Fab", 0.1),
            rect(x0 + 5.5, y0 + 0.22, x0 + 35.5, y0 + 11.78, "F.Fab", 0.1),
            rect(x0 + a[0], y0 + a[1], x0 + a[2], y0 + a[3], "F.Fab", 0.1),
            rect(x0, y0, x0 + OLED_L, y0 + OLED_W, "F.SilkS", 0.12),
            rect(-1.25, -1.25, 1.25, 8.87, "F.CrtYd", 0.05)]   # (the courtyard: the header only; the module floats above)
    return out


def pj_items():
    """the XKB PJ-320A alone (the modular variant's side-wall jacks): its pads, pegs, body and bushing, F side"""
    out = [pth(n, x, y, PJ_PAD, PJ_SLOT) for n, (x, y) in PJ_PINS.items()]
    out += [npth(x, y, PJ_PEG) for x, y in PJ_PEGS]
    out += [rect(-PJ_W / 2, -PJ_D, PJ_W / 2, 0, "F.Fab", 0.1),
            rect(-PJ_BUSH_D / 2, 0, PJ_BUSH_D / 2, PJ_BUSH_OUT, "F.Fab", 0.1),
            line(-PJ_W / 2 - 0.15, -PJ_D, -PJ_W / 2 - 0.15, -1.0, "F.SilkS", 0.12),
            line(PJ_W / 2 + 0.15, -PJ_D, PJ_W / 2 + 0.15, -1.0, "F.SilkS", 0.12)]
    a, e_ = PJ_W / 2 + CY, PJ_BUSH_D / 2 + CY
    pts = [(-a, -PJ_D - CY), (a, -PJ_D - CY), (a, 0), (e_, 0), (e_, PJ_BUSH_OUT + CY), (-e_, PJ_BUSH_OUT + CY), (-e_, 0), (-a, 0)]
    xy = " ".join(f"(xy {x:.3f} {y:.3f})" for x, y in pts)
    out.append(f"  (fp_poly (pts {xy}) {stroke(0.05, 'F.CrtYd')})")
    return out


def xiao_on_headers():
    """the Seeed XIAO footprint (XIAO-ESP32-S3-DIP) with its courtyard on the two pin rows only: on 2.54 mm headers the
    module stands 2.5 mm above the board, so low parts fit under it (the long board: the button, the LEDs; README)"""
    src = open(os.path.join(LIB, "XIAO-ESP32-S3-DIP.kicad_mod")).read()
    i = src.index('(layer "F.CrtYd")')
    a = src.rindex("(fp_rect", 0, i)
    b = src.index("\n\t)", i) + 3
    rows = "\n".join(rect(-10.425, y0, 10.55, y1, "F.CrtYd", 0.05) for y0, y1 in ((-9.95, -6.6), (6.6, 9.95)))
    body = src[:a] + rows.strip() + src[b:]
    body = body.replace("(drill 0.889)", "(drill 1.0)")   # (0.64 mm square header pins are 0.905 across: 1.0 mm holes, the review 2026-11-09)
    return body.replace('(footprint "XIAO-ESP32-S3-DIP"', '(footprint "XIAO-ESP32-S3-DIP_Headers"', 1)


def write():
    os.makedirs(LIB, exist_ok=True)
    w = DIN_W / 2
    names = {p: p for p in JACK_PINS}
    files = {
        "Jack_3.5mm_CUI_SJ1-3523N_RightAngle": footprint(
            "Jack_3.5mm_CUI_SJ1-3523N_RightAngle",
            "3.5 mm stereo jack, right angle, CUI SJ1-3523N (datasheet SJ1-352XN 10/05/2022); body front on the board edge,"
            " the bushing 3.3 mm past it",
            [text("Reference", "REF**", 0, -JACK_D - 1.5, "F.SilkS"), text("Value", "SJ1-3523N", 0, -6, "F.Fab"),
             *jack_items(0.0, names), rect(-JACK_W / 2, -JACK_D, JACK_W / 2, -0.3, "F.SilkS", 0.12),
             courtyard(JACK_W / 2, JACK_D, 0.0)]),
        "DIN-5_180deg_CUI_SDS-50J": footprint(
            "DIN-5_180deg_CUI_SDS-50J",
            "DIN-5 180 degree right angle, CUI SDS-50J (datasheet SDS-J 08/05/2022); origin on the board edge, face flush",
            [text("Reference", "REF**", 0, -DIN_D - 1.5, "F.SilkS"), text("Value", "SDS-50J", 0, -9, "F.Fab"),
             *din_items(), rect(-w - CY, -DIN_D - CY, w + CY, 0, "F.CrtYd", 0.05)]),
        "MIDI_OUT_SDS-50J_or_SJ1-3523N": footprint(
            "MIDI_OUT_SDS-50J_or_SJ1-3523N",
            "One MIDI OUT: fit the DIN-5 (CUI SDS-50J) OR the 3.5 mm TRS (CUI SJ1-3523N, right angle), both fronts on the edge;"
            " jack pads numbered as the DIN pin on the same net (T=5, R=4, S=2)",
            [text("Reference", "REF**", 0, -DIN_D - 1.5, "F.SilkS"), text("Value", "DIN or TRS", 0, -9, "F.Fab"),
             *din_items(), *jack_items(NEST[0], NUM), courtyard(w, DIN_D, NEST[0])]),
        "MIDI_OUT_SDS-50J_or_SJ1-3523N_AB": footprint(
            "MIDI_OUT_SDS-50J_or_SJ1-3523N_AB",
            "One MIDI OUT with a TRS A/B switch: the DIN-5 (CUI SDS-50J) OR the 3.5 mm TRS (CUI SJ1-3523N, right angle), both fronts"
            " on the edge; the jack's pads T, R, S on their own nets (through the A/B switch)",
            [text("Reference", "REF**", 0, -DIN_D - 1.5, "F.SilkS"), text("Value", "DIN or TRS", 0, -9, "F.Fab"),
             *din_items(), *jack_items(NEST[0], {p: p for p in JACK_PINS}), courtyard(w, DIN_D, NEST[0])]),
        "SW_Slide_DPDT_SS3235S_SMD": footprint(
            "SW_Slide_DPDT_SS3235S_SMD",
            "DPDT slide switch, surface mount, XKB SS-3235S-L2 (drawing rev A1 2025-06-21): 6 tabs 0.7 x 1.2 at 2.5 mm, rows"
            " 6.5 mm apart; body 7.7 x 3.55 x 3.2 mm, knob 3.0 mm, travel 1.5 mm along the body",
            [text("Reference", "REF**", 0, -4.9, "F.SilkS"), text("Value", "SS-3235S-L2", 0, 4.9, "F.Fab"),
             *[smd(n, x, y, SWS_PAD) for n, (x, y) in SWS_PINS.items()],
             rect(-SWS_BODY[0] / 2, -SWS_BODY[1] / 2, SWS_BODY[0] / 2, SWS_BODY[1] / 2, "F.Fab", 0.1),
             rect(-1.5, -0.75, 0.0, 0.75, "F.Fab", 0.1),                         # (the knob, at one end of its 1.5 mm travel)
             line(-SWS_BODY[0] / 2 - 0.15, -2.4, -SWS_BODY[0] / 2 - 0.15, 2.4, "F.SilkS", 0.12),
             line(SWS_BODY[0] / 2 + 0.15, -2.4, SWS_BODY[0] / 2 + 0.15, 2.4, "F.SilkS", 0.12),
             rect(-SWS_BODY[0] / 2 - CY, -SWS_SPAN - CY, SWS_BODY[0] / 2 + CY, SWS_SPAN + CY, "F.CrtYd", 0.05)], "smd"),
        "SW_Slide_DPDT_MSS22D18": footprint(
            "SW_Slide_DPDT_MSS22D18",
            "DPDT slide switch, SHOU HAN MSS-22D18 (drawing 2020-07-13): 6 pins 2.5 mm apart, rows 3.4 mm, holes 0.8 mm; body"
            " 9.1 x 3.5 x 3.5 mm, handle 1.5 mm (G2), travel 2.0 mm along the body",
            [text("Reference", "REF**", 0, -2.2, "F.SilkS"), text("Value", "MSS22D18", 0, 5.6, "F.Fab"),
             *[pth(n, x, y, SW_PAD, SW_HOLE) for n, (x, y) in SW_PINS.items()],
             rect(-SW_BODY[0] / 2, 1.7 - SW_BODY[1] / 2, SW_BODY[0] / 2, 1.7 + SW_BODY[1] / 2, "F.Fab", 0.1),
             rect(-1.75, 1.7 - 0.75, 0.25, 1.7 + 0.75, "F.Fab", 0.1),          # (the handle, at one end of its 2.0 mm travel)
             rect(-SW_BODY[0] / 2 - 0.15, -1.05, SW_BODY[0] / 2 + 0.15, 4.45, "F.SilkS", 0.12),   # (round the pads)
             rect(-SW_BODY[0] / 2 - CY, -0.8 - CY, SW_BODY[0] / 2 + CY, 4.2 + CY, "F.CrtYd", 0.05)]),
    }
    files["XIAO-ESP32-S3-DIP_Headers"] = xiao_on_headers()
    files["MIDI_OUT_DIN-504B-M10_or_PJ-320A_AB"] = footprint(
        "MIDI_OUT_DIN-504B-M10_or_PJ-320A_AB",
        "One MIDI OUT with a TRS A/B switch: the DIN-5 (HOOYA DIN-504B-M10) OR the 3.5 mm jack (XKB PJ-320A, right angle),"
        " both fronts on the edge; the jack's pads T, R, S on their own nets, its switch pin SW open",
        [text("Reference", "REF**", 0, -HY_D - 2.5, "F.SilkS"), text("Value", "DIN or TRS", 0, -9, "F.Fab"),
         *din_pj_items()])
    files["MIDI_OUT_PJ-320A_top_DIN-504B-M10_bottom"] = footprint(
        "MIDI_OUT_PJ-320A_top_DIN-504B-M10_bottom",
        "One MIDI OUT, both fitted (v5): the 3.5 mm jack XKB PJ-320A on the top, its front on the edge, and the DIN-5 HOOYA"
        " DIN-504B-M10 on the bottom on the same axis, its plate 1.0 behind the edge; the jack's pads T, R, S on their own nets",
        [text("Reference", "REF**", 0, -HY_D - 3.5, "F.SilkS", True), text("Value", "TRS + DIN", 0, -9, "F.Fab"),
         *v5_items()])
    files["LED_SK6812MINI-HS_3.7x3.5mm_HandSolder"] = footprint(
        "LED_SK6812MINI-HS_3.7x3.5mm_HandSolder",
        "Addressable RGB LED OPSCO SK6812MINI-HS, 3.7 x 3.5 x 1.95 mm, top emitting (datasheet rev B/0); pads lengthened outward",
        [text("Reference", "REF**", 0, -2.8, "F.SilkS", True), text("Value", "SK6812MINI-HS", 0, 2.8, "F.Fab"), *led_items()], "smd")
    # the power USB-C (v5c, 2026-11-09: the GCT USB4125 had no JLC stock): SHOU HAN TYPE-C 6P, LCSC C456012, JLC's own
    # footprint (EasyEDA, via easyeda2kicad 1.0.1, kept in lib/easyeda/), its shell pad "7" renamed "SH" (the symbol's).
    # Its origin is JLC's placement origin; the mouth is 4.63 in front of it, the pins 2.03 behind.
    src = open(os.path.join(HERE, "lib", "easyeda", "TYPE-C-SMD_TYPE-C-6P_1.kicad_mod")).read()
    src = src.replace("(module easyeda2kicad:TYPE-C-SMD_TYPE-C-6P_1", '(module "USB_C_SHOUHAN_TYPE-C-6P"', 1).replace("(pad 7 thru_hole", '(pad "SH" thru_hole')
    files["USB_C_SHOUHAN_TYPE-C-6P"] = src
    files["OLED_0.96in_128x64_I2C_HS96L03W2C03"] = footprint(
        "OLED_0.96in_128x64_I2C_HS96L03W2C03",
        "OLED module 0.96 in 128 x 64 (HS96L03W2C03, SSD1315, I2C) on a 1 x 4 2.54 mm header: GND VCC SCL SDA; the module"
        " 27.3 x 27.8 drawn on F.Fab and the silk, the courtyard the header only (the module floats 2.5 mm up)",
        [text("Reference", "REF**", 3.81, -2.6, "F.SilkS"), text("Value", "OLED 0.96in", 3.81, 12.0, "F.Fab"),
         *oled96_items()])
    files["OLED_0.91in_128x32_I2C_HS91L02W2C01"] = footprint(
        "OLED_0.91in_128x32_I2C_HS91L02W2C01",
        "0.91 inch 128 x 32 OLED module HS91L02W2C01 (SSD1306, I2C), on a 1 x 4 2.54 mm header: 1 GND, 2 VCC, 3 SCL, 4 SDA;"
        " module 38 x 12 mm drawn on F.Fab / F.SilkS (it floats over the board on its header)",
        [text("Reference", "REF**", 3.0, -3.2, "F.SilkS", True), text("Value", "OLED 0.91", 18.0, 3.8, "F.Fab"), *oled_items()])
    files["Jack_3.5mm_XKB_PJ-320A_Horizontal"] = footprint(
        "Jack_3.5mm_XKB_PJ-320A_Horizontal",
        "3.5 mm jack XKB PJ-320A alone (the modular variant's mono outputs: tip = signal, sleeve = GND, ring and switch open);"
        " its front on the board edge, the bushing 2.0 mm past it",
        [text("Reference", "REF**", 0, -PJ_D - 1.5, "F.SilkS", True), text("Value", "PJ-320A", 0, -6, "F.Fab"), *pj_items()])
    for old in ("Jack_3.5mm_Thonkiconn_PJ366ST_Vertical", "MIDI_OUT_SDS-50J_or_PJ366ST"):   # (the vertical jack, replaced)
        if os.path.exists(os.path.join(LIB, old + ".kicad_mod")):
            os.remove(os.path.join(LIB, old + ".kicad_mod"))
    for name, body in files.items():
        with open(os.path.join(LIB, name + ".kicad_mod"), "w") as f:
            f.write(body)
    return list(files)


if __name__ == "__main__":
    print("wrote", write())
