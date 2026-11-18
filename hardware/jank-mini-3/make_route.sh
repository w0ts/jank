#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Place and route the board WITHOUT ever overwriting a good one: each attempt is built in route/try-N.kicad_pcb (with
# its own copy of the schematic and project, so DRC can check parity); it replaces lcd2.kicad_pcb (and is kept as
# route/last-good.kicad_pcb) only when DRC finds 0 violations / 0 unconnected with parity and check_board is clean.
# Up to 3 attempts (Freerouting's result varies), one at a time, nice 19, each waiting until the Mac's 1-minute load is
# under its core count. Exit 1 if no attempt is clean (lcd2.kicad_pcb then untouched).
set -eu
cd "$(dirname "$0")"
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9
KC=/opt/homebrew/bin/kicad-cli
Q() { grep -v -i "wxapp\|fontconfig\|stdpbase\|Debug:\|memory leak" || true; }
CORES=$(sysctl -n hw.ncpu)
mkdir -p route
[ -e route/lib ] || ln -s ../lib route/lib
for N in 1 2 3; do
    for i in $(seq 1 40); do
        load=$(sysctl -n vm.loadavg | awk '{print int($2)}')
        [ "$load" -lt "$CORES" ] && break
        echo "load $load / $CORES cores: waiting"; sleep 30
    done
    T=route/try-$N.kicad_pcb
    cp lcd2.kicad_sch "route/try-$N.kicad_sch"; cp lcd2.kicad_pro "route/try-$N.kicad_pro"
    cp sym-lib-table fp-lib-table lcd2.kicad_dru route/ 2>/dev/null || true
    cp lcd2.kicad_dru "route/try-$N.kicad_dru"
    BOARD="$PWD/$T" "$KP" make_board.py 2>&1 | Q
    nice -n 19 "$KP" route.py "$T" 2>&1 | Q
    "$KP" route.py "$T" --vias 2>&1 | Q
    for i in 1 2 3 4 5 6 7 8; do
        out=$("$KP" route.py "$T" --clean 2>&1 | Q); echo "$out"
        case "$out" in *"clean: 0 "*) break ;; esac
    done
    "$KP" route.py "$T" --fix 2>&1 | Q
    "$KP" route.py "$T" --drcvias 2>&1 | Q
    for i in 1 2 3; do "$KP" route.py "$T" --clean 2>&1 | Q; done
    "$KP" route.py "$T" --pours 2>&1 | Q
    "$KP" silk.py "$T" --trim 2>&1 | Q
    "$KP" silk.py "$T" --labels 2>&1 | Q
    nice -n 19 "$KC" pcb drc --severity-all --schematic-parity -o "route/try-$N.drc.rpt" "$T" 2>&1 | Q >/dev/null
    grep -E "Found" "route/try-$N.drc.rpt"
    if ! grep -q "; error\|; warning" "route/try-$N.drc.rpt" && grep -q "Found 0 unconnected" "route/try-$N.drc.rpt" \
            && "$KP" check_board.py "$T" 2>&1 | Q | tail -1 | grep -q "check_board: 0 problems"; then
        cp "$T" lcd2.kicad_pcb; cp "$T" route/last-good.kicad_pcb
        echo "attempt $N clean: promoted to lcd2.kicad_pcb (and route/last-good.kicad_pcb)"
        exit 0
    fi
    echo "attempt $N not clean: lcd2.kicad_pcb left as it was"
done
exit 1
