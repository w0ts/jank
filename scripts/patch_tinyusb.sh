#!/bin/bash
# The beat-follow bench build's esp_tinyusb: the registry's 3.0.0 (from the component cache) with third_party/esp_tinyusb-uac2.patch,
# which turns TinyUSB's audio class on (a UAC2 speaker), and third_party/esp_tinyusb-msc-weak.patch, which makes its mass storage
# callbacks (tinyusb_msc.c) weak so that the app's own take their place, and third_party/esp_tinyusb-dwc2-setup-guard.patch, which
# drops a SETUP whose DMA pointer is outside the setup buffer (docs/usb.md, "The EP0 SETUP crash"). The result is third_party/esp_tinyusb under the tree given (default: this
# checkout), where firmware/main/idf_component.yml's override_path finds it. Run by scripts/build.sh; never committed.
# Usage: scripts/patch_tinyusb.sh [tree]
set -eu
HERE="$(cd "$(dirname "$0")/.." && pwd)"
TREE="${1:-$HERE}"
. "$HERE/scripts/env.sh"
SRC="$(ls -d "$IDF_COMPONENT_CACHE_PATH"/*/espressif__esp_tinyusb_3.0.0_* 2>/dev/null | head -n 1)"
[ -n "$SRC" ] || { echo "esp_tinyusb 3.0.0 is not in $IDF_COMPONENT_CACHE_PATH: run one build without the override first (git stash the idf_component.yml line)"; exit 1; }
DEST="$TREE/third_party/esp_tinyusb"
# the copy is made again only when the registry's copy or a patch is not what it was made from: new files would get new
# modification times and ninja would compile all of esp_tinyusb again on every build
STAMP="$DEST/.jank-patch-stamp"
NEW_STAMP="$SRC $(cat "$HERE/third_party/esp_tinyusb-uac2.patch" "$HERE/third_party/esp_tinyusb-msc-weak.patch" "$HERE/third_party/esp_tinyusb-dwc2-setup-guard.patch" | shasum | cut -d' ' -f1)"
if [ -d "$DEST" ] && [ "$(cat "$STAMP" 2>/dev/null)" = "$NEW_STAMP" ]; then
    echo "esp_tinyusb with the audio class and weak MSC callbacks: $DEST (as it was)"
    exit 0
fi
rm -rf "$DEST"
mkdir -p "$TREE/third_party"
cp -R "$SRC" "$DEST"
(cd "$DEST" && patch -p1 --silent < "$HERE/third_party/esp_tinyusb-uac2.patch")
# its TinyUSB mass storage callbacks weak: firmware/main/drive.c serves a made-up volume with its own (the streaming drive)
(cd "$DEST" && patch -p1 --silent < "$HERE/third_party/esp_tinyusb-msc-weak.patch")
# the DWC2 device driver drops a SETUP whose DMA pointer is not in the setup buffer (a read of address 0 crashed the boot)
(cd "$DEST" && patch -p1 --silent < "$HERE/third_party/esp_tinyusb-dwc2-setup-guard.patch")
echo "$NEW_STAMP" > "$STAMP"
echo "esp_tinyusb with the audio class and weak MSC callbacks: $DEST"
