/* SPDX-License-Identifier: GPL-3.0-only */
/* The comb checks (jb_comb.h): comb.py's names and order. Python's round() rounds halves to even, as rint() does. */
#include "jb_comb.h"
#include <math.h>

static const double LAGS[3] = {1.0, 2.0, 4.0};
static const double RELATIVES[4] = {0.75, 4.0 / 3.0, 2.0 / 3.0, 1.5};

int jc_curve(struct jc_curve *c, float *x, const float *ring, uint32_t cap, int64_t act_n, int64_t end, int span)
{
    int64_t lo = end - span + 1;
    lo = lo > 0 ? lo : 0;
    lo = lo > act_n - (int64_t)cap ? lo : act_n - (int64_t)cap;
    int64_t n = end + 1 - lo;
    int need = span / 2 > JC_MAX_LAG + 16 ? span / 2 : JC_MAX_LAG + 16;
    if (end >= act_n || n < need || n > JC_MAX_SPAN)
        return 0;
    /* float throughout (the S3 has a single-precision FPU only): four partial sums for the mean and the variance */
    float m0 = 0.0f, m1 = 0.0f, m2 = 0.0f, m3 = 0.0f;
    for (int64_t i = 0; i < n; i++) {
        float v = ring[(uint64_t)(lo + i) % cap];
        switch (i & 3) {
        case 0: m0 += v; break;
        case 1: m1 += v; break;
        case 2: m2 += v; break;
        default: m3 += v; break;
        }
    }
    const float mean = ((m0 + m1) + (m2 + m3)) / (float)n;
    float acc = 0.0f;
    for (int64_t i = 0; i < n; i++) {
        float v = ring[(uint64_t)(lo + i) % cap] - mean;
        x[i] = v;
        acc += v * v;
    }
    if (acc <= 1e-12f)
        return 0;
    c->x = x;
    c->n = (int)n;
    c->var = (double)acc / (double)n;
    return 1;
}

/* r[lag]: the unbiased autocorrelation over the variance */
static double r_at(const struct jc_curve *c, int lag)
{
    const float *x = c->x;
    const int m = c->n - lag;
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
    int i = 0;
    for (; i + 3 < m; i += 4) {
        s0 += x[i] * x[i + lag];
        s1 += x[i + 1] * x[i + 1 + lag];
        s2 += x[i + 2] * x[i + 2 + lag];
        s3 += x[i + 3] * x[i + 3 + lag];
    }
    for (; i < m; i++)
        s0 += x[i] * x[i + lag];
    return ((double)((s0 + s1) + (s2 + s3)) / (double)m) / c->var;
}

double jc_at(const struct jc_curve *c, double lag)
{
    double lo = floor(lag);
    if (lo + 1.0 >= (double)JC_MAX_LAG + 1.0 || lo < 0.0)
        return NAN;
    double w = lag - lo;
    if (w == 0.0)
        return r_at(c, (int)lo);
    return (1.0 - w) * r_at(c, (int)lo) + w * r_at(c, (int)lo + 1);
}

double jc_comb(const struct jc_curve *c, double period)
{
    double sum = 0.0;
    int n = 0;
    for (int i = 0; i < 3; i++) {
        double v = jc_at(c, LAGS[i] * period);
        if (v == v) {
            sum += v;
            n++;
        }
    }
    return n ? sum / n : NAN;
}

double jc_ratio_margin(const struct jc_curve *c, double period)
{
    double own = jc_comb(c, period), best = NAN;
    for (int i = 0; i < 4; i++) {
        double a = jc_comb(c, period * RELATIVES[i]);
        if (a == a && !(best >= a))
            best = a;
    }
    if (own != own || best != best)
        return NAN;
    return own - best;
}

double jc_tempo_agreement(const struct jc_curve *c, double period)
{
    double own = NAN, best = NAN;
    const double cand[3] = {period, 2.0 * period, period / 2.0};
    for (int i = 0; i < 3; i++) {
        if (cand[i] < JC_PERIOD_MIN - 0.5 || cand[i] > JC_PERIOD_MAX + 0.5)
            continue;
        double v = jc_comb(c, cand[i]);
        if (v == v && !(own >= v))
            own = v;
    }
    for (int p = (int)JC_PERIOD_MIN; p <= (int)JC_PERIOD_MAX; p++) {
        double v = jc_comb(c, (double)p);
        if (v == v && !(best >= v))
            best = v;
    }
    if (own != own || best != best || best <= 0.0)
        return 0.0;
    double r = own / best;
    return r < 0.0 ? 0.0 : r > 1.0 ? 1.0 : r;
}

double jc_phase_agreement(const float *ring, uint32_t cap, int64_t act_n, const double *lines, int n_lines, double period,
                          int64_t end)
{
    const int shifts = 8, half = 2;
    if (n_lines < 4 || !(period > 0.0))
        return 0.0;
    double vals[8], best = 0.0;
    for (int s = 0; s < shifts; s++) {
        double sum = 0.0;
        int cnt = 0;
        for (int i = 0; i < n_lines; i++) {
            int64_t f = (int64_t)rint((lines[i] + s * period / shifts) * JC_FPS);
            if (f - half < 0 || f + half > end || f + half >= act_n || f - half < act_n - (int64_t)cap)
                continue;
            float m = ring[(uint64_t)(f - half) % cap];
            for (int64_t k = f - half + 1; k <= f + half; k++) {
                float v = ring[(uint64_t)k % cap];
                if (v > m)
                    m = v;
            }
            sum += (double)m;
            cnt++;
        }
        vals[s] = cnt ? sum / cnt : 0.0;
        if (vals[s] > best || s == 0)
            best = vals[s] > best ? vals[s] : best;
    }
    return best > 0.0 ? vals[0] / best : 0.0;
}
