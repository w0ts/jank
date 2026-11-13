# SPDX-License-Identifier: GPL-3.0-only
"""Routes the placed board (make_board.py with LANE_VIAS=1 NO_STITCH=1 PLACE_ONLY=1: the parts, the drawn RF path, the
fan-out lanes and their vias, the pours) with Freerouting (fetch_freerouting.sh), README "Routing":
  three rounds, each: the board exported (Specctra DSN, every drawn track and via fixed), the rules added (net classes,
  keep-outs), Freerouting, the session read back; then the tracks Freerouting narrowed under 0.2 mm widened where the
  clearance allows, one-sided signal vias removed, GND stitching every 6 mm, a GND via in every cut-off pour piece.
  python freeroute.py chip.kicad_pcb        (KiCad's Python; java; the board is rewritten in place)
"""
import os
import re
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "pcb", "lib"))
from stitch import stitch            # (hardware/pcb/lib/stitch.py)

WORK = os.path.join(HERE, ".freerouting")
JAR = os.path.join(WORK, "freerouting-2.5.0.jar")
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
MM = pcbnew.FromMM
OX = OY = 100.0
ROUNDS = 3
EXTRA = 2
PASSES = "100"
# net classes (widths in um; clearance 0.2 as the rules: the chip's 0.4 mm pitch leaves exactly 0.2 between 0.2 tracks)
PWR = {"VDD3P3": 250, "VSYS": 300, "+3V3": 250, "VBUS": 300, "VUSB_D": 300, "SW": 300}   # (0.25 into the chip.s pins)
DIN_NETS = ["MO_R", "MO_T", "PIN_R", "PIN_T"]   # (0.5 from everything: the iron's room, README "DIN")
IRON = 0.8                                       # (no via within a DIN pad's copper + 0.8 mm)
EDGE = 0.3                                       # (+ the 0.2 clearance: the rules' 0.5 from the board's edge)
# the bottom stays one ground under the chip with its left and lower parts, and under the RF path
CHIP_BLOCK = ["U1", "Y1", "R17", "R24", "R25", "C9", "C10", "C11", "C12", "C13", "C19", "C20", "C21", "C22"]
RF_NETS = {"LNA_IN", "RF", "ANT_FEED", "UFL"}
PAD_VIA_PARTS = ("C23", "C24", "C25", "C26", "J4", "U5")
STITCH = 6.0                                     # (mm between GND stitching vias; the RF line has its own fence)
VIA_D, VIA_DRILL = 0.6, 0.3


def mm(v):
    return pcbnew.ToMM(v)


def dsn_xy(x, y):
    """KiCad page mm -> the DSN's um, y up"""
    return x * 1000, -y * 1000


def din_keepouts(board):
    """no via within a DIN pad's copper + 0.8 mm; no track within 0.5 mm of its ground and unused pins (no track needs
    them: ground joins through the pours); the signal pins keep 0.5 through their net class"""
    out = []
    for f in board.GetFootprints():
        if f.GetReference() in ("J5", "J6"):
            for p in f.Pads():
                r = max(mm(p.GetSize().x), mm(p.GetSize().y)) / 2
                x, y = dsn_xy(mm(p.GetPosition().x), mm(p.GetPosition().y))
                out += [f'    (via_keepout "" (circle {L} {(r + IRON) * 2000:.0f} {x:.0f} {y:.0f}))' for L in ("F.Cu", "B.Cu")]
                if p.GetNetname() in ("", "GND"):
                    out += [f'    (wire_keepout "" (circle {L} {(r + 0.3) * 2000:.0f} {x:.0f} {y:.0f}))' for L in ("F.Cu", "B.Cu")]
    return out


def rect(L, x0, y0, x1, y1, kind="keepout"):
    a, b = dsn_xy(x0, y1)
    c, d = dsn_xy(x1, y0)
    return f'    ({kind} "" (rect {L} {a:.0f} {b:.0f} {c:.0f} {d:.0f}))'


def pts(pads):
    return [(mm(p.GetPosition().x), mm(p.GetPosition().y)) for p in pads]


def chip_keepouts(board):
    """the chip block's box (its right edge on the chip's pins: VDD3P3's via and hop to L2 stay open) and the RF path's"""
    fps = {f.GetReference(): f for f in board.GetFootprints()}
    right = max(x for x, _ in pts(fps["U1"].Pads()))
    chip = [(min(x, right - 0.6), y) for x, y in pts(p for r in CHIP_BLOCK if r in fps for p in fps[r].Pads())]
    rf = pts(p for f in fps.values() if f.GetReference() != "U1" for p in f.Pads() if p.GetNetname() in RF_NETS)
    out = []
    for ps, m in ((chip, 0.6), (rf, 0.4)):
        xs, ys = [p[0] for p in ps], [p[1] for p in ps]
        out.append(rect("B.Cu", min(xs) - m, min(ys) - m, max(xs) + m, max(ys) + m))
    return out


