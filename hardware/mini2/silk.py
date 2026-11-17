# SPDX-License-Identifier: GPL-3.0-only
"""The silkscreen of jank mini 2.2 (stage 2, the user's list; README "Silkscreen"), on the ROUTED board (after
freeroute.py and tie_groups.py: it sees every pad and via). JLCPCB's minimum: text 0.8 mm high, 0.15 mm stroke; nothing
over exposed copper (pads, untented vias) or the board's edge. Everything it adds is in one group, "silk.py", removed and
redrawn on each run.

TOP: every part's reference (where it fits: the ones that do not are listed), the twins' and spares' purposes, the
recovery pads' and the jumper / switch pads' names, "USB ONLY: BRIDGE" at R62, "GND LIFT" at R39, "FB" at R63, the
power toggle's "ON / OFF ->", and the blocks' outlines with their titles (POWER, CHARGE, LINE IN, MIDI OUT 1 / 2, RF, MCU,
CLK / RST), each outline broken where it would cross a pad.
BOTTOM: "jank mini 2.2", the date, a small wordmark, the cheat sheet (twins, the screen's jumpers, MIDI, the D-pad, the
recovery pads, CLK / RST) and "CHECK CELL POLARITY BEFORE PLUGGING".
  .../python3.9 silk.py mini2.kicad_pcb
"""
import math
import os
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import parts as P

MM, TO = pcbnew.FromMM, pcbnew.ToMM
OX, OY = 100.0, 100.0
TEXT_H, STROKE, LINE_W = 0.8, 0.15, 0.15       # (JLCPCB: 0.8 mm text, 0.15 mm stroke minimum)
CLEAR = 0.2                                     # silk to exposed copper / other silk
EDGE = 0.3
DATE = "2026-11-15"
GROUP = "silk.py"
TAGS = {   # short purposes, top (README "Twins", "Spare pads"): placed like a reference, beside it
    "RT15": "SINK", "RT55": "SINK", "RT20": "SINK", "RT57": "SINK",
    "RT33": "BIAS", "RT34": "BIAS", "RT36": "PD", "RT37": "PD",
    "RT38": "DET", "RT51": "ICHG", "RT10": "VBAT", "RT11": "VBAT", "RT60": "SCL", "RT61": "SDA", "RT39": "GND LIFT", "RT120": "CLK GND", "RT121": "RST GND",
    "RT73": "CLK PU", "RT75": "CLK R", "RT79": "RST PU", "RT81": "RST R", "CT8": "VBAT C", "CT3": "VIN",
    "C72": "VREG BULK", "C92": "VREG BULK", "R155": "BULK R", "C27": "VBUS DAMP", "C91": "VBUS DAMP", "R88": "VBUS SENSE", "U11": "ESD", "U12": "ESD", "U15": "IDEAL D", "U18": "5V AUX", "U19": "5V CLAMP", "U2": "3V3 BB", "U2B": "LDO 3V3 ALT", "C26": "PI", "C80": "DB", "C81": "DB", "C82": "DB", "C83": "DB", "C27": "VBUS", "C67": "GS",
    "U17": "CC", "Q9": "VBUS ISO", "RT93": "HOST R", "R98": "LED FILT",
}
NOTES = [("R62", "USB ONLY: BRIDGE"), ("R39", "GND LIFT"), ("R63", "USB ONLY"), ("J11", "<- ON/OFF"),
         ("J10", "OLED 1.54"), ("J10B", "OLED 2.42 LEAD")]
