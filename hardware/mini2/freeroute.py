# SPDX-License-Identifier: GPL-3.0-only
"""Routes the placed board (make_board.py with LANE_VIAS=1 NO_STITCH=1 PLACE_ONLY=1: the parts, the drawn RF path, the
fan-out lanes and their vias, the pours) with Freerouting (fetch_freerouting.sh), README "Routing":
  three rounds, each: the board exported (Specctra DSN, every drawn track and via fixed), the rules added (net classes,
  keep-outs), Freerouting, the session read back; then the tracks Freerouting narrowed under 0.2 mm widened where the
  clearance allows, one-sided signal vias removed, GND stitching every 6 mm, a GND via in every cut-off pour piece.
  python freeroute.py mini2.kicad_pcb        (KiCad's Python; java; the board is rewritten in place)
"""
import math
import os
import re
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "pcb", "lib"))
from stitch import stitch            # (hardware/pcb/lib/stitch.py)
import moats                         # (stage 2: ground pieces cut off by tracks on both layers)
import collections
sys.path.insert(0, HERE)
from parts import TWINS              # (72 x 72: no via on any twin's pad, top or bottom)

WORK = os.path.join(HERE, ".freerouting")
JAR = os.path.join(WORK, "freerouting-2.5.0.jar")
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
MM = pcbnew.FromMM
OX = OY = 100.0
ROUNDS = 3
EXTRA = 2                                        # (review pass 1: rounds 4 .. 7 left the same 18 open)
MOAT_ROUNDS = 3                                  # (review pass 1: rounds past the first moat found none)
PASSES = "100"
# net classes (widths in um; clearance 0.2 as the rules: the chip's 0.4 mm pitch leaves exactly 0.2 between 0.2 tracks)
# (mini 2.2: +3V3 0.25 into the chip's 0.4 mm-pitch pins; VSYS and VBUS 0.3 (the LDO's input, up to ~0.5 A); the cell's
#  nets 0.4 (the cell's current, charge and discharge))
PWR = {"VDD3P3": 250, "+3V3": 250, "VSYS": 500, "VREG": 500, "V5A": 400, "V5C": 300, "V5M": 300, "V3_L1": 500, "V3_L2": 500, "V5_SW": 500, "VBUS": 300, "VUSB": 300, "BAT_P": 400, "BAT_N": 400, "BAT_C": 400}
DIN_NETS = ["MO_R", "MO_T", "MO2_R", "MO2_T", "MD_R", "MD_T", "MD2_R", "MD2_T"]   # (0.5 from everything: the iron's room, README "DIN"; mini 2.2: both MIDI OUTs; stage 1f: the jacks' and the DINs' own)
IRON = 0.8                                       # (no via within a DIN pad's copper + 0.8 mm)
IRON_PARTS = ("J5", "J6", "J2", "J3", "J7", "J8", "J9", "J10", "J10B", "J11", "J13", "J14")   # (stage 2: every part the user solders from the top)
EDGE = 0.3                                       # (+ the 0.2 clearance: the rules' 0.5 from the board's edge)
# the bottom stays one ground under the chip with its left and lower parts, and under the RF path
CHIP_BLOCK = ["U1", "Y1", "R17", "C9", "C10"]   # (mini 2.2 stage 2: the chip and the crystal only: the long vertical routes (VBUS, I2C) need the bottom beside the chip)
TOP_ONLY = []                                    # (nets kept on the top layer: ["LIN_OUT"] routes it there, but only with 0.10 mm tracks and 4 other nets left open: README "Routing, mini 2")
RF_NETS = {"LNA_IN", "RF", "UFL"}               # (mini 2.2: the u.FL only)
RF_TOP_ROOM = 0.6                                # (stage 2: no other net's track on top within this of the line's edge)
PAD_VIA_PARTS = ("C23", "C24", "C28", "J4", "U5")
STITCH = 6.0                                     # (mm between GND stitching vias; the RF line has its own fence)
VIA_D, VIA_DRILL = 0.6, 0.3


def mm(v):
    return pcbnew.ToMM(v)


def dsn_xy(x, y):
    """KiCad page mm -> the DSN's um, y up"""
    return x * 1000, -y * 1000


PIVOT = ((61.5, 62.2), 1.5)                      # (stage 2: make_board.BTN_C / PIVOT_R: the D-pad's pivot, no via)


