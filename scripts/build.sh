#!/bin/bash
# Builds in the scratch folder from a copy, so managed_components, sdkconfig and build/ never appear in the checkout.
# Usage: scripts/build.sh [xiao|lcd2]   (the Seeed XIAO ESP32-S3; the Waveshare ESP32-S3-LCD-2, docs/lcd2.md)
# JANK_VARIANT names the build (build-<variant>, sdkconfig.<variant>; default the board), JANK_EXTRA_DEFAULTS adds
# sdkconfig fragments from firmware/, ';'-separated, after the board's (e.g. JANK_EXTRA_DEFAULTS=sdkconfig.ble).
set -eu
BOARD="${1:-xiao}"
case "$BOARD" in xiao | lcd2) ;; *) echo "unknown board $BOARD: xiao lcd2"; exit 2 ;; esac
VARIANT="${JANK_VARIANT:-$BOARD}"
EXTRA="${JANK_EXTRA_DEFAULTS:+;$JANK_EXTRA_DEFAULTS}"
. "$(dirname "$0")/env.sh"
SRC="$JANK_SCRATCH/src/$VARIANT"
mkdir -p "$SRC/jank"
rsync -a --delete --exclude /worktrees --exclude .git --exclude build --exclude sdkconfig --exclude managed_components --exclude /third_party/esp_tinyusb "$JANK_ROOT/" "$SRC/jank/"
# esp_tinyusb with its audio class on (the speaker for the beat tracker): the registry's copy plus a patch
"$JANK_ROOT/scripts/patch_tinyusb.sh" "$SRC/jank"
cd "$SRC/jank/firmware"
# set-target cleans the build and makes the sdkconfig afresh: only when there is no configured build yet, or the sdkconfig
# fragments changed (stamp). Otherwise ninja alone decides what is out of date: no change in the code, nothing recompiled.
DEFAULTS="sdkconfig.defaults;sdkconfig.usb;sdkconfig.$BOARD;sdkconfig.beat$EXTRA"
BUILD="$JANK_SCRATCH/build-$VARIANT"
STAMP="$BUILD/.jank-defaults-stamp"
NEW_STAMP="$DEFAULTS $(cd "$SRC/jank/firmware" && for f in $(echo "$DEFAULTS" | tr ';' ' '); do cat "$f"; done | shasum | cut -d' ' -f1)"
if [ ! -f "$BUILD/build.ninja" ] || [ ! -f "$JANK_SCRATCH/sdkconfig.$VARIANT" ] || [ "$(cat "$STAMP" 2>/dev/null)" != "$NEW_STAMP" ]; then
    SDKCONFIG_DEFAULTS="$DEFAULTS" idf.py -B "$BUILD" -D SDKCONFIG="$JANK_SCRATCH/sdkconfig.$VARIANT" set-target esp32s3
    echo "$NEW_STAMP" > "$STAMP"
else
    echo "build configured and the sdkconfig fragments unchanged: no set-target, ninja rebuilds only what changed"
fi
# ninja itself, so that the number of jobs can be held down (JANK_JOBS, default 4) and the build can run niced
nice -n 19 ninja -C "$JANK_SCRATCH/build-$VARIANT" -j "${JANK_JOBS:-4}"
# (no storage.bin any more: the JANK drive is made up by the firmware, drive.c)