BLOCKS = [   # title, net prefixes (a part with a pin on one of them belongs), extra parts
    ("MIDI OUT 2", ("MO2_", "MD2_", "MIDI_TX2"), ()),
    ("MIDI OUT 1", ("MO_", "MD_", "MIDI_TX"), ()),
    ("LINE IN", ("LIN_", "LINE_IN", "LINE_SENSE"), ()),
    ("CLK / RST", ("CLK_", "RST_"), ()),
    ("USB HOST", ("HOST", "V5H"), ("U17", "Q9")),
    ("CHARGE", ("BAT_", "XB_VDD", "Q2_G", "PROG", "CHRG"), ()),
    ("POWER", ("VBUS", "VUSB", "VSYS", "VREG"), ()),
    ("5V / 3V3", ("V5_", "V3_"), ("U18", "U2", "L4", "L5", "U19")),
    ("MCU", ("CHIP_PU",), ("U1",)),
]
CHEAT = [
    "TWINS: RTxx / CTxx sit across Rxx / Cxx: a part there",
    "  LOWERS the R (R // Rt) or ADDS to the C",
    "SCREEN: a 1.54 plugs into J10, a 2.42 on a lead to J10B;",
    "  bridge JP1 / JP2 to the module's pin order (P1 G|V: pin 1 GND or VCC)",
    "J11 = the power toggle; R62 (fitted in the USB order) bridges it",
    "MIDI TRS type A: tip = sink (DIN 5), ring = source (DIN 4)",
    "MIDI OUT 1 = GPIO37, MIDI OUT 2 = GPIO42, VBUS sense GPIO14",
    "D-PAD: UP GPIO18, DOWN GPIO4, NO/BOOT GPIO0, YES GPIO5",
    "RECOVERY J12: TX RX 3V3 GND EN BOOT (USB-UART 3.3 V):",
    "  hold BOOT to GND, pulse EN to GND: download mode",
    "CLK OUT GPIO36, RST OUT GPIO21 (0 / 5 V through 1 k)",
    "LINE IN GPIO9 (tip) + GPIO8 (sleeve sense), detect GPIO48, cell GPIO10 (empty 3.3 V)",
    "RGB LEDs GPIO38, 5 V rail GPIO41 (low = off); I2C SCL 39 SDA 35",
    "USB HOST: enable GPIO47, CC INT GPIO13 (battery)",
]


def mm_box(b, grow=0.0):
    return (TO(b.GetLeft()) - grow, TO(b.GetTop()) - grow, TO(b.GetRight()) + grow, TO(b.GetBottom()) + grow)


def hit(a, b, gap=0.0):
    return a[0] < b[2] + gap and b[0] < a[2] + gap and a[1] < b[3] + gap and b[1] < a[3] + gap


def tented(v, layer):
    try:
        return v.IsTented(layer)
    except TypeError:
        return v.IsTented()


class Silk:
    def __init__(self, board):
        self.board = board
        self.group = pcbnew.PCB_GROUP(board)
        self.group.SetName(GROUP)
        board.Add(self.group)
        e = board.GetBoardEdgesBoundingBox()
        self.edge = (TO(e.GetLeft()) + EDGE, TO(e.GetTop()) + EDGE, TO(e.GetRight()) - EDGE, TO(e.GetBottom()) - EDGE)
        self.copper = {pcbnew.F_SilkS: [], pcbnew.B_SilkS: []}
        for f in board.GetFootprints():
            for p in f.Pads():
                for silk, cu, mask in ((pcbnew.F_SilkS, pcbnew.F_Cu, pcbnew.F_Mask), (pcbnew.B_SilkS, pcbnew.B_Cu, pcbnew.B_Mask)):
                    if p.IsOnLayer(cu) or p.IsOnLayer(mask):
                        self.copper[silk].append(mm_box(p.GetBoundingBox()))
        for t in board.GetTracks():
            if t.GetClass() == "PCB_VIA":
                for silk, mask in ((pcbnew.F_SilkS, pcbnew.F_Mask), (pcbnew.B_SilkS, pcbnew.B_Mask)):
                    if not tented(t, mask):
                        self.copper[silk].append(mm_box(t.GetBoundingBox()))
        self.used = {pcbnew.F_SilkS: [], pcbnew.B_SilkS: []}
        for d in board.GetDrawings():            # (make_board.py's own silk: labels, the pivot ring)
            if d.GetLayer() in self.used:
                self.used[d.GetLayer()].append(mm_box(d.GetBoundingBox()))
        for f in board.GetFootprints():
            for item in f.GraphicalItems():
                if item.GetLayer() in self.used:
                    self.used[item.GetLayer()].append(mm_box(item.GetBoundingBox()))
        self.missed = []

    def free(self, box, layer):
        if box[0] < self.edge[0] or box[1] < self.edge[1] or box[2] > self.edge[2] or box[3] > self.edge[3]:
            return False
        return not any(hit(box, c, CLEAR) for c in self.copper[layer]) and not any(hit(box, u, CLEAR) for u in self.used[layer])

    def _style(self, t, layer, h):
        t.SetLayer(layer)
        t.SetMirrored(layer == pcbnew.B_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I(MM(h), MM(h)))
        t.SetTextThickness(MM(STROKE))
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
        t.SetVertJustify(pcbnew.GR_TEXT_V_ALIGN_CENTER)

    def try_text(self, t, spots, layer):
        """t at the first free spot of spots [(x, y, angle)]; True if placed"""
        for x, y, a in spots:
            t.SetTextAngleDegrees(a)
            t.SetPosition(pcbnew.VECTOR2I(MM(OX + x), MM(OY + y)))
            b = mm_box(t.GetBoundingBox())
            if self.free(b, layer):
                self.used[layer].append(b)
                return True
        return False

    def text(self, s, spots, layer=pcbnew.F_SilkS, h=TEXT_H):
        t = pcbnew.PCB_TEXT(self.board)
        t.SetText(s)
        self._style(t, layer, h)
        if self.try_text(t, spots, layer):
            self.board.Add(t)
            self.group.AddItem(t)
            return t
        self.missed.append(s)
        return None

    def line(self, x0, y0, x1, y1, layer):
        """a silk segment, broken where it would come within CLEAR of exposed copper or leave the edge"""
        n = max(int(math.hypot(x1 - x0, y1 - y0) / 0.1), 1)
        run = []
        for i in range(n + 1):
            x, y = x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n
            w = LINE_W / 2
            ok = (not any(hit((x - w, y - w, x + w, y + w), c, CLEAR) for c in self.copper[layer])
                  and not any(hit((x - w, y - w, x + w, y + w), u, 0.1) for u in self.used[layer])
                  and self.edge[0] <= x <= self.edge[2] and self.edge[1] <= y <= self.edge[3])
            if ok:
                run.append((x, y))
            if (not ok or i == n) and len(run) >= 2:
                if math.hypot(run[-1][0] - run[0][0], run[-1][1] - run[0][1]) >= 0.5:
                    self._seg(run[0], run[-1], layer)
            if not ok:
                run = []

    def _seg(self, a, b, layer):
        s = pcbnew.PCB_SHAPE(self.board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pcbnew.VECTOR2I(MM(OX + a[0]), MM(OY + a[1])))
        s.SetEnd(pcbnew.VECTOR2I(MM(OX + b[0]), MM(OY + b[1])))
        s.SetWidth(MM(LINE_W))
        s.SetLayer(layer)
        self.board.Add(s)
        self.group.AddItem(s)


