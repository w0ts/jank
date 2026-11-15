# SPDX-License-Identifier: GPL-3.0-only
"""LINE IN's summing node and output (LIN_SUM, LIN_OUT: the op-amp's high-impedance input and its feedback) kept clear of
the digital nets (review pass 1, LOW 1: UDN ran 0.35 mm from it for 4.9 mm, SDA 0.38 mm for 4.1 mm, MIDI_TX2 beside
LIN_OUT). The rule: no other net's track within KEEP mm (edge to edge, the same layer) of their copper for more than
RUN mm in all. `close_runs` measures it (check_board.py reports it); `rip` removes the offending nets' tracks near them,
so repair.py joins those nets again with the keep-out in its grid.
  .../python3.9 lin_keep.py mini2.kicad_pcb          (rip; then repair.py)
"""
import sys

import pcbnew

KEEP_NETS = ("LIN_SUM", "LIN_OUT")
QUIET = ("GND", "AVDD", "LINE_IN")              # (and every LIN_*: the front end's own nets may run beside it)
KEEP, RUN, STEP = 1.0, 2.0, 0.1
MM, TO = pcbnew.FromMM, pcbnew.ToMM


def quiet(net):
    return not net or net in QUIET or net.startswith("LIN_")


def keep_items(board):
    """(layer, shape, bbox) of LIN_SUM / LIN_OUT's tracks and pads"""
    out = []
    for t in board.GetTracks():
        if t.GetNetname() in KEEP_NETS and t.GetClass() == "PCB_TRACK":
            out.append((t.GetLayer(), t.GetEffectiveShape(t.GetLayer()), t.GetBoundingBox()))
    for f in board.GetFootprints():
        for p in f.Pads():
            if p.GetNetname() in KEEP_NETS:
                for L in (pcbnew.F_Cu, pcbnew.B_Cu):
                    if p.IsOnLayer(L):
                        out.append((L, p.GetEffectiveShape(L), p.GetBoundingBox()))
    return out


def close_runs(board):
    """{net: (mm of its tracks within KEEP of the keep nets' copper, [those tracks])}"""
    items = keep_items(board)
    grow = MM(KEEP + 1.0)
    out = {}
    for t in board.GetTracks():
        n = t.GetNetname()
        if t.GetClass() != "PCB_TRACK" or quiet(n):
            continue
        tb = t.GetBoundingBox()
        tb.Inflate(grow)
        near = [s for L, s, b in items if L == t.GetLayer() and tb.Intersects(b)]
        if not near:
            continue
        a, e = t.GetStart(), t.GetEnd()
        L = TO(t.GetLength())
        k = max(1, int(L / STEP))
        hw = t.GetWidth() // 2
        close = 0
        for i in range(k):
            u = (i + 0.5) / k
            c = pcbnew.SHAPE_CIRCLE(pcbnew.VECTOR2I(int(a.x + (e.x - a.x) * u), int(a.y + (e.y - a.y) * u)), hw)
            if any(s.Collide(c, MM(KEEP)) for s in near):
                close += 1
        if close:
            run, tracks = out.get(n, (0.0, []))
            out[n] = (run + close * L / k, tracks + [t])
    return out


FIXED = ("UDP", "UDN")         # (the drawn USB pair, make_board.py: never ripped; LIN's own tracks beside it are)


def rip(board):
    """every track of a net that runs more than RUN mm in all within KEEP of the keep nets: removed (repair.py joins
    the pieces again, round the keep-out); beside the drawn USB pair, the keep nets' own tracks near it go instead.
    Returns {net: mm}"""
    bad = {n: (run, ts) for n, (run, ts) in close_runs(board).items() if run > RUN}
    gone = {}
    key = lambda t: (t.GetNetname(), t.GetLayer(), t.GetStart().x, t.GetStart().y, t.GetEnd().x, t.GetEnd().y)
    for n, (_run, ts) in bad.items():
        if n not in FIXED:
            gone.update({key(t): t for t in ts})
            continue
        shapes = [(t.GetLayer(), t.GetEffectiveShape(t.GetLayer())) for t in ts]
        for k in board.GetTracks():
            if k.GetNetname() in KEEP_NETS and k.GetClass() == "PCB_TRACK" and any(
                    L == k.GetLayer() and sh.Collide(k.GetEffectiveShape(L), MM(KEEP)) for L, sh in shapes):
                gone[key(k)] = k
    for t in gone.values():
        board.Remove(t)
    return {n: run for n, (run, _ts) in bad.items()}


def main():
    path = sys.argv[1]
    board = pcbnew.LoadBoard(path)
    gone = rip(board)
    print("lin_keep: " + (", ".join(f"{n} {r:.1f} mm" for n, r in sorted(gone.items())) + " ripped near LIN_SUM / LIN_OUT"
                          if gone else "nothing within 1 mm of LIN_SUM / LIN_OUT for more than 2 mm"))
    board.Save(path)


if __name__ == "__main__":
    main()
