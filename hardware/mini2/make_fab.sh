#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# jank chip, everything in order (README "Files"): the schematic and its ERC, the netlist and the PDF; the board (RF
# path drawn, the rest routed by Freerouting: freeroute.py), its DRC, the nets against the schematic, the Gerbers and drill files zipped
# for JLCPCB, the assembly files, the renders (with the parts' 3D models) and the mockup; then the cost and the case.
#   sh hardware/chip/make_fab.sh            (KiCad 10: kicad-cli and its Python; python3; OpenSCAD)
#   ROUTE=0 sh hardware/chip/make_fab.sh    (keep the board as it is: no placement or routing, the rest again)
set -eu
cd "$(dirname "$0")"
KC=/opt/homebrew/bin/kicad-cli
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9
Q() { grep -v "wxApp\|stdpbase\|Fontconfig" || true; }

python3 make_schematic.py
"$KC" sch erc --severity-all -o fab/erc.rpt mini2.kicad_sch 2>&1 | Q >/dev/null
grep -E "ERC messages" fab/erc.rpt
if grep -q "; error" fab/erc.rpt; then grep -B1 "; error" fab/erc.rpt; exit 1; fi
"$KC" sch export netlist --format kicadxml -o fab/mini2.net.xml mini2.kicad_sch 2>&1 | Q >/dev/null
"$KC" sch export pdf -o mini2-schematic.pdf mini2.kicad_sch 2>&1 | Q >/dev/null

if [ "${ROUTE:-1}" = 1 ]; then       # (the placement, the drawn RF path and lanes; Freerouting the rest: README "Routing")
    LANE_VIAS=1 NO_STITCH=1 PLACE_ONLY=1 "$KP" make_board.py 2>&1 | Q
    sh fetch_freerouting.sh >/dev/null
    "$KP" freeroute.py mini2.kicad_pcb 2>&1 | Q
    # (mini 2.2: route_fixups.py, mini 2's fix-ups on its own routing, is gone: none applies to a fresh route)
fi
"$KP" tie_groups.py mini2.kicad_pcb 2>&1 | Q
"$KC" pcb drc --severity-all -o fab/drc.rpt mini2.kicad_pcb 2>&1 | Q >/dev/null
grep -E "Found" fab/drc.rpt
grep -E "^\[" fab/drc.rpt | sed 's/:.*//' | sort | uniq -c || true
if grep -q "; error" fab/drc.rpt; then grep -B1 -A3 "; error" fab/drc.rpt | head -40; exit 1; fi
"$KP" check_nets.py fab/mini2.net.xml fab/intended-nets.json mini2.kicad_pcb 2>&1 | Q
"$KP" make_assembly.py 2>&1 | Q
rm -rf fab/gerbers fab/mini2-gerbers.zip && mkdir -p fab/gerbers
"$KC" pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
    --no-protel-ext --use-drill-file-origin -o fab/gerbers/ mini2.kicad_pcb 2>&1 | Q >/dev/null
"$KC" pcb export drill --format excellon --excellon-separate-th --drill-origin plot -o fab/gerbers/ mini2.kicad_pcb 2>&1 | Q >/dev/null
(cd fab/gerbers && zip -q -r ../mini2-gerbers.zip .)
rm -rf fab/gerbers
"$KC" pcb render --side top --width 1400 --height 1400 --quality high --zoom 0.9 -o render-top.png mini2.kicad_pcb 2>&1 | Q >/dev/null
"$KC" pcb render --side bottom --width 1400 --height 1400 --quality high --zoom 0.9 -o render-bottom.png mini2.kicad_pcb 2>&1 | Q >/dev/null
"$KC" pcb render --width 1600 --height 1200 --quality high --perspective --rotate "-40,0,-25" --zoom 0.85 -o render-3d.png mini2.kicad_pcb 2>&1 | Q >/dev/null
"$KP" make_mockup.py 2>&1 | Q
echo "fab/mini2-gerbers.zip fab/usb/{bom,cpl}.csv fab/battery/{bom,cpl}.csv render-top.png render-bottom.png render-3d.png mockup.stl"
python3 cost.py
sh case/make_stl.sh
