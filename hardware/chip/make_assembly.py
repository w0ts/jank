# SPDX-License-Identifier: GPL-3.0-only
"""JLCPCB's assembly files for jank chip: bom.csv (Comment, Designator, Footprint, LCSC Part #) and cpl.csv (Designator,
Mid X, Mid Y, Layer, Rotation), as hardware/pcb/make_assembly.py writes them, from the board and parts.py.

  .../python3.9 make_assembly.py      (chip.kicad_pcb -> fab/)

Every footprint is JLC's own (lib/jlc.pretty, EasyEDA), so Mid X / Mid Y are the footprint's origin and the rotation is
KiCad's, with no correction table. To prove it, each part's pads are compared with JLC's footprint for its LCSC code
placed at the CPL position and rotation (mirrored for a bottom part, as KiCad mirrors it): every pad within 0.01 mm, or the script stops.
Every part is on the top side (JLC Economic). The cable pads P2 / P3 are not parts; the DNP parts (parts.DNP: the
unfitted matching positions and the u.FL path) are pads only, left out of both files.
Also writes summary.json (parts, lines, joints, sides) for cost.py.
"""
import csv
import json
import math
import os
import re
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import parts as P

NAME = "chip"
FAB = os.path.join(HERE, "fab")
JLC = os.path.join(HERE, "lib", "jlc.pretty")


def pads_of(fp):
    return [(p.GetNumber(), pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y)) for p in fp.Pads() if p.GetNumber()]


def fit_error(fp, x, y, rot, bottom):
    """JLC's footprint at (x, y) (KiCad page mm, y down), turned rot (counter-clockwise seen from the top), mirrored in y
    first for a bottom part: the largest distance from one of its pads to the board's pad of the same number"""
    ref = pcbnew.FootprintLoad(JLC, fp.GetFPID().GetLibItemName().wx_str())
    c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    board = {}
    for n, px, py in pads_of(fp):
        board.setdefault(n, []).append((px, py))
    worst = 0.0
    for n, lx, ly in pads_of(ref):
        if bottom:
            ly = -ly                     # (KiCad's flipped footprint at orientation 0 is its library drawing mirrored in y)
        gx, gy = x + lx * c + ly * s, y - lx * s + ly * c
        worst = max(worst, min(math.hypot(gx - bx, gy - by) for bx, by in board[n]))
    return worst


def main():
    b = pcbnew.LoadBoard(os.path.join(HERE, f"{NAME}.kicad_pcb"))
    ox, oy = (pcbnew.ToMM(v) for v in (b.GetDesignSettings().GetAuxOrigin().x, b.GetDesignSettings().GetAuxOrigin().y))
    info = {p[0]: p for p in P.PARTS}
    rows, places, joints, sides = [], [], 0, set()
    for fp in b.GetFootprints():
        ref = fp.GetReference()
        lcsc = info[ref][4]
        if not lcsc or not P.fitted(ref):
            continue                                     # (the cable pads; the unfitted positions)
        x, y = pcbnew.ToMM(fp.GetPosition().x), pcbnew.ToMM(fp.GetPosition().y)
        bottom = fp.IsFlipped()
        rot = round(fp.GetOrientationDegrees()) % 360
        err = fit_error(fp, x, y, rot, bottom)
        if err > 0.01:
            raise SystemExit(f"{ref}: JLC's footprint at the CPL place is {err:.3f} mm off its pads")
        name = fp.GetFPID().GetLibItemName().wx_str()
        rows.append((info[ref][2], ref, name, lcsc))
        places.append((ref, x - ox, oy - y, "Bottom" if bottom else "Top", rot))
        joints += sum(1 for p in fp.Pads() if p.GetNumber())
        sides.add("Bottom" if bottom else "Top")
    refs = [p[0] for p in places]
    assert len(refs) == len(set(refs))
    W = pcbnew.ToMM(b.GetBoardEdgesBoundingBox().GetWidth())
    H = pcbnew.ToMM(b.GetBoardEdgesBoundingBox().GetHeight())
    for ref, x, y, _s, _r in places:
        assert 0 <= x <= W and 0 <= y <= H, (ref, x, y)
    nat = lambda r: [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", r)]
    groups = {}
    for value, ref, name, code in rows:
        groups.setdefault((value, name, code), []).append(ref)
    os.makedirs(FAB, exist_ok=True)
    with open(os.path.join(FAB, "bom.csv"), "w", newline="") as f:
        w = csv.writer(f, quoting=csv.QUOTE_ALL, lineterminator="\n")
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (value, name, code), rs in sorted(groups.items(), key=lambda g: nat(sorted(g[1], key=nat)[0])):
            w.writerow([value, ",".join(sorted(rs, key=nat)), name, code])
    with open(os.path.join(FAB, "cpl.csv"), "w") as f:
        f.write("Designator,Mid X,Mid Y,Layer,Rotation\n")
        for ref, x, y, side, rot in sorted(places, key=lambda p: nat(p[0])):
            f.write(f"{ref},{x:.3f}mm,{y:.3f}mm,{side},{rot}\n")
    summary = {"variant": NAME, "board_mm": [round(W, 2), round(H, 2)], "parts": len(rows), "lines": len(groups),
               "joints": joints, "sides": sorted(sides),
               "qty": {code: len(rs) for (_v, _n, code), rs in groups.items()}}
    json.dump(summary, open(os.path.join(FAB, "summary.json"), "w"), indent=1)
    print(f"{os.path.relpath(FAB, HERE)}/bom.csv {len(groups)} lines, cpl.csv {len(rows)} parts ({', '.join(sorted(sides))}),"
          f" {joints} joints; every part within 0.01 mm of JLC's footprint")


if __name__ == "__main__":
    main()
