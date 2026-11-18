#!/bin/sh
# jank-mini-3 study: the joint placement + pin-map search. For each layout option (USB top / bottom, LINE IN x 30 / 70,
# the TRS / DIN / CLOCK-RESET pairs swapped or not): place, check (clashes, check_board), score with layout_eval.py.
# One line per option in .t/opt/results.txt. Light: one board at a time, nice 19. From hardware/jank-mini-3.
cd "$(dirname "$0")/.."
KP=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
mkdir -p .t/opt; : > .t/opt/results.txt
for usb in top bottom; do for line in 30 70; do
  [ "$usb" = bottom ] && [ "$line" = 30 ] && continue      # (the routed test: LINE IN beside USB failed, 41-64 open)
  for sw in "" T D M TD TM DM TDM; do
    B=.t/opt/b-$usb-$line-${sw:-none}.kicad_pcb
    export M3_USB=$usb M3_LINE_X=$line M3_SWAP=$sw
    BOARD="$PWD/$B" nice -n 19 "$KP" make_board.py > /dev/null 2>&1 || { echo "$usb $line ${sw:-none} PLACE-FAIL" >> .t/opt/results.txt; continue; }
    c=$(nice -n 19 "$KP" tools/mini3_check.py "$B" 2>/dev/null | tail -1)
    k=$(nice -n 19 "$KP" check_board.py "$B" 2>/dev/null | tail -1)
    s=$(nice -n 19 "$KP" tools/layout_eval.py "$B" 2>/dev/null | tail -1)
    echo "$usb $line ${sw:-none} | $c | $k | $s" >> .t/opt/results.txt
    rm -f "$B"
  done
done; done
echo DONE >> .t/opt/results.txt