def din_keepouts(board):
    """no via within a DIN pad's copper + 0.8 mm; no track within 0.5 mm of its ground and unused pins (no track needs
    them: ground joins through the pours); the signal pins keep 0.5 through their net class"""
    out = []
    for f in board.GetFootprints():
        if f.GetReference() in TWINS:            # (72 x 72: no via on a twin's pad, either side: the router put one on CT8's)
            for p in f.Pads():
                r = max(mm(p.GetSize().x), mm(p.GetSize().y)) / 2
                x, y = dsn_xy(mm(p.GetPosition().x), mm(p.GetPosition().y))
                out += [f'    (via_keepout "" (circle {L} {(r + 0.35) * 2000:.0f} {x:.0f} {y:.0f}))' for L in ("F.Cu", "B.Cu")]
        if f.GetReference() in IRON_PARTS:       # (stage 2: every hand-soldered part)
            for p in f.Pads():
                r = max(mm(p.GetSize().x), mm(p.GetSize().y)) / 2
                x, y = dsn_xy(mm(p.GetPosition().x), mm(p.GetPosition().y))
                out += [f'    (via_keepout "" (circle {L} {(r + IRON) * 2000:.0f} {x:.0f} {y:.0f}))' for L in ("F.Cu", "B.Cu")]
                if p.GetNetname() in ("", "GND"):
                    out += [f'    (wire_keepout "" (circle {L} {(r + 0.36) * 2000:.0f} {x:.0f} {y:.0f}))' for L in ("F.Cu", "B.Cu")]   # (+0.06: Freerouting's octagons)
        if f.GetReference() == "J1":             # (the USB-C's soldered shell legs: tracks 0.3 mm off, no solder bridge)
            for p in f.Pads():
                if p.GetNumber() in ("13", "14"):
                    r = max(mm(p.GetSize().x), mm(p.GetSize().y)) / 2
                    x, y = dsn_xy(mm(p.GetPosition().x), mm(p.GetPosition().y))
                    out += [f'    (wire_keepout "" (circle {L} {(r + 0.1) * 2000:.0f} {x:.0f} {y:.0f}))' for L in ("F.Cu", "B.Cu")]
    (px, py), pr = PIVOT
    x, y = dsn_xy(OX + px, OY + py)
    out += [f'    (via_keepout "" (circle {L} {(pr + 0.3) * 2000:.0f} {x:.0f} {y:.0f}))' for L in ("F.Cu", "B.Cu")]
    return out


def rect(L, x0, y0, x1, y1, kind="keepout"):
    a, b = dsn_xy(x0, y1)
    c, d = dsn_xy(x1, y0)
    return f'    ({kind} "" (rect {L} {a:.0f} {b:.0f} {c:.0f} {d:.0f}))'


def pts(pads):
    return [(mm(p.GetPosition().x), mm(p.GetPosition().y)) for p in pads]


def chip_keepouts(board):
    """the chip block's box (its right edge on the chip's pins: VDD3P3's via and hop to L2 stay open) and the RF path's;
    2026-11-15, the WROOM-1U module: no track or via on top over its EPAD (its pour and the four ground vias round it
    are drawn); elsewhere under its body and on the bottom, free"""
    fps = {f.GetReference(): f for f in board.GetFootprints()}
    if fps["U1"].GetValue().startswith("ESP32-S3-WROOM"):
        # (2026-11-15, the fresh floorplan: only the EPAD's area, its nine pads + 1.6 mm (the four GND vias round it).
        #  Espressif's ESP32-S3 Hardware Design Guidelines ("PCB Layout Design", module placement) set no rule for
        #  tracks under a module's body: only an antenna keep-out for PCB-antenna modules (none on the -1U, its antenna
        #  is on the u.FL) and the EPAD's full ground contact. The first try kept the whole body out: 35 left open)
        ps = pts(p for p in fps["U1"].Pads() if p.GetNumber() == "41")
        xs, ys = [p[0] for p in ps], [p[1] for p in ps]
        return [rect("F.Cu", min(xs) - 1.6, min(ys) - 1.6, max(xs) + 1.6, max(ys) + 1.6)]
    right = max(x for x, _ in pts(fps["U1"].Pads()))
    chip = [(min(x, right - 0.6), y) for x, y in pts(p for r in CHIP_BLOCK if r in fps for p in fps[r].Pads())]
    rf = pts(p for f in fps.values() if f.GetReference() != "U1" for p in f.Pads() if p.GetNetname() in RF_NETS)
    out = []
    for ps, m in ((chip, 0.6), (rf, RF_BOTTOM_ROOM)):
        xs, ys = [p[0] for p in ps], [p[1] for p in ps]
        out.append(rect("B.Cu", min(xs) - m, min(ys) - m, max(xs) + m, max(ys) + m))
    # stage 2: on top, a box over the 50 ohm line's start (its 1.2 mm track's top end, L3, C24): BOOT had been routed
    # 0.15 mm past it (a box over the whole line blocked other routes: 1 open)
    line = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetNetname() == "RF" and mm(t.GetWidth()) > 1.0]
    x0 = min(mm(q.x) for t in line for q in (t.GetStart(), t.GetEnd()))
    y0 = min(mm(q.y) for t in line for q in (t.GetStart(), t.GetEnd()))
    w = max(mm(t.GetWidth()) for t in line) / 2 + RF_TOP_ROOM
    out.append(rect("F.Cu", x0 - w, y0 - w, x0 + w, y0 + 1.2))
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


