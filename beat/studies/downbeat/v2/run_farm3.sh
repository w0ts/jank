#!/bin/bash
# On gpufarm, from the study's farm copy (worktrees/downbeat2/v2 under the farm's beat-listener checkout): cache the
# online beats and log spectrograms of Traktor and GTZAN (extract2.py), one run at a time under flock, nice 19.
# W workers x 1 thread each: take at most half of what is free on the farm (check uptime first).
set -euo pipefail
cd "$(dirname "$0")"
for v in OMP_NUM_THREADS MKL_NUM_THREADS OPENBLAS_NUM_THREADS VECLIB_MAXIMUM_THREADS NUMBA_NUM_THREADS; do export $v=1; done
export BL_REPO=${BL_REPO:-$HOME/optimist/worktrees/beat-listener}
PY=$BL_REPO/.venv/bin/python
W=${W:-14}
CACHE=${CACHE:-../cache}
LIMIT=${LIMIT:+--limit $LIMIT}
for ds in traktor gtzan; do
    flock ../farm.lock nice -n 19 $PY extract2.py --dataset $ds --jobs "$W" --cache "$CACHE" $LIMIT
done
echo ALL DONE

# then the measuring instrument (madmom offline downbeats; ~5.5 GB per worker, so fewer workers)
flock ../farm.lock nice -n 19 $PY mmdb.py --jobs "${WDB:-8}" --cache "$CACHE" $LIMIT
echo MMDB DONE
