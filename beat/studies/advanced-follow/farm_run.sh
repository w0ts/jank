#!/bin/bash
# On gpufarm, from the study's farm copy (~/optimist/worktrees/advfollow-code, data and .venv linked to the farm's
# beat-listener checkout): the pitch-step statistics, then the material's cache. One run at a time under the farm's
# flock, nice 19, one thread per worker. W workers: at most half of what is free (check uptime first).
#   W=20 STEPS="steps extract" bash studies/advanced-follow/farm_run.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
for v in OMP_NUM_THREADS MKL_NUM_THREADS OPENBLAS_NUM_THREADS VECLIB_MAXIMUM_THREADS NUMBA_NUM_THREADS; do export $v=1; done
PY=.venv/bin/python
W=${W:-16}
OUT=${OUT:-$HOME/optimist/worktrees/advfollow-out}
LOCK=$HOME/optimist/farm.lock
RUN="flock $LOCK"; [ "${NOLOCK:-0}" = 1 ] && RUN=""      # NOLOCK=1: beside a long flock holder, Sunday only
mkdir -p "$OUT/steps" "$OUT/stepcache" "$OUT/cache"
for s in ${STEPS:-steps extract}; do
    case $s in
        steps)
            for c in 0.0 0.01 0.02 0.03 0.04 0.05 0.065 0.08; do
                n=150; { [ "$c" = 0.02 ] || [ "$c" = 0.0 ]; } && n=300
                $RUN nice -n 19 $PY studies/advanced-follow/step_stats.py --cases $n --change $c --jobs "$W" \
                    --cache "$OUT/stepcache" --out "$OUT/steps/step_$c.jsonl"
            done
            for c in 0.02 0.05 0.08; do
                for r in 2 8 30; do
                    $RUN nice -n 19 $PY studies/advanced-follow/step_stats.py --cases 100 --change $c --ramp $r \
                        --jobs "$W" --cache "$OUT/stepcache" --out "$OUT/steps/ramp_${c}_${r}.jsonl"
                done
            done ;;
        extract)
            $RUN nice -n 19 $PY studies/advanced-follow/extract.py --out "$OUT/cache" --jobs "$W" ;;
    esac
done
echo FARM RUN DONE