VIA_NAME = ["Via[0-1]_600:300_um"]               # (the DSN's own via padstack: Via[0-3]_... on 4 layers; patch_dsn reads it)


def net_class(name, nets, width, clearance):
    return (f'    (class {name} {" ".join(nets)}\n      (circuit (use_via "{VIA_NAME[0]}"))\n'
            f"      (rule (width {width}) (clearance {clearance}))\n    )\n")


def inner_as_planes(text):
    """4 layers (2026-11-16): In1 / In2 are planes, never routed (type power), and the router sees no plane: GND joins
    through the outer pours and the vias by its pads (as on 2 layers), +3V3 only through its pad vias into In2
    (pre_plane_vias): it is not routed at all"""
    text = re.sub(r"\(layer (In\d+\.Cu)(\s*)\(type signal\)", r"(layer \1\2(type power)", text)
    text = re.sub(r"\n\s*\(plane \S+ \(polygon In\d\.Cu[^)]*\)\)", "", text)   # (2026-11-16: with the planes in
    #  the DSN Freerouting never joined a +3V3 pin to In2 (0 vias, 22 open): the planes are KiCad's; the router sees none)
    text = drop_nets(text, PLANE_NETS if "In1.Cu" in text else ())
    text = re.sub(r"\n\s*\(keepout \"\" \(polygon In\d\.Cu[^)]*\)\)", "", text)
    return text


def patch_dsn(text, keepouts):
    VIA_NAME[0] = re.search(r'\(use_via "([^"]+)"\)', text).group(1)   # (2026-11-16: on 4 layers the classes named a
    text = inner_as_planes(text)                                     #  2-layer via that no longer existed: no power vias)
    i = text.index('    (via "')                     # (the keep-outs: in the structure, before its via)
    text = text[:i] + "\n".join(keepouts) + "\n" + text[i:]
    m = re.search(r"\(class kicad_default (.*?)\(circuit", text, re.S)
    nets = m.group(1).split()
    rest = [n for n in nets if n not in set(PWR) | set(DIN_NETS) | set(TOP_ONLY)]
    tail = text[m.end():]
    end = tail.index("\n    )\n") + len("\n    )\n")
    extra = [net_class(f"pwr{w}", [n for n, v in PWR.items() if v == w and n in nets], w, 200) for w in sorted(set(PWR.values()))]
    if TOP_ONLY:
        extra.append(f'    (class top_only {" ".join(n for n in TOP_ONLY if n in nets)}\n      (circuit (use_via "{VIA_NAME[0]}") (use_layer F.Cu))\n      (rule (width 200) (clearance 200))\n    )\n')
    extra.append(net_class("din", [n for n in DIN_NETS if n in nets], 250, 500))
    return text[:m.start()] + "(class kicad_default " + " ".join(rest) + "\n      (circuit" + tail[:end] + "".join(extra) + tail[end:]


def unconnected(board):
    """KiCad's own count of open connections (the ratsnest), after a fill"""
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.BuildConnectivity()
    return board.GetConnectivity().GetUnconnectedCount(True)


RF_BOTTOM_ROOM = 1.0      # (72 x 72: the RF parts' bottom box 1.0 mm out, was 0.4: BOOT had run 0.4 mm from the path)
# (72 x 72: the nets the earlier runs left open at the chip's pads: routed first, alone, before every other net)
SEED = []   # (tried on 72 x 72 run 1 with the twelve nets the earlier runs left open: they all routed, then the top
#  side's (UP, SDA, LIN_DET, MIDI_TX, MIDI_TX2) and power (VUSB, VBUS, VREG) failed: 11 open, worse than run 9's 6)


PLANE_NETS = ("+3V3",)      # (4 layers: these nets reach their pads only through In2: a via by each pad, never routed)


def drop_nets(text, nets):
    """the DSN without these nets to route (their pins stay as obstacles)"""
    if not nets:
        return text
    def keep_net(m):
        return "" if m.group(1).strip('"') in nets else m.group(0)
    text = re.sub(r"\n    \(net (\S+)\n      \(pins .*?\)\n    \)", keep_net, text, flags=re.S)
    def cls(m):
        names = [n for n in m.group(2).split() if n.strip('"') not in nets]
        return f"\n    (class {m.group(1)} " + " ".join(names) + m.group(3) if names else ""
    text = re.sub(r"\n    \(class (\S+) (.*?)(\n      \(circuit.*?\n    \))", cls, text, flags=re.S)
    for n in nets:                                # (their fixed copper too: the router knows no such net)
        text = re.sub(r"\n\s*\((?:wire|via)\b[^\n]*\(net " + re.escape(n) + r"\)[^\n]*", "", text)
    return text


