#!/bin/bash
# eval_net.py over NSH light-slot shards (one thread each, nice 19).
#   bash train/run_eval.sh DATASET TRACKER NET NSH [NAMES.json]
set -u
cd "$(dirname "$0")/.."
S=worktrees/train-scratch
ds=$1; tracker=$2; net=$3; nsh=$4; names=${5:-}
SLOT=$HOME/optimist/farm-slot.sh
mkdir -p $S/logs
for ((i = 0; i < nsh; i++)); do
    extra=""; [ -n "$names" ] && extra="--names $names"
    nice -n 19 "$SLOT" light env OMP_NUM_THREADS=1 .venv/bin/python -I train/eval_net.py "$ds" "$S/feat_$ds" "$tracker" "$net" $i "$nsh" $extra > "$S/logs/eval_${ds}_${tracker}.$i.log" 2>&1 &
done
wait
echo "$ds $tracker done"
