#!/bin/bash
# A/B of the constant-tempo estimator vs glide mode on gpufarm: 4 single-thread light-slot jobs, nice 19.
set -uo pipefail
cd "$(dirname "$0")/.."
PY=$PWD/.venv/bin/python
SLOT=$HOME/optimist/farm-slot.sh
mkdir -p logs/glide
light() { local name=$1; shift; nice -n 19 "$SLOT" light env OMP_NUM_THREADS=1 "$PY" "$@" > "logs/glide/$name.out" 2>&1 & }
for e in locked glide; do
    light "ramp_$e" -m eval.locked_ramp --estimator "$e"
    light "eval_$e" -m eval.locked_eval --estimator "$e"
done
wait
for f in logs/glide/*.out; do echo "== $f"; grep -v -i warn "$f"; done
