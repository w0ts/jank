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
NUM = {"T": "5", "R": "4", "S": "2"}   # (the jack's pads named after the DIN pin on the same net)
# the A/B slide switch, SHOU HAN MSS-22D18G2 (LCSC C431542; its drawing, read 2026-11-08): pins 1-2-3 and 4-5-6, 2.5 mm apart,
# rows 3.4 mm apart, 0.8 mm holes; body 9.1 x 3.5 mm, 3.5 mm tall, handle 1.5 mm, travel 2.0 mm
SW_PINS = {"1": (-2.5, 0.0), "2": (0.0, 0.0), "3": (2.5, 0.0), "4": (-2.5, 3.4), "5": (0.0, 3.4), "6": (2.5, 3.4)}
SW_PAD, SW_HOLE, SW_BODY = 1.6, 0.9, (9.1, 3.5)


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


def footprint(name, descr, body):
    return "\n".join([f'(footprint "{name}" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      f'  (descr "{descr}")', "  (attr through_hole)", *body, ")"]) + "\n"


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


def xiao_on_headers():
    """the Seeed XIAO footprint (XIAO-ESP32-S3-DIP) with its courtyard on the two pin rows only: on 2.54 mm headers the
    module stands 2.5 mm above the board, so low parts fit under it (the long board: the button, the LEDs; README)"""
    src = open(os.path.join(LIB, "XIAO-ESP32-S3-DIP.kicad_mod")).read()
    i = src.index('(layer "F.CrtYd")')
    a = src.rindex("(fp_rect", 0, i)
    b = src.index("\n\t)", i) + 3
    rows = "\n".join(rect(-10.425, y0, 10.55, y1, "F.CrtYd", 0.05) for y0, y1 in ((-9.95, -6.6), (6.6, 9.95)))
    body = src[:a] + rows.strip() + src[b:]
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
    for old in ("Jack_3.5mm_Thonkiconn_PJ366ST_Vertical", "MIDI_OUT_SDS-50J_or_PJ366ST"):   # (the vertical jack, replaced)
        if os.path.exists(os.path.join(LIB, old + ".kicad_mod")):
            os.remove(os.path.join(LIB, old + ".kicad_mod"))
    for name, body in files.items():
        with open(os.path.join(LIB, name + ".kicad_mod"), "w") as f:
            f.write(body)
    return list(files)


if __name__ == "__main__":
    print("wrote", write())
