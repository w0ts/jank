#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# jank mini 2.2's case: exports mini_base (no DINs), mini_base_din, mini_lid_led (the LED window), mini_lid_s130 ..
# mini_lid_s242 (72 x 72: one lid per screen size; was mini_lid_screen: the
# OLED's window and bezel pocket), both lids with the D-pad's cross hole and the toggle's round hole (stage 2),
# mini_dpad_cap (the D-pad, PETG, face down), mini_led_pipe into stl/; runs the D-pad's tilt check (check_dpad.py) and
# the collision checks for
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
for z in s130 s154 s242; do exp "mini_lid_$z" -D 'part="lid"' -D 'lid_var="screen"' -D "screen=\"$z\""; done   # (72 x 72: one lid per screen size)
exp mini_led_pipe -D 'part="led_pipe"'
exp mini_dpad_cap -D 'part="dpad_cap"'
# empty = no overlap at the stated clearance; "near" checks must NOT be empty
for d in false true; do
    vs="led s130 s154 s242"; [ "$d" = true ] && vs="led s242"   # (the DIN base with the lightest and the hung screen)
    for v in $vs; do
        lv=led; [ "$v" = led ] || lv=screen
        for c in check_base check_lid check_dpad check_dpad_lid check_pwr check_pwr_lid check_posts_touch check_pipe check_pipe_lid check_lid_base check_pads_touch check_lid_pads_touch check_rib_touch; do
            O -D "part=\"$c\"" -D "din=$d" -D "lid_var=\"$lv\"" -D "screen=\"$v\"" $POSD -o "$OUT/.check-$c.stl" "$SCAD" >"$LOG" 2>&1 || true
            if grep -q "top level object is empty" "$LOG"; then echo "din=$d lid=$v $c ok (empty)"; else echo "din=$d lid=$v $c FAILED: not empty"; tail -3 "$LOG"; fail=1; fi
        done
    done
    for n in check_pads_near check_lid_pads_near check_rib_near check_dpad_near check_pivot_near check_posts_near; do
        O -D "part=\"$n\"" -D "din=$d" $POSD -o "$OUT/.check-near.stl" "$SCAD" >"$LOG" 2>&1 || true
        if grep -q "top level object is empty" "$LOG"; then echo "din=$d $n FAILED: too far"; fail=1; else echo "din=$d $n ok (within 0.12)"; fi
    done
done
R() { O --render --colorscheme=Tomorrow "$@" $POSD >/dev/null 2>&1 || fail=1; }
R --imgsize=1400,1000 --camera=36,36,-4,62,0,28,300 -D 'part="assembly"' -D 'lid_var="led"' -o "$REN/closed_led.png" "$SCAD"
for z in s130 s154 s242; do   # (72 x 72: the closed case with each screen size's lid; the 0.96" is out: unreadable)
    R --imgsize=1400,1000 --camera=36,36,-4,62,0,28,320 -D 'part="assembly"' -D 'lid_var="screen"' -D "screen=\"$z\"" -o "$REN/closed_$z.png" "$SCAD"
done
R --imgsize=1400,1000 --camera=36,36,-4,62,0,28,320 -D 'part="assembly"' -D 'lid_var="screen"' -D 'screen="s242"' -D din=true -o "$REN/closed_s242_din.png" "$SCAD"
R --imgsize=1400,1000 --camera=36,36,-4,0,0,0,170 --projection=ortho -D 'part="assembly"' -D 'lid_var="led"' -o "$REN/top_led.png" "$SCAD"
R --imgsize=1400,1000 --camera=36,36,-4,0,0,0,170 --projection=ortho -D 'part="assembly"' -D 'lid_var="screen"' -o "$REN/top_screen.png" "$SCAD"
R --imgsize=1400,1000 --camera=36,36,6,62,0,28,330 -D 'part="open"' -D 'lid_var="screen"' -D din=true -o "$REN/open.png" "$SCAD"
R --imgsize=1600,1200 --camera=43,29,0,62,0,40,300 -D 'part="open"' -D 'lid_var="screen"' -D 'screen="s242"' -D din=true -o "$REN/open_full.png" "$SCAD"   # (the slots and holes against the jacks, DINs, USB and toggle)
R --imgsize=1400,1000 --camera=33,36,-4,90,0,90,190 --projection=ortho -D 'part="section_x"' -D din=true -o "$REN/section_cells.png" "$SCAD"   # (across x: both cells on edge, the rib)
R --imgsize=1400,1000 --camera=36,37,0,90,0,180,190 --projection=ortho -D 'part="section_screen"' -D 'lid_var="screen"' -D din=true -o "$REN/section_screen.png" "$SCAD"
R --imgsize=1400,1000 --camera=36,37,-6,90,0,180,190 --projection=ortho -D 'part="section_din"' -D din=true -o "$REN/section_din.png" "$SCAD"
R --imgsize=1400,1000 --camera=36,37,-2,90,0,180,190 --projection=ortho -D 'part="section_button"' -D 'lid_var="led"' -o "$REN/section_button.png" "$SCAD"   # (across NO / YES: the D-pad's arms, its pivot and bosses, the rib)
R --imgsize=1400,1000 --camera=36,37,-4,90,0,180,190 --projection=ortho -D 'part="section_jack"' -D din=true -o "$REN/section_jack.png" "$SCAD"   # (across J2 / J3: the noses in the base's slots)
R --imgsize=1400,1000 --camera="36,-36,0,180,0,0,190" --projection=ortho -D 'part="lid"' -D 'lid_var="screen"' -o "$REN/lid_screen_printed_roof.png" "$SCAD"
R --imgsize=1400,1000 --camera="36,-36,0,180,0,0,190" --projection=ortho -D 'part="lid"' -D 'lid_var="led"' -o "$REN/lid_led_printed_roof.png" "$SCAD"
R --imgsize=1400,1000 --camera="36,-36,8,50,0,20,200" -D 'part="lid"' -D 'lid_var="led"' -o "$REN/lid_inside_antenna.png" "$SCAD"
set +f       # (the STLs' names: globbing back on)
python3 "$HERE/check_led.py" "$OUT/mini_lid_led.stl" "$POS" || fail=1    # (the LED window over the LEDs, not its mirror image)
python3 "$HERE/check_dpad.py" || fail=1                                    # (the D-pad's tilt: each arm alone, the travel, the hole)
python3 "$HERE/../../check_stl.py" "$OUT"/mini_*.stl || fail=1
exit $fail
