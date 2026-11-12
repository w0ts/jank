#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# jank mini's case: exports mini_base, mini_base_din, mini_lid, mini_button_cap, mini_led_pipe into stl/, runs the
# collision checks for both versions, checks every STL is one closed solid, renders the PNGs into renders/.
# The buttons', the LED's and the DIN's positions and the board's size are read from ../chip.kicad_pcb at build time.
# Needs OpenSCAD (--backend=manifold), python3, ../mockup.stl.
#   sh hardware/chip/case/make_stl.sh
set -euf     # (f: no globbing, the positions' brackets pass as they are)
HERE="$(cd "$(dirname "$0")" && pwd)"
SCAD="$HERE/mini_case.scad"
OUT="$HERE/stl"
REN="$HERE/renders"
OPENSCAD="${OPENSCAD:-openscad}"
command -v "$OPENSCAD" >/dev/null || { echo "openscad not found: set OPENSCAD=/path/to/openscad"; exit 2; }
O() { "$OPENSCAD" --backend=manifold "$@"; }
mkdir -p "$OUT" "$REN"
MINI_PCB="${MINI_PCB:-$HERE/../chip.kicad_pcb}"   # (the finished board gives SW1, SW2, LED1, its size, J5; ../mockup.stl is made from it too)
POS="$(python3 "$HERE/place_args.py" "$MINI_PCB")"
# shellcheck disable=SC2086
set -- $POS
LOG="$(mktemp "$OUT/.log.XXXXXX")"
trap 'rm -f "$LOG" "$OUT"/.check-*.stl' EXIT
fail=0
exp() {   # name, extra args...
    n="$1"; shift
    O -q "$@" $POSD -o "$OUT/$n.stl" "$SCAD" 2>"$LOG" || { cat "$LOG"; exit 1; }
    if grep -qi "warning" "$LOG"; then cat "$LOG"; fail=1; fi
}
POSD=""
for a in $POS; do POSD="$POSD -D $a"; done
exp mini_base -D 'part="base"'
exp mini_base_din -D 'part="base"' -D din=true
exp mini_lid -D 'part="lid"'
exp mini_button_cap -D 'part="button_cap"'
exp mini_led_pipe -D 'part="led_pipe"'
# empty = no overlap at the stated clearance
for d in false true; do
    for c in check_base check_lid check_cap check_cap_lid check_pipe check_pipe_lid check_lid_base check_pads_touch check_lid_pads_touch; do
        O -D "part=\"$c\"" -D "din=$d" $POSD -o "$OUT/.check-$c.stl" "$SCAD" >"$LOG" 2>&1 || true
        if grep -q "top level object is empty" "$LOG"; then echo "din=$d $c ok (empty)"; else echo "din=$d $c FAILED: not empty"; tail -3 "$LOG"; fail=1; fi
    done
    for n in check_pads_near check_lid_pads_near; do
        O -D "part=\"$n\"" -D "din=$d" $POSD -o "$OUT/.check-near.stl" "$SCAD" >"$LOG" 2>&1 || true
        if grep -q "top level object is empty" "$LOG"; then echo "din=$d $n FAILED: pads too far"; fail=1; else echo "din=$d $n ok (within 0.12)"; fi
    done
done
R() { O --render --colorscheme=Tomorrow "$@" $POSD >/dev/null 2>&1 || fail=1; }
for d in false true; do
    s=""; [ "$d" = true ] && s="_din"
    R --imgsize=1400,1000 --camera=22,26,-2,62,0,28,230 -D 'part="assembly"' -D "din=$d" -o "$REN/closed$s.png" "$SCAD"
    R --imgsize=1400,1000 --camera=22,26,6,62,0,28,270 -D 'part="open"' -D "din=$d" -o "$REN/open$s.png" "$SCAD"
done
R --imgsize=1400,1000 --camera=22,31.5,-3,90,0,180,150 --projection=ortho -D 'part="section_din"' -D din=true -o "$REN/section_din.png" "$SCAD"
R --imgsize=1400,1000 --camera=22,45.7,-4,90,0,180,150 --projection=ortho -D 'part="section_jack"' -D din=true -o "$REN/section_jack.png" "$SCAD"
set +f       # (the STLs' names: globbing back on)
python3 "$HERE/../../check_stl.py" "$OUT"/mini_*.stl || fail=1
exit $fail
