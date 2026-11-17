#!/bin/bash
# The mode estimator on the user's tracks, on gpufarm: 3 single-thread light-slot jobs, nice 19.
set -uo pipefail
cd "$(dirname "$0")/.."
PY=$PWD/.venv/bin/python
SLOT=$HOME/optimist/farm-slot.sh
mkdir -p logs/modes
for p in steady splice ramps; do
    nice -n 19 "$SLOT" light env OMP_NUM_THREADS=1 "$PY" -m eval.mode_eval --part "$p" > "logs/modes/$p.out" 2>&1 &
done
wait
for p in steady splice ramps; do grep -v -i warn "logs/modes/$p.out"; done