def around(box, h=TEXT_H, step=0.4, reach=2.4):
    """spots round a part's courtyard box (board mm, OX / OY removed): above, below, right, left, then further out"""
    x0, y0, x1, y1 = box
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    out = []
    d = 0.0
    while d <= reach:
        out += [(cx, y0 - h / 2 - 0.1 - d, 0), (cx, y1 + h / 2 + 0.1 + d, 0),
                (x1 + h / 2 + 0.1 + d, cy, 90), (x0 - h / 2 - 0.1 - d, cy, 90)]
        d += step
    return out


def local(box):
    return (box[0] - OX, box[1] - OY, box[2] - OX, box[3] - OY)


def clear_old(board):
    """the previous run's group and its items, gone (the group emptied first, then each item removed from the board)"""
    for g in [g for g in board.Groups() if g.GetName() == GROUP]:
        items = list(g.GetItems())
        g.RemoveAll()
        for item in items:
            board.Remove(item)
        board.Remove(g)


PARKED = ("P1 G|V", "P2 V|G", "P1", "P2", "G", "V", "CELL 1", "CELL 2")   # (review S4: G / V at the jumpers' end pads)


def fix_labels(board, s):
    """make_board.py's own silk texts (the jumpers', the cells', the buttons', the jacks' names) moved off pads and off
    each other: each to the nearest free spot within 3 mm; one with no free spot goes to User.Comments (not made; listed
    with the missed)"""
    moved = 0
    texts = [d for d in board.GetDrawings() if d.GetClass() == "PCB_TEXT" and (d.GetLayer() in s.used or
             (d.GetLayer() == pcbnew.Cmts_User and d.GetText() in PARKED))]
    for d in texts:                               # (a label parked by an earlier run tried again on the front)
        if d.GetLayer() == pcbnew.Cmts_User:
            d.SetLayer(pcbnew.F_SilkS)
    for d in texts:
        layer = d.GetLayer()
        b = mm_box(d.GetBoundingBox())
        rest = [u for u in s.used[layer] if u != b]
        if not any(hit(b, c, -0.02) for c in s.copper[layer]) and not any(hit(b, u, -0.02) for u in rest):   # (touching is not a hit)
            continue
        s.used[layer] = rest
        x0, y0 = TO(d.GetPosition().x) - OX, TO(d.GetPosition().y) - OY
        a = d.GetTextAngleDegrees()
        spots = sorted(((x0 + i * 0.2, y0 + j * 0.2, a) for i in range(-15, 16) for j in range(-15, 16)),
                       key=lambda q: math.hypot(q[0] - x0, q[1] - y0))
        if s.try_text(d, spots, layer):
            moved += 1
        else:
            d.SetPosition(pcbnew.VECTOR2I(MM(OX + x0), MM(OY + y0)))
            d.SetLayer(pcbnew.Cmts_User)          # (removing it here breaks KiCad's Python for the rest of the run)
            s.missed.append(d.GetText() + " (label)")
    return moved


