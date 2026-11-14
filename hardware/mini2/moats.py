# SPDX-License-Identifier: GPL-3.0-only
"""Ground moats (stage 2, README "Routing"): Freerouting treats the GND pours as planes, so it may ring a piece of pour
with tracks on both layers; that piece, its vias and the ground pads in it are then cut off from the board's ground
(KiCad's DRC: "Missing connection between items: Zone [GND]"). isolated() finds them (a union-find over the pour pieces,
the GND vias, pads and tracks); bridge() lays a short GND track from each cut-off piece to the nearest main-ground via,
removing the other nets' tracks it crosses, which the next routing round puts back elsewhere (freeroute.py main()).
"""
import math

import pcbnew

from parts import TWINS

MM = pcbnew.FromMM
PROTECT = {"LNA_IN", "RF", "UFL", "UDP", "UDN", "USB_DP", "USB_DN"}   # (drawn: never ripped)
BRIDGE_W = 0.25


def _uf():
    par = {}

    def find(a):
        while par.setdefault(a, a) != a:
            par[a] = par[par[a]]
            a = par[a]
        return a

    def union(a, b):
        par[find(a)] = find(b)
    return find, union


def isolated(board):
    """[(anchors, area mm2)] for each cut-off ground group; anchors: [(position, layers)] of its GND vias / pads"""
    find, union = _uf()
    pieces = []
    for z in board.Zones():
        if z.GetNetname() != "GND" or z.GetIsRuleArea():
            continue
        for L in (pcbnew.F_Cu, pcbnew.B_Cu):
            if z.IsOnLayer(L):
                poly = z.GetFilledPolysList(L)
                pieces += [(("P", z.m_Uuid.AsString(), L, i), L, poly.Outline(i)) for i in range(poly.OutlineCount())]
    items = []
    for t in board.GetTracks():
        if t.GetNetname() != "GND":
            continue
        if t.GetClass() == "PCB_VIA":
            items.append((("V", t.m_Uuid.AsString()), (pcbnew.F_Cu, pcbnew.B_Cu), [t.GetPosition()]))
        else:
            items.append((("T", t.m_Uuid.AsString()), (t.GetLayer(),), [t.GetStart(), t.GetEnd()]))
    for f in board.GetFootprints():
        for p in f.Pads():
            if p.GetNetname() == "GND":
                th = p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH
                ls = (pcbnew.F_Cu, pcbnew.B_Cu) if th else ((pcbnew.B_Cu,) if f.IsFlipped() else (pcbnew.F_Cu,))
                items.append((("D", f.GetReference(), p.GetNumber()), ls, [p.GetPosition()]))
    for key, ls, pts in items:
        for pk, L, o in pieces:
            if L in ls and any(o.PointInside(q) for q in pts):
                union(key, pk)
    near = MM(0.3) ** 2
    for key, ls, pts in items:
        if key[0] == "T":
            for k2, l2, p2 in items:
                if k2 != key and ls[0] in l2 and any((q - r).SquaredEuclideanNorm() < near for q in pts for r in p2):
                    union(key, k2)
    groups = {}
    for pk, L, o in pieces:
        groups.setdefault(find(pk), []).append(abs(o.Area()) / 1e12)
    if not groups:
        return [], None
    main = max(groups, key=lambda g: sum(groups[g]))
    out = {}
    for key, ls, pts in items:
        r = find(key)
        if r != main and r in groups and key[0] in ("V", "D"):
            out.setdefault(r, []).append((pts[0], ls))
    main_vias = [pts[0] for key, ls, pts in items if key[0] == "V" and find(key) == main]
    return [(a, sum(groups[r])) for r, a in out.items()], main_vias


def bridge(board, groups, main_vias, reach=8.0):
    """per cut-off group: the shortest top (or bottom) segment from one of its anchors to a main-ground via whose path
    hits no other net's pad nor a protected track; the tracks it crosses are removed (rerouted next round)"""
    gnd = board.FindNet("GND")
    pads = [(p, L) for f in board.GetFootprints() for p in f.Pads() for L in (pcbnew.F_Cu, pcbnew.B_Cu)
            if p.GetNetname() != "GND" and p.IsOnLayer(L)]
    twin_pads = [p.GetPosition() for f in board.GetFootprints() if f.GetReference() in TWINS for p in f.Pads()]
    holes = [(p.GetPosition(), p.GetDrillSize().x / 2) for f in board.GetFootprints() for p in f.Pads()
             if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH]   # (stage 2: no bridge over a jack's peg hole)
    made = 0
    for anchors, _area in groups:
        best = None
        for a, ls in anchors:
            for m in main_vias:
                d = (a - m).EuclideanNorm()
                if d > MM(reach):
                    continue
                for L in ls:
                    seg = pcbnew.SHAPE_SEGMENT(a, m, MM(BRIDGE_W))
                    if any(p.GetEffectiveShape(L).Collide(seg, MM(0.2)) for p, l2 in pads if l2 == L):
                        continue
                    if any(seg.Collide(pcbnew.VECTOR2I(c), int(r + MM(0.3))) for c, r in holes):
                        continue
                    if any(v.GetClass() == "PCB_VIA" and v.GetNetname() != "GND"
                           and v.GetEffectiveShape(L).Collide(seg, MM(0.2)) for v in board.GetTracks()):
                        continue                         # (a via of another net in the way: not removed)
                    hit = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetLayer() == L
                           and t.GetNetname() != "GND" and t.GetEffectiveShape(L).Collide(seg, MM(0.2))]
                    if any(t.GetNetname() in PROTECT or any((t.GetStart() - q).EuclideanNorm() < MM(0.05)
                                                             or (t.GetEnd() - q).EuclideanNorm() < MM(0.05)
                                                             for q in twin_pads) for t in hit):
                        continue
                    cost = d + MM(3) * len(hit)
                    if best is None or cost < best[0]:
                        best = (cost, a, m, L, hit)
        if best is None:
            continue
        _c, a, m, L, hit = best
        for t in hit:
            board.Remove(t)
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(a)
        t.SetEnd(m)
        t.SetWidth(MM(BRIDGE_W))
        t.SetLayer(L)
        t.SetNet(gnd)
        t.SetLocked(True)
        board.Add(t)
        made += 1
        print(f"  moat bridged on {'top' if L == pcbnew.F_Cu else 'bottom'}: {math.hypot(pcbnew.ToMM(a.x - m.x), pcbnew.ToMM(a.y - m.y)):.2f} mm,"
              f" {len(hit)} tracks to reroute ({', '.join(sorted({t.GetNetname() for t in hit})) or 'none'})")
    return made
