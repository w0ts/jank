#!/bin/bash
# Set up the evaluation on gpufarm, everything inside this worktree (uv, Python, caches, venv).
# Run through the farm's light slot: ~/optimist/farm-slot.sh light bash scripts/farm_setup.sh
set -euo pipefail
cd "$(dirname "$0")/.."
export UV_CACHE_DIR=$PWD/.uv-cache UV_PYTHON_INSTALL_DIR=$PWD/.uv-python
export MAX_JOBS=1 CMAKE_BUILD_PARALLEL_LEVEL=1 MAKEFLAGS=-j1   # the light slot is one thread
UV=bin/uv
[ -d .venv ] || nice -n 19 $UV venv -q --python 3.11 .venv
PY=.venv/bin/python
nice -n 19 $UV pip install -q --python $PY --extra-index-url https://download.pytorch.org/whl/cpu numpy scipy mir_eval librosa soundfile soxr numba matplotlib pytest "cython<3" torch torchaudio
nice -n 19 $UV pip install -q --python $PY --no-build-isolation "git+https://github.com/CPJKU/madmom"
nice -n 19 $UV pip install -q --python $PY "git+https://github.com/CPJKU/beat_this"
nice -n 19 $UV pip install -q --python $PY --no-deps BeatNet
# GTZAN audio, straight from the mirror (1.2 GB)
if [ ! -d data/gtzan ]; then
    mkdir -p data/gtzan
    curl -sL https://huggingface.co/datasets/marsyas/gtzan/resolve/main/data/genres.tar.gz | tar -xz -C data/gtzan
fi
$PY -m pytest -q tests
$PY -c "import madmom, beat_this, soundfile; from eval.trackers import TRACKERS; print('ok', soundfile.__libsndfile_version__, len(TRACKERS), 'trackers')"