def ground_edge(board):
    """the GND pours' bottom edge (make_board.py's GE): below it the antenna's strip, no copper"""
    return max(mm(z.GetBoundingBox().GetBottom()) for z in board.Zones() if z.GetNetname() == "GND" and not z.GetIsRuleArea())


def edge_keepouts(board):
    bb = board.GetBoardEdgesBoundingBox()
    x0, x1, y0, y1 = mm(bb.GetLeft()), mm(bb.GetRight()), mm(bb.GetTop()), mm(bb.GetBottom())
    ge = ground_edge(board)
    boxes = [(x0, ge - 0.3, x1, y1 + 1), (x0 - 1, y0 - 1, x0 + EDGE, y1 + 1), (x1 - EDGE, y0 - 1, x1 + 1, y1 + 1),
             (x0 - 1, y0 - 1, x1 + 1, y0 + EDGE)]
    return [rect(L, *b) for b in boxes for L in ("F.Cu", "B.Cu")]


def net_class(name, nets, width, clearance):
    return (f'    (class {name} {" ".join(nets)}\n      (circuit (use_via "Via[0-1]_600:300_um"))\n'
            f"      (rule (width {width}) (clearance {clearance}))\n    )\n")


def patch_dsn(text, keepouts):
    i = text.index('    (via "')                     # (the keep-outs: in the structure, before its via)
    text = text[:i] + "\n".join(keepouts) + "\n" + text[i:]
    m = re.search(r"\(class kicad_default (.*?)\(circuit", text, re.S)
    nets = m.group(1).split()
    rest = [n for n in nets if n not in set(PWR) | set(DIN_NETS)]
    tail = text[m.end():]
    end = tail.index("\n    )\n") + len("\n    )\n")
    extra = [net_class(f"pwr{w}", [n for n, v in PWR.items() if v == w and n in nets], w, 200) for w in sorted(set(PWR.values()))]
    extra.append(net_class("din", [n for n in DIN_NETS if n in nets], 250, 500))
    return text[:m.start()] + "(class kicad_default " + " ".join(rest) + "\n      (circuit" + tail[:end] + "".join(extra) + tail[end:]


