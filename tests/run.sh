#!/bin/bash
# Host tests. Binaries and coverage go to the scratch folder, never into the checkout.
# Needs only a C compiler. The cross-check reads Optimist's sources from the same checkout, at test time.
set -eu
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${JANK_SCRATCH:-$HERE/../worktrees/jank-scratch}/hosttest"
mkdir -p "$OUT"
CC="${CC:-cc}"
FLAGS="-std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I$HERE/core"

CORE="$HERE/core/jank_core.c $HERE/core/jank_ctl.c $HERE/core/jank_ui.c"
LINK="$HERE/../fm1base/link/link.c"
for t in core ctl ui; do
    "$CC" $FLAGS "$HERE/tests/test_$t.c" $CORE -o "$OUT/test_$t"
    "$OUT/test_$t"
done
"$CC" $FLAGS "$HERE/tests/test_log.c" "$HERE/core/jank_log.c" $CORE -o "$OUT/test_log"
"$OUT/test_log"
"$CC" $FLAGS "$HERE/tests/test_clock.c" "$HERE/core/jank_clock.c" -o "$OUT/test_clock"
"$OUT/test_clock"
if [ -f "$HERE/core/jank_peer.c" ]; then
    "$CC" $FLAGS -I"$HERE/../fm1base/link" "$HERE/tests/test_peer.c" "$HERE/core/jank_peer.c" "$HERE/core/jank_clock.c" $CORE "$LINK" -o "$OUT/test_peer"
    "$OUT/test_peer"
fi

OPT="$HERE/../fm1base/wifi"
# The harness pulls in Optimist's FM-1 side of the air format at test time only: nothing of it is copied here or shipped.
"$CC" -std=gnu11 -g -O1 -Wall -fsanitize=address,undefined -fno-sanitize-recover=all -I"$HERE/core" -I"$OPT" -I"$HERE/../fm1base/link" "$HERE/tests/crosscheck_optimist.c" "$HERE/core/jank_peer.c" $CORE "$LINK" -o "$OUT/crosscheck"
"$OUT/crosscheck"

if command -v node >/dev/null; then
    node "$HERE/tests/test_page.js"
else
    echo "page: node not found, control page logic NOT tested"
    exit 1
fi
echo "all host tests passed"
