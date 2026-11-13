# SPDX-License-Identifier: GPL-3.0-only
"""The fix-ups on the board Freerouting leaves (README "Routing, mini 2"). make_fab.sh runs them after freeroute.py when
ROUTE=1; with KiCad's Python by hand:  python3.9 route_fixups.py [mini2.kicad_pcb]
Each fix-up acts only where the router's geometry is the one it was made for, otherwise it says "skipped"; on a board that
has it already it changes nothing. In order:
  1. C20 (placed turned, make_board.py PLACED): MIDI2_TX's corner and the +3V3 hop to pin 20 re-laid beside it
  2. LINE_IN's last 2 mm tidied 0.03 mm further from MIDI2_TX
  3. the LIN_OUT via at the chip moved 0.03 mm (hole to hole 0.4987 -> over 0.5 mm)
  4. the LIN_OUT via by U6 moved 1.2 mm right (the user's mark, 2026-11-13), its bottom leg re-laid"""
import sys

import pcbnew

P = sys.argv[1] if len(sys.argv) > 1 else "mini2.kicad_pcb"
M = pcbnew.FromMM
OX = 100.0                                   # (fix-ups 1 and 2 are written in board mm, 3 and 4 in page mm)


def v(x, y, off=OX):
    return pcbnew.VECTOR2I(M(off + x), M(off + y))


def near(a, x, y, off=OX):
    return abs(pcbnew.ToMM(a.x) - off - x) < 0.02 and abs(pcbnew.ToMM(a.y) - off - y) < 0.02


def has(t, p, q):
    s, e = t.GetStart(), t.GetEnd()
    return (near(s, *p) and near(e, *q)) or (near(s, *q) and near(e, *p))


def track(b, net, layer, x0, y0, x1, y1, w=0.2, off=OX):
    t = pcbnew.PCB_TRACK(b)
    t.SetStart(v(x0, y0, off))
    t.SetEnd(v(x1, y1, off))
    t.SetWidth(M(w))
    t.SetLayer(layer)
    t.SetNet(b.FindNet(net))
    b.Add(t)


def tracks(b, net, layer=None):
    return [t for t in b.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetNetname() == net and (layer is None or t.GetLayer() == layer)]


def c20_midi(b):
    old3 = [((30.58, 38.45), (30.41, 38.62)), ((30.41, 38.62), (30.1, 38.62)), ((30.1, 38.62), (30.1, 39.49))]
    oldm = [((32.1, 38.25), (32.1, 39.49)), ((31.57, 37.72), (32.1, 38.25))]
    new3 = [(31.42, 38.3, 31.42, 38.9), (31.42, 38.9, 30.1, 38.9), (30.1, 38.9, 30.1, 39.49)]
    newm = [(32.1, 39.49, 32.1, 38.0), (32.1, 38.0, 31.82, 37.72), (31.82, 37.72, 31.57, 37.72)]
    rm = [t for t in tracks(b, "+3V3") if any(has(t, p, q) for p, q in old3)] + [t for t in tracks(b, "MIDI2_TX") if any(has(t, p, q) for p, q in oldm)]
    done = all(any(has(t, (s[0], s[1]), (s[2], s[3])) for t in tracks(b, n, pcbnew.F_Cu)) for n, ss in (("+3V3", new3), ("MIDI2_TX", newm)) for s in ss)
    if done:
        return "1 already done"
    if len(rm) != 5:
        return "1 skipped: the router's tracks round C20 are not the ones it was made for (%d of 5 found)" % len(rm)
    for t in rm:
        b.Remove(t)
    c = b.FindFootprintByReference("C20")
    c.SetOrientationDegrees(180)
    c.SetPosition(v(31.0, 38.3))
    for s in new3:
        track(b, "+3V3", pcbnew.F_Cu, *s)
    for s in newm:
        track(b, "MIDI2_TX", pcbnew.F_Cu, *s)
    return "1 done"


