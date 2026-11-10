# SPDX-License-Identifier: GPL-3.0-only
"""lib/tiny.pretty from lib/jlc.pretty (JLC's own footprints, fetch_jlc.sh) and one of ours:
- every JLC footprint as fetched, its 3D model reference dropped, its peg holes made unplated (EasyEDA exports them as
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
OUT = os.path.join(HERE, "lib", "tiny.pretty")
MARGIN = 0.15
TRS_PITCH, TRS_PAD_W, TRS_PAD_H = 2.54, 1.5, 2.0


def bbox(src):
    xs, ys = [], []
    for m in re.finditer(r"\(pad \S+ \S+ \S+ \(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\) \(size ([\d.]+) ([\d.]+)\)", src):
        x, y, r, w, h = float(m[1]), float(m[2]), float(m[3] or 0), float(m[4]), float(m[5])
        if round(r) % 180 == 90:
            w, h = h, w
        xs += [x - w / 2, x + w / 2]
        ys += [y - h / 2, y + h / 2]
    for m in re.finditer(r"\(fp_line \(start ([-\d.]+) ([-\d.]+)\) \(end ([-\d.]+) ([-\d.]+)\) \(layer (F\.SilkS|F\.Fab)\)", src):
        xs += [float(m[1]), float(m[3])]
        ys += [float(m[2]), float(m[4])]
    return min(xs) - MARGIN, min(ys) - MARGIN, max(xs) + MARGIN, max(ys) + MARGIN


def jlc(path):
    src = open(path).read()
    src = re.sub(r'\n\t\(model ".*?\n\t\)', "", src, flags=re.S)
    src = re.sub(r"\n\t\(fp_line [^\n]*F\.CrtYd[^\n]*", "", src)
    # (EasyEDA's unplated peg holes come as plated pads with no ring: make them holes)
    src = re.sub(r'\(pad "" thru_hole circle (\(at [^)]*\)) \(size ([\d.]+) ([\d.]+)\) \(drill ([\d.]+)\) \(layers \*\.Cu \*\.Mask\)\)',
                 r'(pad "" np_thru_hole circle \1 (size \2 \3) (drill \4) (layers *.Cu *.Mask))', src)
    name = os.path.basename(path)[:-len(".kicad_mod")]
    x0, y0, x1, y1 = bbox(src)                   # (the courtyard takes the drawing before the silk goes)
    src = re.sub(r"\n\t\(fp_(?:line|circle|arc|text user) [^\n]*F\.SilkS[^\n]*", "", src)   # (EasyEDA's silk runs over pads and past
    #  the board edge; the board's own labels say what matters)
    src = src.replace(f"(module easyeda2kicad:{name} ", f"(module {name} ", 1)
    cy = "".join(f"\n\t(fp_line (start {a:.3f} {b:.3f}) (end {c:.3f} {d:.3f}) (layer F.CrtYd) (width 0.05))"
                 for a, b, c, d in ((x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)))
    k = src.rstrip().rindex(")")
    return name, src[:k].rstrip() + cy + "\n)\n"


def trs_pads():
    """three surface-mount pads in a column (T, R, S from the top, 2.54 apart), on the top side: a cable soldered
    there leaves through the jack's opening; no hole, so nothing under it on the bottom is in the way"""
    pads, silk = [], []
    for i, n in enumerate("TRS"):
        y = i * TRS_PITCH
        pads.append(f'  (pad "{n}" smd rect (at 0 {y:.2f}) (size {TRS_PAD_W} {TRS_PAD_H}) (layers "F.Cu" "F.Paste" "F.Mask"))')
        silk.append(f'  (fp_text user "{n}" (at {TRS_PAD_W / 2 + 0.75:.2f} {y:.2f}) (layer "F.SilkS") (effects (font (size 0.7 0.7) (thickness 0.12))))')
    a, b = TRS_PAD_W / 2 + 0.2, TRS_PAD_H / 2 + 0.2
    return "\n".join(['(footprint "TRS_PADS_SMD" (version 20240108) (generator "make_footprints.py") (layer "F.Cu")',
                      '  (descr "three pads for a TRS cable (tip, ring, sleeve) beside a jack; not a part, no paste needed")',
                      "  (attr smd exclude_from_pos_files exclude_from_bom)",
                      '  (property "Reference" "REF**" (at 0 -2 0) (layer "F.SilkS") (hide yes) (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      '  (property "Value" "TRS pads" (at 0 -2 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                      *pads, *silk,
                      f'  (fp_rect (start {-a:.2f} {-b:.2f}) (end {a:.2f} {2 * TRS_PITCH + b:.2f}) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
                      ")"]) + "\n"


def write():
    os.makedirs(OUT, exist_ok=True)
    names = []
    for path in sorted(glob.glob(os.path.join(SRC, "*.kicad_mod"))):
        name, body = jlc(path)
        open(os.path.join(OUT, name + ".kicad_mod"), "w").write(body)
        names.append(name)
    open(os.path.join(OUT, "TRS_PADS_SMD.kicad_mod"), "w").write(trs_pads())
    return names + ["TRS_PADS_SMD"]


if __name__ == "__main__":
    print("wrote", write())
