# SPDX-License-Identifier: GPL-3.0-only
"""jank chip: after routing, a ground pour piece can be cut off on both layers at once (a top piece whose only via
lands in a bottom piece that tracks fence in). This finds every group of GND pour pieces, pads, tracks and vias that
is not joined to the main ground, and adds one via where the group's top piece lies over the main bottom pour (or the
reverse), then refills. Run after make_board.py (make_fab.sh does).   python3.9 tie_groups.py chip.kicad_pcb"""
import sys

import pcbnew

VIA_D, VIA_DRILL = 0.6, 0.3
EDGE = 0.27                     # (a via's centre this far inside both pieces: the pours stop 0.2 short of other nets,
                                # so its 0.3 ring keeps 0.17 from them; the refill joins the pours round it)
DIN_ROOM = 0.8 + 0.3            # (no via nearer a DIN pad's copper than the iron's room)
MM = pcbnew.FromMM


def polys(item, layer):
    if hasattr(item, "GetEffectivePolygon"):
        return item.GetEffectivePolygon(layer)
    s = pcbnew.SHAPE_POLY_SET()
    item.TransformShapeToPolygon(s, layer, 0, MM(0.005), pcbnew.ERROR_INSIDE)
    return s


def groups(b):
    par = {}

    def find(a):
        par.setdefault(a, a)
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a

    frags = []
    for z in b.Zones():
        if z.GetNetname() != "GND" or z.GetIsRuleArea():
            continue
        for L in (pcbnew.F_Cu, pcbnew.B_Cu):
            if not z.IsOnLayer(L):
                continue
            poly = z.GetFilledPolysList(L)
            for i in range(poly.OutlineCount()):
                one = pcbnew.SHAPE_POLY_SET()
                one.AddOutline(poly.Outline(i))
                for h in range(poly.HoleCount(i)):
                    one.AddHole(poly.Hole(i, h))
                frags.append((("Z", L, i), L, one))
    items = []
    for f in b.GetFootprints():
        for p in f.Pads():
            if p.GetNetname() == "GND":
                items.append((("P", f.GetReference(), p.GetNumber()), [L for L in (pcbnew.F_Cu, pcbnew.B_Cu) if p.IsOnLayer(L)], p))
    for t in b.GetTracks():
        if t.GetNetname() == "GND":
            ls = [pcbnew.F_Cu, pcbnew.B_Cu] if t.GetClass() == "PCB_VIA" else [t.GetLayer()]
            items.append((("T", t.m_Uuid.AsString()), ls, t))
    shapes = {k: {L: polys(it, L) for L in ls} for k, ls, it in items}
    for k, ls, it in items:
        find(k)
        for L in ls:
            for fk, fl, one in frags:
                if fl == L and one.Collide(shapes[k][L].Outline(0), 0):
                    par[find(k)] = find(fk)
    for i, (k1, l1, _a) in enumerate(items):
        for k2, l2, _c in items[i + 1:]:
            for L in set(l1) & set(l2):
                if shapes[k1][L].Collide(shapes[k2][L].Outline(0), 0):
                    par[find(k1)] = find(k2)
                    break
    out = {}
    for fk, fl, one in frags:
        out.setdefault(find(fk), []).append((fl, one))
    return sorted(out.values(), key=len, reverse=True)


def inside(one, x, y):
    p = pcbnew.VECTOR2I(x, y)
    return one.Contains(p) and one.Outline(0).SquaredDistance(p, True) ** 0.5 >= MM(EDGE) and \
        all(one.Hole(0, h).SquaredDistance(p, True) ** 0.5 >= MM(EDGE) for h in range(one.HoleCount(0)))


def main(path):
    b = pcbnew.LoadBoard(path)
    din = [(p.GetPosition(), p.GetSize().x / 2) for f in b.GetFootprints() if f.GetReference() in ("J5", "J6") for p in f.Pads()]
    # every drilled hole (pads' and vias'): the new via's copper keeps the rules' 0.2 mm hole clearance from each
    holes = [(p.GetPosition(), max(p.GetDrillSize().x, p.GetDrillSize().y) / 2) for f in b.GetFootprints() for p in f.Pads()
             if p.GetDrillSize().x > 0]
    holes += [(t.GetPosition(), t.GetDrillValue() / 2) for t in b.GetTracks() if t.GetClass() == "PCB_VIA"]
    added = 0
    for _ in range(5):
        gs = groups(b)
        if len(gs) == 1:
            break
        main_pieces = gs[0]
        lone = gs[1]
        spot = None
        for fl, one in lone:
            other = [o for l2, o in main_pieces if l2 != fl]
            bb = one.BBox()
            for ix in range(0, bb.GetWidth(), MM(0.05)):
                for iy in range(0, bb.GetHeight(), MM(0.05)):
                    x, y = bb.GetLeft() + ix, bb.GetTop() + iy
                    if not inside(one, x, y) or not any(inside(o, x, y) for o in other):
                        continue
                    if any(((x - c.x) ** 2 + (y - c.y) ** 2) ** 0.5 < r + MM(DIN_ROOM) for c, r in din):
                        continue
                    if any(((x - c.x) ** 2 + (y - c.y) ** 2) ** 0.5 < r + MM(VIA_D / 2 + 0.2) for c, r in holes):
                        continue
                    spot = (x, y)
                    break
                if spot:
                    break
            if spot:
                break
        if not spot:
            print("a ground group with no place for a via:", [pcbnew.ToMM(o.BBox().GetLeft()) for _l, o in lone])
            break
        v = pcbnew.PCB_VIA(b)
        v.SetPosition(pcbnew.VECTOR2I(*spot))
        v.SetWidth(MM(VIA_D))
        v.SetDrill(MM(VIA_DRILL))
        v.SetNet(b.FindNet("GND"))
        b.Add(v)
        added += 1
        print(f"via ties a ground group at ({pcbnew.ToMM(spot[0]) - 100:.2f}, {pcbnew.ToMM(spot[1]) - 100:.2f})")
        pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    print(f"{added} vias tie cut-off ground groups")
    b.Save(path)


if __name__ == "__main__":
    main(sys.argv[1])
