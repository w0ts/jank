"""jank-mini-3 study helper (KiCad's python): per net, which regions its pads sit in (TOP band above the cut-out,
BOT band below it, RIGHT part x > cut-out), so the nets that must go round the LCD-2 show up.
Usage: python3 crossers.py <board.kicad_pcb>"""
import collections
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import make_board as M  # noqa: E402

OX = OY = 100.0


def region(x: float, y: float) -> str:
    if x > M.CUT[2]:
        return "RIGHT"
    return "TOP" if y < M.CUT[1] else "BOT"


def main(path: str) -> None:
    board = pcbnew.LoadBoard(path)
    nets = collections.defaultdict(list)
    for fp in board.GetFootprints():
        for p in fp.Pads():
            n = p.GetNetname().lstrip("/")
            if not n or n.startswith("unconnected") or n == "GND":
                continue
            pos = p.GetPosition()
            nets[n].append((fp.GetReference(), region(pcbnew.ToMM(pos.x) - OX, pcbnew.ToMM(pos.y) - OY)))
    cross = []
    for n, pads in sorted(nets.items()):
        regs = sorted({r for _, r in pads})
        if "TOP" in regs and "BOT" in regs:
            cross.append(n)
            refs = collections.defaultdict(list)
            for ref, r in pads:
                refs[r].append(ref)
            print(f"CROSS {n}: " + "  ".join(f"{r}={','.join(sorted(set(v)))}" for r, v in sorted(refs.items())))
    print(f"{len(cross)} nets cross between the top and bottom bands (they must go round the right of the LCD-2)")
    for band in ("TOP", "BOT", "RIGHT"):
        k = sum(1 for pads in nets.values() if {r for _, r in pads} == {band})
        print(f"nets only in {band}: {k}")


if __name__ == "__main__":
    main(sys.argv[1])
