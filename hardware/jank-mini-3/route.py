# SPDX-License-Identifier: GPL-3.0-only
"""Routes lcd2.kicad_pcb (make_board.py) with Freerouting 2.5.0 (mini 2.2's jar, hardware/mini2/fetch_freerouting.sh):
the board exported to Specctra DSN, the power nets given their own widths, GND left to the two pours; rounds until no
signal connection is open; then the pours filled, GND stitching vias (hardware/pcb/lib/stitch.py), and a GND via by
every top-side SMD ground pad that the pour alone does not reach.   KiCad's python: route.py lcd2.kicad_pcb"""
import os
import re
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "pcb", "lib"))
from stitch import stitch
sys.path.insert(0, HERE)
import make_board  # (the drawn tracks and the cut-out)

JAR = os.path.join(HERE, ".freerouting", "freerouting-2.5.0.jar")   # (sh fetch_freerouting.sh)
MM = pcbnew.FromMM
# widths in um (clearance 200): the cell's and the boost's currents (~0.25 A), USB VBUS, the 5 V rails (V5_SW 0.3:
# the SOT-563's 0.5 mm pitch; the track is ~1 mm long)
PWR = {"GND": 300, "VBAT": 1000, "VIN_B": 400, "V5_SW": 300, "VB_D3": 500, "VBUS_B": 400, "VB_D1": 400, "V5A": 300,
       "V5M": 300, "V5C": 300, "+3V3": 300}
# the LINE IN analogue nets: a wider clearance from everything (0.3 mm): the brief's "away from MIDI / CLK"
QUIET = ["LINE_IN", "LINE_SENSE", "LIN_A1", "LIN_A2", "LIN_B1", "LIN_B2", "LIN_VB", "LIN_VBL"]
# the nets LINE IN keeps 1.0 mm from (check_board.py's LOUD; the power ones are in PWR and keep their own class)
LOUD = {"MIDI_TX", "MIDI_TX2", "MO_R", "MO_T", "MO_K", "MD_R", "MD_T", "MD_K", "MO2_R", "MO2_T", "MO2_K", "MD2_R",
        "MD2_T", "MD2_K", "CLK_IO", "CLK_B", "CLK_N", "CLK_M", "CLK_OUT", "RST_IO", "RST_B", "RST_N", "RST_M", "RST_OUT",
        "HOST5", "HOST5G", "HOST_M", "UDP", "UDN"}
ROUNDS = 3   # (rounds after the first rarely close anything on jank-mini-3: congestion, not counting)
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
FIRST = {"VBAT", "VBUS_B"}   # (jank-mini-3: the two wide tracks that go far, routed alone first; adding the USB pair made it far worse: 2 -> 35 left)


def net_class(name, nets, width, clearance, via):
    return (f'    (class {name} {" ".join(nets)}\n      (circuit (use_via "{via}"))\n'
            f"      (rule (width {width}) (clearance {clearance}))\n    )\n")


def bare(n):
    return n.lstrip("/")


def patch_dsn(text):
    via = re.search(r'\(use_via "([^"]+)"\)', text).group(1)
    m = re.search(r"\(class kicad_default (.*?)\(circuit", text, re.S)
    nets = m.group(1).split()
    key = {n: bare(n.strip('"')) for n in nets}
    rest = [n for n in nets if key[n] not in PWR and key[n] not in QUIET]
    tail = text[m.end():]
    end = tail.index("\n    )\n") + len("\n    )\n")
    extra = [net_class(f"pwr{w}", [n for n in nets if PWR.get(key[n]) == w], w, 200, via) for w in sorted(set(PWR.values()))]
    extra.append(net_class("quiet", [n for n in nets if key[n] in QUIET], 200, 300, via))
    loud = [n for n in rest if key[n] in LOUD]
    if loud:   # (check_board's rule: LINE IN copper >= 1.0 mm from these; given to Freerouting as a class pair)
        rest = [n for n in rest if key[n] not in LOUD]
        extra.append(net_class("loud", loud, 200, 200, via))
        extra.append("    (class_class (classes quiet loud)\n      (rule (clearance 1050))\n    )\n")
    return text[:m.start()] + "(class kicad_default " + " ".join(rest) + "\n      (circuit" + tail[:end] + "".join(extra) + tail[end:]


