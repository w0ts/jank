#!/bin/sh
# one picture per wall of the tray with the placed board inside, seen from a little above (run from hardware/jank-mini-3/case;
# needs ../.t/board.stl: kicad-cli pcb export stl -o ../.t/board.stl <board>)
set -e
W=93; H=62.6; cx=$(echo "$W/2" | bc -l); cy=$(echo "$H/2" | bc -l)
o() { nice -n 19 openscad --projection=perspective --imgsize=1600,800 --colorscheme=Tomorrow --camera=$1,$cx,$cy,12 -o "$2" views.scad 2>&1 | grep -iE "error" || true; }
o "$cx,$(echo "$H+125" | bc),45" ../.t/wall-top.png
o "$cx,-125,45" ../.t/wall-bottom.png
o "$(echo "$W+120" | bc),$cy,45" ../.t/wall-right.png
o "-120,$cy,45" ../.t/wall-left.png
# the whole unit without the lid, from the front-left above; and the board + LCD-2 + cell without the tray, from the side
nice -n 19 openscad --projection=perspective --imgsize=1600,1000 --colorscheme=Tomorrow --camera=-40,-70,110,$cx,$cy,10 -o ../.t/unit.png views.scad 2>&1 | grep -iE "error" || true
nice -n 19 openscad --projection=ortho --imgsize=1600,700 --colorscheme=Tomorrow -D show_tray=false --camera=-75,-20,22,40,31,10 -o ../.t/side-cell.png views.scad 2>&1 | grep -iE "error" || true
