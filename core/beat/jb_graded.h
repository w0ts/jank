/* SPDX-License-Identifier: GPL-3.0-only */
/* The graded confidence, SHOWN beside the unchanged gate (beat-listener studies/follow-robust graded.py): the gate's value is ~1
 * whenever the follower is locked, a wall and then a cliff; the shown value is the mean of four parts that move with the beat's
 * real quality:
 *   soft    F of the tracker's beats against the published lines over the last 8 s, a triangular kernel (1 on the line, 0 half a
 *           period off)
 *   jitter  1 - clip(cv of the last 16 beat intervals / 0.15): how even the beats are
 *   tempo   the activation comb's agreement with the follower's period (jb_robust.h v_tempo, refreshed every 0.5 s)
 *   dbn     the DBN's tempo mass near the follower's tempo (jb_robust.h v_dbn)
 * Display only (the CF bar, HOME, status): nothing gates on it. Needs the robust follower's agreement parts. */
#ifndef JB_GRADED_H
#define JB_GRADED_H
#include <stddef.h>

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

#endif
