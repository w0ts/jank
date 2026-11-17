/* SPDX-License-Identifier: GPL-3.0-only */
/* The comb checks on the activation (M2m), a port of beat-listener's studies/follow-robust/comb.py: the mean-removed
 * activation's autocorrelation over the last `span` frames (unbiased, divided by the window's variance) read at the multiples
 * of a period (1, 2, 4 beats), the ratio check (a period's comb against its 3:4, 4:3, 2:3, 3:2 relatives), the tempo
 * agreement (its comb against the best of the DBN's periods) and the phase agreement (the activation at the published lines
 * against the best of 8 shifts of them).
 *
 * No FFT: each lag is a direct sum over the window (float, 4 partial sums). A comb at one period reads 3 lags (6 with the
 * interpolation of a fractional period), a ratio check 5 combs (~30 lags: ~30 k multiply-adds, only at a lock or a jump), a
 * tempo agreement the combs of 35 periods (~125 lags: ~110 k multiply-adds, every 0.5 s). */
#ifndef JB_COMB_H
#define JB_COMB_H
#include <stdint.h>

#define JC_FPS 100
#define JC_MAX_SPAN 1024               /* frames a curve can cover (10 s = 1000) */
#define JC_MAX_LAG 538                 /* comb.max_lag_for(67): ceil(4 x 67 x 2) + 2 */
#define JC_PERIOD_MIN 33.0             /* the DBN's periods (frames) */
#define JC_PERIOD_MAX 67.0

struct jc_curve {
    const float *x;                    /* the mean-removed window (the caller's scratch), n values */
    int n;
    double var;                        /* r[0] / n */
};

/* comb.curve: the frames end-span+1 .. end of the activation ring (ring[k % cap], frames up to act_n-1 in) copied into x
 * (cap JC_MAX_SPAN) with the mean removed. 0: too short or flat (the Python's None). */
int jc_curve(struct jc_curve *c, float *x, const float *ring, uint32_t cap, int64_t act_n, int64_t end, int span);
/* comb.at: the curve at a fractional lag; NaN beyond JC_MAX_LAG */
double jc_at(const struct jc_curve *c, double lag);
/* comb.comb: the mean over the lags 1, 2, 4 x period (frames) that fit; NaN with none */
double jc_comb(const struct jc_curve *c, double period);
/* comb.ratio_margin: comb(P) - the best comb of P's relatives; NaN when undefined */
double jc_ratio_margin(const struct jc_curve *c, double period);
/* comb.tempo_agreement against the DBN's periods lo..hi (33..67) */
double jc_tempo_agreement(const struct jc_curve *c, double period);
/* comb.phase_agreement: the lines (seconds) of a grid of `period` seconds; end: the last frame in */
double jc_phase_agreement(const float *ring, uint32_t cap, int64_t act_n, const double *lines, int n_lines, double period,
                          int64_t end);

#endif
