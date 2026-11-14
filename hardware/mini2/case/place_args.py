#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prints the OpenSCAD -D arguments for mini_case.scad from the finished board (mini2.kicad_pcb), read as text so KiCad's
Python is not needed; the board, not make_board.py, so every part comes out where it is.
mini 2.2: btn (the four buttons SW3 UP, SW1 NO / BOOT, SW4 DOWN, SW5 YES; stage 1f: no RST), led (the middle of LED1 and LED2:
one window), hdr (J10, the screen's header), j4 (the u.FL), ph (J8, J9, the cells' sockets, on the bottom), W, H, din_y
(J5's axis).
Usage: place_args.py ../mini2.kicad_pcb"""
import math
import re
import sys

OX = OY = 100.0                # (make_board.py's page origin of the board)
src = open(sys.argv[1]).read()


def block(ref):
    m = re.search(r'\(footprint "[^"]*"(?:(?!\(footprint ).)*?\(property "Reference" "%s".*?(?=\n\t\(footprint |\Z)' % re.escape(ref), src, re.S)
    if not m:
        sys.exit("place_args: %s not found in the board" % ref)
    return m.group(0)


def at(ref):
    """the footprint's (at x y)"""
    m = re.search(r'\(at ([-\d.]+) ([-\d.]+)', block(ref))
    return float(m.group(1)) - OX, float(m.group(2)) - OY


def lens(ref):
    """the middle of its pads (the footprint's origin need not be there), turned with the footprint"""
    x0, y0 = at(ref)
    blk = block(ref)
    rot = re.search(r'\(at [-\d.]+ [-\d.]+(?: ([-\d.]+))?\)', blk)
    a = math.radians(float(rot.group(1) or 0)) if rot else 0.0
    pads = [(float(m.group(1)), float(m.group(2))) for m in re.finditer(r'\(pad "[^"]*" smd[^\n]*\n\s*\(at ([-\d.]+) ([-\d.]+)', blk)]
    if not pads:
        return x0, y0
    mx, my = sum(p[0] for p in pads) / len(pads), sum(p[1] for p in pads) / len(pads)
    return x0 + mx * math.cos(a) + my * math.sin(a), y0 - mx * math.sin(a) + my * math.cos(a)


def vec(ps):
    return "[" + ",".join("[%g,%g]" % p for p in ps) + "]"


l1, l2 = lens("LED1"), lens("LED2")
out = ["btn=" + vec([at(r) for r in ("SW3", "SW1", "SW4", "SW5")]),
       "led=[%g,%g]" % ((l1[0] + l2[0]) / 2, (l1[1] + l2[1]) / 2),
       "hdr=[%g,%g]" % at("J10"),
       "j4=[%g,%g]" % at("J4"),
       "ph=" + vec([at(r) for r in ("J8", "J9")])]
xs, ys = [], []
EDGE = (r'\(gr_(?:line|rect)\s*\(start ([-\d.]+) ([-\d.]+)\)\s*\(end ([-\d.]+) ([-\d.]+)\)'
        r'(?:(?!\(gr_).)*?\(layer "Edge\.Cuts"\)')
for m in re.finditer(EDGE, src, re.S):
    xs += [float(m.group(1)), float(m.group(3))]
    ys += [float(m.group(2)), float(m.group(4))]
if not xs:
    sys.exit("place_args: no Edge.Cuts outline in the board")
out.append("W=%g" % (max(xs) - min(xs)))
out.append("H=%g" % (max(ys) - min(ys)))
out.append("din_y=%g" % at("J5")[1])
print(*out)
