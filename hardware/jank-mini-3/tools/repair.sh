#!/bin/sh
# jank-mini-3 study helper: one more Freerouting round on a routed board's open nets, every track kept (the pours set
# aside first, put back after), then the same tail as route_try.sh. Usage: sh tools/repair.sh <board.kicad_pcb>
set -u
cd "$(dirname "$0")/.."
T=$1
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
KC=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
Q() { grep -v -i "wxapp\|fontconfig\|stdpbase\|Debug:\|memory leak\|assert" || true; }
export JAVA_TOOL_OPTIONS="-Xmx3g -XX:ActiveProcessorCount=2 -Djava.awt.headless=true"
nice -n 19 "$KP" route.py "$T" --unpour 2>&1 | Q
nice -n 19 "$KP" route.py "$T" --repair 2>&1 | Q
nice -n 19 "$KP" route.py "$T" --repair 2>&1 | Q
nice -n 19 "$KP" route.py "$T" --vias 2>&1 | Q
for i in 1 2 3 4 5 6; do
    out=$(nice -n 19 "$KP" route.py "$T" --clean 2>&1 | Q); echo "$out"
    case "$out" in *"clean: 0 "*) break ;; esac
done
nice -n 19 "$KP" route.py "$T" --fix 2>&1 | Q
nice -n 19 "$KP" route.py "$T" --drcvias 2>&1 | Q
for i in 1 2 3; do nice -n 19 "$KP" route.py "$T" --clean 2>&1 | Q; done
nice -n 19 "$KP" route.py "$T" --pours 2>&1 | Q
nice -n 19 "$KP" tools/widen_hot.py "$T" 2>&1 | Q | grep -v "^ "
nice -n 19 "$KP" silk.py "$T" --unlabel 2>&1 | Q
nice -n 19 "$KP" silk.py "$T" --trim 2>&1 | Q
nice -n 19 "$KP" silk.py "$T" --labels 2>&1 | Q
nice -n 19 "$KC" pcb drc --severity-all --schematic-parity -o "${T%.kicad_pcb}.drc.rpt" "$T" 2>&1 | Q >/dev/null
grep -E "Found" "${T%.kicad_pcb}.drc.rpt"
nice -n 19 "$KP" check_board.py "$T" 2>&1 | Q | tail -3
