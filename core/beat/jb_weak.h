/* SPDX-License-Identifier: GPL-3.0-only */
/* The weak-signal hold (M2j): a port of beat-listener's studies/tempo-changes/weak.py CoastBpm, which sits on the follower
 * (jb_follow.h) so that through beat-less or chopped passages the grid coasts on the glide's prediction, corrected gently
 * by weak evidence, and the outputs stop only on clear contrary evidence.
 *
 *   strong beats  a tracked beat is strong when the network's activation within 3 frames of it reaches
 *                 max(0.05, min(0.3, 0.4 x LEVEL)), LEVEL the 75th percentile of the last 64 beats' (a quiet pop track
 *                 has a low level; a breakdown is far under its track's). A strong beat goes to the follower as before.
 *                 Before the first lock every beat does.
 *   weak beats    once the follower glides, a weak beat is a weak Kalman observation (noise JW_R_WEAK, taken within
 *                 JW_WINDOW periods of the prediction, no manoeuvre, no miss): the online DBN keeps ticking through a
 *                 breakdown from its tempo model alone. Off the grid (further than OUTLIER) it counts as a miss, so a
 *                 grid the tracked beats left for a consistent one of their own (an off-beat slide) is still jumped from.
 *   coasting      each predicted line that no tracked beat came near: the activation's peak within +-JW_WINDOW periods
 *                 of it, if salient (at least JW_FLOOR and JW_SALIENCE x the median of the +-1 period around it), is a
 *                 weak observation too, and keeps the hold (30 s) running.
 *   confidence    jank_conf.h jcf_weak_confidence: lines with no evidence are not counted against the grid; it only
 *                 holds the outputs once they run (they start on the strict line confidence).
 *
 * The device sees the activation only up to now (the Python reads the whole track's): a beat is fed JW_STRONG_HALF frames
 * after it (30 ms, when its 3 frames after are in), and a line is coasted on once the frames up to a period after it are
 * in. weak.py's `causal` does the same; `causal` 0 here is the study's look-ahead view (the tests: the full activation
 * pushed first).
 *
 * Measured (studies/tempo-changes, the user's 829 tracks, the test half; with the other M2j changes): stopped in beat-less
 * passages 0.60 -> 0.39, back on the beat sooner (phase error 26 -> 18 ms); on Charli XCX's tracks (activation ~0.2)
 * stopped e.g. 0.37 -> 0.14 (Talk talk). Known regression: a tracker that slides to the off-beat for long (Sympathy is a
 * knife: stopped 0.13 -> 0.36).
 *
 * Cost: per beat a max over 7 frames and a 75th percentile of 64 floats; per line coasted on (beat-less only) a max and a
 * median over ~2 periods of activation (~100 floats); per step the weak confidence (a few hundred double operations).
 * Estimated +0.16-0.24 % of core 0 (host time scaled by ModeBpm's soft-double cost measured in QEMU). RAM: struct jb_weak
 * (72 bytes, internal) and struct jw_store (7.3 KB: the activation ring and the beat lists, PSRAM on the device).
 * Portable C, no allocation. The Python and this
 * agree to rounding (tests/test_tempo.c against tests/tempo_ref, beat-listener's scripts/tempo_golden.py). */
#ifndef JB_WEAK_H
#define JB_WEAK_H
#include <stddef.h>
#include <stdint.h>
#include "jb_follow.h"

#define JW_FPS 100
#define JW_STRONG_ABS 0.3
#define JW_STRONG_ABS_MIN 0.05
#define JW_STRONG_REL 0.4
#define JW_LEVEL_BEATS 64
#define JW_STRONG_HALF 3               /* frames each side of a beat for its strength */
#define JW_WINDOW 0.12                 /* periods each side of a line for its evidence, and a weak beat's gate */
#define JW_SALIENCE 2.5
#define JW_FLOOR 0.03
#define JW_R_WEAK 0.012                /* a weak observation's noise (s) */
#define JW_MIN_OBS 4                   /* observed lines in the window, else the confidence holds its last value */
#define JW_LAG_S 0.05
/* the activation kept: a line's evidence needs a period each side (1.4 s at 86 BPM) plus the wait; 2.56 s */
#define JW_ACT_RING 256
/* the beat lists, as the Python's deques (the confidence's window holds under 40) */
#define JW_STRONG_KEEP 256
#define JW_SEEN_KEEP 256
#define JW_WEAK_KEEP 128

struct jw_store {                      /* PSRAM on the device */
    float act[JW_ACT_RING];            /* frame k at k % JW_ACT_RING */
    float levels[JW_LEVEL_BEATS];      /* the last beats' activation (a ring) */
    float work[JW_ACT_RING];           /* the median's and the percentile's copy */
    double strong[JW_STRONG_KEEP];     /* strong beats, oldest first */
    double seen[JW_SEEN_KEEP];         /* every beat fed */
    double weak[JW_WEAK_KEEP];         /* weak observations taken (weak beats and coasted lines), in the order taken */
};

struct jb_weak {
    struct jw_store *st;               /* NULL: off (the follower runs on every beat, as M2i) */
    float *act;                        /* the activation ring: st->act, or the caller's (the tests: a whole track) */
    uint32_t act_cap;
    int64_t act_n;                     /* frames pushed: frame index + 1 of the last */
    uint8_t causal;
    uint8_t has_next;
    uint16_t n_levels, levels_head, n_strong, n_seen, n_weak;
    double next_line;
    double last_conf;                  /* jcf_weak_confidence's held value */
    /* totals since the reset, for `beat` */
    uint32_t coasted;                  /* lines coasted on (weak.py's coasted) */
    uint32_t weak_beats;               /* weak beats taken as weak observations */
    uint32_t weak_misses;              /* weak beats off the grid (misses) */
};

/* st NULL: off. act/act_cap: the activation ring (NULL: st->act, JW_ACT_RING). causal: the device's view. */
void jw_init(struct jb_weak *w, struct jw_store *st, float *act, uint32_t act_cap, int causal);
/* the estimator starts over (silence: chain.py makes a new CoastBpm on the same track): no beats, no line; the activation
 * stays (jw_init clears it, with the tracker's frame count) */
void jw_reset(struct jb_weak *w);
int jw_on(const struct jb_weak *w);
/* the network's activation of frame index (frames in order; a gap is filled with 0) */
void jw_act(struct jb_weak *w, int64_t index, float act);
/* CoastBpm.update: a tracked beat at t (seconds = frame / 100) into the follower. Returns 1 if it was strong. Off: the
 * follower's own update. */
int jw_update(struct jb_weak *w, struct jb_follow *f, double t);
/* CoastBpm.coast: weak observations for the lines no tracked beat came near, once their evidence is in */
void jw_coast(struct jb_weak *w, struct jb_follow *f, double now);
/* the lists for jank_conf.h jcf_weak_confidence */
static inline const double *jw_strong(const struct jb_weak *w) { return w->st ? w->st->strong : NULL; }
static inline const double *jw_seen(const struct jb_weak *w) { return w->st ? w->st->seen : NULL; }
static inline const double *jw_weak(const struct jb_weak *w) { return w->st ? w->st->weak : NULL; }

/* the pieces, for the tests: weak.peak_near, weak.weak_peak (1 found) */
double jw_peak_near(const struct jb_weak *w, double t);
int jw_weak_peak(struct jb_weak *w, double line, double period, double *obs);

#endif