def refs(board, s):
    for f in board.GetFootprints():
        r = f.GetReference()
        if r == "XTRA":
            continue
        layer = pcbnew.B_SilkS if f.IsFlipped() else pcbnew.F_SilkS
        cy = f.GetCourtyard(pcbnew.B_CrtYd if f.IsFlipped() else pcbnew.F_CrtYd).BBox()
        box = local(mm_box(cy))
        t = f.Reference()
        t.SetVisible(True)
        s._style(t, layer, TEXT_H)
        t.SetKeepUpright(True)
        if not s.try_text(t, around(box), layer):
            t.SetVisible(False)
            s.missed.append(r)
        tag = TAGS.get(r)
        if tag:
            s.text(tag, around(box, reach=3.2), layer)


POLAR = ()   # (the tantalums: none since experiment A, 2026-11-15)


def notes(board, s):
    fps = {f.GetReference(): f for f in board.GetFootprints()}
    for r, text in NOTES:
        box = local(mm_box(fps[r].GetCourtyard(pcbnew.F_CrtYd).BBox()))
        s.text(text, around(box, reach=8.0))
    for r in POLAR:                               # (review pass 1: the tantalums' + by pad 1, outward along the part)
        f = fps[r]
        p1, p2 = [next(q for q in f.Pads() if q.GetNumber() == n) for n in ("1", "2")]
        x1, y1 = TO(p1.GetPosition().x) - OX, TO(p1.GetPosition().y) - OY
        x2, y2 = TO(p2.GetPosition().x) - OX, TO(p2.GetPosition().y) - OY
        L = math.hypot(x1 - x2, y1 - y2) or 1.0
        ux, uy = (x1 - x2) / L, (y1 - y2) / L
        s.text("+", [(x1 + ux * d - uy * k, y1 + uy * d + ux * k, 0) for d in (1.4, 1.7, 2.0) for k in (0, 1.0, -1.0)], h=1.0)
    j12 = fps["J12"]
    for p in j12.Pads():
        x, y = TO(p.GetPosition().x) - OX, TO(p.GetPosition().y) - OY
        name = {"1": "TX", "2": "RX", "3": "3V3", "4": "GND", "5": "EN", "6": "BOOT"}[p.GetNumber()]
        s.text(name, [(x, y - 1.25 - 0.1 - d - len(name) * 0.35, 90) for d in (0, 0.3, 0.6)] +
               [(x, y + 1.25 + 0.1 + d + len(name) * 0.35, 90) for d in (0, 0.3, 0.6)])


def blocks(board, s):
    fps = [f for f in board.GetFootprints() if not f.IsFlipped() and f.GetReference() != "XTRA"]
    done = set()
    for title, prefixes, extra in BLOCKS:
        members = []
        for f in fps:
            r = f.GetReference()
            if r in done or r.startswith(("J5", "J6")):
                continue
            nets = {p.GetNetname() for p in f.Pads()}
            if r in extra or any(n.startswith(prefixes) for n in nets if n):
                members.append(f)
        done |= {f.GetReference() for f in members}
        boxes = [local(mm_box(f.GetCourtyard(pcbnew.F_CrtYd).BBox())) for f in members]
        for cl in clusters(boxes):
            x0 = min(b[0] for b in cl) - 0.3
            y0 = min(b[1] for b in cl) - 0.3
            x1 = max(b[2] for b in cl) + 0.3
            y1 = max(b[3] for b in cl) + 0.3
            if (x1 - x0) * (y1 - y0) < 12:
                continue                          # (a lone part: no box)
            for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
                s.line(a[0], a[1], b[0], b[1], pcbnew.F_SilkS)
            w = len(title) * TEXT_H * 0.75
            spots = [(x0 + w / 2 + d, y0 - TEXT_H / 2 - 0.15, 0) for d in (0, 1, 2, 3, 4)] + \
                    [(x0 + w / 2 + d, y1 + TEXT_H / 2 + 0.15, 0) for d in (0, 1, 2, 3, 4)] + \
                    [(x0 + w / 2 + 0.3 + d, y0 + TEXT_H / 2 + 0.3, 0) for d in (0, 1, 2, 3, 4)]
            s.text(title, spots)