def pre_plane_vias(board):
    """4 layers: a via beside every pad of PLANE_NETS (0.6 / 0.3), joined by a short 0.3 mm top track, each item clear of
    other nets by 0.2 mm: the pad then reaches In2's fill straight down. Returns (added, pads without one)"""
    if board.GetCopperLayerCount() < 4:
        return 0, []
    added, missing = 0, []
    for n in PLANE_NETS:
        nt = board.FindNet(n)
        for f in board.GetFootprints():
            for p in f.Pads():
                if p.GetNetname() != n:
                    continue
                if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                    continue                          # (a through-hole pad reaches In2 itself)
                c = p.GetPosition()
                spots = sorted(((math.hypot(i, j) * 0.1, i, j) for i in range(-16, 17) for j in range(-16, 17)
                                if 0.6 <= math.hypot(i, j) * 0.1 <= 1.6), key=lambda t: t[0])
                done = False
                for _d, i, j in spots:
                    q = pcbnew.VECTOR2I(c.x + MM(i * 0.1), c.y + MM(j * 0.1))
                    t = pcbnew.PCB_TRACK(board); t.SetStart(c); t.SetEnd(q); t.SetWidth(MM(0.3)); t.SetLayer(pcbnew.F_Cu); t.SetNet(nt)
                    v = pcbnew.PCB_VIA(board); v.SetPosition(q); v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_DRILL)); v.SetNet(nt)
                    board.Add(t); board.Add(v)
                    if not (collides(board, t, pcbnew.F_Cu, MM(0.2)) or collides(board, v, pcbnew.F_Cu, MM(0.2))
                            or collides(board, v, pcbnew.B_Cu, MM(0.2))):
                        done = True
                        added += 1
                        break
                    board.Delete(t); board.Delete(v)
                if not done:
                    missing.append(f"{f.GetReference()}.{p.GetNumber()}")
    return added, missing


def seed_only(text, keep):
    """the DSN with only the nets in keep to route (every other net's pins stay as obstacles)"""
    def drop(m):
        return m.group(0) if m.group(1).strip('"') in keep else ""
    text = re.sub(r"\n    \(net (\S+)\n      \(pins .*?\)\n    \)", drop, text, flags=re.S)
    def cls(m):
        names = [n for n in m.group(2).split() if n.strip('"') in keep]
        return f"\n    (class {m.group(1)} " + " ".join(names) + m.group(3) if names else ""
    return re.sub(r"\n    \(class (\S+) (.*?)(\n      \(circuit.*?\n    \))", cls, text, flags=re.S)


