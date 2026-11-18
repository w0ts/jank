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
/* The filterbank weights (beat_fb_val, in flash) in internal RAM: the firmware (not the lcd2, which has no RAM to spare) defines
 * JB_FB_IN_RAM and tables() copies them once; every hop reads all 1428, through the flash cache otherwise. */
#ifdef JB_FB_IN_RAM
static float s_fbram[BEAT_FB_NNZ];
#define FB_VAL s_fbram
#else
#define FB_VAL beat_fb_val
#endif
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
#ifdef JB_FFT_FAST
    jb_fft_fast_init();
#endif
#ifdef JB_FB_IN_RAM
    memcpy(s_fbram, beat_fb_val, sizeof s_fbram);
#endif
    s_ready = 1;
}

#ifdef JB_FE_STAGES
int jb_fe_maxbin(void)
{
    tables();
    return s_maxbin;
}
#endif

void jb_fe_init(struct jb_fe *fe)
{
    tables();
    memset(fe, 0, sizeof *fe);
}

/* In-place radix-2 FFT of N2 complex points, interleaved re, im; the input in bit-reversed order is made here. The host's
 * (and every test's) FFT. The firmware builds with JB_FFT_FAST and runs jb_fft_fast (jb_fe.h) instead: the same transform. */
void jb_fft_scalar(float *z)
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
    for (int i = 0; i < N2; i += 2) {                         /* the first stage: its twiddle is 1 (no multiplies, the same values) */
        float *a = z + 2 * i, *b = a + 2;
        float br = b[0], bi = b[1];
        b[0] = a[0] - br;
        b[1] = a[1] - bi;
        a[0] += br;
        a[1] += bi;
    }
    for (int len = 4; len <= N2; len <<= 1) {
        int half = len >> 1, step = N2 / len;                 /* twiddle exp(-2 pi i m / N2) = table[2 m] */
        for (int m = 0; m < half; ++m) {                      /* the twiddle once for the blocks (the same butterflies) */
            const float wr = s_cos[2 * m * step], wi = s_sin[2 * m * step];
            for (int i = 0; i < N2; i += len) {
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

#ifdef JB_FFT_FAST
#define fft jb_fft_fast
#else
#define fft jb_fft_scalar
#endif

/* The frame's stages. With JB_FE_STAGES (the host's stage test, the heat lab's `heat fft`) they are public (jb_fe.h): the lab
 * runs them one by one on a made-up frame to compare variants; otherwise they are static. */
#ifdef JB_FE_STAGES
#define STAGE
#define STAGE_INLINE
#else
#define STAGE static
#define STAGE_INLINE static inline
#endif

/* fe->buf = the windowed frame fe->frame: even samples to re, odd to im (the symmetric window's halves) */
STAGE void jb_fe_stage_window(struct jb_fe *fe)
{
    int64_t start = fe->frame * JB_HOP - JB_FRAME / 2;
    const uint32_t base = (uint32_t)start & (JB_FRAME - 1);    /* (32-bit index arithmetic: the 64-bit adds were library calls) */
    for (int i = 0; i < JB_FRAME; i += 2) {
        float w0 = i < N2 ? s_win[i] : s_win[JB_FRAME - 1 - i];
        float w1 = i + 1 < N2 ? s_win[i + 1] : s_win[JB_FRAME - 2 - i];
        fe->buf[i] = fe->ring[(base + i) & (JB_FRAME - 1)] * w0;
        fe->buf[i + 1] = fe->ring[(base + i + 1) & (JB_FRAME - 1)] * w1;
    }
}

/* sqrt(x) for x >= 0 with no libm: the integer trick's first guess of 1/sqrt(x) (0x5f375a86), two Newton steps, times x. The
 * relative error is under 4.8e-6 over every normal float (measured, tests/test_fe_approx.c) and sqrt(0) = 0; with the software
 * sqrtf the whole stage took 154 cycles a bin on the S3 (measured, heat prof); with this: not yet measured. The 1/sqrt estimate of a denormal is finite, so no NaN. */
STAGE_INLINE float jb_fe_fast_sqrt(float x)
{
    union {
        float f;
        uint32_t u;
    } v = {.f = x};
    v.u = 0x5f375a86u - (v.u >> 1);
    const float h = 0.5f * x;
    float y = v.f;
    y = y * (1.5f - h * y * y);
    y = y * (1.5f - h * y * y);
    return x * y;
}

/* the magnitudes of bins 0..s_maxbin-1 of the 2048-point real FFT, from the 1024-point complex FFT of the packed samples;
 * exact: sqrtf (the reference the stage test compares with), else fast_sqrt */
static inline void split(const float *work, float *mag, int exact)
{
    for (int k = 0; k < s_maxbin; ++k) {
        int kk = k ? N2 - k : 0;                              /* Z[N2 - k], Z[0] for k = 0 */
        float zr = work[2 * k], zi = work[2 * k + 1], cr = work[2 * kk], ci = -work[2 * kk + 1];
        float er = 0.5f * (zr + cr), ei = 0.5f * (zi + ci);   /* even samples' transform */
        float dr = 0.5f * (zr - cr), di = 0.5f * (zi - ci);   /* (Z - conj Z') / 2 = i x odd transform */
        float orr = di, oi = -dr;                             /* divided by i */
        float xr = er + s_cos[k] * orr - s_sin[k] * oi, xi = ei + s_cos[k] * oi + s_sin[k] * orr;
        float q = xr * xr + xi * xi;
        mag[k] = exact ? sqrtf(q) : jb_fe_fast_sqrt(q);
    }
}

STAGE void jb_fe_stage_split(const float *work, float *mag)
{
    split(work, mag, 0);
}

#ifdef JB_FE_STAGES
void jb_fe_stage_split_ref(const float *work, float *mag)
{
    split(work, mag, 1);
}
#endif

/* log10(v) for v >= 1 (the bands' 1 + x) with no libm: v = 2^e m with m in [sqrt(1/2), sqrt(2)) from the float's bits, log10(m)
 * = a degree-7 minimax polynomial in m - 1 (fitted to the interval, no constant term: log10(1) = 0 exactly), plus e log10(2).
 * Absolute error under 6e-7 over 1 .. 1e5 (the front end's range; it grows with the exponent, 1.2e-6 at 1e12; measured,
 * tests/test_fe_approx.c); filterbank + log10f took 798 cycles a band on the S3 (measured, heat prof); with this: not yet measured. */
STAGE_INLINE float jb_fe_fast_log10(float v)
{
    union {
        float f;
        uint32_t u;
    } x = {.f = v};
    const uint32_t t = x.u - 0x3f3504f3u;                     /* 0x3f3504f3: the bits of sqrt(1/2) */
    const int e = (int)t >> 23;
    x.u = (t & 0x007fffffu) + 0x3f3504f3u;
    const float f = x.f - 1.0f;
    float p = 0.0513661071f;
    p = p * f - 0.0820902702f;
    p = p * f + 0.0894849595f;
    p = p * f - 0.108058283f;
    p = p * f + 0.144634387f;
    p = p * f - 0.217155775f;
    p = p * f + 0.434295892f;
    return (float)e * 0.301029996f + p * f;
}

/* the filterbank, log10(1 + x) and the positive difference to fe->prev, from the magnitudes; updates fe->prev. exact: the
 * flash weights and libm's log10f (the reference), else the RAM weights (JB_FB_IN_RAM) and jb_fe_fast_log10 */
static inline void bands(struct jb_fe *fe, const float *mag, float *feat, int exact)
{
    const float *val = exact ? beat_fb_val : FB_VAL;
    for (int j = 0; j < BEAT_BANDS; ++j) {
        float s = 0.0f;
        for (int i = 0; i < beat_fb_len[j]; ++i)
            s += val[i] * mag[beat_fb_start[j] + i];
        val += beat_fb_len[j];
        float lg = exact ? log10f(1.0f + s) : jb_fe_fast_log10(1.0f + s);
        float d = fe->have_prev ? lg - fe->prev[j] : 0.0f;
        feat[j] = lg;
        feat[BEAT_BANDS + j] = d > 0.0f ? d : 0.0f;
        fe->prev[j] = lg;
    }
    fe->have_prev = 1;
}

STAGE void jb_fe_stage_bands(struct jb_fe *fe, const float *mag, float *feat)
{
    bands(fe, mag, feat, 0);
}

#ifdef JB_FE_STAGES
void jb_fe_stage_bands_ref(struct jb_fe *fe, const float *mag, float *feat)
{
    bands(fe, mag, feat, 1);
}
#endif

static void frame_features(struct jb_fe *fe, float *feat)
{
    float mag[BEAT_BINS];
    jb_fe_stage_window(fe);
    fft(fe->buf);
    jb_fe_stage_split(fe->buf, mag);
    if (fe->tap)
        fe->tap(fe->tap_ctx, fe->frame, mag);
    jb_fe_stage_bands(fe, mag, feat);
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
