/* SPDX-License-Identifier: GPL-3.0-only */
/* The playback rate from the spectrum (advanced beat follow, M2i): a port of beat-listener's live/rate.py RateProbe,
 * the causal form of studies/advanced-follow/ratio_probe.py.
 *
 * A pitch fader (varispeed) moves every partial by the tempo's ratio. So the log-frequency spectrum of the last 0.5 s,
 * matched against the 4 s before it, tells the new rate at once, without waiting for beats:
 *   spectrum  the tracker's own frames (jb_fe's |FFT|: no second FFT), linearly resampled to JR_BINS bins at 96 an
 *             octave from 300 Hz to 8 kHz, log(1 + x); every JR_STEP-th frame of a 0.1 s block (10 frames) averaged
 *   match     at every JR_EVERY-th block's end the mean of the last JR_CUR blocks against the mean of the JR_REF before
 *             them: the correlation of the mean-removed spectra at shifts within +-10 % (+-14 bins), the peak refined by
 *             a parabola, ratio = 2^(shift / 96)
 *   event     JR_CONFIRM estimates in a row past 0.6 %, each within 0.3 % of the one before: their median is a rate step
 *             (3 since M2j, the median of three: fewer flips, a step taken ~0.5 beat later; M2i had 2), handed to the follower
 *             (jb_follow_propose, `since` JR_LAG_S before), which takes it only if the next tracked beat backs it;
 *             then no estimate for 4.5 s (until the reference holds only the new rate)
 * It runs only while the follower glides (jb_rate_set_active): its history starts over at each lock.
 *
 * Measured (beat-listener studies/advanced-follow, the user's tracks, 60 steps of 2 % and 60 of 5 % each): every frame
 * and every block (the probe) found 87-90 % of the steps within 3 s, 0.54 s median, the ratio's error 0.11 % median;
 * every second frame the same (rate_decimate.py); every second frame and every second block (the default) 83-85 %,
 * 0.67 s, 0.08-0.10 %, and fewer false events (197 an hour against 266-274: chord changes, made harmless by the next-beat
 * check). Every fifth frame doubled the error.
 *
 * Cost on the S3 (the compiler's code for this file, -O2): 29 instructions a bin, 13.2 k a frame taken (50 a second); a
 * block with its match ~79 k (5 a second), without ~14 k: ~0.7 % of core 0 at 240 MHz (CPI 1.45, the timing audit's),
 * PSRAM misses on top; nothing in the 4.5 s after an event, nothing while not gliding (jank docs/beat.md, M2i). RAM:
 * struct jb_rate (under 100 bytes, internal) and struct jr_store (58 KB: the tables and the block's sums, touched every
 * frame taken; the 4.5 s ring as uint16 and the match's work, touched once a block): PSRAM on the device. Portable C,
 * floats, no allocation. The Python and this agree to rounding (tests/test_follow.c against tests/follow_ref/rate_*.txt). */
#ifndef JB_RATE_H
#define JB_RATE_H
#include <stdint.h>

#define JR_BINS 454                    /* int(log2(8000 / 300) * 96) */
#define JR_MAG_BINS 369                /* the |FFT| bins it reads: 0 .. 368 (8 kHz is bin 371.5 of 2048 at 44.1 kHz) */
#define JR_BLOCK 10                    /* frames a block: 0.1 s */
#define JR_CUR 5                       /* blocks: the last 0.5 s ... */
#define JR_REF 40                      /* ... against the 4 s before */
#define JR_HIST (JR_REF + JR_CUR)
#define JR_MAX_SHIFT 14                /* ceil(log2(1.1) * 96) */
#define JR_STEP 2                      /* the default: every second frame */
#define JR_EVERY 2                     /* the default: an estimate at every second block (half the match's CPU; 1: every block) */
#define JR_LAG_S 0.5                   /* an event comes this long after the step (the study's median) */
#define JR_CONFIRM 3                   /* live/rate.py CONFIRM (M2j: studies/tempo-changes tuned-l16; M2i 2) */
#define JR_CONFIRM_MAX 5
#define JR_Q 8192.0f                   /* the ring's uint16: a block's mean log spectrum (0 .. 7) times this */

struct jr_store {                      /* everything but a few counters: PSRAM on the device (internal RAM is short) */
    float acc[JR_BINS];                /* this block's sum of frames (touched every frame taken) */
    uint16_t idx[JR_BINS];             /* each bin's place on the FFT's bins: |X| = m[idx] + frac (m[idx + 1] - m[idx]) */
    float frac[JR_BINS];
    float log[256][2];                 /* ln(1 + j / 256) and the step to the next: the log's table */
    uint16_t ring[JR_HIST][JR_BINS];   /* the blocks, oldest at head (touched once a block from here on) */
    int32_t ref[JR_BINS], cur[JR_BINS];   /* the sums over the JR_REF oldest and the JR_CUR newest: exact integers */
    float a[JR_BINS], b[JR_BINS];      /* the match: the two means, mean removed */
    float pa[JR_BINS + 1], pb[JR_BINS + 1];   /* their prefix sums of squares (the norms at each shift) */
};

struct jb_rate {                       /* (about 130 bytes: internal RAM) */
    struct jr_store *st;               /* NULL: no rate */
    uint8_t acc_n, in_block, step, every, active;
    uint8_t count, head;               /* blocks in the ring, the oldest's row */
    uint8_t run_n;                     /* estimates in a row past the threshold (0 .. confirm - 1) ... */
    double run_last;                   /* ... the last of them */
    double run[JR_CONFIRM_MAX];        /* ... all of them */
    uint8_t confirm;                   /* CONFIRM: JR_CONFIRM by jb_rate_init (2 .. JR_CONFIRM_MAX) */
    uint8_t has_last;                  /* an event was decided ... */
    double last_t;                     /* ... at this time */
    uint8_t has_event;                 /* jb_rate_take's: an event not taken yet */
    double ev_t, ev_ratio;
    /* for the stats */
    uint32_t estimates, events;
    double est_ratio;                  /* the last estimate */
    float est_peak;
};

/* st: the store (sizeof struct jr_store; NULL: never active); step: 1, 2 or 5 (frames a block taken: 10 / step); every: 1
 * or 2 (an estimate at each block or each second one). Inactive. */
void jb_rate_init(struct jb_rate *r, struct jr_store *st, int step, int every);
/* On: a history starts at the next block. Off: everything goes (and nothing is computed per frame). */
void jb_rate_set_active(struct jb_rate *r, int on);
/* One of the tracker's frames: its index (frame i centred on sample 441 i, the tracker's i / 100 s) and its |FFT| bins
 * 0 .. JR_MAG_BINS - 1 at least. Frames come in order; a frame whose block is done may decide an event. */
void jb_rate_frame(struct jb_rate *r, int64_t index, const float *mag);
/* An event decided and not taken yet: its time and ratio, once. 0 none. */
int jb_rate_take(struct jb_rate *r, double *t, double *ratio);
/* the log-frequency value of one frame's bin i (the test's view of the front of the chain); r initialised with a store */
float jb_rate_bin(const struct jb_rate *r, const float *mag, int i);

#endif
