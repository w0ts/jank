#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# jank chip, everything in order (README "Files"): the schematic and its ERC, the netlist and the PDF; the board (RF
# path drawn, the rest routed by Freerouting: freeroute.py), its DRC, the nets against the schematic, the Gerbers and drill files zipped
# for JLCPCB, the assembly files, the renders (with the parts' 3D models) and the mockup; then the cost and the case.
# (stage 2: the simulations are separate: python3 sim/linein.py, sim/midi.py, sim/midi_faults.py, sim/power.py,
#  sim/reset.py, sim/cv.py)
#   sh hardware/chip/make_fab.sh            (KiCad 10: kicad-cli and its Python; python3; OpenSCAD)
#   ROUTE=0 sh hardware/chip/make_fab.sh    (keep the board as it is: no placement or routing, the rest again)
set -eu
cd "$(dirname "$0")"
KC=/opt/homebrew/bin/kicad-cli
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9
Q() { grep -v "wxApp\|stdpbase\|Fontconfig" || true; }
# (the renders and the case open a thread per core: run them at the lowest priority, background QoS (macOS: the
#  efficiency cores), so a user's audio or another session's build keeps the machine; the silk close-ups at "basic")
LOW="nice -n 19 taskpolicy -b"

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
    "$KP" repair.py mini2.kicad_pcb 2>&1 | Q        # (stage 2: what Freerouting leaves open, joined; twins' courtyards; stubs)
    "$KP" fatten.py mini2.kicad_pcb 2>&1 | Q        # (stage 2: VBUS / VUSB / +3V3 widened where the DRC allows)
    rm -f fab/repair.rpt fab/fatten.rpt             # (their working DRC reports; fab/drc.rpt below is the board's)
    # (mini 2.2: route_fixups.py, mini 2's fix-ups on its own routing, is gone: none applies to a fresh route)
fi
"$KP" tie_groups.py mini2.kicad_pcb 2>&1 | Q
"$KP" silk.py mini2.kicad_pcb 2>&1 | Q        # (stage 2: references, names, block outlines, the bottom's cheat sheet)
"$KC" pcb drc --severity-all -o fab/drc.rpt mini2.kicad_pcb 2>&1 | Q >/dev/null
grep -E "Found" fab/drc.rpt
grep -E "^\[" fab/drc.rpt | sed 's/:.*//' | sort | uniq -c || true
if grep -q "; error" fab/drc.rpt; then grep -B1 -A3 "; error" fab/drc.rpt | head -40; exit 1; fi
"$KP" check_nets.py fab/mini2.net.xml fab/intended-nets.json mini2.kicad_pcb 2>&1 | Q
"$KP" check_board.py mini2.kicad_pcb 2>&1 | Q    # (stage 2: courtyards both ways, the iron's rooms, vias, twins, ground islands, RF)
"$KP" make_assembly.py 2>&1 | Q
rm -rf fab/gerbers fab/mini2-gerbers.zip && mkdir -p fab/gerbers
"$KC" pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
    --no-protel-ext --use-drill-file-origin -o fab/gerbers/ mini2.kicad_pcb 2>&1 | Q >/dev/null
"$KC" pcb export drill --format excellon --excellon-separate-th --drill-origin plot -o fab/gerbers/ mini2.kicad_pcb 2>&1 | Q >/dev/null
(cd fab/gerbers && zip -q -r ../mini2-gerbers.zip .)
rm -rf fab/gerbers
$LOW "$KC" pcb render --side top --width 1400 --height 1400 --quality high --zoom 0.9 -o render-top.png mini2.kicad_pcb 2>&1 | Q >/dev/null
$LOW "$KC" pcb render --side bottom --width 1400 --height 1400 --quality high --zoom 0.9 -o render-bottom.png mini2.kicad_pcb 2>&1 | Q >/dev/null
$LOW "$KC" pcb render --width 1600 --height 1200 --quality high --perspective --rotate "-40,0,-25" --zoom 0.85 -o render-3d.png mini2.kicad_pcb 2>&1 | Q >/dev/null
$LOW "$KP" render_full.py 2>&1 | Q                  # (stage 2: render-3d-full.png, render-bottom-full.png: every user part fitted)
# (stage 2: the silk, readable on a phone: each side at ~48 px/mm in four quarters, render-silk-{top,bottom}-{0..3}.png)
for side in top bottom; do
    $LOW "$KC" pcb render --side $side --width 3200 --height 3200 --quality basic --zoom 1 -o "render-silk-$side.png" mini2.kicad_pcb 2>&1 | Q >/dev/null
    magick "render-silk-$side.png" -trim +repage -crop 2x2+60+60@ +repage "render-silk-$side-%d.png"
    rm -f "render-silk-$side.png"
done
$LOW "$KP" make_mockup.py 2>&1 | Q
echo "fab/mini2-gerbers.zip fab/usb/{bom,cpl}.csv fab/battery/{bom,cpl}.csv render-top.png render-bottom.png render-3d.png mockup.stl"
python3 cost.py
$LOW sh case/make_stl.sh
