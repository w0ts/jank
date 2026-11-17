#!/bin/bash
# Glide-mode trigger tuning on gpufarm: single-thread light-slot jobs, nice 19.
#   bash scripts/farm_glide_tune.sh            the quad trigger, one job per entry threshold
#   bash scripts/farm_glide_tune.sh restarts   the same-way-restarts trigger, one job per restart count
set -uo pipefail
cd "$(dirname "$0")/.."
PY=$PWD/.venv/bin/python
SLOT=$HOME/optimist/farm-slot.sh
mode=${1:-quad}
mkdir -p "logs/glide_tune_$mode"
if [ "$mode" = restarts ]; then args=("--restart-count 2" "--restart-count 3" "--restart-count 4"); else args=("--enter 0.006" "--enter 0.01" "--enter 0.015" "--enter 0.02"); fi
for a in "${args[@]}"; do
    nice -n 19 "$SLOT" light env OMP_NUM_THREADS=1 "$PY" -m eval.glide_tune $a > "logs/glide_tune_$mode/${a// /_}.out" 2>&1 &
done
wait
grep -h "^{" "logs/glide_tune_$mode"/*.out
