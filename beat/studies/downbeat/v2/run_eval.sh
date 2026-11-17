#!/bin/bash
# On gpufarm after run_farm3.sh: descriptors (build.py) then the experiments (evaluate2.py) -> ../results/v2_*.
# J workers / threads: at most half of what is free on the farm. REF_LAG: one constant (s), the tracker's measured lag
# behind the reference beats, applied to the reference beats before describing them.
set -euo pipefail
cd "$(dirname "$0")"
export BL_REPO=${BL_REPO:-$HOME/optimist/worktrees/beat-listener}
export PYTHONPATH=..:$BL_REPO
for v in OMP_NUM_THREADS MKL_NUM_THREADS OPENBLAS_NUM_THREADS NUMBA_NUM_THREADS; do export $v=1; done
PY=$BL_REPO/.venv/bin/python
J=${J:-6}
CACHE=${CACHE:-../cache}
REF_LAG=${REF_LAG:--0.033}
if [ -z "${SKIP_BUILD:-}" ]; then
    for ds in traktor gtzan; do
        nice -n 19 $PY build.py --dataset $ds --jobs "$J" --cache "$CACHE" --out "$CACHE/${ds}_desc.pkl" --ref-lag "$REF_LAG"
    done
fi
nice -n 19 $PY evaluate3.py --cache "$CACHE" --jobs "$J" ${QUICK:+--quick} ${SHIP:+--ship $SHIP}
echo EVAL DONE
