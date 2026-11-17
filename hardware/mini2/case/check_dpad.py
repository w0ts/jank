#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The D-pad's tilt (stage 2, README "Buttons"): the cap rests on its centre pivot (a point on the board) and turns about
it when an arm is pressed. From mini_case.scad's figures (read from the file): the travel a full press gives each
switch, that a press anywhere along an arm (near its tip, near the centre) presses that switch and not its neighbours
(up to a sideways offset, reported), that the cap's top stays inside its hole and the flange under the roof. A point at
height h over the pivot and distance p from it along the press, turned by t, drops by h (1 - cos t) + p sin t and moves
h sin t toward the press. The switch: TS-1187A-B-A-B, travel 0.2 +- 0.1 to the click, 0.25 (its drawing).
  python3 hardware/mini2/case/check_dpad.py        (exit 1 on a failed check)
"""
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = open(os.path.join(HERE, "mini_case.scad")).read()


def num(name):
    m = re.search(r"\b%s\s*=\s*([-\d.]+)\s*;" % re.escape(name), SRC)
    if not m:
        sys.exit("check_dpad: %s not found in mini_case.scad" % name)
    return float(m.group(1))


bt, roof = num("bt"), num("roof")
z_in = bt + 11.0                                  # (mini_case.scad: z_in = bt + 11.0)
zt = z_in + roof
arm_w, arm_l = num("arm_w"), num("arm_l")
gap_side, gap_end, fl_gap = num("gap_side"), num("gap_end"), num("fl_gap")
boss_r, boss_gap, proud = num("boss_r"), num("boss_gap"), num("proud")
sw_top = bt + 1.5
TRAVEL_CLICK, TRAVEL_FULL = 0.3, 0.25            # (0.2 + 0.1: the click at the latest; the full travel)
need = boss_gap + TRAVEL_FULL                     # the boss's drop that bottoms its switch
h_boss = sw_top + boss_gap - bt                   # the boss's foot over the pivot point
h_top = zt + proud - bt                           # the cap's top over the pivot point


def drop(h, p, t):
    return h * (1 - math.cos(t)) + p * math.sin(t)


def tilt_for(p_along):
    """the tilt at which a boss at p_along (along the press) has dropped `need`"""
    lo, hi = 0.0, 0.5
    for _ in range(60):
        mid = (lo + hi) / 2
        if drop(h_boss, p_along, mid) < need:
            lo = mid
        else:
            hi = mid
    return hi


fail = 0
out = ["D-pad tilt (check_dpad.py): arm %.1f wide, tip at %.1f, bosses at %.1f, %.2f mm over the actuators; the cap's "
       "top %.1f mm over the pivot" % (arm_w, arm_l, boss_r, boss_gap, h_top)]
t_full = tilt_for(boss_r)
shift = h_top * math.sin(t_full)
rise = -min(drop(fl_h, -arm_l, t_full) for fl_h in (z_in - fl_gap - bt,))
out.append("a full press (boss down %.2f: the gap + 0.25 travel): tilt %.2f deg; the cap's top moves %.2f mm sideways "
           "(hole gaps: %.2f sides, %.2f tips); the far flange rises %.2f mm (gap under the roof %.2f)"
           % (need, math.degrees(t_full), shift, gap_side, gap_end, rise, fl_gap))
if shift > min(gap_side, gap_end) - 0.05:
    out.append("  FAILED: the cap's top would hit its hole")
    fail = 1
if rise > fl_gap - 0.1:
    out.append("  FAILED: the flange would hit the roof before the switch bottoms")
    fail = 1
# presses along the arm (s from the centre), sideways offset l: the press direction u = (s, l) / |(s, l)|; the pressed
# boss lies at (boss_r, 0): its distance along u = boss_r s / r; a neighbour at (0, boss_r): boss_r l / r
out.append("presses on one arm (s: from the centre; l: sideways): the tilt that bottoms the pressed switch, the "
           "neighbour's drop (it clicks at %.2f + 0.1 .. 0.3)" % boss_gap)
for s in (1.5, 3.0, 5.0, 6.5, 8.0):
    row = []
    for l in (0.0, 0.5, 1.0, 1.5, 2.0):
        r = math.hypot(s, l)
        t = tilt_for(boss_r * s / r)
        nb = drop(h_boss, boss_r * l / r, t)
        ok = nb < boss_gap
        row.append("l %.1f: %s %.2f" % (l, "ok" if ok else "BOTH", nb))
        if l <= 0.5 and not ok:
            fail = 1
    out.append("  s %.1f: " % s + ", ".join(row))
out.append("on the arm's centre line every press tilts toward that arm only (the neighbours lie on the turning axis: "
           "they drop %.3f, the pivot's height effect); a press s from the centre needs %.1f x the switch's force at the "
           "tip's 8.0 / s lever" % (drop(h_boss, 0, t_full), boss_r / 3.0))
print("\n".join(out))
print("check_dpad:", "FAILED" if fail else "ok")
sys.exit(fail)
