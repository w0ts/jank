/* SPDX-License-Identifier: GPL-3.0-only */
/* The graded confidence, SHOWN beside the unchanged gate (beat-listener studies/follow-robust graded.py): the gate's value is ~1
 * whenever the follower is locked, a wall and then a cliff; the shown value is the mean of four parts that move with the beat's
 * real quality:
 *   soft    F of the tracker's beats against the published lines over the last 8 s, a triangular kernel (1 on the line, 0 half a
 *           period off)
 *   jitter  1 - clip(cv of the last 16 beat intervals / 0.15): how even the beats are
 *   tempo   the activation comb's agreement with the follower's period (jb_robust.h v_tempo, refreshed every 0.5 s)
 *   dbn     the DBN's tempo mass near the follower's tempo (jb_robust.h v_dbn)
 * Display only (the CF bar, HOME, status): nothing gates on it.
 *
 * On the TUNED follower (the default) all four parts exist with data it already has: the beats it was fed (jw_seen), the
 * published lines, the DBN it runs every frame (its forward mass near the follower's tempo) and the activation ring the robust
 * follower keeps (8 KB, PSRAM, already allocated: jrb_act is called with every frame, one store). Only the comb's tempo
 * agreement is new work, so jg_tuned_step does all four once a second (JG_EVERY_S): about 0.3 .. 0.5 M cycles/s on the S3
 * (estimated from the operation counts, not measured on the jank), no heap. The robust follower still computes its parts at
 * its own 0.5 s refresh and the same per-step soft F (beat.c). */
#ifndef JB_GRADED_H
#define JB_GRADED_H
#include <stddef.h>
#include "jb_dbn.h"
#include "jb_robust.h"

#define JG_WINDOW_S 8.0
#define JG_DELAY_S 0.1
#define JG_JIT_CV 0.15
#define JG_BEATS 64                      /* the beats soft F reads at most (the Python's beats[-64:]) */

/* graded.soft_f: beats (ascending, every beat fed), lines (ascending, the published lines) */
double jg_soft_f(const double *beats, size_t n_beats, const double *lines, size_t n_lines, double period, double now);
/* graded.jitter */
double jg_jitter(const double *beats, size_t n_beats);
/* graded.graded: the mean of the four parts, clipped to 0..1 */
double jg_graded(double soft, double jit, double tempo, double dbn);

/* the tuned follower's graded value: the four parts refreshed together every JG_EVERY_S, the mean in between */
#define JG_EVERY_S 1.0
struct jg_tuned {
    double next_t;                     /* the next refresh (stream time, s) */
    double value;                      /* the mean of the four parts at the last refresh, clipped to 0..1 */
};
void jg_tuned_reset(struct jg_tuned *g);
/* The value now, 0..1. No grid (the follower gives no tempo) or no beat list: 0, and the next call refreshes. r: the robust
 * follower's activation ring (jrb_act fed with every frame; its gate and agreement are not used), dbn: the tracker's DBN, seen:
 * the beats fed (jw_seen), lines: the published lines. */
double jg_tuned_step(struct jg_tuned *g, const struct jb_robust *r, const struct jb_dbn *dbn, const double *seen, size_t n_seen,
                     const double *lines, size_t n_lines, int has_grid, double period, double now);

#endif
