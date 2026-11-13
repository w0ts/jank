#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prints the OpenSCAD -D arguments (sw, sw2, led, W, H, din_y) for mini_case.scad from the finished board (mini2.kicad_pcb:
SW1's, SW2's and LED1's places, the outline's size, J5's axis), read as text so KiCad's Python is not needed; the board,
not make_board.py, so the parts the placer moved (make_board.py's PLACED) come out where they are.
Usage: place_args.py ../mini2.kicad_pcb"""
import math
import re
import sys

OX = OY = 100.0                # (make_board.py's page origin of the board)
src = open(sys.argv[1]).read()


def at(ref):
    """the footprint's (at x y): the line after its (footprint ...) header, before its Reference"""
    m = re.search(r'\(footprint "[^"]*"\s*\(layer "[^"]*"\)\s*\(uuid "[^"]*"\)\s*\(at ([-\d.]+) ([-\d.]+)[^)]*\)'
                  r'(?:(?!\(footprint ).)*?\(property "Reference" "%s"' % re.escape(ref), src, re.S)
    if not m:
        sys.exit("place_args: %s not found in the board" % ref)
    return float(m.group(1)) - OX, float(m.group(2)) - OY


def lens(ref):
    """the LED's light: the middle of its pads (the footprint's origin need not be there), turned with the footprint"""
    x0, y0 = at(ref)
    blk = re.search(r'\(footprint "[^"]*"(?:(?!\(footprint ).)*?\(property "Reference" "%s".*?(?=\n\t\(footprint |\Z)' % re.escape(ref), src, re.S).group(0)
    rot = re.search(r'\(at [-\d.]+ [-\d.]+(?: ([-\d.]+))?\)', blk)
    a = math.radians(float(rot.group(1) or 0)) if rot else 0.0
    pads = [(float(m.group(1)), float(m.group(2))) for m in re.finditer(r'\(pad "[^"]*" smd[^\n]*\n\s*\(at ([-\d.]+) ([-\d.]+)', blk)]
    if not pads:
        return x0, y0
    mx, my = sum(p[0] for p in pads) / len(pads), sum(p[1] for p in pads) / len(pads)
    return x0 + mx * math.cos(a) + my * math.sin(a), y0 - mx * math.sin(a) + my * math.cos(a)


out = ["%s=[%g,%g]" % ((var,) + (lens if ref == "LED1" else at)(ref)) for ref, var in (("SW1", "sw"), ("SW2", "sw2"), ("LED1", "led"))]
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
