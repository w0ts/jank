#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# jank tiny, everything in order (README "Files"): the schematic and its ERC, the netlist and the PDF; then for each
# variant (tiny: two-sided, the board; tiny-1s: one-sided, for the cost comparison) the board (routed by route.py), its
# DRC, the nets against the schematic, the Gerbers and drill files zipped for JLCPCB, the assembly files, renders and the
# mockup; then the cost estimate and the case.
#   sh hardware/tiny/make_fab.sh            (KiCad 10: kicad-cli and its Python; python3; OpenSCAD)
#   ROUTE=0 sh hardware/tiny/make_fab.sh    (keep the boards as they are: no placement or routing, the rest again)
set -eu
cd "$(dirname "$0")"
KC=/opt/homebrew/bin/kicad-cli
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9
Q() { grep -v "wxApp\|stdpbase\|Fontconfig" || true; }

python3 make_schematic.py
"$KC" sch erc --severity-all -o fab/erc.rpt tiny.kicad_sch 2>&1 | Q >/dev/null
grep -E "ERC messages" fab/erc.rpt
if grep -q "; error" fab/erc.rpt; then grep -B1 "; error" fab/erc.rpt; exit 1; fi
"$KC" sch export netlist --format kicadxml -o fab/tiny.net.xml tiny.kicad_sch 2>&1 | Q >/dev/null
"$KC" sch export pdf -o tiny-schematic.pdf tiny.kicad_sch 2>&1 | Q >/dev/null

build() {   # $1: 2s or 1s
    if [ "$1" = 2s ]; then B=tiny; D=fab; else B=tiny-1s; D=fab/1s; fi
    echo "== $B"
    mkdir -p "$D"
    cp tiny.kicad_pro "$B.kicad_pro" 2>/dev/null || true
    [ "$B" = tiny ] || cp tiny.kicad_dru "$B.kicad_dru"
    if [ "${ROUTE:-1}" = 1 ]; then VARIANT=$1 "$KP" make_board.py 2>&1 | Q; fi
    "$KC" pcb drc --severity-all -o "$D/drc.rpt" "$B.kicad_pcb" 2>&1 | Q >/dev/null
    grep -E "Found" "$D/drc.rpt"
    grep -E "^\[" "$D/drc.rpt" | sed 's/:.*//' | sort | uniq -c || true
    if grep -q "; error" "$D/drc.rpt"; then grep -B1 -A3 "; error" "$D/drc.rpt" | head -40; exit 1; fi
    "$KP" check_nets.py fab/tiny.net.xml fab/intended-nets.json "$B.kicad_pcb" 2>&1 | Q
    VARIANT=$1 "$KP" make_assembly.py 2>&1 | Q
    rm -rf "$D/gerbers" "$D/$B-gerbers.zip" && mkdir -p "$D/gerbers"
    "$KC" pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
        --no-protel-ext --use-drill-file-origin -o "$D/gerbers/" "$B.kicad_pcb" 2>&1 | Q >/dev/null
    "$KC" pcb export drill --format excellon --excellon-separate-th --drill-origin plot -o "$D/gerbers/" "$B.kicad_pcb" 2>&1 | Q >/dev/null
    (cd "$D/gerbers" && zip -q -r "../$B-gerbers.zip" .)
    rm -rf "$D/gerbers"
    T=""; [ "$B" = tiny ] || T="-1s"
    "$KC" pcb render --side top --width 1400 --height 1400 --quality high --zoom 0.9 -o "render-top$T.png" "$B.kicad_pcb" 2>&1 | Q >/dev/null
    "$KC" pcb render --side bottom --width 1400 --height 1400 --quality high --zoom 0.9 -o "render-bottom$T.png" "$B.kicad_pcb" 2>&1 | Q >/dev/null
    VARIANT=$1 "$KP" make_mockup.py 2>&1 | Q
    echo "$D/$B-gerbers.zip $D/bom.csv $D/cpl.csv render-top$T.png render-bottom$T.png"
}
build 2s
build 1s
python3 cost.py
sh case/make_stl.sh
