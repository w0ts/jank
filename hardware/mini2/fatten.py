# SPDX-License-Identifier: GPL-3.0-only
"""After routing and repair.py (72 x 72, stage 2): the supply tracks widened where they have the room. Freerouting
routes VBUS / VUSB at 0.3 mm and +3V3 at 0.25 mm (freeroute.PWR), and necks some +3V3 pieces down to 0.2 mm; ~500 mA
peaks (the radio's bursts) want more. In steps (0.5, 0.4, 0.3 mm): every narrower track of those nets is set to the
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
NETS = ("VBUS", "VUSB", "+3V3")
STEPS = (0.5, 0.4, 0.3)
RULES = ("clearance", "copper_edge_clearance", "hole_clearance", "tracks_crossing", "shorting_items")


def key(t):
    return (t.GetNetname(), t.GetLayer(), t.GetStart().x, t.GetStart().y, t.GetEnd().x, t.GetEnd().y)


def bad_tracks(path):
    """the (net, x, y) of every supply track the DRC names in a clearance-type violation"""
    rpt = os.path.join(HERE, "fab", "fatten.rpt")
    subprocess.run(["nice", "-n", "19", KC, "pcb", "drc", "-o", rpt, path], capture_output=True)
    out = []
    for block in re.split(r"\n(?=\[)", open(rpt).read()):
        if not any(block.startswith(f"[{r}]") for r in RULES):
            continue
        for x, y, net in re.findall(r"@\(([-\d.]+) mm, ([-\d.]+) mm\): Track \[([^\]]+)\]", block):
            if net in NETS:
                out.append((net, float(x), float(y)))
    return out


def fill_save(board, path):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(path)


def step(board, path, width):
    old = {}
    for t in board.GetTracks():
        if t.GetClass() == "PCB_TRACK" and t.GetNetname() in NETS and TO(t.GetWidth()) < width - 1e-6:
            old[key(t)] = t.GetWidth()
            t.SetWidth(MM(width))
    fill_save(board, path)
    tried = len(old)
    for _ in range(12):
        bad = bad_tracks(path)
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


def main():
    path = sys.argv[1]
    board = pcbnew.LoadBoard(path)
    for width in STEPS:
        step(board, path, width)
    for net in NETS:
        by = {}
        for t in board.GetTracks():
            if t.GetClass() == "PCB_TRACK" and t.GetNetname() == net:
                w = round(TO(t.GetWidth()), 2)
                by[w] = by.get(w, 0) + TO(t.GetLength())
        print(f"fatten: {net}: mm of track by width " + ", ".join(f"{w}: {v:.1f}" for w, v in sorted(by.items(), reverse=True)))


if __name__ == "__main__":
    main()
