#!/bin/bash
# On gpufarm: the follower's manoeuvre settings swept on the dev half (real tracks, variations, pitch moves), quick mode
# only, to find the setting that keeps today's steady behaviour and still follows pitch moves. nice 19, one thread per
# worker; NOLOCK=1 beside a long flock holder (Sunday only).
set -euo pipefail
cd "$(dirname "$0")/../.."
for v in OMP_NUM_THREADS MKL_NUM_THREADS OPENBLAS_NUM_THREADS VECLIB_MAXIMUM_THREADS NUMBA_NUM_THREADS; do export $v=1; done
OUT=${OUT:-$HOME/optimist/worktrees/advfollow-out}
RUN="flock $HOME/optimist/farm.lock"; [ "${NOLOCK:-0}" = 1 ] && RUN=""
W=${W:-10}
mkdir -p "$OUT/sweep"
for s in 2.0 2.5 3.0 4.0; do
    for r in 2 3; do
        $RUN nice -n 19 .venv/bin/python studies/advanced-follow/study_eval.py --cache "$OUT/cache" --split dev \
            --modes quick --jobs "$W" --kinds track drums_out vocal_only lpf pitch_step pitch_ramp \
            --follow "{\"MANOEUVRE_SIGMA\": $s, \"MANOEUVRE_RUN\": $r}" --out "$OUT/sweep/s${s}_r${r}.jsonl"
    done
done
$RUN nice -n 19 .venv/bin/python studies/advanced-follow/study_eval.py --cache "$OUT/cache" --split dev --modes today \
    --jobs "$W" --kinds track drums_out vocal_only lpf pitch_step pitch_ramp --out "$OUT/sweep/today.jsonl"
echo SWEEP DONE
