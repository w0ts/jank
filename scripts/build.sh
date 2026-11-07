#!/bin/bash
# Builds in the scratch folder from a copy, so managed_components, sdkconfig and build/ never appear in the checkout.
# Usage: scripts/build.sh [atoms3u|xiao|lolin32]   default atoms3u
set -eu
BOARD="${1:-atoms3u}"
case "$BOARD" in atoms3u|xiao) TARGET=esp32s3; USB="sdkconfig.usb;" ;; lolin32) TARGET=esp32; USB="" ;; *) echo "unknown board $BOARD: atoms3u, xiao or lolin32"; exit 2 ;; esac
. "$(dirname "$0")/env.sh"
SRC="$JANK_SCRATCH/src/$BOARD"
mkdir -p "$SRC/jank"
rsync -a --delete --exclude /worktrees --exclude .git --exclude build --exclude sdkconfig --exclude managed_components "$JANK_ROOT/" "$SRC/jank/"
cd "$SRC/jank/firmware"
SDKCONFIG_DEFAULTS="sdkconfig.defaults;${USB}sdkconfig.$BOARD" idf.py -B "$JANK_SCRATCH/build-$BOARD" -D SDKCONFIG="$JANK_SCRATCH/sdkconfig.$BOARD" set-target "$TARGET" build
# the drive's name: fatfsgen has no option for it, and FAT's label is plain bytes with no checksum (boot sector, root entry)
if [ -f "$JANK_SCRATCH/build-$BOARD/storage.bin" ]; then
    python3 -c "import sys; p = sys.argv[1]; d = open(p, 'rb').read(); n = d.count(b'Espressif  '); open(p, 'wb').write(d.replace(b'Espressif  ', b'JANK       ')); print('drive label JANK:', n, 'places')" "$JANK_SCRATCH/build-$BOARD/storage.bin"
fi
