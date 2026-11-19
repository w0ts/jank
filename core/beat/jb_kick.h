/* SPDX-License-Identifier: GPL-3.0-only */
/* The kick band and the input level per tracker frame: evidence for LOST (jb_lost.h) that does not depend on the network.
 *
 * Why (the user, 2026-11-19, two tracks that stopped with the kick plainly there): on some tracks the firmware's online
 * network is weak on a full four-on-the-floor kick (Rhyw - Driptych: activation 0.12-0.35 where the offline network gives
 * 0.8-0.95), so few tracked beats pass the strong test. The kick band shows the kick directly: a transient under 120 Hz on
 * the line, well above the band's level between the lines (Driptych: 25-35 dB on the kick, 7-10 dB in its beat-less
 * part).
 *
 *   kick band   the input under 120 Hz (two one-pole low-passes, 12 dB an octave), its RMS per tracker hop (441 samples:
 *               hop k is the network's frame k), dB. A line has KICK EVIDENCE when the band's peak within +-tol x period
 *               of it (plus JBK_BODY frames for the kick's body) is >= JBK_FLOOR_DB, >= JBK_CONTRAST_DB above the band's
 *               minimum 0.35-0.85 period after the line (the gap before the next one), and within JBK_REL_DB of the band's
 *               loudest hop of the last ~8 s (a faint kick of another layer is not the beat).
 *   level       the input's RMS per hop, dBFS (full scale 32768); a line's level is the mean power over its period.
 *
 * Cost: per sample a few float multiply-adds; per hop two log10; per line judged a max and a min over ~50 frames and a
 * mean over a period. RAM: struct jb_kick (~2.2 KB, the caller's: PSRAM on the device). Portable C, no allocation. */
#ifndef JB_KICK_H
#define JB_KICK_H
#include <stddef.h>
#include <stdint.h>

#define JBK_RING 256                    /* frames kept: 2.56 s (a line is judged a period after it) */
#define JBK_CUTOFF_HZ 120.0
#define JBK_CONTRAST_DB 15.0
#define JBK_FLOOR_DB -50.0              /* dBFS of the band's RMS */
#define JBK_REL_DB 20.0                 /* the peak within this of the band's loudest hop of the last JBK_MAX_BLOCKS blocks */
#define JBK_BODY 3                      /* frames after the window for the kick's body */
#define JBK_BLOCK 50                    /* frames a block of the loudest-hop memory (0.5 s) */
#define JBK_MAX_BLOCKS 16               /* blocks remembered: 8 s */
#define JBK_NONE (-200.0f)

struct jb_kick {
    float a;                           /* the low-passes' coefficient */
    float y1, y2;
    float acc_k, acc_x;                /* the hop's sums of squares: kick band, input */
    uint16_t n_acc;
    int64_t frames;                    /* hops completed: the frame index + 1 of the last */
    float kick[JBK_RING];               /* kick band dB, frame k at k % JBK_RING */
    float level[JBK_RING];              /* input dBFS */
    float block_max[JBK_MAX_BLOCKS];    /* the loudest kick-band hop of each finished block (a ring) */
    float cur_max;                     /* ... of the block being filled */
    uint8_t blocks, block_head;
};

void jbk_init(struct jb_kick *k);
/* the stream starts over (a restart of the tracker, silence): no frames; the filters' state goes too */
void jbk_reset(struct jb_kick *k);
/* samples at 44.1 kHz, exactly those the tracker gets (jb_beat_push_s16), in the same order */
void jbk_push_s16(struct jb_kick *k, const int16_t *x, size_t n);
/* 1 when the line (seconds = frame / 100) has kick evidence; 0 when not, or when its frames are not (or no longer) in */
int jbk_on_line(const struct jb_kick *k, double line, double period, double tol);
/* the input's level over [line, line + period), dBFS; JBK_NONE when those frames are not (or no longer) in */
double jbk_line_level(const struct jb_kick *k, double line, double period);

#endif
