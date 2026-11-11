#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prints the OpenSCAD -D arguments (sw, led) for mini_case.scad from make_board.py's PLACE (SW1, LED1), read as text
so KiCad's Python is not needed. Usage: place_args.py ../make_board.py"""
import re
import sys

src = open(sys.argv[1]).read()
out = []
for ref, var in (("SW1", "sw"), ("LED1", "led")):
    m = re.search(r'"%s":\s*\(([^,]+),\s*([^,]+),' % ref, src)
    if not m:
        sys.exit("place_args: %s not found in PLACE" % ref)
    W = 44.0
    x = eval(m.group(1), {"W": W}) if "W" in m.group(1) else float(m.group(1))
    y = float(m.group(2))
    out.append("%s=[%g,%g]" % (var, x, y))
print(*out)
