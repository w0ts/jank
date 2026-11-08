#!/bin/bash
# Host tests. Binaries and coverage go to the scratch folder, never into the checkout.
# Needs only a C compiler. The cross-check reads Optimist's fm1-base sources at test time: FM1BASE, default ../optimist/fm1base.
set -eu
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${JANK_SCRATCH:-$HERE/worktrees/jank-scratch}/hosttest"
mkdir -p "$OUT"
CC="${CC:-cc}"
FLAGS="-std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I$HERE/core"

CORE="$HERE/core/jank_core.c $HERE/core/jank_ctl.c $HERE/core/jank_ui.c"
LINK="$HERE/third_party/link/link.c"
for t in core ctl ui; do
    "$CC" $FLAGS "$HERE/tests/test_$t.c" $CORE -o "$OUT/test_$t"
    "$OUT/test_$t"
done
"$CC" $FLAGS "$HERE/tests/test_clock.c" "$HERE/core/jank_clock.c" -o "$OUT/test_clock"
"$CC" $FLAGS "$HERE/tests/test_sysex.c" "$HERE/core/jank_sysex.c" -o "$OUT/test_sysex"
"$CC" $FLAGS "$HERE/tests/test_net.c" "$HERE/core/jank_net.c" -o "$OUT/test_net"
"$OUT/test_net"
"$OUT/test_sysex"
"$OUT/test_clock"
if [ -f "$HERE/core/jank_peer.c" ]; then
    "$CC" $FLAGS -I"$HERE/third_party/link" "$HERE/tests/test_peer.c" "$HERE/core/jank_peer.c" "$HERE/core/jank_clock.c" $CORE "$LINK" -o "$OUT/test_peer"
    "$OUT/test_peer"
fi

FM1BASE="${FM1BASE:-$HERE/../optimist/fm1base}"
if [ ! -f "$FM1BASE/wifi/wifi_jam.c" ]; then echo "crosscheck: no fm1-base Wi-Fi sources at $FM1BASE (set FM1BASE to an Optimist checkout's fm1base)"; exit 1; fi
OPT="$FM1BASE/wifi"
# The harness pulls in Optimist's FM-1 side of the air format at test time only: nothing of it is copied here or shipped.
"$CC" -std=gnu11 -g -O1 -Wall -fsanitize=address,undefined -fno-sanitize-recover=all -I"$HERE/core" -I"$OPT" -I"$HERE/third_party/link" "$HERE/tests/crosscheck_optimist.c" "$HERE/core/jank_peer.c" $CORE "$LINK" -o "$OUT/crosscheck"
"$OUT/crosscheck"

if command -v node >/dev/null; then
    node "$HERE/tests/test_page.js"
else
    echo "page: node not found, control page logic NOT tested"
    exit 1
fi
echo "all host tests passed"
