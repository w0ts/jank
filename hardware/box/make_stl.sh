#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# v5 (one build: every output has both its jack and its DIN): exports the box, the lid, the printed caps (2 x
# button_cap, 4 x slider_cap: print that many), the encoder knob and the wall test strip into
# stl/, runs the collision checks (must be EMPTY) and the brace checks (must NOT be empty: a connector moved by its
# play meets the box or the lid), checks that every STL is one closed, manifold solid, and renders the PNGs.
# Needs OpenSCAD (a build with --backend=manifold) and python3.
# Usage: hardware/box/make_stl.sh
#        OPENSCAD=/path/to/openscad hardware/box/make_stl.sh
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
SCAD="$HERE/jank_box.scad"
OUT="$HERE/stl"
OPENSCAD="${OPENSCAD:-openscad}"
command -v "$OPENSCAD" >/dev/null || { echo "openscad not found: install OpenSCAD, or set OPENSCAD=/path/to/openscad"; exit 2; }
"$OPENSCAD" --help 2>&1 | grep -q -- "--backend" || { echo "this OpenSCAD has no --backend: use a 2024+ build (manifold)"; exit 2; }
O() { "$OPENSCAD" --backend=manifold "$@"; }
mkdir -p "$OUT" "$HERE/renders"
LOG="$(mktemp "$OUT/.log.XXXXXX")"
trap 'rm -f "${LOG:?}" "${OUT:?}"/.check-*.stl' EXIT
fail=0
for p in box lid button_cap slider_cap knob front_test; do
    O -q -D "part=\"$p\"" -o "$OUT/jank_box_${p}.stl" "$SCAD" 2>"$LOG" || { cat "$LOG"; exit 1; }
    if grep -qi "warning" "$LOG"; then cat "$LOG"; fail=1; fi
done
for c in check_box check_lid check_lidbox check_drop check_board_drop check_text check_caps brace_caps brace_pull brace_push brace_trs brace_floor brace_lid brace_ledge; do
    O -D "part=\"$c\"" -o "$OUT/.check-$c.stl" "$SCAD" >"$LOG" 2>&1 || true
    empty=0; grep -q "top level object is empty" "$LOG" && empty=1
    case "$c" in check_*) want=1 ;; *) want=0 ;; esac
    if [ "$empty" = "$want" ]; then
        echo "$c ok ($([ $empty = 1 ] && echo empty || echo contact))"
    else
        echo "$c FAILED ($([ $empty = 1 ] && echo empty || echo not empty)); render part=\"$c\" in OpenSCAD to see where"; tail -3 "$LOG"; fail=1
    fi
done
grep -o "jank box v5: outer [^\"]*" "$LOG" || true
R() { O --render --colorscheme=Tomorrow "$@" >/dev/null 2>&1 || fail=1; }
R --imgsize=1400,900 --camera=44,35,14,55,0,330,220 -D 'part="open"' -o "$HERE/renders/open.png" "$SCAD"
R --imgsize=1400,800 --camera=44,0,18,90,0,0,160 -D 'part="front"' -o "$HERE/renders/front.png" "$SCAD"
R --imgsize=1400,800 --camera=44,70,18,90,0,180,160 -D 'part="back"' -o "$HERE/renders/back.png" "$SCAD"
R --imgsize=1400,900 --camera=44,35,14,55,0,330,220 -D 'part="box"' -o "$HERE/renders/box.png" "$SCAD"
R --imgsize=1400,900 --camera=44,35,0,35,0,25,190 -D 'part="lid"' -o "$HERE/renders/lid_underside.png" "$SCAD"
R --imgsize=1400,1100 --camera=44.3,35.4,0,0,0,0,190 --projection=ortho -D 'part="lid_top"' -o "$HERE/renders/lid_top.png" "$SCAD"
R --imgsize=1400,900 --camera=44,35,20,55,0,330,200 -D 'part="caps_view"' -o "$HERE/renders/caps.png" "$SCAD"
python3 "$HERE/../check_stl.py" "$OUT"/jank_box_*.stl || fail=1
exit $fail
