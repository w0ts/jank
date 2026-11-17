/* SPDX-License-Identifier: GPL-3.0-only */
/* The playback rate from the spectrum (jb_rate.h), a port of live/rate.py: names, constants and the order of the steps
 * are its. */
#include "jb_rate.h"
#include <math.h>
#include <string.h>

#define SR 44100.0
#define FRAME 2048.0
#define BPO 96.0
#define F_LO 300.0
#define EVENT_TH 0.006
#define AGREE 0.003
#define REFRACTORY_S 4.5               /* (JR_REF + JR_CUR) blocks */
#define SHIFTS (2 * JR_MAX_SHIFT + 1)

static void tables(struct jr_store *s)
{
    for (int i = 0; i < JR_BINS; ++i) {
        double pos = F_LO * pow(2.0, (double)i / BPO) / (SR / FRAME), lo = floor(pos);
        s->idx[i] = (uint16_t)lo;
        s->frac[i] = (float)(pos - lo);
    }
    for (int j = 0; j < 256; ++j) {
        double a = log1p(j / 256.0), b = log1p((j + 1) / 256.0);
        s->log[j][0] = (float)a;
        s->log[j][1] = (float)(b - a);
    }
}

/* log(1 + x) for x >= 0 with no libm, no division and no long dependent chain: the exponent apart, the mantissa's top 8
 * bits pick a cell of a 256-entry table of ln(1 + j / 256) and its slope, the remaining 15 bits interpolate. The chord's
 * error is under h^2 / 8 = 1.9e-6 (tests/test_follow.c: under 3e-6 over the spectrum's range). On the S3 the whole bin,
 * interpolation and sum included, is 29 instructions (a degree-8 polynomial took 46, 8 multiply-adds in a row) */
static inline float log1p_fast(const float (*tab)[2], float x)
{
    union {
        float f;
        uint32_t u;
    } v = {.f = 1.0f + x};
    const float *c = tab[(v.u >> 15) & 0xffu];
    float r = (float)(v.u & 0x7fffu) * (1.0f / 32768.0f);
    return c[0] + c[1] * r + (float)((int)(v.u >> 23) - 127) * 0.69314718f;
}

float jb_rate_bin(const struct jb_rate *r, const float *mag, int i)
{
    const struct jr_store *s = r->st;
    float lo = mag[s->idx[i]];
    return log1p_fast(s->log, lo + s->frac[i] * (mag[s->idx[i] + 1] - lo));
}

static void reset(struct jb_rate *r)
{
    r->acc_n = r->in_block = 0;
    r->count = r->head = 0;
    r->run_n = 0;
    r->has_last = r->has_event = 0;
}

void jb_rate_init(struct jb_rate *r, struct jr_store *st, int step, int every)
{
    memset(r, 0, sizeof *r);
    r->st = st;
    r->step = (uint8_t)(step == 1 || step == 2 || step == 5 ? step : JR_STEP);
    r->every = (uint8_t)(every == 1 || every == 2 ? every : JR_EVERY);
    r->est_ratio = 1.0;
    r->confirm = JR_CONFIRM;
    if (st)
        tables(st);
}

void jb_rate_set_active(struct jb_rate *r, int on)
{
    on = on && r->st;
    if (!on == !r->active)
        return;
    r->active = (uint8_t)(on != 0);
    reset(r);
}

/* the dot product of n floats: four sums, so the multiply-adds do not wait on each other */
static float dot4(const float *x, const float *z, int n)
{
    float d0 = 0.0f, d1 = 0.0f, d2 = 0.0f, d3 = 0.0f;
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        d0 += x[i] * z[i];
        d1 += x[i + 1] * z[i + 1];
        d2 += x[i + 2] * z[i + 2];
        d3 += x[i + 3] * z[i + 3];
    }
    for (; i < n; ++i)
        d0 += x[i] * z[i];
    return (d0 + d1) + (d2 + d3);
}

/* the match of the two means (live/rate.py match_ratio): the ratio, and the peak correlation in *peak */
static double match(struct jr_store *s, float *peak)
{
    const float rq = 1.0f / ((float)JR_REF * JR_Q), cq = 1.0f / ((float)JR_CUR * JR_Q);
    float ma = 0.0f, mb = 0.0f, corr[SHIFTS];
    for (int i = 0; i < JR_BINS; ++i) {
        s->a[i] = (float)s->ref[i] * rq;
        s->b[i] = (float)s->cur[i] * cq;
        ma += s->a[i];
        mb += s->b[i];
    }
    ma *= 1.0f / (float)JR_BINS;
    mb *= 1.0f / (float)JR_BINS;
    s->pa[0] = s->pb[0] = 0.0f;
    for (int i = 0; i < JR_BINS; ++i) {
        s->a[i] -= ma;
        s->b[i] -= mb;
        s->pa[i + 1] = s->pa[i] + s->a[i] * s->a[i];
        s->pb[i + 1] = s->pb[i] + s->b[i] * s->b[i];
    }
    int k = 0;
    for (int j = 0; j < SHIFTS; ++j) {
        int sh = j - JR_MAX_SHIFT, n = JR_BINS - (sh < 0 ? -sh : sh);
        const float *x = sh >= 0 ? s->a : s->a - sh, *z = sh >= 0 ? s->b + sh : s->b;
        float nx = sh >= 0 ? s->pa[n] : s->pa[JR_BINS] - s->pa[-sh];
        float nz = sh >= 0 ? s->pb[JR_BINS] - s->pb[sh] : s->pb[n];
        corr[j] = dot4(x, z, n) / (sqrtf(nx > 0.0f ? nx : 0.0f) * sqrtf(nz > 0.0f ? nz : 0.0f) + 1e-12f);
        if (corr[j] > corr[k])
            k = j;
    }
    double frac = 0.0;
    if (k > 0 && k < SHIFTS - 1) {
        double d = (double)corr[k - 1] - 2.0 * corr[k] + corr[k + 1];
        frac = d < 0.0 ? 0.5 * ((double)corr[k - 1] - corr[k + 1]) / d : 0.0;
    }
    *peak = corr[k];
    return exp2((k - JR_MAX_SHIFT + frac) / BPO);
}

