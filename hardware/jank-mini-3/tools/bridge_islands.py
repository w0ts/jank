"""jank-mini-3 helper (KiCad's python): join the two copper islands of one net that DRC leaves unconnected, by the
shortest clean link: a straight track on a shared layer, or a via plus a track on each side. Every candidate is checked
against every other-net item (0.16 mm clearance, 0.21 mm to drilled pads), the best few by KiCad's DRC; the board is
written only when DRC is clean. Usage: python3 bridge_islands.py <board> <net, e.g. /+3V3>"""
import itertools
import json
import math
import os
import shutil
import subprocess
import sys

import pcbnew

KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
MM, TO = pcbnew.FromMM, pcbnew.ToMM
LAYERS = (pcbnew.F_Cu, pcbnew.B_Cu)


def drc(p):
    r = p + ".b.json"
    subprocess.run([KC, "pcb", "drc", "--schematic-parity", "--severity-all", "--format", "json", "-o", r, p],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    d = json.load(open(r))
    os.remove(r)
    return len(d.get("violations", [])), len(d.get("unconnected_items", []))


def islands(board, net):
    """the net's copper split into islands: [(points as (x, y, layers)), ...]"""
    its = [t for t in board.GetTracks() if t.GetNetname() == net]
    pads = [p for f in board.GetFootprints() for p in f.Pads() if p.GetNetname() == net]
    nodes = its + pads
    parent = list(range(len(nodes)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for i, j in itertools.combinations(range(len(nodes)), 2):
        a, b = nodes[i], nodes[j]
        for L in LAYERS:
            if a.IsOnLayer(L) and b.IsOnLayer(L) and a.GetEffectiveShape(L).Collide(b.GetEffectiveShape(L), 0):
                parent[find(i)] = find(j)
                break
    groups = {}
    for i, n in enumerate(nodes):
        pts = groups.setdefault(find(i), [])
        if n.Type() == pcbnew.PCB_TRACE_T:
            for e in (n.GetStart(), n.GetEnd()):
                pts.append((TO(e.x), TO(e.y), (n.GetLayer(),)))
        else:
            pos = n.GetPosition()
            pts.append((TO(pos.x), TO(pos.y), tuple(L for L in LAYERS if n.IsOnLayer(L))))
    return list(groups.values())


def main(path, net):
    b0 = pcbnew.LoadBoard(path)
    isl = islands(b0, net)
    print(f"{net}: {len(isl)} islands")
    if len(isl) < 2:
        return 0
    others = [t for t in b0.GetTracks() if t.GetNetname() != net]
    opads = [p for f in b0.GetFootprints() for p in f.Pads() if p.GetNetname() != net]

    def clear(shape, layer, gap=0.16):
        for it in others:
            if (it.Type() == pcbnew.PCB_VIA_T or it.GetLayer() == layer) and it.GetEffectiveShape(layer).Collide(shape, MM(gap)):
                return False
        for p in opads:
            g = gap + (0.05 if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH else 0)
            if p.IsOnLayer(layer) and p.GetEffectiveShape(layer).Collide(shape, MM(g)):
                return False
        return True

    def seg(a, c, L):
        return clear(pcbnew.SHAPE_SEGMENT(pcbnew.VECTOR2I(MM(a[0]), MM(a[1])), pcbnew.VECTOR2I(MM(c[0]), MM(c[1])), MM(0.2)), L)

    def via(x, y):
        c = pcbnew.SHAPE_CIRCLE(pcbnew.VECTOR2I(MM(x), MM(y)), MM(0.3))
        return all(clear(c, L, 0.21) for L in LAYERS)
    big = sorted(isl, key=len, reverse=True)
    A, B = big[0], big[1]
    cands = []
    for a in A:
        for b in B:
            d = math.dist(a[:2], b[:2])
            if d > 6.0:
                continue
            for L in set(a[2]) & set(b[2]):                       # (straight, one layer)
                if seg(a[:2], b[:2], L):
                    cands.append((d, [(a[:2], b[:2], L)], None))
            for La in a[2]:                                        # (a via near either end, one track each side)
                for Lb in b[2]:
                    if La == Lb:
                        continue
                    for o in (a, b):
                        for i in range(-6, 7):
                            for j in range(-6, 7):
                                x, y = o[0] + i * 0.25, o[1] + j * 0.25
                                if via(x, y) and seg(a[:2], (x, y), La) and seg((x, y), b[:2], Lb):
                                    cands.append((math.dist(a[:2], (x, y)) + math.dist((x, y), b[:2]),
                                                  [(a[:2], (x, y), La), ((x, y), b[:2], Lb)], (x, y)))
    cands.sort(key=lambda c: c[0])
    print(f"{len(cands)} clean-looking links")
    tmp = os.path.splitext(path)[0] + ".bridge.kicad_pcb"
    for e in ("kicad_pro", "kicad_sch", "kicad_dru"):
        src = os.path.splitext(path)[0] + "." + e
        if os.path.exists(src):
            shutil.copy(src, os.path.splitext(tmp)[0] + "." + e)
    base = drc(path)
    for length, segs, v in cands[:8]:
        b = pcbnew.LoadBoard(path)
        n = b.FindNet(net)
        if v:
            pv = pcbnew.PCB_VIA(b)
            pv.SetPosition(pcbnew.VECTOR2I(MM(v[0]), MM(v[1])))
            pv.SetWidth(MM(0.6))
            pv.SetDrill(MM(0.3))
            pv.SetNet(n)
            b.Add(pv)
        for a, c, L in segs:
            if math.dist(a, c) < 0.001:
                continue
            t = pcbnew.PCB_TRACK(b)
            t.SetStart(pcbnew.VECTOR2I(MM(a[0]), MM(a[1])))
            t.SetEnd(pcbnew.VECTOR2I(MM(c[0]), MM(c[1])))
            t.SetWidth(MM(0.2))
            t.SetLayer(L)
            t.SetNet(n)
            b.Add(t)
        pcbnew.ZONE_FILLER(b).Fill(b.Zones())
        b.Save(tmp)
        nv, nu = drc(tmp)
        print(f"{length:.2f} mm {'via ' + str(tuple(round(q, 2) for q in v)) if v else 'straight'}: {nv} violations, {nu} unconnected (was {base})")
        if nv <= base[0] and nu < base[1]:
            shutil.copy(tmp, path)
            print("written")
            return 0
    print("no clean link: board unchanged")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
