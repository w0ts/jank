#!/bin/bash
# Host tests. Binaries and coverage go to the scratch folder, never into the checkout.
# Needs only a C compiler. The cross-check reads Optimist's fm1-base sources at test time: FM1BASE, default ../optimist/fm1base.
set -eu
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${JANK_SCRATCH:-$HERE/worktrees/jank-scratch}/hosttest"
mkdir -p "$OUT"
CC="${CC:-cc}"
FLAGS="-std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I$HERE/core"

CORE="$HERE/core/jank_core.c $HERE/core/jank_ctl.c $HERE/core/jank_ui.c $HERE/core/jank_phone.c $HERE/core/jank_usbrole.c $HERE/core/jank_hold.c $HERE/core/jank_fold.c"
LINK="$HERE/third_party/link/link.c $HERE/core/jank_start.c"
for t in core ctl ui; do
    "$CC" $FLAGS "$HERE/tests/test_$t.c" $CORE -o "$OUT/test_$t"
    "$OUT/test_$t"
done
"$CC" $FLAGS "$HERE/tests/test_clock.c" "$HERE/core/jank_clock.c" -o "$OUT/test_clock"
"$CC" $FLAGS "$HERE/tests/test_sysex.c" "$HERE/core/jank_sysex.c" -o "$OUT/test_sysex"
"$CC" $FLAGS "$HERE/tests/test_net.c" "$HERE/core/jank_net.c" -o "$OUT/test_net"
"$CC" $FLAGS "$HERE/tests/test_ota.c" "$HERE/core/jank_ota.c" -o "$OUT/test_ota"
"$OUT/test_ota"
"$CC" $FLAGS "$HERE/tests/ota_replay.c" "$HERE/core/jank_ota.c" -o "$OUT/ota_replay"
python3 "$HERE/tests/test_ota.py" "$OUT/ota_replay" "$OUT/ota"
"$OUT/test_net"
"$OUT/test_sysex"
"$CC" $FLAGS "$HERE/tests/test_blemidi.c" "$HERE/core/jank_blemidi.c" "$HERE/core/jank_sysex.c" -o "$OUT/test_blemidi"
"$OUT/test_blemidi"
# the packets the firmware sends, for the page's port of the framing (tests/test_page.js)
"$OUT/test_blemidi" --vectors "$OUT/blemidi_vectors.json"
# the phone link (Bluetooth on/off, the HOME line) and the made-up JANK drive that streams jank.bin into the spare slot
"$CC" $FLAGS "$HERE/tests/test_phone.c" $CORE -o "$OUT/test_phone"
"$OUT/test_phone"
"$CC" $FLAGS "$HERE/tests/test_vdrive.c" "$HERE/core/jank_vdrive.c" "$HERE/core/jank_ota.c" -o "$OUT/test_vdrive"
"$OUT/test_vdrive"
"$OUT/test_clock"
if [ -f "$HERE/core/jank_peer.c" ]; then
    "$CC" $FLAGS -I"$HERE/third_party/link" "$HERE/tests/test_peer.c" "$HERE/core/jank_peer.c" "$HERE/core/jank_clock.c" $CORE $LINK -o "$OUT/test_peer"
    "$OUT/test_peer"
fi

# USB host mode: the port's role, the TUSB320 CC chip, a hosted device's audio descriptors and its PCM.
"$CC" $FLAGS "$HERE/tests/test_usbrole.c" "$HERE/core/jank_usbrole.c" -o "$OUT/test_usbrole"
"$OUT/test_usbrole"
"$CC" $FLAGS "$HERE/tests/test_tusb320.c" "$HERE/core/jank_tusb320.c" -o "$OUT/test_tusb320"
"$OUT/test_tusb320"
"$CC" $FLAGS -I"$HERE/firmware/main" "$HERE/tests/test_uacdesc.c" "$HERE/core/jank_uacdesc.c" -o "$OUT/test_uacdesc"
"$OUT/test_uacdesc"
"$CC" $FLAGS "$HERE/tests/test_midiq.c" "$HERE/core/jank_midiq.c" -o "$OUT/test_midiq"
"$OUT/test_midiq"
"$CC" $FLAGS "$HERE/tests/test_pcm.c" "$HERE/core/jank_pcm.c" -lm -o "$OUT/test_pcm"
"$OUT/test_pcm"

# The USB speaker's descriptor (firmware/main/usb_audio.h: macros only).
"$CC" $FLAGS -I"$HERE/firmware/main" "$HERE/tests/test_uac_desc.c" -o "$OUT/test_uac_desc"
"$OUT/test_uac_desc"

# Sound on the source (AUDIO mode) and the input meter; AUDIO mode's Stop / Start of the outputs.
"$CC" $FLAGS "$HERE/tests/test_live.c" "$HERE/core/jank_live.c" "$HERE/core/jank_hold.c" -lm -o "$OUT/test_live"
"$CC" $FLAGS "$HERE/tests/test_fold.c" "$HERE/core/jank_fold.c" -lm -o "$OUT/test_fold"
"$OUT/test_fold"
"$OUT/test_live"

# The beat tracker as the Link session's master.
"$CC" $FLAGS -I"$HERE/third_party/link" "$HERE/tests/test_beatlink.c" "$HERE/core/jank_beatlink.c" $LINK -lm -o "$OUT/test_beatlink"
"$OUT/test_beatlink"

# The beat tracker. BEAT_WAV_DIR (mono 44.1 kHz WAVs, see test_beat.c) adds the run on audio against madmom's results.
"$CC" $FLAGS -std=c11 -I"$HERE/core/beat" "$HERE/tests/test_beat.c" "$HERE"/core/beat/*.c -lm -o "$OUT/test_beat"
"$OUT/test_beat" "$HERE/tests/beat_ref"

# AUTO BAR: the downbeat estimator against beat-listener's live/downbeat.py (golden vectors, tests/down_ref).
"$CC" $FLAGS -I"$HERE/core/beat" "$HERE/tests/test_down.c" "$HERE/core/beat/jb_down.c" "$HERE/core/beat/down_model.c" -lm -o "$OUT/test_down"
"$OUT/test_down" "$HERE/tests/down_ref"

FM1BASE="${FM1BASE:-$HERE/../optimist/fm1base}"
if [ ! -f "$FM1BASE/wifi/wifi_jank.c" ]; then echo "crosscheck: no fm1-base Wi-Fi sources at $FM1BASE (set FM1BASE to an Optimist checkout's fm1base)"; exit 1; fi
OPT="$FM1BASE/wifi"
# The harness pulls in Optimist's FM-1 side of the air format at test time only: nothing of it is copied here or shipped.
"$CC" -std=gnu11 -g -O1 -Wall -fsanitize=address,undefined -fno-sanitize-recover=all -I"$HERE/core" -I"$OPT" -I"$HERE/third_party/link" "$HERE/tests/crosscheck_optimist.c" "$HERE/core/jank_peer.c" $CORE $LINK -o "$OUT/crosscheck"
"$OUT/crosscheck"

if command -v node >/dev/null; then
    BLEMIDI_VECTORS="$OUT/blemidi_vectors.json" node "$HERE/tests/test_page.js"
else
    echo "page: node not found, control page logic NOT tested"
    exit 1
fi
echo "all host tests passed"
