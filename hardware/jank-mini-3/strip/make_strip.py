# SPDX-License-Identifier: GPL-3.0-only
"""jank-mini-3's bridge strip (Henri, 2026-11-18): a small 2-layer PCB with two 1 x 14 female sockets (C2905420,
8.5 mm) 3.81 mm apart, each socket pin joined straight across to its partner. Pushed up from below onto the LCD-2's
pins and our male row beside them; 2 per unit (identical: the same strip serves both rows). No screws, no other parts.
Writes strip.kicad_pcb next to this file.   KiCad's python: make_strip.py"""
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


def main() -> None:
    board = pcbnew.BOARD()
    board.SetFileName(os.path.join(HERE, "strip.kicad_pcb"))
    board.GetDesignSettings().SetCopperLayerCount(2)
    for a, b in (((0, 0), (L, 0)), ((L, 0), (L, W)), ((L, W), (0, W)), ((0, W), (0, 0))):
        seg(board, *a, *b, pcbnew.Edge_Cuts, 0.1)
    y_a, y_b = W / 2 - D / 2, W / 2 + D / 2
    fa, fb = socket(board, "J1", y_a), socket(board, "J2", y_b)
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
    board.Save(board.GetFileName())
    print(f"strip {L:.2f} x {W:.2f} mm, 2 x 1x14 sockets {D} mm apart, {N} pins joined straight across")


if __name__ == "__main__":
    main()
