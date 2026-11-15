/* SPDX-License-Identifier: GPL-3.0-only */
/* LOW CONFIDENCE: the grid-fit confidence and its gate (jank_conf.h). */
#include "jank_conf.h"
#include <math.h>
#include <string.h>

#define JCF_SLOTS_MAX 4096              /* a period under 2 ms (no tracker gives one): not a grid, confidence 0 */

/* the first index whose beat is >= x (Python's bisect_left) */
static size_t lower_bound(const double *beats, size_t n, double x)
{
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        if (beats[mid] < x)
            lo = mid + 1u;
        else
            hi = mid;
    }
    return lo;
}

/* slot k of the grid, computed as numpy does (t0 + period * k, the product rounded first: two statements, so no
 * compiler fuses them into one multiply-add) */
static double slot(double t0, double period, int64_t k)
{
    double step = period * (double)k;
    return t0 + step;
}

double jcf_grid_confidence(const double *beats, size_t n, int has_grid, double t0, double period, double now,
                           double window_s, double delay_s, double tol)
{
    double lo = now - window_s, hi = now - delay_s, margin, recall, precision;
    double k0d, k1d;
    int64_t k0, k1, k;
    size_t a, b, i, hits = 0, on_grid = 0;
    if (!has_grid || !(period > 0.0))
        return 0.0;
    if (hi <= lo)
        return 0.0;
    k0d = ceil((lo - t0) / period);
    k1d = floor((hi - t0) / period);
    if (!(k1d >= k0d))
        return 0.0;                     /* no slot in the window */
    if (k1d - k0d + 1.0 > (double)JCF_SLOTS_MAX)
        return 0.0;
    k0 = (int64_t)k0d;
    k1 = (int64_t)k1d;
    margin = tol * period;
    a = lower_bound(beats, n, lo - margin);
    b = lower_bound(beats, n, hi + margin);
    if (b <= a)
        return 0.0;
    /* Both sorted: a merge. The same test as the Python's (|beat - slot| <= margin, as rounded), on fewer pairs: a rounded
     * difference grows with the distance, so a beat too far before a slot is too far before every later one, and past
     * the slot the first one too far ends the search. Soft doubles on the device: ~50 pairs a step, not ~700 */
    i = a;
    for (k = k0; k <= k1; k++) {        /* a slot is hit: some recent beat within the margin */
        double s = slot(t0, period, k);
        while (i < b && beats[i] < s && fabs(beats[i] - s) > margin)
            i++;
        for (size_t j = i; j < b; j++) {
            if (fabs(beats[j] - s) <= margin) {
                hits++;
                break;
            }
            if (beats[j] > s)
                break;
        }
    }
    k = k0;
    for (i = a; i < b; i++) {           /* a beat is on the grid: some slot within the margin */
        while (k <= k1 && slot(t0, period, k) < beats[i] && fabs(beats[i] - slot(t0, period, k)) > margin)
            k++;
        for (int64_t q = k; q <= k1; q++) {
            double s = slot(t0, period, q);
            if (fabs(beats[i] - s) <= margin) {
                on_grid++;
                break;
            }
            if (s > beats[i])
                break;
        }
    }
    recall = (double)hits / (double)(k1 - k0 + 1);
    precision = (double)on_grid / (double)(b - a);
    return recall + precision == 0.0 ? 0.0 : 2.0 * recall * precision / (recall + precision);
}

void jcf_init(struct jcf *c) { memset(c, 0, sizeof *c); }

int jcf_samples(struct jcf *c, uint64_t sumsq, size_t n)
{
    int was = c->silent;
    if (!n)
        return 0;
    if ((double)sumsq < JCF_QUIET_MS_INT16 * (double)n) {
        uint64_t q = (uint64_t)c->quiet + n;
        c->quiet = q > JCF_SILENCE_SAMPLES ? JCF_SILENCE_SAMPLES : (uint32_t)q;
    } else
        c->quiet = 0;
    c->silent = (uint8_t)(c->quiet >= JCF_SILENCE_SAMPLES);
    if (!c->silent || was)
        return 0;
    c->n = 0;                           /* new music after silence: start over */
    c->silences++;
    return 1;
}

void jcf_beat(struct jcf *c, double t)
{
    if (c->silent)
        return;
    if (c->n && t < c->t[c->n - 1u])
        return;                         /* out of order: the window's search needs them ascending */
    if (c->n == JCF_KEEP) {
        memmove(c->t, c->t + 1, sizeof c->t[0] * (JCF_KEEP - 1u));
        c->n--;
    }
    c->t[c->n++] = t;
}

double jcf_confidence(const struct jcf *c, int has_grid, double t0, double period, double now)
{
    if (c->silent)
        return 0.0;
    return jcf_grid_confidence(c->t, c->n, has_grid, t0, period, now, JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
}

uint8_t jcf_pct(double conf)
{
    double p = floor(conf * 100.0 + 0.5);
    return (uint8_t)(p < 0.0 ? 0.0 : p > 100.0 ? 100.0 : p);
}

uint8_t jcf_gate(uint8_t gated, double conf, uint8_t lowconf, uint8_t threshold_pct)
{
    double th = (double)threshold_pct / 100.0, open;
    if (lowconf != JCF_STOP || threshold_pct == 0u)
        return 0;
    if (!gated)
        return (uint8_t)(conf < th);
    open = th + (double)JCF_HYST_PCT / 100.0;      /* as the Python adds them: 0.8 + 0.05 (= 0.8500000000000001) */
    return (uint8_t)(conf < (open < 1.0 ? open : 1.0));
}
