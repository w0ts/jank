# SPDX-License-Identifier: GPL-3.0-only
"""lib/chip.pretty from lib/jlc.pretty (JLC's own footprints, fetch_jlc.sh) and one of ours:
- every JLC footprint as fetched, its 3D model pointed at lib/3d (${KIPRJMOD}, for kicad-cli's render), the QFN's nine
  thermal-via "pads" (unnumbered, no net) dropped (make_board.py puts GND vias in the exposed pad instead), its peg
  holes made unplated (EasyEDA exports them as
  plated pads with no ring), its silkscreen dropped (it runs over pads and past the board edge) and its courtyard redrawn as one rectangle round its
  pads and its drawing (EasyEDA's courtyards are often smaller than the pads: R0603's is 1.6 x 0.8 round pads that
  reach 2.3 mm), 0.15 mm out; the pads, the origin and the orientation stay JLC's (the CPL depends on them);
- TRS_PADS_SMD: three top-side pads (T, R, S; 1.5 x 2.0 mm, 2.54 mm apart) for a soldered cable.

  python3 make_footprints.py      (run by make_board.py)
"""
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "lib", "jlc.pretty")
OUT = os.path.join(HERE, "lib", "chip.pretty")
MARGIN = 0.15
TRS_PITCH, TRS_PAD_W, TRS_PAD_H = 2.54, 1.5, 2.0


PADS_ONLY = ("C0402", "R0402", "L0402")   # (their body, 1.0 x 0.5, lies inside the pads; EasyEDA's silk box round them
#  is 1.9 x 1.1, which kept the RF and decoupling parts too far from the chip's pins)


def bbox(src, pads_only=False):
    xs, ys = [], []
    for m in re.finditer(r"\(pad \S+ \S+ \S+ \(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\) \(size ([\d.]+) ([\d.]+)\)", src):
        x, y, r, w, h = float(m[1]), float(m[2]), float(m[3] or 0), float(m[4]), float(m[5])
        if round(r) % 180 == 90:
            w, h = h, w
        xs += [x - w / 2, x + w / 2]
        ys += [y - h / 2, y + h / 2]
    for m in re.finditer(r"\(fp_line \(start ([-\d.]+) ([-\d.]+)\) \(end ([-\d.]+) ([-\d.]+)\) \(layer (F\.SilkS|F\.Fab)\)",
                         "" if pads_only else src):
        xs += [float(m[1]), float(m[3])]
        ys += [float(m[2]), float(m[4])]
    return min(xs) - MARGIN, min(ys) - MARGIN, max(xs) + MARGIN, max(ys) + MARGIN


def jlc(path):
    src = open(path).read()
    src = re.sub(r'\(model "[^"]*/([^"/]+\.wrl)"', r'(model "${KIPRJMOD}/lib/3d/\1"', src)
    src = re.sub(r'\n\t\(pad "" thru_hole circle [^\n]*\*\.Paste[^\n]*', "", src)   # (the QFN's thermal vias)
    src = re.sub(r"\n\t\(fp_line [^\n]*F\.CrtYd[^\n]*", "", src)
    # (EasyEDA's unplated peg holes come as plated pads with no ring: make them holes)
    src = re.sub(r'\(pad "" thru_hole circle (\(at [^)]*\)) \(size ([\d.]+) ([\d.]+)\) \(drill ([\d.]+)\) \(layers \*\.Cu \*\.Mask\)\)',
                 r'(pad "" np_thru_hole circle \1 (size \2 \3) (drill \4) (layers *.Cu *.Mask))', src)
    name = os.path.basename(path)[:-len(".kicad_mod")]
    if name == DIN:
        src = din_pads(src)
    if name == PH2:                               # (mini 2.2: pin 1 square, the usual mark of pin 1 = + on a PH2.0)
        src = src.replace("(pad 1 thru_hole circle", "(pad 1 thru_hole rect", 1)
    x0, y0, x1, y1 = bbox(src, name in PADS_ONLY)                 # (the courtyard takes the drawing before the silk goes)
    src = re.sub(r"\n\t\(fp_(?:line|circle|arc|poly|text user) [^\n]*F\.SilkS[^\n]*", "", src)   # (EasyEDA's silk runs over pads and past
    #  the board edge; the board's own labels say what matters)
    src = src.replace(f"(module easyeda2kicad:{name} ", f"(module {name} ", 1)
    cy = "".join(f"\n\t(fp_line (start {a:.3f} {b:.3f}) (end {c:.3f} {d:.3f}) (layer F.CrtYd) (width 0.05))"
                 for a, b, c, d in ((x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)))
    k = src.rstrip().rindex(")")
    return name, src[:k].rstrip() + cy + "\n)\n"