def open_nets(board):
    """the signal nets KiCad's connectivity still sees unconnected (GND left out: the pours and stitching join it)"""
    import json
    tmp = board.GetFileName() + ".open.kicad_pcb"   # (KiCad's own DRC: its python has no ratsnest access)
    board.Save(tmp)
    rpt = tmp + ".json"
    subprocess.run(["nice", "-n", "19", KICAD_CLI, "pcb", "drc", "--format", "json", "-o", rpt, tmp],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    out = set()
    try:
        for v in json.load(open(rpt)).get("unconnected_items", []):
            for it in v.get("items", []):
                m = re.search(r"\[([^\]]+)\]", it.get("description", ""))
                if m and bare(m.group(1)) != "GND":
                    out.add(bare(m.group(1)))
    finally:
        for f in (tmp, rpt):
            if os.path.exists(f):
                os.remove(f)
    return out


def unconnected(board):
    board.BuildConnectivity()
    return board.GetConnectivity().GetUnconnectedCount(True)


def only_nets(text, keep):
    """round 0: every net but `keep` cut to its first pin (a one-pin net has nothing to route; its pads stay obstacles)"""
    def cut(m):
        name, pins = m.group(1), m.group(2).split()
        return m.group(0) if bare(name.strip('"')) in keep else m.group(0).replace(m.group(2), pins[0])
    return re.sub(r"\(net (\S+)\s+\(pins ([^)]*)\)", cut, text)


def route_round(board, k, only=None):
    # (only the drawn tracks and round 0's power tracks are locked: Freerouting's own wires from an earlier round stay
    #  movable, so a later round can push or reroute them; locking all made every later round worse, 6 -> 20 -> 27)
    fixed = set(FIRST) | {d[0] for d in make_board.DRAWN}
    for t in board.GetTracks():
        if bare(t.GetNetname()) in fixed:
            t.SetLocked(True)
    fn = board.GetFileName()   # (a work folder per board: parallel attempts must not share round files)
    work = os.path.join(HERE, ".freerouting", f"{os.path.basename(os.path.dirname(fn))}-{os.path.splitext(os.path.basename(fn))[0]}")
    os.makedirs(work, exist_ok=True)
    dsn, ses, log = (os.path.join(work, f"round{k}.{e}") for e in ("dsn", "ses", "log"))
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        sys.exit("DSN export failed")
    text = patch_dsn(open(dsn).read())
    if only:
        text = only_nets(text, only)
    open(dsn, "w").write(text)
    if os.path.exists(ses):
        os.remove(ses)
    r = subprocess.run(["nice", "-n", "19", "java", "-Djava.awt.headless=true", "-jar", JAR, "-de", dsn, "-do", ses, "--router.autorouter.max_passes=100",
                        "-mt", "1", "--gui.enabled=false"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    open(log, "w").write(r.stdout)
    if r.returncode != 0 or not os.path.exists(ses):
        sys.exit(f"Freerouting failed ({r.returncode}), see {log}")
    if not pcbnew.ImportSpecctraSES(board, ses):
        sys.exit("SES import failed")
    done = re.findall(r"\((\d+) unrouted", r.stdout)
    return int(done[-1]) if done else -1


def drop_one_sided_vias(board):
    """Freerouting's leftovers: a signal via with copper (tracks or a pad) on one layer only; collected first, each
    item removed once (removing an item twice broke SWIG's track list)"""
    tracks = [t for t in board.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]
    pads = [p for f in board.GetFootprints() for p in f.Pads()]
    dead = {}
    for v in [t for t in board.GetTracks() if t.Type() == pcbnew.PCB_VIA_T]:
        q = v.GetPosition()
        layers = {t.GetLayer() for t in tracks if t.GetNetCode() == v.GetNetCode() and (t.GetStart() == q or t.GetEnd() == q)}
        for p in pads:
            if p.GetNetCode() == v.GetNetCode() and p.HitTest(q):
                layers.update(l for l in (pcbnew.F_Cu, pcbnew.B_Cu) if p.IsOnLayer(l))
        if not layers and bare(v.GetNetname()) != "GND":     # (only vias touching nothing: a via with a track on one
                                                              #  side may still join a pad through the pour or a mid-track)
            dead[v.m_Uuid.AsString()] = v
    for v in dead.values():
        board.Remove(v)
    return len(dead)


def drop_drc_dangling_vias(board):
    """the signal vias KiCad's DRC itself calls dangling (copper on one layer only, e.g. a via in the middle of a top-layer
    track), never GND (the pours join those); then --clean takes any track left hanging. drop_one_sided_vias only
    removes vias touching nothing: it cannot tell a mid-track joint on the other layer, DRC can"""
    import json
    tmp = board.GetFileName() + ".vias.kicad_pcb"
    board.Save(tmp)
    rpt = tmp + ".json"
    subprocess.run(["nice", "-n", "19", KICAD_CLI, "pcb", "drc", "--format", "json", "-o", rpt, tmp],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    spots = []
    try:
        for v in json.load(open(rpt)).get("violations", []):
            if v.get("type") != "via_dangling":
                continue
            for it in v.get("items", []):
                m = re.search(r"Via \[([^\]]+)\]", it.get("description", ""))
                pos = it.get("pos", {})
                if m and bare(m.group(1)) != "GND" and "x" in pos:
                    spots.append((m.group(1), pos["x"], pos["y"]))
    finally:
        for f in (tmp, rpt):
            if os.path.exists(f):
                os.remove(f)
    dead = {}
    for v in [t for t in board.GetTracks() if t.Type() == pcbnew.PCB_VIA_T]:
        x, y = pcbnew.ToMM(v.GetPosition().x), pcbnew.ToMM(v.GetPosition().y)
        if any(n == v.GetNetname() and abs(x - sx) < 0.01 and abs(y - sy) < 0.01 for n, sx, sy in spots):
            dead[v.m_Uuid.AsString()] = v
    for v in dead.values():
        board.Remove(v)
    return len(dead)


def isolated_gnd_pads(board):
    """GND pads that the filled pours do not reach (KiCad's connectivity, after a fill)"""
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.BuildConnectivity()
    conn = board.GetConnectivity()
    out = []
    for f in board.GetFootprints():
        for p in f.Pads():
            if bare(p.GetNetname()) == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
                if not any(z.Type() == pcbnew.PCB_ZONE_T for z in conn.GetConnectedItems(p)):
                    out.append(p)
    return out


def gnd_pad_vias(board):
    """for each SMD GND pad the pours miss: a GND via 1.0 .. 1.8 mm away in one of 8 directions, joined by a 0.3 mm
    track, wherever both clear every other net by 0.2 mm"""
    gnd = board.FindNet("/GND")
    added = 0
    for p in isolated_gnd_pads(board):
        c = p.GetPosition()
        done = False
        for r in (1.0, 1.3, 1.6, 1.9):
            for k in range(8):
                a = k * 3.14159 / 4
                x, y = c.x + MM(r * __import__("math").cos(a)), c.y + MM(r * __import__("math").sin(a))
                v = pcbnew.PCB_VIA(board)
                v.SetPosition(pcbnew.VECTOR2I(int(x), int(y)))
                v.SetWidth(MM(0.6))
                v.SetDrill(MM(0.3))
                v.SetNet(gnd)
                t = pcbnew.PCB_TRACK(board)
                t.SetStart(c)
                t.SetEnd(pcbnew.VECTOR2I(int(x), int(y)))
                t.SetWidth(MM(0.3))
                t.SetLayer(p.GetLayer() if p.IsOnLayer(pcbnew.F_Cu) else pcbnew.B_Cu)
                t.SetNet(gnd)
                if clear(board, v, own=p) and clear_track(board, t, own=p):
                    board.Add(v)
                    board.Add(t)
                    added += 1
                    done = True
                    break
            if done:
                break
    return added


def clear_track(board, t, own=None, gap=0.2):
    """the track's segment clear of every other net's pads and tracks by `gap` (shape test)"""
    sh = t.GetEffectiveShape()
    for tr in board.GetTracks():
        if bare(tr.GetNetname()) != "GND" and tr.GetEffectiveShape().Collide(sh, MM(gap)):
            return False
    for f in board.GetFootprints():
        for p in f.Pads():
            if p is own or bare(p.GetNetname()) == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
                continue
            for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
                if p.IsOnLayer(layer) and t.GetLayer() == layer and p.GetEffectiveShape(layer).Collide(sh, MM(gap)):
                    return False
    return True


def clear(board, v, gap=0.2, own=None):
    vb = v.GetBoundingBox()
    vb.Inflate(MM(gap))
    if not board.GetBoardEdgesBoundingBox().Contains(vb):
        return False
    for t in board.GetTracks():
        if bare(t.GetNetname()) != "GND" and t.GetBoundingBox().Intersects(vb):
            return False
        if t.Type() == pcbnew.PCB_VIA_T and t.GetBoundingBox().Intersects(vb):
            return False
    for f in board.GetFootprints():
        for p in f.Pads():
            if p is own:
                continue
            pb = p.GetBoundingBox()
            if p.GetDrillSize().x > 0:
                pb.Inflate(MM(0.35))
            if pb.Intersects(vb):
                return False
    for z in board.Zones():
        if z.GetIsRuleArea() and z.Outline().Collide(pcbnew.VECTOR2I(v.GetPosition()), MM(0.5)):
            return False
    return True


def drop_dangling_once(board):
    """tracks with an end that touches nothing of their net (pad, via, other track): one pass"""
    gone = 0
    for _once in (1,):
        ends = {}
        tracks = [t for t in board.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]
        for t in tracks:
            for q in (t.GetStart(), t.GetEnd()):
                ends.setdefault((t.GetNetCode(), q.x, q.y, t.GetLayer()), []).append(t)
        vias = {(v.GetNetCode(), v.GetPosition().x, v.GetPosition().y) for v in board.GetTracks() if v.Type() == pcbnew.PCB_VIA_T}
        pads = [p for f in board.GetFootprints() for p in f.Pads()]
        dead = []
        for t in tracks:
            for q in (t.GetStart(), t.GetEnd()):
                if len(ends[(t.GetNetCode(), q.x, q.y, t.GetLayer())]) > 1 or (t.GetNetCode(), q.x, q.y) in vias:
                    continue
                if any(p.GetNetCode() == t.GetNetCode() and p.IsOnLayer(t.GetLayer()) and p.HitTest(q) for p in pads):
                    continue
                # (a track end lying on another track of its net, mid-segment)
                if any(o is not t and o.GetNetCode() == t.GetNetCode() and o.GetLayer() == t.GetLayer() and o.HitTest(q)
                       for o in tracks):
                    continue
                dead.append(t)
                break
        uniq = {t.m_Uuid.AsString(): t for t in dead}
        for t in uniq.values():
            board.Remove(t)
            gone += 1
    return gone


def widen(board, minw=0.10, gap=0.15):
    """Freerouting's neck-downs under `minw` (JLC 2 layers: 0.10 mm): set to `minw` where that clears every other
    net's copper by `gap`"""
    n = 0
    others = [t for t in board.GetTracks()]
    pads = [p for f in board.GetFootprints() for p in f.Pads()]
    for t in [t for t in board.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T and t.GetWidth() < MM(minw)]:
        old = t.GetWidth()
        t.SetWidth(MM(minw))
        sh = t.GetEffectiveShape()
        bad = any(o.GetNetCode() != t.GetNetCode() and o.IsOnLayer(t.GetLayer()) and o.GetEffectiveShape(t.GetLayer()).Collide(sh, MM(gap))
                  for o in others) or any(p.GetNetCode() != t.GetNetCode() and p.IsOnLayer(t.GetLayer())
                                          and p.GetEffectiveShape(t.GetLayer()).Collide(sh, MM(gap)) for p in pads)
        if bad:
            t.SetWidth(old)
        else:
            n += 1
    return n


def fits(board, item, gap=0.15):
    """item clear of every other net's copper (tracks, vias, pads) by `gap`, on each of its copper layers"""
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        if not item.IsOnLayer(layer):
            continue
        sh = item.GetEffectiveShape(layer)
        for o in board.GetTracks():
            if o is not item and o.GetNetCode() != item.GetNetCode() and o.IsOnLayer(layer) and o.GetEffectiveShape(layer).Collide(sh, MM(gap)):
                return False
        for f in board.GetFootprints():
            for p in f.Pads():
                if p.GetNetCode() != item.GetNetCode() and p.IsOnLayer(layer) and p.GetEffectiveShape(layer).Collide(sh, MM(gap)):
                    return False
    return True


def space_vias(board, minhole=0.5):
    """two vias of different nets whose drills are under `minhole` apart: the second moved away along the line
    between them (its tracks' ends follow)"""
    vias = [v for v in board.GetTracks() if v.Type() == pcbnew.PCB_VIA_T]
    tracks = [t for t in board.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]
    moved = 0
    for i, a in enumerate(vias):
        for b in vias[i + 1:]:
            if a.GetNetCode() == b.GetNetCode():
                continue
            d = (b.GetPosition() - a.GetPosition()).EuclideanNorm()
            gap = pcbnew.ToMM(d) - pcbnew.ToMM(a.GetDrill() + b.GetDrill()) / 2
            if gap >= minhole or d == 0:
                continue
            k = (minhole - gap + 0.03) / pcbnew.ToMM(d)
            dx = int((b.GetPosition().x - a.GetPosition().x) * k)
            dy = int((b.GetPosition().y - a.GetPosition().y) * k)
            for v, sgn in ((b, 1), (a, -1)):              # (move b away from a; if that collides, a away from b)
                old = v.GetPosition()
                new = pcbnew.VECTOR2I(old.x + sgn * dx, old.y + sgn * dy)
                mine = [t for t in tracks if t.GetNetCode() == v.GetNetCode() and (t.GetStart() == old or t.GetEnd() == old)]
                for t in mine:
                    t.SetStart(new) if t.GetStart() == old else t.SetEnd(new)
                v.SetPosition(new)
                if all(fits(board, x) for x in [v] + mine):
                    moved += 1
                    break
                for t in mine:
                    t.SetStart(old) if t.GetStart() == new else t.SetEnd(old)
                v.SetPosition(old)
    return moved


def main():
    """stages, each its own process (a board reloaded inside one process came back broken from SWIG):
      route.py BOARD          pours set aside, Freerouting rounds (GND routed like any net), saved (and .routed.kicad_pcb)
      route.py BOARD --clean  one pass: one-sided signal vias and dangling tracks removed (run until it prints 0)
      route.py BOARD --pours  the pours back (make_board.pours), filled, GND stitching"""
    path = sys.argv[1]
    if "--vias" in sys.argv:
        board = pcbnew.LoadBoard(path)
        nv = drop_one_sided_vias(board)
        board.Save(path)
        print(f"vias: {nv} one-sided signal vias removed")
        return
    if "--drcvias" in sys.argv:
        board = pcbnew.LoadBoard(path)
        nv = drop_drc_dangling_vias(board)
        board.Save(path)
        print(f"drcvias: {nv} signal vias DRC calls dangling removed")
        return
    if "--clean" in sys.argv:
        board = pcbnew.LoadBoard(path)
        nd = drop_dangling_once(board)
        board.Save(path)
        print(f"clean: {nd} dangling tracks")
        return
    if "--fix" in sys.argv:
        board = pcbnew.LoadBoard(path)
        print(f"fix: {widen(board)} neck-downs widened to 0.10 mm; {space_vias(board)} vias moved apart (drills >= 0.5 mm)")
        board.Save(path)
        return
    if "--pours" in sys.argv:
        board = pcbnew.LoadBoard(path)
        import make_board
        make_board.pours(board)
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        gnd = board.FindNet("/GND")

        def add_via(x, y):
            c = make_board.CUT     # (jank-mini-3: no via in the LCD-2's cut-out or within 1 mm of its edge)
            if c and c[0] - 1.0 < x - make_board.OX < c[2] + 1.0 and c[1] - 1.0 < y - make_board.OY < c[3] + 1.0:
                return
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
            v.SetWidth(MM(0.6))
            v.SetDrill(MM(0.3))
            v.SetNet(gnd)
            if clear(board, v):
                board.Add(v)
        bb = board.GetBoardEdgesBoundingBox()
        x0, y0, x1, y1 = (pcbnew.ToMM(v) for v in (bb.GetLeft(), bb.GetTop(), bb.GetRight(), bb.GetBottom()))
        n = stitch(board, add_via, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], spacing=6.0)
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        print(f"stitching: {len(n) if n else 0} GND vias; {unconnected(board)} unconnected after the pours")
        board.Save(path)
        return
    if not os.path.exists(JAR):
        sys.exit(f"no {JAR}: sh fetch_freerouting.sh")
    board = pcbnew.LoadBoard(path)        # (make_board.py adds no pours: removing zones here broke SWIG's lists)
    if FIRST:   # (the wide power tracks on the empty board first, then locked: the rest routes round them)
        left = route_round(board, 0, only=FIRST)
        print(f"round 0 ({', '.join(sorted(FIRST))} first): Freerouting left {left}")
        for t in board.GetTracks():
            t.SetLocked(True)
    for k in range(1, ROUNDS + 1):
        # (round 1 routes everything; later rounds only the nets KiCad still sees open, all others cut to one pin:
        #  re-exported, Freerouting read ~20 of its own round-1 connections as open and rerouted them, 2 -> 26)
        still = None if k == 1 else open_nets(board)
        if still is not None and not still:
            break
        left = route_round(board, k, only=still)
        print(f"round {k}{'' if still is None else ' (' + ', '.join(sorted(still)) + ')'}: Freerouting left {left}")
        if left == 0 and k == 1:
            break
    board.Save(path)
    board.Save(os.path.join(HERE, ".routed.kicad_pcb"))


if __name__ == "__main__":
    main()
