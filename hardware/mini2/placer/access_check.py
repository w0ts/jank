# SPDX-License-Identifier: GPL-3.0-only
"""the pin-access check on a KiCad board, with rules.py's lanes: KiCad's python3.9 access_check.py BOARD.kicad_pcb
[OUT.json]. Prints the pads short of their lanes (signal SMD pads need 1, the converter loops' pads 2, the connectors'
signal pins a 2.5 mm lane clear of other parts' courtyards); OUT.json lists them for the overlay."""
import json, os, sys
import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model as M
import rules as R

T = lambda v: pcbnew.ToMM(v) - 100.0
b = pcbnew.LoadBoard(sys.argv[1])
pads, owner, boxes = [], {}, {}
for f in b.GetFootprints():
    ref = f.GetReference()
    cy = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd).BBox()
    if not f.IsFlipped():
        boxes[ref] = (T(cy.GetLeft()), T(cy.GetTop()), T(cy.GetRight()), T(cy.GetBottom()))
    for p in f.Pads():
        bb = p.GetBoundingBox()
        attr = {pcbnew.PAD_ATTRIB_PTH: "pth", pcbnew.PAD_ATTRIB_NPTH: "npth"}.get(p.GetAttribute(), "smd")
        q = (f"{ref}.{p.GetNumber()}", p.GetNetname(), T(p.GetPosition().x), T(p.GetPosition().y),
             pcbnew.ToMM(bb.GetWidth()), pcbnew.ToMM(bb.GetHeight()), attr)
        if attr == "smd" and f.IsFlipped():
            continue
        pads.append(q)
        owner[q[0]] = (ref, f.IsFlipped())
ep = [q for q in pads if q[0] == "U1.41"]
EP = (min(q[2] for q in ep) - 1.6, min(q[3] for q in ep) - 1.6, max(q[2] for q in ep) + 1.6, max(q[3] for q in ep) + 1.6)
singles = {n for n in {q[1] for q in pads} if sum(1 for q in pads if q[1] == n) < 2}


class U:      # (rules.need's view of a part)
    def __init__(self, ref, flipped):
        self.ref, self.side = ref, ("B" if flipped else "T")


out, every = [], []
for p in pads:
    ref, fl = owner[p[0]]
    nd = R.need(U(ref, fl), p)
    if not nd or p[1] in singles:
        continue
    near = [q for q in pads if q is not p and abs(q[2] - p[2]) < 5 and abs(q[3] - p[3]) < 5]
    bx = [v for r, v in boxes.items() if r != ref and not r.startswith(("RT", "CT"))] if nd == "conn" else []
    n = R.free_count(p, near, bx, EP, nd == "conn")
    every.append((p[0], nd, n, round(p[2], 2), round(p[3], 2)))
    if not R.ok(nd, n):
        out.append((p[0], p[1], nd, n, round(p[2], 2), round(p[3], 2)))
for x in sorted(out):
    print(*x)
print(f"short of lanes: {len(out)}, of them with no way out: {sum(1 for x in out if x[3] == 0)}")
if len(sys.argv) > 2:
    json.dump({"short": out, "all": every}, open(sys.argv[2], "w"))