def trs_pads():
    """three surface-mount pads in a column (T, R, S from the top, 2.54 apart; no letters: no room for them), on the top side: a cable soldered
    there leaves through the jack's opening; no hole, so nothing under it on the bottom is in the way"""
    pads, silk = [], []
    for i, n in enumerate("TRS"):
        y = i * TRS_PITCH
        pads.append(f'  (pad "{n}" smd rect (at 0 {y:.2f}) (size {TRS_PAD_W} {TRS_PAD_H}) (layers "F.Cu" "F.Paste" "F.Mask"))')
    a, b = TRS_PAD_W / 2 + 0.2, TRS_PAD_H / 2 + 0.2
    return "\n".join(['(footprint "TRS_PADS_SMD" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      '  (descr "three pads for a TRS cable (tip, ring, sleeve) beside a jack; not a part, no paste needed")',
                      "  (attr smd exclude_from_pos_files exclude_from_bom)",
                      '  (property "Reference" "REF**" (at 0 -2 0) (layer "F.SilkS") (hide yes) (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      '  (property "Value" "TRS pads" (at 0 -2 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      *pads, *silk,
                      f'  (fp_rect (start {-a:.2f} {-b:.2f}) (end {a:.2f} {2 * TRS_PITCH + b:.2f}) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
                      ")"]) + "\n"


# The DIN sockets are soldered by hand (README "DIN"): JLC's 2.2 mm pads on the 1.5 mm holes (HOOYA's drawing: 7 x
# 1.5 mm) leave a 0.35 mm ring. Pins 1, 2, 4, 5 (3.54 mm apart in pairs) get 3.0 x 4.2 mm ovals: 1.5 mm of copper
# round the hole on the side of the pair's other pin (0.54 mm between them), stretched 1.2 mm away from it; pin 3 (the
# middle, 5.6 mm from its neighbours) and the shell's two pins get 4.0 mm round pads. (pad: angle of the oval's long axis
# in KiCad's sense, or None for a round pad; size)
DIN = "CONN-TH_DIN-504B-M10"
PH2 = "CONN-TH_B2B-PH-K-S"                        # (mini 2.2: the cells' sockets)
DIN_PADS = {"1": (None, 2.4), "2": (None, 3.0), "4": (None, 3.0), "5": (None, 2.4), "3": (None, 4.0), "6": (None, 3.5),
            "7": (None, 3.5)}
DIN_STRETCH = 1.2


def din_pads(src):
    def pad(m):
        num, x, y = m[1], m[2], m[3]
        angle, size = DIN_PADS[num]
        if angle is None:
            return f'(pad {num} thru_hole circle (at {x} {y} 0) (size {size} {size}) (drill 1.5) (layers *.Cu *.Mask))'
        return (f'(pad {num} thru_hole oval (at {x} {y} {angle}) (size {size + DIN_STRETCH} {size})'
                f' (drill 1.5 (offset {DIN_STRETCH / 2} 0)) (layers *.Cu *.Mask))')
    return re.sub(r"\(pad (\d) thru_hole circle \(at ([-\d.]+) ([-\d.]+) [-\d.]+\) \(size [\d.]+ [\d.]+\) \(layers \*\.Cu \*\.Mask\)\(drill [\d.]+\)\)",
                  pad, src)


def write():
    os.makedirs(OUT, exist_ok=True)
    names = []
    for path in sorted(glob.glob(os.path.join(SRC, "*.kicad_mod"))):
        name, body = jlc(path)
        open(os.path.join(OUT, name + ".kicad_mod"), "w").write(body)
        names.append(name)
    open(os.path.join(OUT, "TRS_PADS_SMD.kicad_mod"), "w").write(trs_pads())
    open(os.path.join(OUT, "SJ3_SMD.kicad_mod"), "w").write(sj3())
    open(os.path.join(OUT, "R_TWIN_HAND.kicad_mod"), "w").write(twin())
    open(os.path.join(OUT, "PADS4_SMD.kicad_mod"), "w").write(pads4())
    open(os.path.join(OUT, "C_TWIN_HAND.kicad_mod"), "w").write(twin("C_TWIN_HAND", CTWIN_PAD, CTWIN_GAP))
    open(os.path.join(OUT, "PADS2_HAND.kicad_mod"), "w").write(pads2())
    open(os.path.join(OUT, "PADS6_SMD.kicad_mod"), "w").write(pads6())
    return names + ["TRS_PADS_SMD", "SJ3_SMD", "R_TWIN_HAND", "C_TWIN_HAND", "PADS4_SMD", "PADS2_HAND", "PADS6_SMD"]


def pads6():
    """stage 2: the recovery pads: six bare top pads 1.5 x 2.5 mm, 2.54 mm apart (a USB-UART adapter's wires or a
    pogo / header strip held on them): TX, RX, GND, 3V3, EN (CHIP_PU), BOOT (GPIO0)"""
    pads = [f'  (pad "{i + 1}" smd rect (at {(i - 2.5) * 2.54:.2f} 0) (size 1.5 2.5) (layers "F.Cu" "F.Mask"))' for i in range(6)]
    a, b = 2.5 * 2.54 + 0.75 + 0.2, 1.25 + 0.2
    return "\n".join(['(footprint "PADS6_SMD" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      '  (descr "six recovery pads, 2.54 mm pitch, not a part")',
                      '  (attr smd exclude_from_bom exclude_from_pos_files)',
                      '  (property "Reference" "J**" (at 0 -2.2 0) (layer "F.SilkS") (hide yes) (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      '  (property "Value" "pads" (at 0 2.2 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      *pads,
                      f'  (fp_rect (start {-a:.2f} {-b:.2f}) (end {a:.2f} {b:.2f}) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
                      ")", ""])


def pads4():
    """stage 2, 72 x 72: J10B, the 2.42" screen's lead pads: four bare top pads as J12's (1.5 x 2.5 mm, 2.54 mm apart)"""
    pads = [f'  (pad "{i + 1}" smd rect (at {(i - 1.5) * 2.54:.2f} 0) (size 1.5 2.5) (layers "F.Cu" "F.Mask"))' for i in range(4)]
    a, b = 1.5 * 2.54 + 0.75 + 0.2, 1.25 + 0.2
    return "\n".join(['(footprint "PADS4_SMD" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      '  (descr "four lead pads, 2.54 mm pitch, not a part")',
                      '  (attr smd exclude_from_bom exclude_from_pos_files)',
                      '  (property "Reference" "J**" (at 0 -2.2 0) (layer "F.SilkS") (hide yes) (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      '  (property "Value" "pads" (at 0 2.2 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      *pads,
                      f'  (fp_rect (start {-a:.2f} {-b:.2f}) (end {a:.2f} {b:.2f}) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
                      ")", ""])


def pads2():
    """stage 2: two hand-solder pads for a wired part (the power switch in the lid): 2 x 3 mm copper both sides round a
    1.0 mm hole (a wire goes through), 4 mm apart, a silk box"""
    pads = [f'  (pad "{n}" thru_hole rect (at {x:.2f} 0) (size 2.0 3.0) (drill 1.0) (layers "*.Cu" "*.Mask"))'
            for n, x in (("1", -2.0), ("2", 2.0))]
    return "\n".join(['(footprint "PADS2_HAND" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      '  (descr "two wire pads (a panel-mount switch), not a part")',
                      '  (attr through_hole exclude_from_bom exclude_from_pos_files)',
                      '  (property "Reference" "J**" (at 0 -2.4 0) (layer "F.SilkS") (hide yes) (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      '  (property "Value" "pads" (at 0 2.4 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      *pads,
                      '  (fp_rect (start -3.15 -1.65) (end 3.15 1.65) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
                      '  (fp_rect (start -3.15 -1.65) (end 3.15 1.65) (stroke (width 0.05) (type solid)) (fill none) (layer "B.CrtYd"))',
                      ")", ""])


def twin(name="R_TWIN_HAND", pad=None, gap=None):
    """stage 2 (the user's): a twin as a hand-solder land, as the SD board's: two bare pads 1.4 x 2.0 mm, 1.2 mm apart
    (a 0603, 0805 or 1206, or a leaded resistor's legs, soldered with an iron), no paste, not a part. C_TWIN_HAND: the
    capacitors' spare lands (the "extra DNP pads"), an 0805 hand land 1.0 x 1.4 mm, 0.9 mm apart (an 0603 or 0805 part)"""
    w, h = pad or TWIN_PAD
    x = (gap or TWIN_GAP) / 2 + w / 2
    pads = [f'  (pad "{n}" smd rect (at {s * x:.2f} 0) (size {w} {h}) (layers "F.Cu" "F.Mask"))' for n, s in (("1", -1), ("2", 1))]
    a, b = x + w / 2 + TWIN_ROOM, h / 2 + TWIN_ROOM
    return "\n".join([f'(footprint "{name}" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      '  (descr "a twin: hand-solder pads for a resistor in parallel with its neighbour; not a part, no paste")',
                      '  (attr smd exclude_from_bom exclude_from_pos_files)',
                      '  (property "Reference" "RT**" (at 0 -1.8 0) (layer "F.SilkS") (hide yes) (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      '  (property "Value" "twin" (at 0 1.8 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      *pads,
                      f'  (fp_rect (start {-a:.2f} {-b:.2f}) (end {a:.2f} {b:.2f}) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
                      ")", ""])


CTWIN_PAD, CTWIN_GAP = (1.0, 1.4), 0.9
TWIN_PAD, TWIN_GAP, TWIN_ROOM = (1.4, 2.0), 1.2, 0.5   # (stage 2: the twins' hand-solder lands; the courtyard 0.5 out:
#  two twins' copper 1.0 mm apart or more; make_board.py keeps other parts 1.0 mm off too)
SJ_PITCH, SJ_PAD_W, SJ_PAD_H = 1.3, 1.0, 1.5    # (mini 2.2: KiCad's SolderJumper-3_P1.3mm_Open_Pad1.0x1.5mm sizes)


def sj3():
    """mini 2.2: a 3-pad solder jumper (pads 1, 2, 3 in a row, 2 in the middle; 0.3 mm gaps a blob bridges), bare
    copper, no part: the screen's VCC / GND choice (JP1 / JP2)"""
    pads = [f'  (pad "{i + 1}" smd rect (at {(i - 1) * SJ_PITCH:.2f} 0) (size {SJ_PAD_W} {SJ_PAD_H}) (layers "F.Cu" "F.Mask"))'
            for i in range(3)]
    a, b = SJ_PITCH + SJ_PAD_W / 2 + 0.15, SJ_PAD_H / 2 + 0.15
    return "\n".join(['(footprint "SJ3_SMD" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      '  (descr "3-pad solder jumper, open: bridge 1-2 or 2-3 with solder; not a part")',
                      '  (attr smd exclude_from_bom exclude_from_pos_files)',
                      '  (property "Reference" "JP**" (at 0 -1.6 0) (layer "F.SilkS") (hide yes) (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      '  (property "Value" "SJ3" (at 0 1.6 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      *pads,
                      f'  (fp_rect (start {-a:.2f} {-b:.2f}) (end {a:.2f} {b:.2f}) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
                      ")", ""])


if __name__ == "__main__":
    print("wrote", write())
