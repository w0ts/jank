#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# jank tiny's case: exports the base, the lid and the button cap into stl/, runs the collision checks (each must be
# EMPTY: the case and the board's mockup do not meet), checks every STL is one closed solid, and renders a view.
# Needs OpenSCAD with --backend=manifold, python3, and ../mockup.stl (make_mockup.py).
#   sh hardware/tiny/case/make_stl.sh
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
SCAD="$HERE/tiny_case.scad"
OUT="$HERE/stl"
O() { openscad --backend=manifold "$@"; }
mkdir -p "$OUT"
LOG="$(mktemp "$OUT/.log.XXXXXX")"
trap 'rm -f "$LOG" "$OUT"/.check-*.stl' EXIT
fail=0
for p in base lid button_cap; do
    O -q -D "part=\"$p\"" -o "$OUT/tiny_case_$p.stl" "$SCAD" 2>"$LOG" || { cat "$LOG"; exit 1; }
    if grep -qi "warning" "$LOG"; then cat "$LOG"; fail=1; fi
done
for c in check_base check_lid check_cap; do
    O -D "part=\"$c\"" -o "$OUT/.check-$c.stl" "$SCAD" >"$LOG" 2>&1 || true
    if grep -q "top level object is empty" "$LOG"; then echo "$c ok (empty)"; else echo "$c FAILED: the case meets the board"; fail=1; fi
done
O --render --colorscheme=Tomorrow --imgsize=1200,900 --camera=15,20,10,60,0,30,110 -D 'part="assembly"' -o "$HERE/assembly.png" "$SCAD" >/dev/null 2>&1 || fail=1
python3 "$HERE/../../check_stl.py" "$OUT"/tiny_case_*.stl "$HERE/../mockup.stl" || fail=1
exit $fail
