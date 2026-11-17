# SPDX-License-Identifier: GPL-3.0-only
"""the board's parts for the placer: python3.9 (KiCad's) dump.py BOARD.kicad_pcb OUT.json (every footprint: place,
side, courtyard box, pads with nets, through-hole flags)"""
import json, sys, pcbnew
b = pcbnew.LoadBoard(sys.argv[1])
T = lambda v: round(pcbnew.ToMM(v), 3)
out = {}
for f in b.GetFootprints():
    r = f.GetReference()
    cy = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd)
    if cy.OutlineCount():
        bb = cy.BBox(); box = [T(bb.GetX()) - 100, T(bb.GetY()) - 100, T(bb.GetRight()) - 100, T(bb.GetBottom()) - 100]
    else:
        bb = f.GetBoundingBox(False); box = [T(bb.GetX()) - 100, T(bb.GetY()) - 100, T(bb.GetRight()) - 100, T(bb.GetBottom()) - 100]
    pads = [[p.GetNumber(), p.GetNetname(), T(p.GetPosition().x) - 100, T(p.GetPosition().y) - 100, {pcbnew.PAD_ATTRIB_PTH: 'pth', pcbnew.PAD_ATTRIB_NPTH: 'npth'}.get(p.GetAttribute(), 'smd'), T(p.GetBoundingBox().GetWidth()), T(p.GetBoundingBox().GetHeight()), T(max(p.GetSize().x, p.GetSize().y)), T(p.GetDrillSize().x)] for p in f.Pads()]
    out[r] = dict(x=T(f.GetPosition().x) - 100, y=T(f.GetPosition().y) - 100, rot=f.GetOrientationDegrees(), side="B" if f.IsFlipped() else "T",
                  fp=f.GetFPID().GetLibItemName().wx_str(), val=f.GetValue(), box=box, pads=pads, dnp=f.IsDNP(), h=None)
json.dump(out, open(sys.argv[2], "w"), indent=0)
print(len(out))
