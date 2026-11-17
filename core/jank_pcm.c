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

uint32_t jp_rs_delay_us(const struct jp_rs *r, uint32_t in_rate)
{
    if ((r->up == 1u && r->down == 1u) || !in_rate)
        return 0;
    /* (L x TAPS - 1) / 2 prototype samples at L x in_rate */
    uint64_t den = 2u * (uint64_t)r->up * in_rate;
    return (uint32_t)(((uint64_t)(r->up * JP_TAPS - 1u) * 1000000u + den / 2u) / den);
}
