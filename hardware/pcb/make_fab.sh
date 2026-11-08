#!/bin/sh
# The jank board: the schematic (make_schematic.py) and its ERC, the board (make_board.py, 84 x 52 mm), its design-rule
# check, the schematic's nets against the board's, the Gerbers and drill files zipped for JLCPCB (bare board, 2 layers),
# a top and bottom render, and the printable mockups (make_mockup.py). make_board.py's square layout is unused.
#   sh make_fab.sh        (from anywhere; needs KiCad 10 (kicad-cli and its Python), python3 and OpenSCAD)
set -eu
cd "$(dirname "$0")"
KC=/opt/homebrew/bin/kicad-cli
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9
python3 make_schematic.py
"$KC" sch erc --severity-all -o fab/erc.rpt jank-pcb.kicad_sch >/dev/null 2>&1 || true
grep -E "ERC messages" fab/erc.rpt
if grep -q "; error" fab/erc.rpt; then grep -B1 "; error" fab/erc.rpt; exit 1; fi
"$KC" sch export netlist --format kicadxml -o fab/jank-pcb.net.xml jank-pcb.kicad_sch >/dev/null 2>&1
"$KC" sch export pdf -o jank-pcb-schematic.pdf jank-pcb.kicad_sch >/dev/null 2>&1

layout() {   # $1: long | square; its files: jank-pcb[-square].kicad_pcb, fab/ or fab/square/, render-*[-square].png
    if [ "$1" = long ]; then T=""; D=fab; else T="-$1"; D=fab/$1; fi
    B=jank-pcb$T
    mkdir -p "$D"
    echo "== $1"
    "$KP" make_board.py "$1" 2>&1 | grep -v "wxApp\|stdpbase" || true
    "$KC" pcb drc --severity-all -o "$D/drc.rpt" "$B.kicad_pcb" >/dev/null 2>&1 || true
    grep -E "Found" "$D/drc.rpt"
    grep -E "^\[" "$D/drc.rpt" | sed 's/:.*//' | sort | uniq -c || true          # (by kind; the errors stop the run)
    if grep -q "; error" "$D/drc.rpt"; then grep -B1 -A3 "; error" "$D/drc.rpt" | head -40; exit 1; fi
    "$KP" check_nets.py "$D/jank-pcb.net.xml" "$D/intended-nets.json" "$B.kicad_pcb" 2>&1 | grep -v "wxApp\|stdpbase\|^  pad"
    rm -rf "$D/gerbers" "$D/$B-gerbers.zip" && mkdir -p "$D/gerbers"
    "$KC" pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
        --no-protel-ext --use-drill-file-origin -o "$D/gerbers/" "$B.kicad_pcb" >/dev/null 2>&1
    "$KC" pcb export drill --format excellon --excellon-separate-th --drill-origin plot -o "$D/gerbers/" "$B.kicad_pcb" >/dev/null 2>&1
    (cd "$D/gerbers" && zip -q -r "../$B-gerbers.zip" .)
    "$KC" pcb render --side top --width 1600 --height 1200 --quality high --zoom 0.95 -o "render-top$T.png" "$B.kicad_pcb" >/dev/null 2>&1
    "$KC" pcb render --side bottom --width 1600 --height 1200 --quality high --zoom 0.95 -o "render-bottom$T.png" "$B.kicad_pcb" >/dev/null 2>&1
    "$KP" make_mockup.py "$1" 2>&1 | grep -v "wxApp\|stdpbase"
    echo "$D/$B-gerbers.zip render-top$T.png render-bottom$T.png mockup$T-din.stl mockup$T-trs.stl"
}
layout long
