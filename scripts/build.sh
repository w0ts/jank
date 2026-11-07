#!/bin/bash
# Builds in the scratch folder from a copy, so managed_components, sdkconfig and build/ never appear in the checkout.
# Usage: scripts/build.sh [atoms3u|xiao|lolin32]   default atoms3u
set -eu
BOARD="${1:-atoms3u}"
case "$BOARD" in atoms3u|xiao) TARGET=esp32s3; USB="sdkconfig.usb;" ;; lolin32) TARGET=esp32; USB="" ;; *) echo "unknown board $BOARD: atoms3u, xiao or lolin32"; exit 2 ;; esac
. "$(dirname "$0")/env.sh"
SRC="$JANK_SCRATCH/src/$BOARD"
mkdir -p "$SRC/jank" "$SRC/fm1base/link"
rsync -a --delete --exclude build --exclude sdkconfig --exclude managed_components "$JANK_ROOT/" "$SRC/jank/"
rsync -a "$JANK_ROOT/../fm1base/link/" "$SRC/fm1base/link/"
cd "$SRC/jank/firmware"
SDKCONFIG_DEFAULTS="sdkconfig.defaults;${USB}sdkconfig.$BOARD" idf.py -B "$JANK_SCRATCH/build-$BOARD" -D SDKCONFIG="$JANK_SCRATCH/sdkconfig.$BOARD" set-target "$TARGET" build
