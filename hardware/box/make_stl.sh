#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Exports the printable STLs of every output set into stl/, runs the collision checks (must be EMPTY) and the brace
# checks (must NOT be empty: the connector moved by its play meets the box), checks that every STL is one closed,
# manifold solid, and renders the PNGs. Needs OpenSCAD (a build with --backend=manifold) and python3.
# Usage: hardware/box/make_stl.sh                 all sets
#        SETS="DDTT" hardware/box/make_stl.sh     one set (OUT1..OUT4, OUT1 = the back one)
#        OPENSCAD=/path/to/openscad hardware/box/make_stl.sh
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
SCAD="$HERE/jank_box.scad"
OUT="$HERE/stl"
OPENSCAD="${OPENSCAD:-openscad}"
SETS="${SETS:-DDDD TTTT DDTT DTTT}"
command -v "$OPENSCAD" >/dev/null || { echo "openscad not found: install OpenSCAD, or set OPENSCAD=/path/to/openscad"; exit 2; }
"$OPENSCAD" --help 2>&1 | grep -q -- "--backend" || { echo "this OpenSCAD has no --backend: use a 2024+ build (manifold)"; exit 2; }
O() { "$OPENSCAD" --backend=manifold "$@"; }
mkdir -p "$OUT" "$HERE/renders"
LOG="$(mktemp "$OUT/.log.XXXXXX")"
trap 'rm -f "${LOG:?}" "${OUT:?}"/.check-*.stl' EXIT
fail=0
for s in $SETS; do
    for p in box lid; do
        O -q -D "outs=\"$s\"" -D "part=\"$p\"" -o "$OUT/jank_box_${s}_${p}.stl" "$SCAD" 2>"$LOG" || { cat "$LOG"; exit 1; }
        if grep -qi "warning" "$LOG"; then cat "$LOG"; fail=1; fi
    done
    for c in check_box check_lid check_lidbox check_drop check_text brace_pull brace_push brace_trs brace_floor brace_lid brace_ledge; do
        O -D "outs=\"$s\"" -D "part=\"$c\"" -o "$OUT/.check-$c.stl" "$SCAD" >"$LOG" 2>&1 || true
        empty=0; grep -q "top level object is empty" "$LOG" && empty=1
        case "$c" in
            check_*) want=1 ;;
            brace_trs) [[ "$s" == *T* ]] && want=0 || want=1 ;;
            brace_pull|brace_lid|brace_ledge) want=0 ;;
            brace_push|brace_floor) [[ "$s" == *D* ]] && want=0 || want=1 ;;
        esac
        if [ "$empty" = "$want" ]; then
            echo "$s: $c ok ($([ $empty = 1 ] && echo empty || echo contact))"
        else
            echo "$s: $c FAILED ($([ $empty = 1 ] && echo empty || echo not empty)); render part=\"$c\" in OpenSCAD to see where"; tail -3 "$LOG"; fail=1
        fi
    done
    grep -o "jank box [A-Z]*: outer [^\"]*" "$LOG" || true
    O --render --colorscheme=Tomorrow --imgsize=1400,900 --camera=44,24,10,55,0,330,200 \
        -D "outs=\"$s\"" -D 'part="open"' -o "$HERE/renders/open_${s}.png" "$SCAD" >/dev/null 2>&1 || fail=1
    O --render --colorscheme=Tomorrow --imgsize=1400,700 --camera=40,0,14,90,0,0,150 \
        -D "outs=\"$s\"" -D 'part="front"' -o "$HERE/renders/front_${s}.png" "$SCAD" >/dev/null 2>&1 || fail=1
    O --render --colorscheme=Tomorrow --imgsize=1400,700 --camera=40,58,14,90,0,180,150 \
        -D "outs=\"$s\"" -D 'part="back"' -o "$HERE/renders/back_${s}.png" "$SCAD" >/dev/null 2>&1 || fail=1
    O --render --colorscheme=Tomorrow --imgsize=1400,900 --camera=44,24,10,55,0,330,200 \
        -D "outs=\"$s\"" -D 'part="box"' -o "$HERE/renders/box_${s}.png" "$SCAD" >/dev/null 2>&1 || fail=1
    O --render --colorscheme=Tomorrow --imgsize=1400,900 --camera=44,26,0,35,0,25,170 \
        -D "outs=\"$s\"" -D 'part="lid"' -o "$HERE/renders/lid_${s}.png" "$SCAD" >/dev/null 2>&1 || fail=1
    O --render --colorscheme=Tomorrow --imgsize=1400,1000 --camera=45.3,29,0,0,0,0,175 --projection=ortho \
        -D "outs=\"$s\"" -D 'part="lid_top"' -o "$HERE/renders/lid_top_${s}.png" "$SCAD" >/dev/null 2>&1 || fail=1
done
O -q -D 'part="front_test"' -o "$OUT/jank_box_front_test.stl" "$SCAD" 2>"$LOG" || { cat "$LOG"; exit 1; }
python3 "$HERE/../check_stl.py" "$OUT"/jank_box_*.stl || fail=1
exit $fail
