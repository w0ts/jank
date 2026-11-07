#!/bin/bash
# Installs ESP-IDF inside the scratch folder only. Python 3.13: Homebrew's default 3.15 is too new for IDF's pinned wheels.
set -eu
S="$(cd "$(dirname "$0")/.." && pwd)/worktrees/jank-scratch"; mkdir -p "$S"
mkdir -p "$S/pybin" "$S/pipcache"
ln -sf /opt/homebrew/bin/python3.13 "$S/pybin/python3"
ln -sf /opt/homebrew/bin/python3.13 "$S/pybin/python"
export PATH="$S/pybin:$PATH"
export IDF_TOOLS_PATH="$S/espressif"
export PIP_CACHE_DIR="$S/pipcache"
if [ ! -d "$S/esp-idf/.git" ]; then
    git clone --depth 1 --branch v5.5.6 --recurse-submodules --shallow-submodules https://github.com/espressif/esp-idf.git "$S/esp-idf"
fi
"$S/esp-idf/install.sh" esp32s3
echo INSTALL_DONE
