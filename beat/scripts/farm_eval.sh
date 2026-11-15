#!/bin/bash
# The evaluation on gpufarm. Every job goes through the farm's heavy slot (JOBS = the farm's cap, 20 min max per job).
#   bash scripts/farm_eval.sh gtzan [tracker...]   the trackers not yet run on all of GTZAN (or the ones named)
#   bash scripts/farm_eval.sh traktor   decode the user's tracks, then every tracker in 4 shards
set -uo pipefail
cd "$(dirname "$0")/.."
PY=$PWD/.venv/bin/python
SLOT=$HOME/optimist/farm-slot.sh
mkdir -p runs logs
job() { echo "$(date +%T) load $(cut -d' ' -f1 /proc/loadavg): $*"; nice -n 19 "$SLOT" heavy "$PY" "$@" >> "logs/$(date +%F).log" 2>&1 || echo "  FAILED (exit $?): $*"; }

stage=${1:?gtzan or traktor}
case "$stage" in
gtzan)
    shift
    for t in ${@:-madmom_dbn_offline madmom_downbeat_offline madmom_online beatnet_online_pf}; do
        job -m eval.run --dataset gtzan --trackers "$t"
    done ;;
traktor)
    [ -f data/traktor/index.json ] || job -m eval.traktor prep --nml data/collection.nml --tracks tracks --out data/traktor
    # live trackers only (the product hears a live feed); offline ones stay a GTZAN reference
    for t in madmom_online beatnet_online_pf; do
        for s in 0 1 2 3; do job -m eval.run --dataset traktor --shard "$s/4" --trackers "$t"; done
    done
    # streaming Beat This! recomputes a 10 s window every 0.2 s: 100 tracks, 2 min from the middle of each
    for s in 0 1 2 3; do job -m eval.lookahead --dataset traktor --every 8 --excerpt 120 --shard "$s/4"; done ;;
esac
echo "$(date +%T) done: $stage"
