# SPDX-License-Identifier: GPL-3.0-only
"""JLC's assembly files from lcd2.kicad_pcb: fab/lcd2-bom.csv (Comment, Designator, Footprint, LCSC) and
fab/lcd2-cpl.csv (Designator, Mid X, Mid Y, Layer, Rotation). Every footprint is JLC's own (EasyEDA) for its LCSC
code, so the CPL position and rotation are the footprint's origin and KiCad rotation (as mini 2.2's make_assembly.py).
Fitted parts only (parts.DNP left out). jank-mini-3 (mots, 2026-11-18): JLC assembles the TOP only; every part under
the board (the DINs, the jacks, the male rows J15 / J16, the cell sockets) is hand-soldered by mots and goes to
fab/lcd2-hand.csv instead (ref, value, LCSC, joints; "TO PICK" where no part is chosen yet). Older layouts: the
through-hole parts on the BOM too (JLC hand-solders them). Writes fab/summary.json (codes, counts, joints).
KiCad's python: make_assembly.py"""
import csv
import json
import os
import sys

import pcbnew

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import parts as P
import make_board as B

OX = OY = 100.0


def main():
    board = pcbnew.LoadBoard(os.path.join(HERE, "lcd2.kicad_pcb"))
    part = {p[0]: p for p in P.PARTS}
    rows, bom, joints, tht_joints, hand = [], {}, 0, 0, []
    for f in board.GetFootprints():
        r = f.GetReference()
        if r not in part or not P.fitted(r) or not (part[r][4] or (B.MINI3 and f.IsFlipped())):
            continue
        _r, _k, value, fp, lcsc, _pins = part[r]
        if B.MINI3 and f.IsFlipped():   # (mots solders it: the hand list, not JLC's files)
            hand.append([r, value, P.FP[fp], "TO PICK (male 1x14 header)" if r in ("J15", "J16") else lcsc,
                         len([p for p in f.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH])])
            continue
        pos = f.GetPosition()
        rows.append([r, f"{pcbnew.ToMM(pos.x) - OX:.4f}", f"{-(pcbnew.ToMM(pos.y) - OY):.4f}",
                     "Bottom" if f.IsFlipped() else "Top", f"{f.GetOrientationDegrees() % 360:.0f}"])
        key = (value, P.FP[fp], lcsc)
        bom.setdefault(key, []).append(r)
        n = len([p for p in f.Pads() if p.GetAttribute() in (pcbnew.PAD_ATTRIB_SMD, pcbnew.PAD_ATTRIB_PTH)])
        if all(p.GetAttribute() != pcbnew.PAD_ATTRIB_SMD for p in f.Pads()):
            tht_joints += n
        else:
            joints += n
    os.makedirs(os.path.join(HERE, "fab"), exist_ok=True)
    with open(os.path.join(HERE, "fab", "lcd2-cpl.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writerows(sorted(rows, key=lambda x: x[0]))
    with open(os.path.join(HERE, "fab", "lcd2-bom.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC"])
        for (value, fpn, lcsc), refs in sorted(bom.items(), key=lambda kv: kv[0][2]):
            w.writerow([value, ",".join(sorted(refs)), fpn, lcsc])
    if B.MINI3:
        with open(os.path.join(HERE, "fab", "lcd2-hand.csv"), "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["Designator", "Value", "Footprint", "LCSC", "Joints"])
            w.writerows(sorted(hand))
        print(f"hand-soldered by mots: {len(hand)} parts, {sum(h[4] for h in hand)} joints (fab/lcd2-hand.csv)")
    qty = {}
    for (_v, _f, lcsc), refs in bom.items():
        qty[lcsc] = qty.get(lcsc, 0) + len(refs)
    json.dump({"qty": qty, "smt_joints": joints, "tht_joints": tht_joints, "parts": len(rows), "types": len(qty)},
              open(os.path.join(HERE, "fab", "summary.json"), "w"), indent=1)
    print(f"assembly: {len(rows)} parts, {len(qty)} part types, {joints} SMT pads, {tht_joints} THT joints")


if __name__ == "__main__":
    main()
