#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Fetches Freerouting 2.5.0 (github.com/freerouting/freerouting, GPL-3.0) into .freerouting/ (git-ignored) and checks
# its SHA-256; freeroute.py runs it (java 21 or later).
set -eu
cd "$(dirname "$0")"
J=.freerouting/freerouting-2.5.0.jar
SUM=f6f51bb02245e8e717f9359bd260cc9c5c0b1bc0acc8b7cb2cd5b8ffeb5de3c7
mkdir -p .freerouting
[ -f "$J" ] || curl -fsSL -o "$J" https://github.com/freerouting/freerouting/releases/download/v2.5.0/freerouting-2.5.0.jar
echo "$SUM  $J" | shasum -a 256 -c -
