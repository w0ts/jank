#!/bin/bash
# The beat-follow bench build's esp_tinyusb: the registry's 3.0.0 (from the component cache) with third_party/esp_tinyusb-uac2.patch,
# which turns TinyUSB's audio class on (a UAC2 speaker). The result is third_party/esp_tinyusb under the tree given (default: this
# checkout), where firmware/main/idf_component.yml's override_path finds it. Run by scripts/build.sh; never committed.
# Usage: scripts/patch_tinyusb.sh [tree]
set -eu
HERE="$(cd "$(dirname "$0")/.." && pwd)"
TREE="${1:-$HERE}"
. "$HERE/scripts/env.sh"
SRC="$(ls -d "$IDF_COMPONENT_CACHE_PATH"/*/espressif__esp_tinyusb_3.0.0_* 2>/dev/null | head -n 1)"
[ -n "$SRC" ] || { echo "esp_tinyusb 3.0.0 is not in $IDF_COMPONENT_CACHE_PATH: run one build without the override first (git stash the idf_component.yml line)"; exit 1; }
DEST="$TREE/third_party/esp_tinyusb"
rm -rf "$DEST"
mkdir -p "$TREE/third_party"
cp -R "$SRC" "$DEST"
(cd "$DEST" && patch -p1 --silent < "$HERE/third_party/esp_tinyusb-uac2.patch")
echo "esp_tinyusb with the audio class: $DEST"
