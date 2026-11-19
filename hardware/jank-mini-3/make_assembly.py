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


FULL = os.environ.get("JLC_FULL") == "1"   # (fab/full/: the bottom through-hole parts on JLC's BOM / CPL too)


def main():
    board = pcbnew.LoadBoard(os.path.join(HERE, "lcd2.kicad_pcb"))
    org = board.GetDesignSettings().GetAuxOrigin()   # (the Gerbers' origin, make_fab.sh --use-drill-file-origin)
    ox, oy = pcbnew.ToMM(org.x), pcbnew.ToMM(org.y)
    if (ox, oy) == (0.0, 0.0):
        sys.exit("make_assembly: lcd2.kicad_pcb has no aux origin; the CPL would not match the Gerbers")
    part = {p[0]: p for p in P.PARTS}
    rows, bom, joints, tht_joints, hand = [], {}, 0, 0, []
    for f in board.GetFootprints():
        r = f.GetReference()
        if r not in part or not P.fitted(r) or not (part[r][4] or (B.MINI3 and f.IsFlipped())):
            continue
        _r, _k, value, fp, lcsc, _pins = part[r]
        if B.MINI3 and (f.IsFlipped() or r in getattr(P, "HAND", ())) and not FULL:   # (mots solders it: the hand list)
            hand.append([r, value, P.FP[fp], lcsc,
                         len([p for p in f.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH])])
            continue
        pos = f.GetPosition()
        rows.append([r, f"{pcbnew.ToMM(pos.x) - ox:.4f}", f"{-(pcbnew.ToMM(pos.y) - oy):.4f}",
                     "Bottom" if f.IsFlipped() else "Top", f"{f.GetOrientationDegrees() % 360:.0f}"])
        key = (P.FP[fp], lcsc)   # (one BOM line per LCSC code: JLC refuses two lines matched to one part, 2026-11-19)
        bom.setdefault(key, []).append((r, value))
        n = len([p for p in f.Pads() if p.GetAttribute() in (pcbnew.PAD_ATTRIB_SMD, pcbnew.PAD_ATTRIB_PTH)])
        if all(p.GetAttribute() != pcbnew.PAD_ATTRIB_SMD for p in f.Pads()):
            tht_joints += n
        else:
            joints += n
    out = os.path.join(HERE, "fab", "full") if FULL else os.path.join(HERE, "fab")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "lcd2-cpl.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writerows(sorted(rows, key=lambda x: x[0]))
    with open(os.path.join(out, "lcd2-bom.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC"])
        for (fpn, lcsc), items in sorted(bom.items(), key=lambda kv: kv[0][1]):
            values = sorted({v for _r, v in items})
            w.writerow([" / ".join(values)[:80], ",".join(sorted(r for r, _v in items)), fpn, lcsc])
    if FULL:   # (the quote variant: JLC also solders the bottom through-hole parts; Standard PCBA, both sides)
        print(f"full assembly: {len(rows)} parts in fab/full/ (bottom through-hole rotations: check JLC's preview)")
        return
    if B.MINI3:
        with open(os.path.join(HERE, "fab", "lcd2-hand.csv"), "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["Designator", "Value", "Footprint", "LCSC", "Joints"])
            w.writerows(sorted(hand))
            # (review 5B, mots 2026-11-19: the bridge strips too, per unit: 2 strip PCBs (strip/), each with two 1x14
            #  sockets (the LCD-2's pins, ours): 28 joints a strip; nothing soldered between the strip and our board)
            w.writerow(["STRIP x2", "bridge strip PCB (strip/fab)", "strip.kicad_pcb", "", 0])
            w.writerow(["STRIP-J1,J2 x2", "1x14 F socket 8.5 mm, 2 a strip", "HDR-TH_14P-P2.54-V-F", "C2905420", 56])
        print(f"hand-soldered by mots: {len(hand)} parts, {sum(h[4] for h in hand)} joints (fab/lcd2-hand.csv)")
    qty = {}
    for (_f, lcsc), items in bom.items():
        qty[lcsc] = qty.get(lcsc, 0) + len(items)
    json.dump({"qty": qty, "smt_joints": joints, "tht_joints": tht_joints, "parts": len(rows), "types": len(qty)},
              open(os.path.join(HERE, "fab", "summary.json"), "w"), indent=1)
    print(f"assembly: {len(rows)} parts, {len(qty)} part types, {joints} SMT pads, {tht_joints} THT joints")


if __name__ == "__main__":
    main()
