#!/bin/bash
# DJ sets / live recordings on gpufarm: decode, then the two offline references and the live tracker, through the
# farm's heavy slot (JOBS = the farm cap, 20 min per job, so 3 shards per tracker), then score.
# Shards whose result file exists are skipped, so a rerun only does what is missing.
set -uo pipefail
cd "$(dirname "$0")/.."
PY=$PWD/.venv/bin/python
SLOT=$HOME/optimist/farm-slot.sh
mkdir -p logs
job() { echo "$(date +%T) load $(cut -d' ' -f1 /proc/loadavg): $*"; nice -n 19 "$SLOT" heavy "$PY" "$@" >> "logs/mixes-$(date +%F).log" 2>&1 || echo "  FAILED (exit $?): $*"; }
[ -f data/mixes/index.json ] || job -m eval.mixes prep --src data/mixes_src --out data/mixes
for t in madmom_online beat_this_final0 madmom_dbn_offline_chunked; do
    for s in 0 1 2; do
        [ -f "runs/mixes.${s}of3_$t.json" ] && continue
        job -m eval.run --dataset mixes --shard "$s/3" --trackers "$t"
    done
done
nice -n 19 "$PY" -m eval.mix_eval 2>&1 | grep -v -i warn
