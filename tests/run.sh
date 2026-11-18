#!/bin/bash
# Host tests. Binaries and coverage go to the scratch folder, never into the checkout.
# Needs only a C compiler. The cross-check reads Optimist's fm1-base sources at test time: FM1BASE, default ../optimist/fm1base.
set -eu
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${JANK_SCRATCH:-$HERE/worktrees/jank-scratch}/hosttest"
mkdir -p "$OUT"
CC="${CC:-cc}"
FLAGS="-std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I$HERE/core"

CORE="$HERE/core/jank_core.c $HERE/core/jank_ctl.c $HERE/core/jank_ui.c $HERE/core/jank_home.c $HERE/core/jank_usbrole.c $HERE/core/jank_hold.c $HERE/core/jank_fold.c $HERE/core/jank_lat.c"
LINK="$HERE/third_party/link/link.c $HERE/core/jank_start.c"
for t in core ctl ui lat; do
    "$CC" $FLAGS "$HERE/tests/test_$t.c" $CORE -o "$OUT/test_$t"
    "$OUT/test_$t"
done
# The device's screen, menus and buttons against the browser emulator's own pixels (tests/oled_ref: gen_cases.js runs
# player.html's code in node; PLAYER_HTML set = make the cases again from it and check they are the ones kept here), and
# the LED language of a unit without a screen. No contraction into fused multiply-adds: the emulator's numbers are
# JavaScript's, rounded after each operation.
OLED="$HERE/core/jank_oled_gfx.c $HERE/core/jank_oled_menu.c $HERE/core/jank_oled_press.c $HERE/core/jank_oled_draw.c $HERE/core/jank_oled_lcd2.c $HERE/core/jank_oled_qr.c $HERE/core/jank_oled_input.c"
if [ -n "${PLAYER_HTML:-}" ]; then
    node "$HERE/tests/oled_ref/gen_cases.js" "$PLAYER_HTML" > "$OUT/oled_cases.txt"
    cmp "$OUT/oled_cases.txt" "$HERE/tests/oled_ref/cases.txt" || { echo "oled: tests/oled_ref/cases.txt is not what $PLAYER_HTML draws: make it again"; exit 1; }
fi
# the colour layout's references come from ui/oled.js itself: made again here, they must be the ones kept (else: make them again and
# look at what the C layout does with the change)
if command -v node >/dev/null; then
    node "$HERE/tests/oled_ref/gen_cases.js" --ink > "$OUT/oled_cases_ink.txt" 2>/dev/null
    cmp "$OUT/oled_cases_ink.txt" "$HERE/tests/oled_ref/cases_ink.txt" || { echo "oled: tests/oled_ref/cases_ink.txt is not what ui/oled.js draws: make it again (gen_cases.js --ink)"; exit 1; }
fi
"$CC" $FLAGS -ffp-contract=off "$HERE/tests/test_oled.c" $OLED -o "$OUT/test_oled"
"$OUT/test_oled" "$HERE/tests/oled_ref/cases.txt" "$HERE/tests/oled_ref/cases_ink.txt"
"$CC" $FLAGS -ffp-contract=off "$HERE/tests/test_led.c" "$HERE/core/jank_led.c" $OLED $CORE -o "$OUT/test_led"
"$OUT/test_led"
"$CC" $FLAGS "$HERE/tests/test_clock.c" "$HERE/core/jank_clock.c" -o "$OUT/test_clock"
"$CC" $FLAGS "$HERE/tests/test_lcd.c" "$HERE/core/jank_lcd.c" -o "$OUT/test_lcd"
"$OUT/test_lcd"
"$CC" $FLAGS "$HERE/tests/test_sysex.c" "$HERE/core/jank_sysex.c" -o "$OUT/test_sysex"
"$CC" $FLAGS "$HERE/tests/test_net.c" "$HERE/core/jank_net.c" -o "$OUT/test_net"
"$CC" $FLAGS "$HERE/tests/test_ota.c" "$HERE/core/jank_ota.c" -o "$OUT/test_ota"
"$OUT/test_ota"
"$CC" $FLAGS "$HERE/tests/ota_replay.c" "$HERE/core/jank_ota.c" -o "$OUT/ota_replay"
python3 "$HERE/tests/test_ota.py" "$OUT/ota_replay" "$OUT/ota"
"$OUT/test_net"
"$OUT/test_sysex"
# the HOME line and the made-up JANK drive that streams jank.bin into the spare slot
"$CC" $FLAGS "$HERE/tests/test_home.c" $CORE -o "$OUT/test_home"
"$OUT/test_home"
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
# LINE IN: the ADC framing, the timestamp fit, DC / gain / level, the cell, the cubic resampler and the chain on clicks
"$CC" $FLAGS "$HERE/tests/test_line.c" "$HERE/core/jank_line.c" "$HERE/core/jank_pcm.c" -lm -o "$OUT/test_line"
"$OUT/test_line"

