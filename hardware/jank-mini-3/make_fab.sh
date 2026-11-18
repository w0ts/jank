#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# jank LCD-2 add-on, everything in order: the schematic (ERC, netlist, net check, PDF), the board (make_route.sh:
# placement, Freerouting, pours, silk; ROUTE=0 keeps the routed board), DRC with schematic parity, check_board, the
# JLC files (Gerbers + drill zip, BOM, CPL), the renders. KiCad 10 (kicad-cli and its Python), java, python3.
#   sh hardware/jank-mini-3/make_fab.sh            ROUTE=0 sh hardware/jank-mini-3/make_fab.sh
set -eu
cd "$(dirname "$0")"
KC=/opt/homebrew/bin/kicad-cli
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9
PY=/opt/homebrew/bin/python3.14
Q() { grep -v -i "wxapp\|fontconfig\|stdpbase\|Debug:\|memory leak" || true; }
LOW="nice -n 19 taskpolicy -b"
mkdir -p fab
$PY make_schematic.py
"$KC" sch erc --severity-all -o fab/erc.rpt lcd2.kicad_sch 2>&1 | Q >/dev/null
grep -E "ERC messages" fab/erc.rpt
if grep -q "; error" fab/erc.rpt; then exit 1; fi
"$KC" sch export netlist --format kicadxml -o fab/lcd2.net.xml lcd2.kicad_sch 2>&1 | Q >/dev/null
$PY check_nets.py fab/lcd2.net.xml fab/intended-nets.json
"$KC" sch export pdf -o lcd2-schematic.pdf lcd2.kicad_sch 2>&1 | Q >/dev/null
if [ "${ROUTE:-1}" = 1 ]; then sh make_route.sh; fi
$LOW "$KC" pcb drc --severity-all --schematic-parity -o fab/drc.rpt lcd2.kicad_pcb 2>&1 | Q >/dev/null
grep -E "Found" fab/drc.rpt
if grep -q "; error\|; warning" fab/drc.rpt; then grep -B1 -A3 "; error\|; warning" fab/drc.rpt | head -30; exit 1; fi
"$KP" check_board.py lcd2.kicad_pcb 2>&1 | Q
"$KP" make_assembly.py 2>&1 | Q
rm -rf fab/gerbers fab/lcd2-gerbers.zip && mkdir -p fab/gerbers
"$KC" pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
    --no-protel-ext --use-drill-file-origin -o fab/gerbers/ lcd2.kicad_pcb 2>&1 | Q >/dev/null
"$KC" pcb export drill --format excellon --excellon-separate-th --drill-origin plot -o fab/gerbers/ lcd2.kicad_pcb 2>&1 | Q >/dev/null
(cd fab/gerbers && zip -q -r ../lcd2-gerbers.zip .)
rm -rf fab/gerbers
$LOW "$KC" pcb render --side top --width 1600 --height 1100 --quality basic --zoom 0.95 -o render-top.png lcd2.kicad_pcb 2>&1 | Q >/dev/null
$LOW "$KC" pcb render --side bottom --width 1600 --height 1100 --quality basic --zoom 0.95 -o render-bottom.png lcd2.kicad_pcb 2>&1 | Q >/dev/null
$LOW "$KC" pcb render --width 1600 --height 1200 --quality basic --perspective --rotate "-40,0,-25" --zoom 0.85 -o render-3d.png lcd2.kicad_pcb 2>&1 | Q >/dev/null
ls -la fab/lcd2-gerbers.zip fab/lcd2-bom.csv fab/lcd2-cpl.csv render-top.png render-bottom.png render-3d.png
