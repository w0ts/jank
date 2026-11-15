/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat tracker's feature front end (jb_fe.h). The window and the twiddles are shared by all instances. */
#include "jb_fe.h"
#include <math.h>
#include <string.h>

#define N2 (JB_FRAME / 2)              /* the complex FFT's size */
#define PI_D 3.14159265358979323846

static float s_win[N2];                /* np.hanning(2048), the first half: it is symmetric */
static float s_cos[N2], s_sin[N2];     /* exp(-2 pi i k / 2048), k < 1024 */
static uint8_t s_ready;
static int s_maxbin;                   /* the highest bin any filter reads, plus one */

static void tables(void)
{
    if (s_ready)
        return;
    for (int k = 0; k < N2; ++k) {
        s_win[k] = (float)(0.5 - 0.5 * cos(2.0 * PI_D * k / (JB_FRAME - 1)));
        s_cos[k] = (float)cos(2.0 * PI_D * k / JB_FRAME);
        s_sin[k] = (float)-sin(2.0 * PI_D * k / JB_FRAME);
    }
    for (int j = 0; j < BEAT_BANDS; ++j)
        if (beat_fb_start[j] + beat_fb_len[j] > s_maxbin)
            s_maxbin = beat_fb_start[j] + beat_fb_len[j];
    s_ready = 1;
}

void jb_fe_init(struct jb_fe *fe)
{
    tables();
    memset(fe, 0, sizeof *fe);
}

/* In-place radix-2 FFT of N2 complex points, interleaved re, im; the input in bit-reversed order is made here. */
static void fft(float *z)
{
    for (int i = 1, j = 0; i < N2; ++i) {
        int bit = N2 >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            float tr = z[2 * i], ti = z[2 * i + 1];
            z[2 * i] = z[2 * j];
            z[2 * i + 1] = z[2 * j + 1];
            z[2 * j] = tr;
            z[2 * j + 1] = ti;
        }
    }
    for (int len = 2; len <= N2; len <<= 1) {
        int half = len >> 1, step = N2 / len;                 /* twiddle exp(-2 pi i m / N2) = table[2 m] */
        for (int i = 0; i < N2; i += len) {
            for (int m = 0; m < half; ++m) {
                float wr = s_cos[2 * m * step], wi = s_sin[2 * m * step];
                float *a = z + 2 * (i + m), *b = z + 2 * (i + m + half);
                float xr = b[0] * wr - b[1] * wi, xi = b[0] * wi + b[1] * wr;
                b[0] = a[0] - xr;
                b[1] = a[1] - xi;
                a[0] += xr;
                a[1] += xi;
            }
        }
    }
}

/* the magnitudes of bins 0..s_maxbin-1 of the 2048-point real FFT of fe->buf's windowed samples, into mag */
static void magnitudes(float *work, float *mag)
{
    fft(work);
    for (int k = 0; k < s_maxbin; ++k) {
        int kk = k ? N2 - k : 0;                              /* Z[N2 - k], Z[0] for k = 0 */
        float zr = work[2 * k], zi = work[2 * k + 1], cr = work[2 * kk], ci = -work[2 * kk + 1];
        float er = 0.5f * (zr + cr), ei = 0.5f * (zi + ci);   /* even samples' transform */
        float dr = 0.5f * (zr - cr), di = 0.5f * (zi - ci);   /* (Z - conj Z') / 2 = i x odd transform */
        float orr = di, oi = -dr;                             /* divided by i */
        float xr = er + s_cos[k] * orr - s_sin[k] * oi, xi = ei + s_cos[k] * oi + s_sin[k] * orr;
        mag[k] = sqrtf(xr * xr + xi * xi);
    }
}

static void frame_features(struct jb_fe *fe, float *feat)
{
    float mag[BEAT_BINS];
    int64_t start = fe->frame * JB_HOP - JB_FRAME / 2;
    for (int i = 0; i < JB_FRAME; i += 2) {                   /* even samples to re, odd to im, windowed */
        float w0 = s_win[i < N2 ? i : JB_FRAME - 1 - i], w1 = s_win[i + 1 < N2 ? i + 1 : JB_FRAME - 2 - i];
        fe->buf[i] = fe->ring[(start + i) & (JB_FRAME - 1)] * w0;
        fe->buf[i + 1] = fe->ring[(start + i + 1) & (JB_FRAME - 1)] * w1;
    }
    magnitudes(fe->buf, mag);
    if (fe->tap)
        fe->tap(fe->tap_ctx, fe->frame, mag);
    const float *val = beat_fb_val;
    for (int j = 0; j < BEAT_BANDS; ++j) {
        float s = 0.0f;
        for (int i = 0; i < beat_fb_len[j]; ++i)
            s += val[i] * mag[beat_fb_start[j] + i];
        val += beat_fb_len[j];
        float lg = log10f(1.0f + s);
        float d = fe->have_prev ? lg - fe->prev[j] : 0.0f;
        feat[j] = lg;
        feat[BEAT_BANDS + j] = d > 0.0f ? d : 0.0f;
        fe->prev[j] = lg;
    }
    fe->have_prev = 1;
}

size_t jb_fe_push(struct jb_fe *fe, const float *x, size_t n, float *feat, int *ready)
{
    int64_t due = fe->frame * JB_HOP + JB_FRAME / 2;          /* samples needed before this frame is complete */
    size_t take = 0;
    *ready = 0;
    while (take < n && fe->n < due)
        fe->ring[(fe->n++) & (JB_FRAME - 1)] = x[take++];
    if (fe->n >= due) {
        frame_features(fe, feat);
        ++fe->frame;
        *ready = 1;
    }
    return take;
}

int64_t jb_fe_last_frame(const struct jb_fe *fe)
{
    return fe->frame - 1;
}