# The USB speaker's descriptor (firmware/main/usb_audio.h: macros only).
"$CC" $FLAGS -I"$HERE/firmware/main" "$HERE/tests/test_uac_desc.c" -o "$OUT/test_uac_desc"
"$OUT/test_uac_desc"

# Sound on the source (AUDIO mode) and the input meter; AUDIO mode's Stop / Start of the outputs.
"$CC" $FLAGS "$HERE/tests/test_live.c" "$HERE/core/jank_live.c" "$HERE/core/jank_hold.c" -lm -o "$OUT/test_live"
"$CC" $FLAGS "$HERE/tests/test_fold.c" "$HERE/core/jank_fold.c" -lm -o "$OUT/test_fold"
# LOW CONFIDENCE: the grid-fit confidence and its gate against beat-listener's live/confidence.py (golden vectors, tests/conf_ref).
"$CC" $FLAGS -I"$HERE/core/beat" "$HERE/tests/test_conf.c" "$HERE/core/jank_conf.c" "$HERE/core/beat/jb_mode.c" -lm -o "$OUT/test_conf"
"$OUT/test_conf" "$HERE/tests/conf_ref"
# Advanced beat follow (M2i): the tempo glide, the line confidence and the spectral rate against beat-listener's
# eval/follow_bpm.py, live/follow.py and live/rate.py (golden vectors, tests/follow_ref).
"$CC" $FLAGS -I"$HERE/core/beat" "$HERE/tests/test_follow.c" "$HERE/core/jank_conf.c" "$HERE/core/beat/jb_follow.c" "$HERE/core/beat/jb_mode.c" "$HERE/core/beat/jb_rate.c" "$HERE/core/beat/jb_fe.c" "$HERE/core/beat/beat_model.c" -lm -o "$OUT/test_follow"
"$OUT/test_follow" "$HERE/tests/follow_ref" "$HERE/tests/tempo_ref"
# The tuned follow (M2j): the weak-signal hold, the fast jump, the weak confidence and gate against beat-listener's
# studies/tempo-changes chain (tuned-l16; golden vectors, tests/tempo_ref, scripts/tempo_golden.py).
"$CC" $FLAGS -I"$HERE/core/beat" "$HERE/tests/test_tempo.c" "$HERE/core/jank_conf.c" "$HERE/core/beat/jb_follow.c" "$HERE/core/beat/jb_weak.c" "$HERE/core/beat/jb_mode.c" -lm -o "$OUT/test_tempo"
"$OUT/test_tempo" "$HERE/tests/tempo_ref"
# The robust follower, all-d-f60 (M2m): the comb checks, the DBN's tempo prior, the lock and jump gate, the agreement
# confidence and the coast against beat-listener's studies/follow-robust (golden vectors, tests/robust_ref, scripts/robust_golden.py);
# and SKIP (jb_skip.c: ACT / PAT) with the graded shown confidence (jb_graded.c) against its skip configs (tests/skip_ref, robust_golden.py --skip-out).
"$CC" $FLAGS -I"$HERE/core/beat" "$HERE/tests/test_robust.c" "$HERE/core/jank_conf.c" "$HERE/core/beat/jb_follow.c" "$HERE/core/beat/jb_weak.c" "$HERE/core/beat/jb_mode.c" "$HERE/core/beat/jb_dbn.c" "$HERE/core/beat/jb_comb.c" "$HERE/core/beat/jb_robust.c" "$HERE/core/beat/jb_skip.c" "$HERE/core/beat/jb_graded.c" -lm -o "$OUT/test_robust"
"$OUT/test_robust" "$HERE/tests/robust_ref" "$HERE/tests/skip_ref"
"$OUT/test_fold"
"$OUT/test_live"

# The beat tracker as the Link session's master.
"$CC" $FLAGS -I"$HERE/third_party/link" "$HERE/tests/test_beatlink.c" "$HERE/core/jank_beatlink.c" "$HERE/core/jank_clock.c" $LINK -lm -o "$OUT/test_beatlink"
"$OUT/test_beatlink"
# AUDIO mode alone: a session of its own led by the tracker, the MIDI clock on its bar line; peers coming and going.
"$CC" $FLAGS -I"$HERE/third_party/link" "$HERE/tests/test_solo.c" "$HERE/core/jank_peer.c" "$HERE/core/jank_beatlink.c" "$HERE/core/jank_hold.c" "$HERE/core/jank_clock.c" $LINK -lm -o "$OUT/test_solo"
"$OUT/test_solo"

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
    node "$HERE/tests/test_page.js"
    node "$HERE/tests/test_stick.js"
    node "$HERE/tests/test_lcd2.js"
    node "$HERE/tests/test_lcd_palette.js"
    node "$HERE/tests/test_player.js"
    node "$HERE/tests/test_tags.js"
    node "$HERE/tests/test_connect.js"
else
    echo "page: node not found, control page logic NOT tested"
    exit 1
fi
echo "all host tests passed"
