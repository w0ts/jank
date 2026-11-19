#!/bin/sh
# box.scad's pictures (run from hardware/jank-mini-3/case, at nice 19): the assembly, exploded, the top face, the
# inside (lid + board, the shell off), two sections: x through J5's DIN and the D-pad (seen from +x), y = 41 through
# the D-pad centre, the screen and the LCD-2's USB-C (seen from the bottom wall, the top half kept); the left wall from
# outside (the power switch); a section y = psw_y through the power switch (seen from the bottom wall, top half kept)
set -e
o() { nice -n 19 openscad --backend=manifold --render --colorscheme=Tomorrow "$@" box.scad 2>&1 | grep -iE "error|warning" || true; }
o --imgsize=1600,1200 -D 'part="assembly"' --projection=perspective --camera=-75,-200,150,48,-41,-8 -o box-assembly.png
o --imgsize=1600,1200 -D 'part="exploded"' --projection=perspective --camera=-170,-370,215,48,-41,6 -o box-exploded.png
o --imgsize=1600,1300 -D 'part="top"' --projection=ortho --camera=0,0,0,0,0,0,0 --viewall --autocenter -o box-top.png
o --imgsize=1600,1200 -D 'part="inside"' --projection=perspective --camera=-90,-250,190,48,-41,-14 -o box-inside.png
o --imgsize=1600,700 -D 'part="section_din"' --projection=ortho --camera=41,-41,-9,90,0,90,135 -o box-section-din.png
o --imgsize=1600,700 -D 'part="section_screen"' --projection=ortho --camera=48,-41,-9,90,0,0,150 -o box-section-screen.png
o --imgsize=1600,800 -D 'part="left_wall"' --projection=ortho --camera=-2,-41,-9,90,0,270,125 -o box-left-wall.png
o --imgsize=1600,700 -D 'part="section_switch"' --projection=ortho --camera=14,-72,-9,90,0,0,80 -o box-section-switch.png
