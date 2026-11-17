#!/bin/bash
# On gpufarm, from the study's farm copy (~/optimist/worktrees/tempochg-code: data/traktor, data/torch and .venv linked to
# the farm's beat-listener checkout, data/mmdb_traktor copied from the Mac). One run at a time under the farm's flock,
# nice 19, one thread per worker. W workers: at most half of what is free (uptime first; Mon-Fri 9:00-17:30 at most 32).
#   W=12 STEPS="extract rate ref base sweep" bash studies/tempo-changes/farm_run.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
for v in OMP_NUM_THREADS MKL_NUM_THREADS OPENBLAS_NUM_THREADS VECLIB_MAXIMUM_THREADS NUMBA_NUM_THREADS; do export $v=1; done
PY=.venv/bin/python
W=${W:-12}
OUT=${OUT:-$HOME/optimist/worktrees/tempochg-out}
LOCK=$HOME/optimist/farm.lock
RUN="flock $LOCK nice -n 19"
SWEEP=${SWEEP:-studies/tempo-changes/sweep1.json}
TAG=${TAG:-1}
for s in ${STEPS:-extract rate ref base sweep}; do
    case $s in
        extract) $RUN $PY studies/tempo-changes/extract_live.py --out "$OUT/cache" --jobs "$W" ;;
        rate)    $RUN $PY studies/tempo-changes/extract_live.py --out "$OUT/cache" --jobs "$W" --rate-only ;;
        ref)     $RUN $PY studies/tempo-changes/reference.py --cache "$OUT/cache" --out "$OUT/ref.json" \
                     --table "$OUT/reference.md" > "$OUT/ref.log" ;;
        base)    $RUN $PY studies/tempo-changes/study.py --cache "$OUT/cache" --ref "$OUT/ref.json" \
                     --out "$OUT/base.jsonl" --configs today m2i m2i+rate weak+rate --jobs "$W" ;;
        sweep)   $RUN $PY studies/tempo-changes/study.py --cache "$OUT/cache" --ref "$OUT/ref.json" \
                     --out "$OUT/sweep$TAG.jsonl" --configs m2i+rate weak+rate --sweep "$SWEEP" --all-sweep \
                     --split dev --jobs "$W" ;;
        final)   $RUN $PY studies/tempo-changes/study.py --cache "$OUT/cache" --ref "$OUT/ref.json" \
                     --out "$OUT/final.jsonl" --configs today m2i m2i+rate --sweep studies/tempo-changes/final.json \
                     --all-sweep --jobs "$W" ;;
        steps)   $RUN $PY studies/tempo-changes/steps.py --cache "$OUT/cache" --ref "$OUT/ref.json" \
                     --out "$OUT/steps_test.jsonl" --configs today m2i m2i+rate m2i+c3 tuned tuned-hard tuned-hard-norate \
                     --sweep studies/tempo-changes/final.json --split test --jobs "$W" ;;
        tuned)   $RUN $PY studies/tempo-changes/study.py --cache "$OUT/cache" --ref "$OUT/ref.json" \
                     --out "$OUT/final_tuned.jsonl" --configs tuned tuned-l16 tuned-hard tuned-hard-norate \
                     --sweep studies/tempo-changes/final.json --jobs "$W"
                 $RUN $PY studies/tempo-changes/steps.py --cache "$OUT/cache" --ref "$OUT/ref.json" \
                     --out "$OUT/steps_tuned.jsonl" --configs tuned tuned-l16 tuned-hard tuned-hard-norate \
                     --sweep studies/tempo-changes/final.json --split test --jobs "$W" ;;
    esac
done
echo FARM RUN DONE
