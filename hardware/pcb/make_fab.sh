#!/bin/sh
# The jank board, two projects from the same scripts (README): jank-pcb (v5, 84 x 67 mm) and jank-pcb-mod (the modular
# variant, MODULAR=1: CLOCK / RESET outputs on the side walls, 84 x 79.5 mm). For each: the schematic (make_schematic.py)
# and its ERC, the board (make_board.py), its design-rule check, the schematic's nets against the board's, the Gerbers
# and drill files zipped for JLCPCB (2 layers), the JLCPCB assembly files (make_assembly.py: bom.csv, cpl.csv SMD only;
# full/ with the through-hole parts), a top and bottom render, and the printable mockup (make_mockup.py).
# make_board.py's square layout is unused.
#   sh make_fab.sh        (from anywhere; needs KiCad 10 (kicad-cli and its Python), python3 and OpenSCAD)
set -eu
cd "$(dirname "$0")"
KC=/opt/homebrew/bin/kicad-cli
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9

build() {   # $1: "" (v5) or "mod" (MODULAR=1); files: jank-pcb[-mod].*, fab/ or fab/mod/, render-*[-mod].png, mockup[-mod].stl
    if [ -z "$1" ]; then T=""; D=fab; M=0; else T="-$1"; D=fab/$1; M=1; fi
    B=jank-pcb$T
    mkdir -p "$D"
    echo "== $B"
    MODULAR=$M python3 make_schematic.py >/dev/null
    "$KC" sch erc --severity-all -o "$D/erc.rpt" "$B.kicad_sch" >/dev/null 2>&1 || true
    grep -E "ERC messages" "$D/erc.rpt"
    if grep -q "; error" "$D/erc.rpt"; then grep -B1 "; error" "$D/erc.rpt"; exit 1; fi
    "$KC" sch export netlist --format kicadxml -o "$D/$B.net.xml" "$B.kicad_sch" >/dev/null 2>&1
    "$KC" sch export pdf -o "$B-schematic.pdf" "$B.kicad_sch" >/dev/null 2>&1
    MODULAR=$M "$KP" make_board.py long 2>&1 | grep -v "wxApp\|stdpbase" || true
    "$KC" pcb drc --severity-all -o "$D/drc.rpt" "$B.kicad_pcb" >/dev/null 2>&1 || true
    grep -E "Found" "$D/drc.rpt"
    grep -E "^\[" "$D/drc.rpt" | sed 's/:.*//' | sort | uniq -c || true          # (by kind; the errors stop the run)
    if grep -q "; error" "$D/drc.rpt"; then grep -B1 -A3 "; error" "$D/drc.rpt" | head -40; exit 1; fi
    "$KP" check_nets.py "$D/$B.net.xml" "$D/intended-nets.json" "$B.kicad_pcb" 2>&1 | grep -v "wxApp\|stdpbase\|^  pad"
    MODULAR=$M "$KP" make_assembly.py 2>&1 | grep -v "wxApp\|stdpbase"            # (JLCPCB assembly: bom.csv, cpl.csv, full/)
    rm -rf "$D/gerbers" "$D/$B-gerbers.zip" && mkdir -p "$D/gerbers"
    "$KC" pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
        --no-protel-ext --use-drill-file-origin -o "$D/gerbers/" "$B.kicad_pcb" >/dev/null 2>&1
    "$KC" pcb export drill --format excellon --excellon-separate-th --drill-origin plot -o "$D/gerbers/" "$B.kicad_pcb" >/dev/null 2>&1
    (cd "$D/gerbers" && zip -q -r "../$B-gerbers.zip" .)
    "$KC" pcb render --side top --width 1600 --height 1200 --quality high --zoom 0.95 -o "render-top$T.png" "$B.kicad_pcb" >/dev/null 2>&1
    "$KC" pcb render --side bottom --width 1600 --height 1200 --quality high --zoom 0.95 -o "render-bottom$T.png" "$B.kicad_pcb" >/dev/null 2>&1
    MODULAR=$M "$KP" make_mockup.py 2>&1 | grep -v "wxApp\|stdpbase"
    echo "$D/$B-gerbers.zip $D/bom.csv $D/cpl.csv render-top$T.png render-bottom$T.png mockup$T.stl"
}
# build ""   (the plain variant is parked since the 23 mm output pitch, 2026-11-09: its OUT4 groups land on the encoder;
#             the user builds the modular one. Its files stay as last generated.)
build mod