def route_round(board, k):
    for t in board.GetTracks():
        t.SetLocked(True)
    work = os.path.splitext(board.GetFileName())[0] + ".freerouting"   # (per board: parallel runs never share files)
    os.makedirs(work, exist_ok=True)
    dsn, ses, log = (os.path.join(work, f"round{k}.{e}") for e in ("dsn", "ses", "log"))
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        sys.exit("DSN export failed")
    with open(dsn) as fh:
        text = patch_dsn(fh.read(), din_keepouts(board) + edge_keepouts(board) + chip_keepouts(board))
    with open(dsn, "w") as fh:
        fh.write(text)
    if os.path.exists(ses):
        os.remove(ses)
    r = subprocess.run(["java", "-jar", JAR, "-de", dsn, "-do", ses, "--router.autorouter.max_passes=" + PASSES,
                        "--gui.enabled=false"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    with open(log, "w") as fh:
        fh.write(r.stdout)
    if r.returncode != 0 or not os.path.exists(ses):
        sys.exit(f"Freerouting failed ({r.returncode}), see {log}")
    if not pcbnew.ImportSpecctraSES(board, ses):
        sys.exit("SES import failed")
    done = re.findall(r"Auto-routing stage completed: .*?\((\d+) unrouted", r.stdout)
    print(f"round {k}: {done[-1] if done else '?'} connections left to Freerouting")
    return int(done[-1]) if done else -1


def collides(board, item, layer, gap):
    sh = item.GetEffectiveShape(layer)
    n = item.GetNetname()
    if any(o is not item and o.GetNetname() != n and o.IsOnLayer(layer) and o.GetEffectiveShape(layer).Collide(sh, gap)
           for o in board.GetTracks()):
        return True
    return any(p.GetNetname() != n and p.IsOnLayer(layer) and p.GetEffectiveShape(layer).Collide(sh, gap)
               for f in board.GetFootprints() for p in f.Pads())


def widen(board):
    """Freerouting narrows tracks to pass: back to 0.2 wherever 0.2 keeps the clearance (to 1 um)"""
    kept = []
    for t in [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetWidth() < MM(0.2)]:
        old = t.GetWidth()
        t.SetWidth(MM(0.2))
        if collides(board, t, t.GetLayer(), MM(0.199)):
            t.SetWidth(old)
            kept.append(f"{t.GetNetname()} {mm(old):.2f}")
    return kept


def drop_one_sided_vias(board):
    gone = 0
    for v in [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() != "GND"]:
        c, r, n = v.GetPosition(), v.GetWidth(pcbnew.F_Cu) / 2, v.GetNetname()
        layers = {t.GetLayer() for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetNetname() == n
                  and min((t.GetStart() - c).EuclideanNorm(), (t.GetEnd() - c).EuclideanNorm()) <= r}
        layers |= {L for f in board.GetFootprints() for p in f.Pads() if p.GetNetname() == n
                   for L in (pcbnew.F_Cu, pcbnew.B_Cu) if p.IsOnLayer(L) and p.HitTest(c)}
        if len(layers) < 2:
            board.Delete(v)
            gone += 1
    return gone


def din_rooms(board):
    return [(mm(p.GetPosition().x), mm(p.GetPosition().y), max(mm(p.GetSize().x), mm(p.GetSize().y)) / 2 + IRON + VIA_D / 2)
            for f in board.GetFootprints() if f.GetReference() in ("J5", "J6") for p in f.Pads()]


def via_adder(board):
    gnd, rooms = board.FindNet("GND"), din_rooms(board)

    def add(x, y):
        if any((x - a) ** 2 + (y - b) ** 2 < r * r for a, b, r in rooms):
            return False
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
        v.SetWidth(MM(VIA_D))
        v.SetDrill(MM(VIA_DRILL))
        v.SetNet(gnd)
        board.Add(v)
        return True
    return add


def island_vias(board, add):
    """a GND pour piece with no via and no GND through-hole pad: one via, as near its first GND pad as the clearances allow"""
    anchors = [t.GetPosition() for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == "GND"]
    anchors += [p.GetPosition() for f in board.GetFootprints() for p in f.Pads()
                if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    others = []
    for L in (pcbnew.F_Cu, pcbnew.B_Cu):
        others += [p.GetEffectiveShape(L) for f in board.GetFootprints() for p in f.Pads() if p.IsOnLayer(L) and p.GetNetname() != "GND"]
        others += [t.GetEffectiveShape(L) for t in board.GetTracks() if t.IsOnLayer(L) and t.GetNetname() != "GND"]
    others += [p.GetEffectiveHoleShape() for f in board.GetFootprints() for p in f.Pads() if p.GetDrillSize().x > 0]
    added = 0
    for z in [z for z in board.Zones() if z.GetNetname() == "GND"]:
        for L in [L for L in (pcbnew.F_Cu, pcbnew.B_Cu) if z.IsOnLayer(L)]:
            polys = z.GetFilledPolysList(L)
            for i in range(polys.OutlineCount()):
                ol = polys.Outline(i)
                if any(ol.PointInside(a) for a in anchors):
                    continue
                c = best_spot(board, ol, L, others)
                if c is not None and add(mm(c.x), mm(c.y)):
                    anchors.append(c)
                    added += 1
    return added


def best_spot(board, ol, layer, others, near=None):
    bb = ol.BBox()
    pads = [p.GetPosition() for f in board.GetFootprints() for p in f.Pads()
            if p.GetNetname() == "GND" and p.IsOnLayer(layer) and ol.PointInside(p.GetPosition())]
    ref = near if near is not None else (pads[0] if pads else bb.Centre())
    if near is not None:                         # (search a 3 mm box round the pad, not the whole pour)
        bb = pcbnew.BOX2I(pcbnew.VECTOR2I(near.x - MM(1.5), near.y - MM(1.5)), pcbnew.VECTOR2L(MM(3), MM(3)))
    step = MM(0.05)
    cands = [pcbnew.VECTOR2I(x, y) for x in range(bb.GetLeft(), bb.GetRight() + 1, step)
             for y in range(bb.GetTop(), bb.GetBottom() + 1, step)]
    cands.sort(key=lambda c: (c.x - ref.x) ** 2 + (c.y - ref.y) ** 2)
    r = MM(VIA_D / 2)
    for c in cands:
        if not all(ol.PointInside(pcbnew.VECTOR2I(c.x + dx, c.y + dy)) for dx, dy in ((0, 0), (r, 0), (-r, 0), (0, r), (0, -r))):
            continue
        circ = pcbnew.SHAPE_CIRCLE(c, r)
        if not any(o.Collide(circ, MM(0.2)) for o in others) and not any(g.Collide(circ, MM(0.15)) for g in gnd_pads(board, layer)):
            return c
    return None


def gnd_pads(board, layer):
    """every GND pad's copper: a via goes beside a pad, never into it (solder would run down the hole)"""
    return [p.GetEffectiveShape(layer) for f in board.GetFootprints() for p in f.Pads()
            if p.GetNetname() == "GND" and p.IsOnLayer(layer) and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD
            and not (f.GetReference() == "U1" and p.GetNumber() == "57")]


def pad_ground_vias(board, add, others):
    """a GND via within 1.2 mm of every GND pad of the RF parts, the u.FL and the USB ESD (route.py's per-pad vias)"""
    vias = [t.GetPosition() for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == "GND"]
    added = 0
    for f in board.GetFootprints():
        if f.GetReference() not in PAD_VIA_PARTS:
            continue
        for p in f.Pads():
            if p.GetNetname() != "GND" or any((v - p.GetPosition()).EuclideanNorm() < MM(1.2) for v in vias):
                continue
            z = [z for z in board.Zones() if z.GetNetname() == "GND" and z.IsOnLayer(pcbnew.F_Cu)][0]
            polys = z.GetFilledPolysList(pcbnew.F_Cu)
            ol = next((polys.Outline(i) for i in range(polys.OutlineCount()) if polys.Outline(i).PointInside(p.GetPosition())), None)
            if ol is None:                       # (the pad's own pour piece: the outline that touches it)
                ol = min((polys.Outline(i) for i in range(polys.OutlineCount())),
                         key=lambda o: o.SquaredDistance(p.GetPosition()))
            c = best_spot(board, ol, pcbnew.F_Cu, others, near=p.GetPosition())
            if c is not None and (c - p.GetPosition()).EuclideanNorm() < MM(1.5) and add(mm(c.x), mm(c.y)):
                vias.append(c)
                added += 1
    return added


def tidy(board):
    kept = widen(board)
    gone = drop_one_sided_vias(board)
    add = via_adder(board)
    bb = board.GetBoardEdgesBoundingBox()
    x0, x1, y0 = mm(bb.GetLeft()), mm(bb.GetRight()), mm(bb.GetTop())
    ge = ground_edge(board)
    st = stitch(board, lambda x, y: add(x, y), [(x0, y0), (x1, y0), (x1, ge - 0.6), (x0, ge - 0.6)], spacing=STITCH)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    isl = island_vias(board, add)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    others = []
    for L in (pcbnew.F_Cu, pcbnew.B_Cu):
        others += [p.GetEffectiveShape(L) for f in board.GetFootprints() for p in f.Pads() if p.IsOnLayer(L) and p.GetNetname() != "GND"]
        others += [t.GetEffectiveShape(L) for t in board.GetTracks() if t.IsOnLayer(L) and t.GetNetname() != "GND"]
    others += [p.GetEffectiveHoleShape() for f in board.GetFootprints() for p in f.Pads() if p.GetDrillSize().x > 0]
    pv = pad_ground_vias(board, add, others)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    print(f"  {len(st)} stitching vias, {isl} island vias, {pv} pad vias, {gone} one-sided vias removed; narrow kept: {kept or 'none'}")


def relabel(board):
    """the buttons' silk labels under the buttons where they are (the placer moves the buttons after make_board.py's silk)"""
    for ref, text in (("SW1", "BOOT"), ("SW2", "RST")):
        p = board.FindFootprintByReference(ref).GetPosition()
        for t in board.GetDrawings():
            if t.GetClass() == "PCB_TEXT" and t.GetText() == text:
                t.SetPosition(pcbnew.VECTOR2I(p.x, p.y + MM(4.2)))


def main():
    path = sys.argv[1]
    if not os.path.exists(JAR):
        sys.exit(f"no {JAR}: sh fetch_freerouting.sh")
    board = pcbnew.LoadBoard(path)
    relabel(board)
    for k in range(1, ROUNDS + EXTRA + 1):       # (EXTRA rounds only while Freerouting still leaves connections)
        left = route_round(board, k)
        tidy(board)
        if k >= ROUNDS and left == 0:
            break
    board.Save(path)
    starved = starved_pads(path)
    if starved:                                  # (spokes that could not reach the pour: those pads joined solid)
        for f in board.GetFootprints():
            for p in f.Pads():
                if f"{f.GetReference()}.{p.GetNumber()}" in starved:
                    p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        board.Save(path)
    print(f"solid instead of spokes: {sorted(starved) or 'none'}")
    print("saved", path)


def starved_pads(path):
    """the pads KiCad's DRC finds with fewer thermal spokes than its rule (starved_thermal)"""
    rpt = os.path.splitext(path)[0] + ".freerouting/starved.rpt"
    subprocess.run([KICAD_CLI, "pcb", "drc", "--severity-all", "-o", rpt, path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    text = open(rpt).read() if os.path.exists(rpt) else ""
    out = set()
    for block in text.split("[starved_thermal]")[1:]:
        m = re.search(r"Pad (\S+) \[GND\] of (\S+) on", block.split("\n[")[0])
        if m:
            out.add(f"{m.group(2)}.{m.group(1)}")
    return out


if __name__ == "__main__":
    main()
