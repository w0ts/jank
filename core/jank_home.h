/* SPDX-License-Identifier: GPL-3.0-only */
/* The HOME line: the page reads it with the control line's `home` and shows the session's tempo, bar, level and confidence. */
#ifndef JANK_HOME_H
#define JANK_HOME_H
#include <stddef.h>
#include <stdint.h>
#include "jank_core.h"

#define JHM_LINE_MAX 63u               /* characters: the control line's buffer is one more */

/* The HOME line, the control line's `home` answer (`home=h ...`), space separated:
 *   h <bpm x100> <net> <transport> <bar> <quantum> <link peers> <air peers> <battery> <level> <conf> <min> <max> <mode>
 *   bpm x100   the session's tempo, 0 with no session
 *   net        j<group> in MODE JANK; w1 on the Wi-Fi network, w0 still looking for it
 *   transport  0 stopped, 1 playing, 2 a start armed for a bar line ahead (AUDIO mode: the clock outputs, jank_hold.h)
 *   bar        where the session is in the bar, hundredths of a beat (0 .. quantum x 100 - 1); "-" with no session
 *   quantum    beats a bar
 *   link peers Link peers in the session, the stick not counted
 *   air peers  units heard on the group's channel (MODE JANK; 0 on Wi-Fi)
 *   battery    percent, "-" with no gauge (jank mini 2.2 reads its cell: firmware/main/linein.c)
 *   level      the input meter: RMS over 0.3 s, a value every 250 ms, dBFS (-99 .. 0); "-" with no audio stream
 *   conf       the confidence: the last 8 s of tracked beats against the published grid (jank_conf.h), percent; "-"
 *              with no audio stream
 *   min max    the lowest and highest meter values over the last 3 s (the meter's lines); "-" with no audio stream
 *   mode       1 AUDIO (the source has sound: the tracker leads, YES / play / stop are the hold), 0 LINK; AUDIO folded
 *              (core/jank_fold.h): x2 x4 /2 /4
 *              (M2k) "<pct>h" (pct 99 at most) while the confidence holds (no evidence: a break, a track change): the
 *              value shown, easing toward the threshold, not a measurement (jank_conf.h jcf_show_step); the CF bar HOLD
 *              "<pct>c" while the outputs COAST (the robust follower runs them under the threshold: a measurement, the CF bar
 *              hatched with COAST); with the graded confidence on the value is the graded one (jank_conf.h's gate is unchanged)
 * -> its length, 0 when it does not fit */
size_t jhm_home_line(const struct jk_cfg *c, const struct jk_snapshot *s, uint32_t now_ms, char *out, size_t cap);

#endif