def clusters(boxes, link=3.0):
    left = list(boxes)
    out = []
    while left:
        cl = [left.pop()]
        grew = True
        while grew:
            grew = False
            for b in list(left):
                if any(hit(b, c, link) for c in cl):
                    cl.append(b)
                    left.remove(b)
                    grew = True
        out.append(cl)
    return out


def bottom(board, s, w, h):
    """the bottom: title, date, wordmark, then the cheat sheet, each line at the first free height (in order, top down,
    centred or shifted up to 8 mm sideways: the bottom twins, jacks and holders' pads cut the free bands)"""
    B = pcbnew.B_SilkS
    cx = w / 2
    s.text("jank mini 2.2", [(cx, y, 0) for y in (17.0, 17.5, 18.0, 18.5, 19.0)], B, 1.6)
    s.text(DATE + "  72 x 72  stage 2", [(cx, y, 0) for y in (19.6, 20.1, 20.6, 21.1)], B, 0.8)
    wordmark(board, s, cx, 22.6)
    y = 24.6
    for line in CHEAT:
        spots = [(cx + dx, y + k * 0.2, 0) for k in range(int((h - 3.0 - y) / 0.2)) for dx in (0, -2, 2, -4, 4, -6, 6, -8, 8)]
        t = s.text(line, spots, B, 0.8)
        if t is not None:
            y = TO(t.GetPosition().y) - OY + 1.15
    s.text("CHECK CELL POLARITY BEFORE PLUGGING", [(cx + dx, h - 16.0 - k * 0.25, 0) for k in range(40) for dx in (0, -3, 3, -6, 6)] +
           [(cx + dx, h - 16.0 + k * 0.25, 0) for k in range(1, 50) for dx in (0, -3, 3, -6, 6)], B, 1.0)


def wordmark(board, s, cx, cy):
    """a small logo of its own: a square wave (a clock) running into a sine (the line in), 14 x 2 mm, bottom silk"""
    pts = []
    x = cx - 7.0
    for k in range(3):                            # (three clock pulses)
        pts += [(x, cy + 0.9), (x, cy - 0.9), (x + 1.0, cy - 0.9), (x + 1.0, cy + 0.9), (x + 2.0, cy + 0.9)]
        x += 2.0
    for i in range(41):                           # (then a sine, 8 mm, one and a half periods)
        u = i / 40
        pts.append((x + 8.0 * u, cy - 0.9 * math.sin(3 * math.pi * u)))
    for a, b in zip(pts, pts[1:]):
        if a != b:
            s.line(a[0], a[1], b[0], b[1], pcbnew.B_SilkS)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "mini2.kicad_pcb")
    if "--clear" in sys.argv:                     # (the old silk removed in a process of its own: after the removals
        board = pcbnew.LoadBoard(path)            #  KiCad's Python returns bare SWIG objects, so the rest runs fresh)
        clear_old(board)
        board.Save(path)
        return
    subprocess.run([sys.executable, os.path.abspath(__file__), path, "--clear"], check=True, capture_output=True)
    board = pcbnew.LoadBoard(path)
    s = Silk(board)
    e = board.GetBoardEdgesBoundingBox()
    w, h = TO(e.GetWidth()), TO(e.GetHeight())
    print(f"silk: {fix_labels(board, s)} of make_board.py's labels moved off pads / each other")
    notes(board, s)
    bottom(board, s, w, h)
    blocks(board, s)
    refs(board, s)
    board.Save(path)
    shown = sum(1 for f in board.GetFootprints() if f.Reference().IsVisible())
    print(f"silk: {shown} references shown; no room for {len(s.missed)}: {', '.join(s.missed)}")


if __name__ == "__main__":
    main()
