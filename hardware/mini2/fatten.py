# SPDX-License-Identifier: GPL-3.0-only
"""After routing and repair.py (72 x 72, stage 2): the supply tracks widened where they have the room. Freerouting
routes VBUS / VUSB at 0.3 mm and +3V3 at 0.25 mm (freeroute.PWR), and necks some +3V3 pieces down to 0.2 mm; ~500 mA
peaks (the radio's bursts) want more (signoff3: the cell's BAT_P / BAT_C / BAT_N too, 1.0 .. 0.5 mm). In steps (0.5, 0.4, 0.3 mm): every narrower track of those nets is set to the
step; KiCad's DRC then names the ones that break a clearance, and those go back to their old width, until the DRC has
none left.
  .../python3.9 fatten.py mini2.kicad_pcb
"""
import os
import re
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
KC = "/opt/homebrew/bin/kicad-cli"
MM, TO = pcbnew.FromMM, pcbnew.ToMM
GROUPS = ((("VBUS", "VUSB", "+3V3"), (0.5, 0.4, 0.3)),
          # (signoff3, round 3 R3: the cell's path, ~150 mohm of 0.4 mm copper: 1 mm wherever the DRC allows)
          (("BAT_P", "BAT_C", "BAT_N"), (1.0, 0.8, 0.6, 0.5)))
RULES = ("clearance", "copper_edge_clearance", "hole_clearance", "tracks_crossing", "shorting_items")


def key(t):
    return (t.GetNetname(), t.GetLayer(), t.GetStart().x, t.GetStart().y, t.GetEnd().x, t.GetEnd().y)


def bad_tracks(path, nets):
    """the (net, x, y) of every supply track the DRC names in a clearance-type violation"""
    rpt = os.path.join(HERE, "fab", "fatten.rpt")
    subprocess.run(["nice", "-n", "19", KC, "pcb", "drc", "-o", rpt, path], capture_output=True)
    out = []
    for block in re.split(r"\n(?=\[)", open(rpt).read()):
        if not any(block.startswith(f"[{r}]") for r in RULES):
            continue
        for x, y, net in re.findall(r"@\(([-\d.]+) mm, ([-\d.]+) mm\): Track \[([^\]]+)\]", block):
            if net in nets:
                out.append((net, float(x), float(y)))
    return out


def fill_save(board, path):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(path)


def step(board, path, nets, width):
    old = {}
    for t in board.GetTracks():
        if t.GetClass() == "PCB_TRACK" and t.GetNetname() in nets and TO(t.GetWidth()) < width - 1e-6:
            old[key(t)] = t.GetWidth()
            t.SetWidth(MM(width))
    fill_save(board, path)
    tried = len(old)
    for _ in range(12):
        bad = bad_tracks(path, nets)
        if not bad:
            break
        back = 0
        for t in board.GetTracks():
            if t.GetClass() == "PCB_TRACK" and key(t) in old and any(
                    n == t.GetNetname() and t.HitTest(pcbnew.VECTOR2I(MM(x), MM(y)), MM(0.05)) for n, x, y in bad):
                t.SetWidth(old.pop(key(t)))
                back += 1
        fill_save(board, path)
        if not back:
            print("fatten: a violation names no widened track; left for make_fab's DRC")
            break
    print(f"fatten: {width} mm: {len(old)} of {tried} tracks widened")


SPLIT = ("BAT_P", "BAT_C", "BAT_N")   # (signoff3: cut into ~1 mm pieces first, so each piece widens where it has room)


def split(board, nets, piece=1.0):
    n = 0
    for t in [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetNetname() in nets]:
        k = int(TO(t.GetLength()) // piece)
        if k < 2:
            continue
        a, b = t.GetStart(), t.GetEnd()
        pts = [pcbnew.VECTOR2I(int(a.x + (b.x - a.x) * j / k), int(a.y + (b.y - a.y) * j / k)) for j in range(k + 1)]
        for j in range(k):
            u = pcbnew.PCB_TRACK(board)
            u.SetStart(pts[j])
            u.SetEnd(pts[j + 1])
            u.SetWidth(t.GetWidth())
            u.SetLayer(t.GetLayer())
            u.SetNet(t.GetNet())
            board.Add(u)
        board.Remove(t)
        GRAVE.append(t)
        n += 1
    return n


GRAVE = []


def main():
    path = sys.argv[1]
    board = pcbnew.LoadBoard(path)
    print(f"fatten: {split(board, SPLIT)} cell-path tracks cut into ~1 mm pieces")
    for nets, steps in GROUPS:
        for width in steps:
            step(board, path, nets, width)
    for net in [n for nets, _s in GROUPS for n in nets]:
        by = {}
        for t in board.GetTracks():
            if t.GetClass() == "PCB_TRACK" and t.GetNetname() == net:
                w = round(TO(t.GetWidth()), 2)
                by[w] = by.get(w, 0) + TO(t.GetLength())
        print(f"fatten: {net}: mm of track by width " + ", ".join(f"{w}: {v:.1f}" for w, v in sorted(by.items(), reverse=True)))


if __name__ == "__main__":
    main()