def line_in_tidy(b):
    new = [(32.91, 37.82, 32.55, 37.82), (32.55, 37.82, 32.3, 37.57), (32.3, 37.57, 32.3, 37.25), (32.3, 37.25, 32.1, 37.05), (32.1, 37.05, 31.93, 37.05)]
    if all(any(has(t, s[:2], s[2:]) for t in tracks(b, "LINE_IN", pcbnew.F_Cu)) for s in new):
        return "2 already done"
    old = [t for t in tracks(b, "LINE_IN", pcbnew.F_Cu)
           if has(t, (32.91, 37.82), (32.39, 37.82)) or has(t, (32.12, 37.05), (31.93, 37.05)) or has(t, (32.27, 37.2), (32.12, 37.05))
           or (pcbnew.ToMM(max(t.GetStart().x, t.GetEnd().x)) - OX < 32.9 and pcbnew.ToMM(min(t.GetStart().y, t.GetEnd().y)) - OX > 37.0
               and pcbnew.ToMM(max(t.GetStart().y, t.GetEnd().y)) - OX < 37.9)]
    if not old:
        return "2 skipped: no LINE_IN kink where it was made for"
    for t in old:
        b.Remove(t)
    for s in new:
        track(b, "LINE_IN", pcbnew.F_Cu, *s)
    return "2 done (%d segments replaced)" % len(old)


def move_via(b, x, y, nx, ny, ends=True):
    """the LIN_OUT via at page (x, y) to (nx, ny), the track ends on it with it (ends); None when there is none there"""
    old, new = v(x, y, 0), v(nx, ny, 0)
    vias = [t for t in b.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == "LIN_OUT" and (t.GetPosition() - old).EuclideanNorm() < M(0.02)]
    if not vias:
        return None
    vias[0].SetPosition(new)
    for t in tracks(b, "LIN_OUT") if ends else []:
        if (t.GetStart() - old).EuclideanNorm() < M(0.02):
            t.SetStart(new)
        if (t.GetEnd() - old).EuclideanNorm() < M(0.02):
            t.SetEnd(new)
    return vias[0]


def chip_via_nudge(b):
    if move_via(b, 134.85, 135.89, 134.85, 135.86) is None:
        return "3 already done or skipped: no LIN_OUT via at (134.85, 135.89)"
    return "3 done"


def u6_via_right(b):
    """via (122.727, 120.079): its top track runs to the NW (45 degrees), its bottom leg to (130.73, 128.082); the via goes 1.2 mm
    east, a short top track joins it, the bottom leg is laid again as 45 degrees then straight down"""
    x, y, d = 122.727, 120.079, 1.2
    old = v(x, y, 0)
    bottom = [t for t in tracks(b, "LIN_OUT", pcbnew.B_Cu) if (t.GetStart() - old).EuclideanNorm() < M(0.02) or (t.GetEnd() - old).EuclideanNorm() < M(0.02)]
    if len(bottom) != 1 or move_via(b, x, y, x + d, y, ends=False) is None:
        return "4 already done or skipped: no LIN_OUT via with one bottom leg at (122.727, 120.079)"
    far = bottom[0].GetEnd() if (bottom[0].GetStart() - old).EuclideanNorm() < M(0.02) else bottom[0].GetStart()
    fx, fy = pcbnew.ToMM(far.x), pcbnew.ToMM(far.y)
    b.Remove(bottom[0])
    dx = fx - (x + d)
    track(b, "LIN_OUT", pcbnew.B_Cu, x + d, y, x + d + dx, y + dx, off=0.0)          # (45 degrees to the old end's column)
    track(b, "LIN_OUT", pcbnew.B_Cu, x + d + dx, y + dx, fx, fy, off=0.0)            # (then straight to the old end)
    track(b, "LIN_OUT", pcbnew.F_Cu, x, y, x + d, y, off=0.0)                        # (the top track on to the moved via)
    return "4 done"


def main():
    b = pcbnew.LoadBoard(P)
    for fix in (c20_midi, line_in_tidy, chip_via_nudge, u6_via_right):
        print("fix-up", fix(b))
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    b.Save(P)


if __name__ == "__main__":
    main()
