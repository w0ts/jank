#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# the case STLs and a preview from case.scad (OpenSCAD); low priority (shared Mac)
set -eu
cd "$(dirname "$0")"
for p in tray lid plungers; do nice -n 19 openscad -q -D "part=\"$p\"" -o "case_$p.stl" case.scad; done
nice -n 19 openscad -q -D "part=\"assembly\"" --render --imgsize=1400,1000 --camera=50,32,20,55,0,25,260 --colorscheme=Tomorrow -o case-preview.png case.scad
ls -la case_*.stl case-preview.png
