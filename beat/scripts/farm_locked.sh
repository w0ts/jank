#!/bin/bash
# The BPM estimator studies on gpufarm, each in a single-thread light slot (7 jobs side by side), nice 19.
set -uo pipefail
cd "$(dirname "$0")/.."
PY=$PWD/.venv/bin/python
SLOT=$HOME/optimist/farm-slot.sh
mkdir -p logs/locked
light() { local name=$1; shift; nice -n 19 "$SLOT" light env OMP_NUM_THREADS=1 "$PY" "$@" > "logs/locked/$name.out" 2>&1 & }
for rel in 0.005 0.01 0.02 0.04; do light "tune_$rel" -m eval.locked_tune --rel "$rel"; done
light ramp -m eval.locked_ramp
light phase_0 -m eval.locked_phase --delay 0.0
light phase_03 -m eval.locked_phase --delay 0.3
wait
echo "$(date +%T) locked studies done"
