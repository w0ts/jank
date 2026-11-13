#!/bin/bash
# Builds in the scratch folder from a copy, so managed_components, sdkconfig and build/ never appear in the checkout.
# Usage: scripts/build.sh [xiao]   (the Seeed XIAO ESP32-S3, the only board)
set -eu
BOARD="${1:-xiao}"
case "$BOARD" in xiao) ;; *) echo "unknown board $BOARD: xiao"; exit 2 ;; esac
. "$(dirname "$0")/env.sh"
SRC="$JANK_SCRATCH/src/$BOARD"
mkdir -p "$SRC/jank"
rsync -a --delete --exclude /worktrees --exclude .git --exclude build --exclude sdkconfig --exclude managed_components --exclude /third_party/esp_tinyusb "$JANK_ROOT/" "$SRC/jank/"
# esp_tinyusb with its audio class on (the speaker for the beat tracker): the registry's copy plus a patch
"$JANK_ROOT/scripts/patch_tinyusb.sh" "$SRC/jank"
cd "$SRC/jank/firmware"
SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.usb;sdkconfig.$BOARD;sdkconfig.beat" idf.py -B "$JANK_SCRATCH/build-$BOARD" -D SDKCONFIG="$JANK_SCRATCH/sdkconfig.$BOARD" set-target esp32s3
# ninja itself, so that the number of jobs can be held down (JANK_JOBS, default 4) and the build can run niced
nice -n 19 ninja -C "$JANK_SCRATCH/build-$BOARD" -j "${JANK_JOBS:-4}"
# the drive's name: fatfsgen has no option for it, and FAT's label is plain bytes with no checksum (boot sector, root entry)
if [ -f "$JANK_SCRATCH/build-$BOARD/storage.bin" ]; then
    python3 -c "import sys; p = sys.argv[1]; d = open(p, 'rb').read(); n = d.count(b'Espressif  '); open(p, 'wb').write(d.replace(b'Espressif  ', b'JANK       ')); print('drive label JANK:', n, 'places')" "$JANK_SCRATCH/build-$BOARD/storage.bin"
fi
