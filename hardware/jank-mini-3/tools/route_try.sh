#!/bin/sh
# jank-mini-3 study helper: place + route one attempt at a given board height, in its own folder (never touches
# lcd2.kicad_pcb). Same steps as make_route.sh. Usage (from hardware/jank-mini-3): sh tools/route_try.sh <H> <N>
set -u
H=$1; N=$2
cd "$(dirname "$0")/.."
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
KC=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
Q() { grep -v -i "wxapp\|fontconfig\|stdpbase\|Debug:\|memory leak\|assert" || true; }
D=.t/route-h$H; mkdir -p $D; [ -e $D/lib ] || ln -s ../../lib $D/lib
T=$D/try-$N.kicad_pcb
cp lcd2.kicad_sch "$D/try-$N.kicad_sch"; cp lcd2.kicad_pro "$D/try-$N.kicad_pro"; cp lcd2.kicad_dru "$D/try-$N.kicad_dru"
cp sym-lib-table fp-lib-table $D/ 2>/dev/null || true
export LCD2_H=$H JAVA_TOOL_OPTIONS="-Xmx3g -XX:ActiveProcessorCount=2 -Djava.awt.headless=true"
BOARD="$PWD/$T" nice -n 19 "$KP" make_board.py 2>&1 | Q
nice -n 19 "$KP" route.py "$T" 2>&1 | Q
nice -n 19 "$KP" route.py "$T" --vias 2>&1 | Q
for i in 1 2 3 4 5 6 7 8; do
    out=$(nice -n 19 "$KP" route.py "$T" --clean 2>&1 | Q); echo "$out"
    case "$out" in *"clean: 0 "*) break ;; esac
done
nice -n 19 "$KP" route.py "$T" --fix 2>&1 | Q
nice -n 19 "$KP" route.py "$T" --drcvias 2>&1 | Q
for i in 1 2 3; do nice -n 19 "$KP" route.py "$T" --clean 2>&1 | Q; done
nice -n 19 "$KP" route.py "$T" --pours 2>&1 | Q
nice -n 19 "$KP" silk.py "$T" --trim 2>&1 | Q
nice -n 19 "$KP" silk.py "$T" --labels 2>&1 | Q
nice -n 19 "$KC" pcb drc --severity-all --schematic-parity -o "$D/try-$N.drc.rpt" "$T" 2>&1 | Q >/dev/null
grep -E "Found" "$D/try-$N.drc.rpt"
nice -n 19 "$KP" check_board.py "$T" 2>&1 | Q | tail -4
echo "DONE H=$H N=$N"
