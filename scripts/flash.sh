#!/bin/bash
# Install a firmware image on a running jank stick (an OTA build), never a full flash:
#   1. the JANK drive mounted -> copy the image onto it as jank.bin (fast; docs/usb.md), wait for the restart,
#      print UPDATE.TXT;
#   2. no drive -> the update over MIDI (scripts/ota.py; slower, and no other MIDI client may talk to the stick).
# Usage: scripts/flash.sh IMAGE.bin   (make flash [IMAGE=...])
set -euo pipefail
IMAGE="${1:?usage: scripts/flash.sh IMAGE.bin}"
FW_DIR="${FW_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"
PY="${PY:-python3}"
DRIVE="${JANK_DRIVE:-/Volumes/JANK}"

[ -f "$IMAGE" ] || { echo "no image: $IMAGE"; exit 2; }
# an ESP32 app image starts with the byte 0xE9
[ "$(head -c1 "$IMAGE" | od -An -tx1 | tr -d ' ')" = "e9" ] || { echo "not an ESP32 app image: $IMAGE"; exit 2; }
echo "image: $IMAGE ($(wc -c < "$IMAGE" | tr -d ' ') bytes, $(date -r "$IMAGE" '+%Y-%m-%d %H:%M'))"

if [ -d "$DRIVE" ]; then
    echo "drive: copying onto $DRIVE/jank.bin"
    "$PY" "$FW_DIR/scripts/copy_progress.py" "$IMAGE" "$DRIVE/jank.bin"
    sync
    echo "waiting for the stick to restart (it restarts after 1.5 s of quiet)..."
    gone=0
    for _ in $(seq 1 60); do
        if [ ! -d "$DRIVE" ]; then gone=1; break; fi
        sleep 0.5
    done
    if [ "$gone" = 0 ]; then       # no restart: the stick did not take the copy; the update over MIDI says why, or does it
        echo "the stick did not restart after the copy: updating over MIDI instead (close the Chrome control page and players that talk to the stick)"
        exec "$PY" "$FW_DIR/scripts/ota.py" "$IMAGE"
    fi
    for _ in $(seq 1 60); do [ -f "$DRIVE/UPDATE.TXT" ] && break; sleep 0.5; done
    if [ -f "$DRIVE/UPDATE.TXT" ]; then
        cat "$DRIVE/UPDATE.TXT"
    else
        echo "the drive did not come back within 30 s: look at the stick (UPDATE.TXT tells what happened)"
        exit 1
    fi
else
    echo "no $DRIVE drive: updating over MIDI (close the Chrome control page and players that talk to the stick)"
    exec "$PY" "$FW_DIR/scripts/ota.py" "$IMAGE"
fi
