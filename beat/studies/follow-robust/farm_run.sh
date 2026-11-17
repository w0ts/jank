#!/bin/bash
# On gpufarm, from the study's farm copy (~/optimist/worktrees/followrobust-code: data/traktor, data/torch and .venv linked
# to the farm's beat-listener checkout, data/mmdb_traktor copied from the Mac). The feature cache (extract_live.py) is
# kept in ~/optimist/worktrees/followrobust-cache. One run at a time under the farm's flock, nice 19, one thread per
# worker. W workers: at most half of what is free (uptime first; Mon-Fri 9:00-17:30 at most 32).
#   W=24 SWEEP=studies/follow-robust/sweep_dev.json SPLIT=dev TAG=dev1 bash studies/follow-robust/farm_run.sh
#   W=24 SWEEP=studies/follow-robust/final.json CONFIGS="fw a b" TAG=final bash studies/follow-robust/farm_run.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
for v in OMP_NUM_THREADS MKL_NUM_THREADS OPENBLAS_NUM_THREADS VECLIB_MAXIMUM_THREADS NUMBA_NUM_THREADS; do export $v=1; done
PY=.venv/bin/python
W=${W:-24}
OUT=${OUT:-$HOME/optimist/worktrees/followrobust-out}
CACHE=${CACHE:-$HOME/optimist/worktrees/followrobust-cache}
export NUMBA_CACHE_DIR=$OUT/numba
LOCK=$HOME/optimist/farm.lock
TAG=${TAG:-run}
ARGS=(--cache "$CACHE" --ref "$OUT/ref.json" --out "$OUT/$TAG.jsonl" --sweep "$SWEEP" --jobs "$W")
if [ -n "${CONFIGS:-}" ]; then ARGS+=(--configs $CONFIGS); else ARGS+=(--all-sweep); fi
if [ -n "${SPLIT:-}" ]; then ARGS+=(--split "$SPLIT"); fi
if [ -n "${LIMIT:-}" ]; then ARGS+=(--limit "$LIMIT"); fi
flock "$LOCK" nice -n 19 $PY studies/follow-robust/rtrack.py "${ARGS[@]}" > "$OUT/$TAG.log" 2>&1
echo "FARM RUN DONE $TAG" >> "$OUT/$TAG.log"
