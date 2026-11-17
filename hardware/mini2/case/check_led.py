#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The printed lid's LED window must sit where LED1 is: its recess (4.2 mm, mini 2.2) round the board's LED position (the middle of LED1 and LED2) on the roof's outer face
(z 0 in the print, the lid turned over about x: y = -(H - led_y)); a mirror image would put it at W - led_x.
Usage: check_led.py mini_lid_led.stl "<place_args output>"   (exit 1 when the window is not there)"""
import re
import sys

args = dict(re.findall(r"(\w+)=(\[[^\]]*\]|[-\d.]+)", sys.argv[2]))
led = [float(c) for c in args["led"].strip("[]").split(",")]
w, h = float(args["W"]), float(args["H"])
pts = {tuple(round(float(c), 3) for c in ln.split()[1:]) for ln in open(sys.argv[1]) if "vertex" in ln}
face = [(p[0], p[1]) for p in pts if abs(p[2]) < 0.01]


def ring(cx, cy):
    """vertices of the 4.2 mm recess's circle (radius 2.1 +- 0.05) round (cx, cy) on the roof's outer face"""
    return [p for p in face if abs(((p[0] - cx) ** 2 + (p[1] - cy) ** 2) ** 0.5 - 2.1) < 0.05]


here, mirrored = ring(led[0], -(h - led[1])), ring(w - led[0], -(h - led[1]))
print("lid LED window: %d recess vertices at LED1 (%.2f, %.2f), %d at the mirror image" % (len(here), led[0], led[1], len(mirrored)))
sys.exit(0 if len(here) >= 8 and not mirrored else 1)
