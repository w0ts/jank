#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Starts JLCONE (JLCPCB's desktop app) in the BACKGROUND with its debugging port 9222, which the quote scripts here
# attach to, and hides its window (mots, 2026-11-19: "make sure it does not pop up in front / steal focus").
# Already listening: nothing to do. JLCONE running WITHOUT the port: say so and stop (it must be quit first).
# Usage: sh scripts/jlcone/start.sh
set -u
PORT=9222
up() { curl -s -m 2 "http://127.0.0.1:$PORT/json/version" >/dev/null 2>&1; }
if up; then echo "JLCONE: already listening on $PORT"; exit 0; fi
if pgrep -xq JLCONE; then
    echo "JLCONE is running without the debugging port: quit it, then run this again"; exit 1
fi
open -g -a JLCONE --args --remote-debugging-port=$PORT          # (-g: launched behind the front app)
for i in $(seq 1 60); do up && break; sleep 1; done
up || { echo "JLCONE: no debugging port after 60 s"; exit 1; }
osascript -e 'tell application "System Events" to set visible of process "JLCONE" to false' >/dev/null 2>&1 || true
echo "JLCONE: listening on $PORT, window hidden"
