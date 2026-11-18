/* SPDX-License-Identifier: GPL-3.0-only */
/* The graded confidence (jb_graded.h): graded.py's names and order. */
#include "jb_graded.h"
#include <math.h>

static double kern(double x, double period)
{
    double k = 1.0 - x / (0.5 * period);
    return k < 0.0 ? 0.0 : k > 1.0 ? 1.0 : k;
}

double jg_soft_f(const double *beats, size_t n_beats, const double *lines, size_t n_lines, double period, double now)
{
    const double lo = now - JG_WINDOW_S, hi = now - JG_DELAY_S;
    const double *b = beats + (n_beats > JG_BEATS ? n_beats - JG_BEATS : 0);
    size_t nb = n_beats > JG_BEATS ? JG_BEATS : n_beats;
    double rec = 0.0, prec = 0.0;
    size_t nl = 0, nbw = 0;
    for (size_t j = 0; j < n_lines; j++)
        if (lines[j] >= lo && lines[j] <= hi)
            nl++;
    for (size_t i = 0; i < nb; i++)
        if (b[i] >= lo - period && b[i] <= hi + period)
            nbw++;
    if (nl == 0 || nbw == 0)
        return 0.0;
    for (size_t j = 0; j < n_lines; j++) {
        if (!(lines[j] >= lo && lines[j] <= hi))
            continue;
        double dmin = INFINITY;
        for (size_t i = 0; i < nb; i++)
            if (b[i] >= lo - period && b[i] <= hi + period)
                dmin = fmin(dmin, fabs(b[i] - lines[j]));
        rec += kern(dmin, period);
    }
    for (size_t i = 0; i < nb; i++) {
        if (!(b[i] >= lo - period && b[i] <= hi + period))
            continue;
        double dmin = INFINITY;
        for (size_t j = 0; j < n_lines; j++)
            if (lines[j] >= lo && lines[j] <= hi)
                dmin = fmin(dmin, fabs(b[i] - lines[j]));
        prec += kern(dmin, period);
    }
    rec /= (double)nl;
    prec /= (double)nbw;
    return rec + prec == 0.0 ? 0.0 : 2.0 * rec * prec / (rec + prec);
}

static void sort_d(double *v, int n)
{
    for (int i = 1; i < n; i++) {
        double x = v[i];
        int j = i;
        for (; j > 0 && v[j - 1] > x; j--)
            v[j] = v[j - 1];
        v[j] = x;
    }
}

double jg_jitter(const double *beats, size_t n_beats)
{
    if (n_beats < 6)
        return 0.0;
    const double *b = beats + (n_beats > 17 ? n_beats - 17 : 0);
    int n = (int)(n_beats > 17 ? 17 : n_beats) - 1;
    double ibi[16], srt[16], mean = 0.0, var = 0.0;
    for (int i = 0; i < n; i++) {
        srt[i] = ibi[i] = b[i + 1] - b[i];
        mean += ibi[i];
    }
    sort_d(srt, n);
    double med = n % 2 ? srt[n / 2] : (srt[n / 2 - 1] + srt[n / 2]) / 2.0;
    if (!(med > 0.0))
        return 0.0;
    mean /= n;
    for (int i = 0; i < n; i++)
        var += (ibi[i] - mean) * (ibi[i] - mean);
    double cv = sqrt(var / n) / med / JG_JIT_CV;
    return 1.0 - (cv < 1.0 ? cv : 1.0);
}

double jg_graded(double soft, double jit, double tempo, double dbn)
{
    double g = (soft + jit + tempo + dbn) / 4.0;
    return g < 0.0 ? 0.0 : g > 1.0 ? 1.0 : g;
}

void jg_tuned_reset(struct jg_tuned *g)
{
    *g = (struct jg_tuned){0};
}

double jg_tuned_step(struct jg_tuned *g, const struct jb_robust *r, const struct jb_dbn *dbn, const double *seen, size_t n_seen,
                     const double *lines, size_t n_lines, int has_grid, double period, double now)
{
    if (!has_grid || !(period > 0.0) || !seen) {
        g->next_t = 0.0;
        g->value = 0.0;
        return 0.0;
    }
    if (now >= g->next_t || now < g->next_t - 2.0 * JG_EVERY_S) {   /* (a stream time that went back: the tracker restarted) */
        g->next_t = now + JG_EVERY_S;
        g->value = jg_graded(jg_soft_f(seen, n_seen, lines, n_lines, period, now), jg_jitter(seen, n_seen),
                             jrb_tempo_agreement(r, now, period), (double)jb_dbn_mass_near(dbn, 60.0 / period, JRB_DBN_NEAR));
    }
    return g->value;
}
