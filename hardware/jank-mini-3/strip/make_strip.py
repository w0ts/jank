# SPDX-License-Identifier: GPL-3.0-only
"""jank-mini-3's bridge strip (mots, 2026-11-18; round 4, review mini3-4B W1): a small 2-layer PCB, ONE 1 x 14 female
socket (C2905420, 8.5 mm) for the LCD-2's pins, and beside it, 3.81 mm away, a row of 14 plated holes (1.15 mm) for OUR
male row's long pins (J15 / J16: C44526, 11.5 mm below their plastic), each socket pin joined straight across to its
hole. mots seats the LCD-2 in its socket first, then solders our pins in the holes from below (they stand ~1.4 mm
under the strip): the LCD-2 stays removable, the strip is fixed to our board. (Was: two sockets, at the same height;
our pins end ~6 mm deeper than the LCD-2's, so its pins would barely enter theirs.) 2 per unit, identical: turned
180 degrees, the same strip serves the other row. Writes strip.kicad_pcb.   KiCad's python: make_strip.py"""
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
LIB = os.path.join(HERE, "..", "lib", "chip.pretty")
FP = "HDR-TH_14P-P2.54-V-F"
MM = pcbnew.FromMM
OX = OY = 100.0
PITCH, N, D = 2.54, 14, 3.81            # pin pitch, pins per row, row to row (make_board.D_ROW)
L = N * PITCH + 1.5                     # 37.06: the sockets' 35.56 plus 0.75 a side
W = D + 4.6                             # 8.41: the rows plus 2.3 a side (the socket bodies are 2.54 wide)
TRACK = 0.5


def seg(board, x0, y0, x1, y1, layer, width):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(pcbnew.VECTOR2I(MM(OX + x0), MM(OY + y0)))
    s.SetEnd(pcbnew.VECTOR2I(MM(OX + x1), MM(OY + y1)))
    s.SetLayer(layer)
    s.SetWidth(MM(width))
    board.Add(s)


def socket(board, ref, y):
    fp = pcbnew.FootprintLoad(LIB, FP)
    if fp is None:
        sys.exit(f"no footprint {FP}")
    fp.SetReference(ref)
    fp.SetValue("1x14 F 8.5 (C2905420)")
    fp.SetFPID(pcbnew.LIB_ID("chip", FP))
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    board.Add(fp)
    fp.SetOrientationDegrees(0)
    fp.SetPosition(pcbnew.VECTOR2I(MM(OX + L / 2), MM(OY + y)))
    return fp


def holes(board, ref, y, beside):
    """14 plated holes straight across from the socket's pins: 1.15 mm drill, 1.9 mm pads (a 0.64 mm square pin is 0.905 mm across; review 5B R4: 1.0 left 0.015 mm at JLC's -0.08)"""
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.SetValue("our row: plated holes")
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    board.Add(fp)
    for p in beside.Pads():
        pad = pcbnew.PAD(fp)
        pad.SetNumber(p.GetNumber())
        pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        pad.SetShape(pcbnew.PAD_SHAPE_RECT if p.GetNumber() == "1" else pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(pcbnew.VECTOR2I(MM(1.9), MM(1.9)))
        pad.SetDrillSize(pcbnew.VECTOR2I(MM(1.15), MM(1.15)))
        pad.SetLayerSet(pcbnew.PAD.PTHMask())
        fp.Add(pad)
        pad.SetPosition(pcbnew.VECTOR2I(p.GetPosition().x, MM(OY + y)))
    return fp


def main() -> None:
    board = pcbnew.BOARD()
    board.SetFileName(os.path.join(HERE, "strip.kicad_pcb"))
    board.GetDesignSettings().SetCopperLayerCount(2)
    for a, b in (((0, 0), (L, 0)), ((L, 0), (L, W)), ((L, W), (0, W)), ((0, W), (0, 0))):
        seg(board, *a, *b, pcbnew.Edge_Cuts, 0.1)
    y_a, y_b = W / 2 - D / 2, W / 2 + D / 2
    fa = socket(board, "J1", y_a)             # (the LCD-2's row)
    fb = holes(board, "J2", y_b, fa)          # (our row: plated holes for the long pins, soldered from below)
    for k in range(1, N + 1):
        n = pcbnew.NETINFO_ITEM(board, f"P{k}")
        board.Add(n)
        pa = next(p for p in fa.Pads() if p.GetNumber() == str(k))
        pb = next(p for p in fb.Pads() if p.GetNumber() == str(k))
        pa.SetNet(n)
        pb.SetNet(n)
        if abs(pa.GetPosition().x - pb.GetPosition().x) > MM(0.01):
            sys.exit(f"pin {k}: the two rows are not straight across")
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):          # (both layers: two parallel paths per pin)
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(pa.GetPosition())
            t.SetEnd(pb.GetPosition())
            t.SetWidth(MM(TRACK))
            t.SetLayer(layer)
            t.SetNet(n)
            board.Add(t)
    # (review mini3-3B R5: fab files need an origin and a legible part: the pin-1 end marked on both sides, a name)
    board.GetDesignSettings().SetAuxOrigin(pcbnew.VECTOR2I(MM(OX), MM(OY + W)))
    p1 = next(p for p in fa.Pads() if p.GetNumber() == "1").GetPosition()
    x1 = pcbnew.ToMM(p1.x) - OX
    for layer, mirror in ((pcbnew.F_SilkS, False), (pcbnew.B_SilkS, True)):
        for text, x in (("1", 0.7 if x1 < L / 2 else L - 0.7), ("14", L - 0.7 if x1 < L / 2 else 0.7)):
            t = pcbnew.PCB_TEXT(board)
            t.SetText(text)
            t.SetLayer(layer)
            t.SetTextSize(pcbnew.VECTOR2I(MM(1.0), MM(1.0)))   # (JLC minimum 1.0, review 5B)
            t.SetTextThickness(MM(0.15))
            t.SetTextAngleDegrees(90)
            t.SetMirrored(mirror)
            t.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + W / 2)))
            board.Add(t)
    board.Save(board.GetFileName())
    print(f"strip {L:.2f} x {W:.2f} mm: one 1x14 socket (the LCD-2) and 14 plated holes (our pins) {D} mm apart, {N} pins joined straight across")


if __name__ == "__main__":
    main()
