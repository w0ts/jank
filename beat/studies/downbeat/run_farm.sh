#!/bin/bash
# On gpufarm, from this folder: the online beats + features for Traktor and GTZAN, then madmom's offline downbeat
# tracker on a quarter of Traktor (its DBN takes up to ~5.5 GB per worker, so fewer workers).
set -euo pipefail
cd "$(dirname "$0")"
PY=../../.venv/bin/python
W=${W:-16}
WDB=${WDB:-8}
nice -n 19 $PY extract.py --dataset traktor --jobs "$W" --out results/traktor.pkl
nice -n 19 $PY extract.py --dataset gtzan --jobs "$W" --out results/gtzan.pkl
nice -n 19 $PY extract.py --dataset traktor --shard 0/4 --jobs "$WDB" --madmom-db --out results/traktor_mmdb.0of4.pkl
echo ALL DONE
