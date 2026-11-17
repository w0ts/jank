#!/bin/bash
# On gpufarm: the same extraction with the band-group features (22 per beat), Traktor and GTZAN.
set -euo pipefail
cd "$(dirname "$0")"
PY=../../.venv/bin/python
W=${W:-12}
nice -n 19 $PY extract.py --dataset traktor --jobs "$W" --out results/traktor.pkl
nice -n 19 $PY extract.py --dataset gtzan --jobs "$W" --out results/gtzan.pkl
echo ALL DONE
