#!/bin/bash
# One heavy-slot chunk (the slot kills it after 20 min): JOBS shards of a python module, nice 19, one thread each.
#   farm-slot.sh heavy bash train/run_chunk.sh prep_features.py ARGS-before-shard...  (shard args appended)
# Usage: run_chunk.sh SCRIPT "ARGS" [EXTRA]      -> SCRIPT ARGS <shard> <JOBS> EXTRA
set -u
cd "$(dirname "$0")/.."
n=${JOBS:-12}
script=$1; args=$2; extra=${3:-}
for ((i = 0; i < n; i++)); do
    nice -n 19 env OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 .venv/bin/python -I "train/$script" $args $i $n $extra > "worktrees/train-scratch/logs/${script%.py}.$i.log" 2>&1 &
done
wait
