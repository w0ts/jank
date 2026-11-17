/* SPDX-License-Identifier: GPL-3.0-only */
/* Sound on the tracker's source, and its level for the input meter: AUDIO mode is "the chosen source has SOUND", not "a
 * stream is open" (a computer keeps its stream open, sending zeros, as long as jank is its output).
 *
 * Fed the tracker's own samples (mono int16, 44.1 kHz, the source AUDIO > SOURCE picked), in 50 ms blocks of mean square:
 *
 *   detector  the RMS over the last 0.75 s. LIVE once it stayed above JLV_ON_DB for JLV_ON_MS; back to not live once it
 *             stayed under JLV_OFF_DB for JLV_OFF_MS. Between the two levels neither count runs (a level hysteresis: a
 *             noise floor near the threshold cannot flap it). The 0.75 s window holds a kick at any tempo from 80 BPM up,
 *             so a kick-only intro with digital silence between the hits counts as sound; a short click or a notification
 *             of under ~0.25 s cannot hold it for a whole second.
 *   meter     the RMS over the last 0.3 s, a value every 250 ms, and the lowest and highest of those over the last 3 s:
 *             the emulator's model (player.html METER, engine.py level_fast_db). dBFS, -99 .. 0 (silence -99).
 *
 * Time is the samples' own (44100 a second): no clock. A source that stops sending (a stream closed) is the caller's to
 * report: jlv_init again, or jlv_push with NULL for the time that passed without samples (silence). No allocation. */
#ifndef JANK_LIVE_H
#define JANK_LIVE_H
#include <stddef.h>
#include <stdint.h>

#define JLV_RATE 44100u
#define JLV_BLOCK 2205u                 /* 50 ms */
#define JLV_BLOCK_MS 50u
#define JLV_SOUND_BLOCKS 15u            /* the detector's window: 0.75 s */
#define JLV_ON_DB (-50)                 /* sound: RMS above this ... */
#define JLV_ON_MS 1000u                 /* ... this long */
#define JLV_OFF_DB (-55)                /* silence: RMS under this ... */
#define JLV_OFF_MS 4000u                /* ... this long (after the window emptied: ~4.75 s after the last sound) */
#define JLV_METER_BLOCKS 6u             /* the meter's RMS: 0.3 s */
#define JLV_METER_EVERY 5u              /* a meter value every 250 ms */
#define JLV_SPAN 12u                    /* min / max over the last 12 values: 3 s */
#define JLV_FLOOR_DB (-99)

struct jlv {
    uint64_t block[JLV_SOUND_BLOCKS];   /* the last blocks' sums of squares, newest at head - 1 */
    uint8_t head, nblocks;
    uint64_t acc;                       /* the block being filled */
    uint32_t acc_n;
    uint32_t blocks;                    /* whole blocks since the start (the meter's cadence) */
    uint32_t loud_ms, quiet_ms;
    uint8_t live;
    uint32_t lives;                     /* times it went live (for `beat`) */
    int16_t rms_db;                     /* the meter: the last value, and the extremes of the last JLV_SPAN */
    int16_t min_db, max_db;
    int16_t span[JLV_SPAN];
    uint8_t nspan, span_head;
};

void jlv_init(struct jlv *l);
/* n samples of the source; x NULL: n samples' time of silence (the source sent nothing) */
void jlv_push(struct jlv *l, const int16_t *x, size_t n);
static inline int jlv_live(const struct jlv *l) { return l->live; }

/* dBFS of a mean square of int16 samples, rounded, JLV_FLOOR_DB .. 0 */
int16_t jlv_db(double mean_square);
#endif