static uint16_t quantize(float v)
{
    float q = v * JR_Q + 0.5f;
    return (uint16_t)(q < 65535.0f ? q : 65535.0f);
}

/* a block done: into the ring (the sums kept exact as integers) */
static void push_block(struct jb_rate *r)
{
    struct jr_store *s = r->st;
    const float inv = 1.0f / (float)r->acc_n;
    if (r->count < JR_HIST) {
        uint16_t *row = s->ring[(r->head + r->count) % JR_HIST];
        for (int i = 0; i < JR_BINS; ++i)
            row[i] = quantize(s->acc[i] * inv);
        if (++r->count < JR_HIST)
            return;
        for (int i = 0; i < JR_BINS; ++i) {    /* full for the first time: the sums from the rows */
            int32_t a = 0, b = 0;
            for (int j = 0; j < JR_REF; ++j)
                a += s->ring[(r->head + j) % JR_HIST][i];
            for (int j = JR_REF; j < JR_HIST; ++j)
                b += s->ring[(r->head + j) % JR_HIST][i];
            s->ref[i] = a;
            s->cur[i] = b;
        }
        return;
    }
    uint16_t *old = s->ring[r->head], *mid = s->ring[(r->head + JR_REF) % JR_HIST];
    for (int i = 0; i < JR_BINS; ++i) {
        uint16_t nw = quantize(s->acc[i] * inv);
        s->ref[i] += (int32_t)mid[i] - (int32_t)old[i];
        s->cur[i] += (int32_t)nw - (int32_t)mid[i];
        old[i] = nw;                     /* the oldest row becomes the newest */
    }
    r->head = (uint8_t)((r->head + 1u) % JR_HIST);
}

/* the estimate at time t and the event rule (RateProbe._estimate) */
static void estimate(struct jb_rate *r, double t)
{
    if (r->count < JR_HIST || (r->has_last && t - r->last_t < REFRACTORY_S))
        return;
    float peak;
    double ratio = match(r->st, &peak);
    r->estimates++;
    r->est_ratio = ratio;
    r->est_peak = peak;
    if (fabs(ratio - 1.0) > EVENT_TH && (!r->run_n || fabs(ratio / r->run_last - 1.0) < AGREE))
        r->run_n++;
    else
        r->run_n = fabs(ratio - 1.0) > EVENT_TH ? 1u : 0u;
    if (r->run_n)
        r->run[r->run_n - 1u] = ratio;
    r->run_last = ratio;
    int confirm = r->confirm >= 2 && r->confirm <= JR_CONFIRM_MAX ? r->confirm : JR_CONFIRM;
    if (r->run_n >= confirm) {           /* CONFIRM: their median (np.median: the middle one, or the mean of two) */
        double v[JR_CONFIRM_MAX];
        int n = confirm;
        memcpy(v, r->run, sizeof(double) * (size_t)n);
        for (int i = 1; i < n; ++i)
            for (int j = i; j > 0 && v[j] < v[j - 1]; --j) {
                double tmp = v[j];
                v[j] = v[j - 1];
                v[j - 1] = tmp;
            }
        r->ev_t = t;
        r->ev_ratio = n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
        r->has_event = 1;
        r->events++;
        r->has_last = 1;
        r->last_t = t;
        r->run_n = 0;
    }
}

void jb_rate_frame(struct jb_rate *r, int64_t index, const float *mag)
{
    if (!r->active || index < 0)
        return;
    struct jr_store *s = r->st;
    int pos = (int)(index % JR_BLOCK);
    if (pos == 0) {
        memset(s->acc, 0, sizeof s->acc);
        r->acc_n = 0;
        r->in_block = 1;
    }
    if (!r->in_block)
        return;                          /* begun mid-block: from the next one on */
    if (pos % r->step == 0) {
        for (int i = 0; i < JR_BINS; ++i) {
            float lo = mag[s->idx[i]];
            s->acc[i] += log1p_fast(s->log, lo + s->frac[i] * (mag[s->idx[i] + 1] - lo));
        }
        r->acc_n++;
    }
    if (pos != JR_BLOCK - 1)
        return;
    push_block(r);
    if (((index + 1) / JR_BLOCK) % r->every == 0)
        estimate(r, (double)(index + 1) / 100.0);
}

int jb_rate_take(struct jb_rate *r, double *t, double *ratio)
{
    if (!r->has_event)
        return 0;
    r->has_event = 0;
    *t = r->ev_t;
    *ratio = r->ev_ratio;
    return 1;
}
