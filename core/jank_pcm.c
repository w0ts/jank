/* SPDX-License-Identifier: GPL-3.0-only */
/* PCM conversion and the resampler (jank_pcm.h). */
#include "jank_pcm.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int16_t top16(const uint8_t *s, unsigned subframe)
{
    return (int16_t)(uint16_t)(s[subframe - 2u] | (uint16_t)s[subframe - 1u] << 8);
}

size_t jp_to_mono16(const uint8_t *in, size_t bytes, unsigned channels, unsigned subframe, int16_t *out, size_t cap)
{
    size_t frame = (size_t)channels * subframe, frames;
    if (!channels || subframe < 2u || subframe > 4u)
        return 0;
    frames = bytes / frame;
    if (frames > cap)
        frames = cap;
    for (size_t i = 0; i < frames; i++) {
        const uint8_t *f = in + i * frame;
        if (channels == 1u)
            out[i] = top16(f, subframe);
        else                            /* the floor of the mean, as an arithmetic shift does */
            out[i] = (int16_t)(((int32_t)top16(f, subframe) + top16(f + subframe, subframe)) >> 1);
    }
    return frames;
}

static uint32_t gcd(uint32_t a, uint32_t b)
{
    while (b) {
        uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* the prototype at L x in_rate: a sinc cut at 0.45 x the lower rate, Blackman window, length L x JP_TAPS; each phase scaled
 * to a DC gain of exactly 1 so that the phases do not ripple against each other */
static void design(struct jp_rs *r, uint32_t in_rate)
{
    uint32_t len = r->up * JP_TAPS;
    double lower = in_rate < JP_OUT_RATE ? (double)in_rate : (double)JP_OUT_RATE;
    double fc = 0.45 * lower / ((double)in_rate * r->up);   /* cycles per prototype sample */
    double mid = (len - 1u) / 2.0;
    for (uint32_t p = 0; p < r->up; p++) {
        double h[JP_TAPS], sum = 0.0;
        for (uint32_t k = 0; k < JP_TAPS; k++) {
            double t = k * r->up + p - mid;
            double x = 2.0 * fc * t;
            double s = fabs(t) < 1e-9 ? 2.0 * fc : sin(M_PI * x) / (M_PI * t);
            double w = 0.42 - 0.5 * cos(2.0 * M_PI * (k * r->up + p + 0.5) / len) + 0.08 * cos(4.0 * M_PI * (k * r->up + p + 0.5) / len);
            h[k] = s * w;
            sum += h[k];
        }
        for (uint32_t k = 0; k < JP_TAPS; k++) {
            double q = h[k] / sum * 32768.0;
            r->coef[p][k] = (int16_t)(q > 32767.0 ? 32767 : q < -32768.0 ? -32768 : lround(q));
        }
    }
}

int jp_rs_init(struct jp_rs *r, uint32_t in_rate)
{
    uint32_t g;
    memset(r, 0, sizeof *r);
    if (!in_rate)
        return -1;
    g = gcd(in_rate, JP_OUT_RATE);
    r->up = JP_OUT_RATE / g;
    r->down = in_rate / g;
    if (r->up > JP_PHASES_MAX)
        return -1;
    if (r->up != 1u || r->down != 1u)
        design(r, in_rate);
    return 0;
}

void jp_rs_reset(struct jp_rs *r)
{
    memset(r->hist, 0, sizeof r->hist);
    r->phase = 0;
    r->pos = 0;
}

static int16_t one(const struct jp_rs *r, uint32_t phase)
{
    const int16_t *c = r->coef[phase];
    const int16_t *x = r->hist + r->pos + JP_TAPS - 1u;   /* the newest sample: coef[.][0] goes with it */
    int32_t acc = 1 << 14;              /* rounding; sum |c| < 2 for these filters, so the products fit in 32 bits */
    for (uint32_t k = 0; k < JP_TAPS; k++)
        acc += (int32_t)c[k] * x[-(int32_t)k];
    acc >>= 15;
    return (int16_t)(acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc);
}

size_t jp_rs_run(struct jp_rs *r, const int16_t *in, size_t n, int16_t *out, size_t cap)
{
    size_t w = 0;
    if (r->up == 1u && r->down == 1u) {
        if (n > cap)
            n = cap;
        memcpy(out, in, n * sizeof *in);
        return n;
    }
    for (size_t i = 0; i < n; i++) {
        r->hist[r->pos] = in[i];
        r->hist[r->pos + JP_TAPS] = in[i];
        if (++r->pos == JP_TAPS)
            r->pos = 0;
        /* every output whose input index is this one: phase n x M - i x L in [0, L) */
        while (r->phase < r->up) {
            if (w < cap)
                out[w++] = one(r, r->phase);
            r->phase += r->down;
        }
        r->phase -= r->up;
    }
    return w;
}

int jp_cr_init(struct jp_cr *r, uint32_t in_rate_num, uint32_t in_rate_den, uint32_t out_rate)
{
    uint64_t num = in_rate_num, den = (uint64_t)in_rate_den * out_rate, g;
    memset(r, 0, sizeof *r);
    if (!num || !den)
        return -1;
    g = num;                            /* gcd */
    for (uint64_t b = den; b;) {
        uint64_t t = g % b;
        g = b;
        b = t;
    }
    num /= g;
    den /= g;
    if (den > 65535u || num > 65535u)
        return -1;                      /* (ph x 32768 / den stays in 32 bits) */
    r->num = (uint32_t)num;
    r->den = (uint32_t)den;
    jp_cr_reset(r);
    return 0;
}

void jp_cr_reset(struct jp_cr *r)
{
    memset(r->x, 0, sizeof r->x);
    r->ph = 2u * r->den;                /* output 0 sits on input 0, which is x[1] once inputs 0..2 are in */
}

/* Catmull-Rom between x[1] and x[2] at t = f / 32768, in doubled coefficients so that every term is an integer */
static int16_t cr_one(const int16_t *x, int32_t f)
{
    int64_t a = -x[0] + 3 * x[1] - 3 * x[2] + x[3];
    int64_t b = 2 * x[0] - 5 * x[1] + 4 * x[2] - x[3];
    int64_t c = x[2] - x[0];
    int64_t y = ((a * f) >> 15) + b;
    y = ((y * f) >> 15) + c;
    y = ((y * f) >> 15) + 2 * x[1];     /* twice the value */
    y = (y + 1) >> 1;
    return (int16_t)(y > 32767 ? 32767 : y < -32768 ? -32768 : y);
}

size_t jp_cr_run(struct jp_cr *r, const int16_t *in, size_t n, int16_t *out, size_t cap)
{
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        r->x[0] = r->x[1];
        r->x[1] = r->x[2];
        r->x[2] = r->x[3];
        r->x[3] = in[i];
        /* x[1] is input k - 2 now; every output whose position is in [k - 2, k - 1) */
        while (r->ph < r->den) {
            if (w < cap)
                out[w++] = cr_one(r->x, (int32_t)((r->ph << 15) / r->den));
            r->ph += r->num;
        }
        r->ph -= r->den;
    }
    return w;
}

uint32_t jp_rs_delay_us(const struct jp_rs *r, uint32_t in_rate)
{
    if ((r->up == 1u && r->down == 1u) || !in_rate)
        return 0;
    /* (L x TAPS - 1) / 2 prototype samples at L x in_rate */
    uint64_t den = 2u * (uint64_t)r->up * in_rate;
    return (uint32_t)(((uint64_t)(r->up * JP_TAPS - 1u) * 1000000u + den / 2u) / den);
}
