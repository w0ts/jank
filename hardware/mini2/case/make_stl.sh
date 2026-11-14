#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# jank mini 2.2's case: exports mini_base (no DINs), mini_base_din, mini_lid_led (the LED window), mini_lid_screen (the
# OLED's window and bezel pocket), mini_button_cap (print four), mini_led_pipe into stl/; runs the collision checks for
# both bases and both lids; checks every STL is one closed solid; renders the PNGs into renders/. The parts' positions
# and the board's size are read from ../mini2.kicad_pcb at build time (place_args.py). The cells' size is a parameter
# (mini_case.scad "The cells"): CELLS="-D cell_len=62 -D cell_h=15.5 -D cell_t=6.7" sh make_stl.sh tries other cells.
# Needs OpenSCAD (--backend=manifold), python3, ../mockup.stl.
#   sh hardware/mini2/case/make_stl.sh
set -euf     # (f: no globbing, the positions' brackets pass as they are)
HERE="$(cd "$(dirname "$0")" && pwd)"
SCAD="$HERE/mini_case.scad"
OUT="$HERE/stl"
REN="$HERE/renders"
OPENSCAD="${OPENSCAD:-openscad}"
command -v "$OPENSCAD" >/dev/null || { echo "openscad not found: set OPENSCAD=/path/to/openscad"; exit 2; }
O() { nice -n 19 "$OPENSCAD" --backend=manifold "$@"; }
mkdir -p "$OUT" "$REN"
MINI_PCB="${MINI_PCB:-$HERE/../mini2.kicad_pcb}"
POS="$(python3 "$HERE/place_args.py" "$MINI_PCB")"
LOG="$(mktemp "$OUT/.log.XXXXXX")"
trap 'rm -f "$LOG" "$OUT"/.check-*.stl' EXIT
fail=0
POSD="${CELLS:-}"
for a in $POS; do POSD="$POSD -D $a"; done
# shellcheck disable=SC2086
exp() {   # name, extra args...
    n="$1"; shift
    O -q "$@" $POSD -o "$OUT/$n.stl" "$SCAD" 2>"$LOG" || { cat "$LOG"; exit 1; }
    if grep -qi "warning" "$LOG"; then cat "$LOG"; fail=1; fi
}
exp mini_base -D 'part="base"'
exp mini_base_din -D 'part="base"' -D din=true
exp mini_lid_led -D 'part="lid"' -D 'lid_var="led"'
exp mini_lid_screen -D 'part="lid"' -D 'lid_var="screen"'
exp mini_button_cap -D 'part="button_cap"'
exp mini_led_pipe -D 'part="led_pipe"'
# empty = no overlap at the stated clearance; "near" checks must NOT be empty
for d in false true; do
    for v in led screen; do
        for c in check_base check_lid check_cap check_cap_lid check_pipe check_pipe_lid check_lid_base check_pads_touch check_lid_pads_touch check_rib_touch; do
            O -D "part=\"$c\"" -D "din=$d" -D "lid_var=\"$v\"" $POSD -o "$OUT/.check-$c.stl" "$SCAD" >"$LOG" 2>&1 || true
            if grep -q "top level object is empty" "$LOG"; then echo "din=$d lid=$v $c ok (empty)"; else echo "din=$d lid=$v $c FAILED: not empty"; tail -3 "$LOG"; fail=1; fi
        done
    done
    for n in check_pads_near check_lid_pads_near check_rib_near; do
        O -D "part=\"$n\"" -D "din=$d" $POSD -o "$OUT/.check-near.stl" "$SCAD" >"$LOG" 2>&1 || true
        if grep -q "top level object is empty" "$LOG"; then echo "din=$d $n FAILED: too far"; fail=1; else echo "din=$d $n ok (within 0.12)"; fi
    done
done
R() { O --render --colorscheme=Tomorrow "$@" $POSD >/dev/null 2>&1 || fail=1; }
R --imgsize=1400,1000 --camera=33,29,-4,62,0,28,300 -D 'part="assembly"' -D 'lid_var="led"' -o "$REN/closed_led.png" "$SCAD"
R --imgsize=1400,1000 --camera=33,29,-4,62,0,28,300 -D 'part="assembly"' -D 'lid_var="screen"' -o "$REN/closed_screen.png" "$SCAD"
R --imgsize=1400,1000 --camera=33,29,-4,62,0,28,300 -D 'part="assembly"' -D 'lid_var="screen"' -D din=true -o "$REN/closed_screen_din.png" "$SCAD"
R --imgsize=1400,1000 --camera=33,29,-4,0,0,0,170 --projection=ortho -D 'part="assembly"' -D 'lid_var="led"' -o "$REN/top_led.png" "$SCAD"
R --imgsize=1400,1000 --camera=33,29,-4,0,0,0,170 --projection=ortho -D 'part="assembly"' -D 'lid_var="screen"' -o "$REN/top_screen.png" "$SCAD"
R --imgsize=1400,1000 --camera=33,29,6,62,0,28,330 -D 'part="open"' -D 'lid_var="screen"' -D din=true -o "$REN/open.png" "$SCAD"
R --imgsize=1400,1000 --camera=30,29,-4,90,0,90,190 --projection=ortho -D 'part="section_x"' -D din=true -o "$REN/section_cells.png" "$SCAD"   # (across x: both cells on edge, the rib)
R --imgsize=1400,1000 --camera=33,30,0,90,0,180,190 --projection=ortho -D 'part="section_screen"' -D 'lid_var="screen"' -D din=true -o "$REN/section_screen.png" "$SCAD"
R --imgsize=1400,1000 --camera=33,30,-6,90,0,180,190 --projection=ortho -D 'part="section_din"' -D din=true -o "$REN/section_din.png" "$SCAD"
R --imgsize=1400,1000 --camera="33,-29,0,180,0,0,190" --projection=ortho -D 'part="lid"' -D 'lid_var="screen"' -o "$REN/lid_screen_printed_roof.png" "$SCAD"
R --imgsize=1400,1000 --camera="33,-29,0,180,0,0,190" --projection=ortho -D 'part="lid"' -D 'lid_var="led"' -o "$REN/lid_led_printed_roof.png" "$SCAD"
R --imgsize=1400,1000 --camera="33,-29,8,50,0,20,200" -D 'part="lid"' -D 'lid_var="led"' -o "$REN/lid_inside_antenna.png" "$SCAD"
set +f       # (the STLs' names: globbing back on)
python3 "$HERE/check_led.py" "$OUT/mini_lid_led.stl" "$POS" || fail=1    # (the LED window over the LEDs, not its mirror image)
python3 "$HERE/../../check_stl.py" "$OUT"/mini_*.stl || fail=1
exit $fail
