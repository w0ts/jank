#!/bin/bash
# Repeat heavy-slot chunks of prep_features until every piece of the datasets has its files. Background-safe.
#   nohup bash train/drive_prep.sh > worktrees/train-scratch/logs/drive_prep.log 2>&1 &
set -u
cd "$(dirname "$0")/.."
S=worktrees/train-scratch
mkdir -p $S/logs
SLOT=$HOME/optimist/farm-slot.sh
want() { find "$S/feat_$1" -name "*.$2" 2>/dev/null | wc -l; }
declare -A total=([traktor]=829 [gtzan]=999)
for ds in traktor gtzan; do
    for try in 1 2 3 4 5 6 7 8; do
        have=$(want $ds offline.json)
        echo "$(date +%T) $ds offline $have / ${total[$ds]}"
        [ "$have" -ge $((${total[$ds]} - 12)) ] && break
        "$SLOT" heavy bash train/run_chunk.sh prep_features.py "$ds $S/feat_$ds" --offline
    done
done
echo "$(date +%T) done"