def route_round(board, k, only=None):
    for t in board.GetTracks():
        t.SetLocked(True)
    work = os.path.splitext(board.GetFileName())[0] + ".freerouting"   # (per board: parallel runs never share files)
    os.makedirs(work, exist_ok=True)
    dsn, ses, log = (os.path.join(work, f"round{k}.{e}") for e in ("dsn", "ses", "log"))
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        sys.exit("DSN export failed")
    with open(dsn) as fh:
        text = patch_dsn(fh.read(), din_keepouts(board) + edge_keepouts(board) + chip_keepouts(board))
    if only:
        text = seed_only(text, set(only))
    with open(dsn, "w") as fh:
        fh.write(text)
    if os.path.exists(ses):
        os.remove(ses)
    r = subprocess.run(["nice", "-n", "19", "java", "-jar", JAR, "-de", dsn, "-do", ses, "--router.autorouter.max_passes=" + PASSES, "-mt", "1",
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
            for f in board.GetFootprints() if f.GetReference() in IRON_PARTS for p in f.Pads()
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]


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
                rooms = din_rooms(board)
                c = best_spot(board, ol, L, others,
                              ok=lambda c: not any((mm(c.x) - a) ** 2 + (mm(c.y) - b) ** 2 < r * r for a, b, r in rooms))
                if c is not None and add(mm(c.x), mm(c.y)):
                    anchors.append(c)
                    added += 1
    return added


def best_spot(board, ol, layer, others, near=None, ok=None):
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
        if ok is not None and not ok(c):        # (stage 2: e.g. out of the iron's rooms: try the next spot)
            continue
        circ = pcbnew.SHAPE_CIRCLE(c, r)
        if not any(o.Collide(circ, MM(0.21)) for o in others) and not any(g.Collide(circ, MM(0.15)) for g in gnd_pads(board, layer)):
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
    gs = ground_stubs(board)
    pd = prune_dangling(board)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    print(f"  {len(st)} stitching vias, {isl} island vias, {pv} pad vias, {gs} ground stubs, {pd} loose ends cut, {gone} one-sided vias removed; narrow kept: {kept or 'none'}")


DIRECT = {"VSYS": 0.3}                             # (net: width) pads of these nets within 4 mm joined straight first (stage 2: C34 -> U7, which Freerouting left open twice)
DIRECT_ONLY = {("C34", "U7"), ("U7", "C34")}       # (stage 2: only these pairs: a C3 -> U10 link changed the rest of the routing)


def direct_links(board):
    """a straight track between two pads of the same net close together (the buck's switch node U2 -> L1), when nothing
    of another net is in the way: Freerouting sometimes leaves such a short link open"""
    for n, w in DIRECT.items():
        pads = [p for f in board.GetFootprints() for p in f.Pads() if p.GetNetname() == n]
        for i in range(len(pads)):
            for j in range(i + 1, len(pads)):
                a, c = pads[i].GetPosition(), pads[j].GetPosition()
                if (a - c).EuclideanNorm() > MM(4) or pads[i].GetParentFootprint() == pads[j].GetParentFootprint():
                    continue
                if (pads[i].GetParentFootprint().GetReference(), pads[j].GetParentFootprint().GetReference()) not in DIRECT_ONLY:
                    continue
                for path in ([a, c], [a, pcbnew.VECTOR2I(c.x, a.y), c], [a, pcbnew.VECTOR2I(a.x, c.y), c]):
                    if add_path(board, n, path, w):   # (straight, else L-shaped: out of the pin sideways first)
                        print(f"direct link {n}: {pads[i].GetParentFootprint().GetReference()} -> "
                              f"{pads[j].GetParentFootprint().GetReference()}")
                        break


def add_path(board, n, path, w, layer=pcbnew.F_Cu):
    """tracks along path (points) when none of them comes within 0.2 mm of another net's copper; True if added"""
    segs = []
    for p, q in zip(path, path[1:]):
        if p == q:
            continue
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(p)
        t.SetEnd(q)
        t.SetWidth(MM(w))
        t.SetLayer(layer)
        t.SetNet(board.FindNet(n))
        if collides(board, t, layer, MM(0.2)):
            return False
        segs.append(t)
    for t in segs:
        board.Add(t)
    return True


def ground_stubs(board):
    """a GND pour piece too small for a via (a capacitor's ground pad boxed in by tracks): a short ground track from its pad
    to the nearest point of the largest pour piece on that layer, if one fits"""
    added = 0
    anchors = [t.GetPosition() for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == "GND"]
    anchors += [p.GetPosition() for f in board.GetFootprints() for p in f.Pads()
                if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    for z in [z for z in board.Zones() if z.GetNetname() == "GND" and z.IsOnLayer(pcbnew.F_Cu)]:
        polys = z.GetFilledPolysList(pcbnew.F_Cu)
        outs = [polys.Outline(i) for i in range(polys.OutlineCount())]
        main_ol = max(outs, key=lambda o: o.Area())
        for ol in outs:
            if ol is main_ol or any(ol.PointInside(a) for a in anchors):
                continue
            pads = [p for f in board.GetFootprints() for p in f.Pads()
                    if p.GetNetname() == "GND" and p.IsOnLayer(pcbnew.F_Cu) and ol.PointInside(p.GetPosition())]
            for p in pads:
                c = p.GetPosition()
                near = main_ol.NearestPoint(c)
                tip = pcbnew.VECTOR2I(near.x + (near.x - c.x) // 10, near.y + (near.y - c.y) // 10)   # (a little into it)
                if (near - c).EuclideanNorm() < MM(5) and add_path(board, "GND", [c, tip], 0.25):
                    added += 1
                    break
    return added


def prune_dangling(board):
    """track ends that touch nothing of their net (left over after widening and rerouting) removed, repeatedly; the
    antenna's end pad extension (ANT_END) stays"""
    gone = 0
    while True:
        tracks = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetNetname() not in ("ANT_END", "")]
        cut = []
        for t in tracks:
            for e in (t.GetStart(), t.GetEnd()):
                n, L = t.GetNetname(), t.GetLayer()
                hit = any(o is not t and o.GetNetname() == n and o.GetClass() == "PCB_TRACK" and o.GetLayer() == L and
                          min((o.GetStart() - e).EuclideanNorm(), (o.GetEnd() - e).EuclideanNorm()) <= max(t.GetWidth(), o.GetWidth()) // 2
                          for o in tracks)
                hit = hit or any(v.GetClass() == "PCB_VIA" and v.GetNetname() == n and (v.GetPosition() - e).EuclideanNorm() <= v.GetWidth(L) // 2
                                 for v in board.GetTracks())
                hit = hit or any(p.GetNetname() == n and p.IsOnLayer(L) and p.HitTest(e) for f in board.GetFootprints() for p in f.Pads())
                hit = hit or (n == "GND" and any(z.GetNetname() == "GND" and z.IsOnLayer(L) and z.GetFilledPolysList(L).Contains(e)
                                                 for z in board.Zones()))
                if not hit:
                    cut.append(t)
                    break
        if not cut:
            return gone
        for t in cut:
            board.Delete(t)
            gone += 1


# the converters' switch nodes (2026-11-15): pin -> inductor pad, drawn straight along the template's axis before
# anything else (Freerouting left both of the TPS63802's open in every run: pre_ground_vias put PGND's via right between
# the switch pins and L4, and the 0.5 mm pitch leaves no other way out)
SWITCH = [("U2", "9", "L4", "1"), ("U2", "7", "L4", "2"), ("U18", "5", "L5", "2")]


def signal_opens(b):
    """the open links of every net but GND (the pours carry it): pads joined through tracks and vias (2026-11-15:
    Freerouting's own count includes GND, which every tidy's new vias add to, so it never reached 0)"""
    bynet = collections.defaultdict(lambda: {"pads": [], "tr": [], "via": []})
    for fp in b.GetFootprints():
        for p in fp.Pads():
            if p.GetNetname():
                bynet[p.GetNetname()]["pads"].append(p)
    for t in b.GetTracks():
        bynet[t.GetNetname()]["via" if t.GetClass() == "PCB_VIA" else "tr"].append(t)
    inner = collections.defaultdict(list)       # (4 layers, 2026-11-16: the inner planes, filled, join what they reach)
    for z in b.Zones():
        if not z.GetIsRuleArea() and z.GetLayer() not in (pcbnew.F_Cu, pcbnew.B_Cu):
            inner[z.GetNetname()].append(z)
    if inner:
        pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    res = []
    for n, d in bynet.items():
        if n == "GND" or len(d["pads"]) < 2:
            continue
        items = d["pads"] + d["tr"] + d["via"]
        par = list(range(len(items)))

        def f(x):
            while par[x] != x:
                par[x] = par[par[x]]
                x = par[x]
            return x

        def u(a, c):
            par[f(a)] = f(c)
        pts = []
        for i, it in enumerate(items):
            if it.GetClass() == "PCB_TRACK":
                for q in (it.GetStart(), it.GetEnd()):
                    pts.append((q, {it.GetLayer()}, i, it.GetWidth() / 2))
            elif it.GetClass() == "PCB_VIA":
                pts.append((it.GetPosition(), {pcbnew.F_Cu, pcbnew.B_Cu}, i, it.GetWidth(pcbnew.F_Cu) / 2))
        for q, ls, i, r in pts:
            for j, it in enumerate(items):
                if j == i:
                    continue
                if it.GetClass() == "PAD":
                    if any(it.IsOnLayer(L) for L in ls) and it.HitTest(q, int(r * 0.5)):
                        u(i, j)
                elif it.GetClass() == "PCB_TRACK":
                    if it.GetLayer() in ls and it.HitTest(q, int(r * 0.5)):
                        u(i, j)
                elif it.GetClass() == "PCB_VIA":
                    if (it.GetPosition() - q).EuclideanNorm() <= it.GetWidth(pcbnew.F_Cu) / 2 + r * 0.5:
                        u(i, j)
        for z in inner.get(n, []):                 # (4 layers: vias and through-hole pads on one piece of an inner plane)
            L = z.GetLayer()
            polys = z.GetFilledPolysList(L)
            through = [k for k, it in enumerate(items) if it.GetClass() == "PCB_VIA" or
                       (it.GetClass() == "PAD" and it.GetAttribute() == pcbnew.PAD_ATTRIB_PTH)]
            for oi in range(polys.OutlineCount()):
                inside = [k for k in through if polys.Contains(items[k].GetPosition(), oi)]
                for k in inside[1:]:
                    u(inside[0], k)
        groups = collections.defaultdict(list)
        for i, p in enumerate(d["pads"]):
            groups[f(i)].append(p)
        gl = list(groups.values())
        done, rest = [gl[0]], gl[1:]
        while rest:
            best = None
            for k, g in enumerate(rest):
                for a in (x for dd in done for x in dd):
                    for z in g:
                        dist = (a.GetPosition() - z.GetPosition()).EuclideanNorm()
                        if best is None or dist < best[0]:
                            best = (dist, k, a, z)
            dist, k, a, z = best
            nm = lambda p: f"{p.GetParentFootprint().GetReference()}.{p.GetNumber()}"
            res.append((n, nm(a), nm(z), round(pcbnew.ToMM(dist), 1)))
            done.append(rest.pop(k))
    return sorted(res)


def switch_links(board):
    done = 0
    for ic, pin, ind, ipad in SWITCH:
        fi, fl = board.FindFootprintByReference(ic), board.FindFootprintByReference(ind)
        if fi is None or fl is None:
            continue
        p = next(q for q in fi.Pads() if q.GetNumber() == pin)
        lp = next(q for q in fl.Pads() if q.GetNumber() == ipad)
        a, c = fi.GetPosition(), fl.GetPosition()
        dx, dy = c.x - a.x, c.y - a.y
        n = (dx * dx + dy * dy) ** 0.5
        s = ((lp.GetPosition().x - p.GetPosition().x) * dx + (lp.GetPosition().y - p.GetPosition().y) * dy) / n
        q = pcbnew.VECTOR2I(int(p.GetPosition().x + dx / n * s), int(p.GetPosition().y + dy / n * s))
        if not lp.HitTest(q):
            q = lp.GetPosition()
        for w in (0.3, 0.25, 0.2):
            if add_path(board, p.GetNetname(), [p.GetPosition(), q], w):
                done += 1
                break
        else:
            print(f"switch link {ic}.{pin} -> {ind}.{ipad} blocked")
    return done


def pre_ground_vias(board):
    """a GND via beside every SMD ground pad outside the chip block (crowded: its pads join the pour as they are) before routing, as
    route.py did: the routes then go round them and no pad's pour pocket can close without one"""
    add = via_adder(board)
    others = []
    for L in (pcbnew.F_Cu, pcbnew.B_Cu):
        others += [p.GetEffectiveShape(L) for f in board.GetFootprints() for p in f.Pads() if p.IsOnLayer(L) and p.GetNetname() != "GND"]
        others += [t.GetEffectiveShape(L) for t in board.GetTracks() if t.IsOnLayer(L) and t.GetNetname() != "GND"]
    others += [p.GetEffectiveHoleShape() for f in board.GetFootprints() for p in f.Pads() if p.GetDrillSize().x > 0]
    z = [z for z in board.Zones() if z.GetNetname() == "GND" and z.IsOnLayer(pcbnew.F_Cu)][0]
    polys = z.GetFilledPolysList(pcbnew.F_Cu)
    added = 0
    for f in board.GetFootprints():
        for p in f.Pads():
            if p.GetNetname() != "GND" or p.GetAttribute() != pcbnew.PAD_ATTRIB_SMD or f.GetReference() in CHIP_BLOCK + ["C28"]:
                continue
            vias = [t.GetPosition() for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == "GND"]
            if any((v - p.GetPosition()).EuclideanNorm() < MM(1.2) for v in vias):
                continue
            ol = min((polys.Outline(i) for i in range(polys.OutlineCount())), key=lambda o: o.SquaredDistance(p.GetPosition()))
            c = best_spot(board, ol, pcbnew.F_Cu, others, near=p.GetPosition())
            if c is not None and (c - p.GetPosition()).EuclideanNorm() < MM(1.5) and add(mm(c.x), mm(c.y)):
                added += 1
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    return added


def relabel(board):
    """the buttons' silk labels on the same side of both buttons (above or below, the first offset clear of every pad for
    both), where the placer put the buttons"""
    pads = [p for f in board.GetFootprints() for p in f.Pads() if p.IsOnLayer(pcbnew.F_Cu)]
    items = []
    for ref, text in (("SW1", "NO / BOOT"), ("SW3", "UP"), ("SW4", "DOWN"), ("SW5", "YES")):
        p = board.FindFootprintByReference(ref).GetPosition()
        items += [(t, p) for t in board.GetDrawings() if t.GetClass() == "PCB_TEXT" and t.GetText() == text]

    def clear(dy):
        for t, p in items:
            t.SetPosition(pcbnew.VECTOR2I(p.x, p.y + MM(dy)))
            bb = t.GetBoundingBox()
            bb.Inflate(MM(0.2))
            if any(bb.Intersects(q.GetBoundingBox()) for q in pads):
                return False
        return True
    for dy in (-4.2, 4.2, -4.8, 4.8, -5.4, 5.4, -6.0, 6.0):
        if clear(dy):
            return dy
    clear(-4.2)
    return None

def main():
    path = sys.argv[1]
    if not os.path.exists(JAR):
        sys.exit(f"no {JAR}: sh fetch_freerouting.sh")
    board = pcbnew.LoadBoard(path)
    relabel(board)
    print(f"{switch_links(board)} switch-node links drawn")
    direct_links(board)
    print(f"{pre_ground_vias(board)} ground vias beside the SMD ground pads before routing")
    pv, miss = pre_plane_vias(board)
    print(f"{pv} plane vias by the {'/'.join(PLANE_NETS)} pads; without one: {miss or 'none'}")
    if SEED:
        print(f"round 0 (the seed nets alone): {route_round(board, 0, SEED)} left")
        tidy(board)
    for k in range(1, ROUNDS + EXTRA + 1):       # (2026-11-15: until no SIGNAL connection is open: GND is the pours')
        route_round(board, k)
        tidy(board)
        opens = signal_opens(board)
        print(f"  signal opens after round {k}: {len(opens)} {opens}")
        if not opens:
            break
    left = len(signal_opens(board))
    for j in range(MOAT_ROUNDS):                 # (stage 2: bridge every cut-off ground piece, reroute what the bridge
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())   #  crossed, until no piece is cut off and nothing is left open)
        groups, main_vias = moats.isolated(board)
        print(f"moats: {len(groups)} cut-off ground groups, {left} connections open")
        if not groups and left == 0:
            break
        if groups and not moats.bridge(board, groups, main_vias) and left == 0:
            break
        route_round(board, ROUNDS + EXTRA + 1 + j)
        tidy(board)
        left = len(signal_opens(board))
    print(f"{unconnected(board)} connections open (KiCad's count)")
    print(f"{space_holes(board)} GND vias moved off another via's drill")
    print(f"{drop_close_gnd_vias(board)} GND vias too close to another removed; {drop_stubs(board)} stubs removed")
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(path)
    gaps = close_gaps(board, path)
    if gaps:
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        board.Save(path)
    print(f"{gaps} open connections closed by a short track")
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


def close_gaps(board, path):
    """the connections KiCad's DRC still finds open (two pieces of a signal net): a short straight or L-shaped track
    between their nearest ends, on top, where it keeps 0.2 mm from everything else"""
    rpt = os.path.splitext(path)[0] + ".freerouting/gaps.rpt"
    subprocess.run([KICAD_CLI, "pcb", "drc", "--severity-all", "-o", rpt, path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    text = open(rpt).read() if os.path.exists(rpt) else ""
    closed = 0
    for block in text.split("[unconnected_items]")[1:]:
        pts = re.findall(r"@\(([-\d.]+) mm, ([-\d.]+) mm\): (?:Track|Pad[^\[]*) \[([^\]]+)\]", block.split("\n[")[0])
        if len(pts) != 2 or pts[0][2] != pts[1][2] or pts[0][2] == "GND":
            continue
        n = pts[0][2]
        ends = []
        for x, y, _n in pts:                     # (the reported item: its end or centre nearest the other)
            at = pcbnew.VECTOR2I(MM(float(x)), MM(float(y)))
            cands = [at] + [e for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetNetname() == n
                            and t.HitTest(at) for e in (t.GetStart(), t.GetEnd())]
            ends.append(cands)
        a0, c0 = min(((p, q) for p in ends[0] for q in ends[1]), key=lambda pq: (pq[0] - pq[1]).EuclideanNorm())
        if (a0 - c0).EuclideanNorm() > MM(6):
            continue
        for route in ([a0, c0], [a0, pcbnew.VECTOR2I(c0.x, a0.y), c0], [a0, pcbnew.VECTOR2I(a0.x, c0.y), c0]):
            if add_path(board, n, route, 0.2):
                closed += 1
                break
    return closed


def space_holes(board):
    """a GND via whose drill comes within 0.5 mm (edge to edge) of another via's: moved straight away by the shortfall
    + 0.02 mm when it stays 0.2 mm clear of every other net's copper (Freerouting's octagons leave 0.47-0.49)"""
    moved = 0
    vias = [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    for v in [v for v in vias if v.GetNetname() == "GND"]:
        for o in vias:
            if o is v:
                continue
            d = (v.GetPosition() - o.GetPosition()).EuclideanNorm()
            need = MM(0.5) + (v.GetDrillValue() + o.GetDrillValue()) // 2 + MM(0.02)
            if 0 < d < need:
                old = v.GetPosition()
                step = need - d
                v.SetPosition(old + pcbnew.VECTOR2I(int((old.x - o.GetPosition().x) * step / d), int((old.y - o.GetPosition().y) * step / d)))
                if collides(board, v, pcbnew.F_Cu, MM(0.2)) or collides(board, v, pcbnew.B_Cu, MM(0.2)):
                    v.SetPosition(old)
                else:
                    moved += 1
    return moved


def drop_close_gnd_vias(board):
    """72 x 72: a GND via still within 0.5 mm (drill to drill) of another after space_holes: removed if the board's open
    count stays the same without it (a stitching via; route 9 left one 0.49 mm from an SDA via)"""
    gone = 0
    vias = [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    for v in [v for v in vias if v.GetNetname() == "GND"]:
        if any(o is not v and 0 < (v.GetPosition() - o.GetPosition()).EuclideanNorm() <
               MM(0.5) + (v.GetDrillValue() + o.GetDrillValue()) // 2 for o in vias):
            before = unconnected(board)
            board.Remove(v)
            if unconnected(board) > before:
                board.Add(v)
            else:
                gone += 1
    return gone


def drop_stubs(board):
    """72 x 72: tracks under 0.2 mm long with an end touching nothing (route 9 left two: BTN_YES, U0TXD)"""
    ends = []
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            ends.append((t.GetPosition(), None, t))
        else:
            ends += [(t.GetStart(), t.GetLayer(), t), (t.GetEnd(), t.GetLayer(), t)]
    pads = [(p, p.GetPosition()) for f in board.GetFootprints() for p in f.Pads()]
    gone = 0
    for t in [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetLength() < MM(0.2)]:
        for e in (t.GetStart(), t.GetEnd()):
            touch = any(o is not t and (lay is None or lay == t.GetLayer()) and (q - e).EuclideanNorm() < MM(0.01)
                        for q, lay, o in ends)
            touch = touch or any(p.IsOnLayer(t.GetLayer()) and p.HitTest(e) for p, _ in pads)
            if not touch:
                board.Remove(t)
                gone += 1
                break
    return gone


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
