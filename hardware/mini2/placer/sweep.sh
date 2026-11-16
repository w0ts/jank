#!/bin/bash
# the farm sweep (2026-11-16): K_W x C_W x T0 x seeds, one python each, all at once, nice 19
cd "$(dirname "$0")"
i=0
for kw in 1 2; do for cw in 0.5 1.0; do for t0 in 1.5 3.0; do for s in 1 2; do
  i=$((i+1)); n="k${kw}c${cw}t${t0}s${s}"
  K_W=$kw C_W=$cw nice -n 19 python3 run.py place.json "sw-$n.json" 400000 $((100+i)) $t0 > "sw-$n.log" 2>&1 &
done; done; done; done
wait
echo done > sweep.done
