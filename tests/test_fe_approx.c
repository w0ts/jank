/* SPDX-License-Identifier: GPL-3.0-only */
/* The front end's cheap stages against their exact references (core/beat/jb_fe.c, built with JB_FE_STAGES): the magnitude stage's
 * sqrt and the bands' log10, each on its own over the range they see and on the frames of a synthetic signal, and the 81-band log-mel features and the
 * network's activations of the whole pipeline with the cheap stages against the same pipeline with libm's. Exit status: the
 * number of failed checks.
 *
 * test_fe_approx */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jb_fe.h"
#include "jb_lstm.h"
#include "beatw_load.h"

#define PI_D 3.14159265358979323846
#define SECONDS 20
#define SAMPLES (SECONDS * JB_RATE)

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_sqrt(void)
{
    double worst = 0.0;
    for (double lx = -37.9; lx <= 15.0; lx += 0.0003) {           /* every normal float from 1e-38 on, up to 1e15 */
        float x = (float)pow(10.0, lx);
        worst = fmax(worst, fabs((double)jb_fe_fast_sqrt(x) - sqrt((double)x)) / sqrt((double)x));
    }
    CHECK(jb_fe_fast_sqrt(0.0f) == 0.0f);
    CHECK(jb_fe_fast_sqrt(1e-42f) >= 0.0f && jb_fe_fast_sqrt(1e-42f) < 2e-21f);   /* a denormal: finite, no NaN */
    CHECK(worst < 5e-6);
    printf("fast sqrt: max relative error %.3g over 1e-38 .. 1e15\n", worst);
}

static void test_log10(void)
{
    double worst = 0.0;
    for (double lx = 0.0; lx <= 5.0; lx += 0.00002) {             /* 1 + x from 1 to 1e5: a full-scale tone's bin is about 1e3 */
        float v = (float)pow(10.0, lx);
        worst = fmax(worst, fabs((double)jb_fe_fast_log10(v) - log10((double)v)));
    }
    for (int i = 0; i < 2000000; ++i) {                           /* 1 + small x: where 1.0f + s has few bits */
        float v = 1.0f + (float)i * 1e-7f;
        worst = fmax(worst, fabs((double)jb_fe_fast_log10(v) - log10((double)v)));
    }
    CHECK(jb_fe_fast_log10(1.0f) == 0.0f);
    CHECK(worst < 6e-7);
    printf("fast log10: max absolute error %.3g over 1 .. 1e5 (libm's log10f: 2.4e-7 there)\n", worst);
}

/* ---- the whole pipeline, cheap stages against exact ones ---- */
static uint32_t s_rng = 99u;
static float rnd(void)
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return (float)(s_rng >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

static float s_x[SAMPLES];

static void synth(void)                                           /* drum-like clicks, a bass tone, a chord and noise, loud and quiet parts */
{
    for (int i = 0; i < SAMPLES; ++i) {
        const double t = (double)i / JB_RATE, beat = fmod(t * 2.0, 1.0);
        double v = 0.25 * sin(2 * PI_D * 55.0 * t) * exp(-6.0 * beat);
        v += 0.1 * (sin(2 * PI_D * 440.0 * t) + sin(2 * PI_D * 554.4 * t) + sin(2 * PI_D * 659.3 * t));
        v += (beat < 0.02 ? 0.5 : 0.0) * rnd() + 0.02 * rnd();
        s_x[i] = (float)(v * (t > 14.0 ? 0.001 : 1.0));           /* the last seconds nearly silent */
    }
}

static void features(struct jb_fe *fe, int frame, int exact, float *feat)
{
    static float mag[BEAT_BINS];
    const long due = (long)frame * JB_HOP + JB_FRAME / 2;
    for (long n = due - JB_FRAME; n < due; ++n)                   /* the ring holds the samples n mod 2048 */
        fe->ring[(n < 0 ? n + (1L << 20) : n) & (JB_FRAME - 1)] = n < 0 || n >= SAMPLES ? 0.0f : s_x[n];
    fe->frame = frame;
    jb_fe_stage_window(fe);
    jb_fft_scalar(fe->buf);
    if (exact)
        jb_fe_stage_split_ref(fe->buf, mag);
    else
        jb_fe_stage_split(fe->buf, mag);
    if (exact)
        jb_fe_stage_bands_ref(fe, mag, feat);
    else
        jb_fe_stage_bands(fe, mag, feat);
}

static void test_pipeline(void)
{
    static struct jb_fe fe_new, fe_ref;
    static struct jb_lstm net_new, net_ref;
    static float mag_new[BEAT_BINS], mag_ref[BEAT_BINS];
    float fn[BEAT_INPUTS], fr[BEAT_INPUTS];
    const int frames = SECONDS * 100 - 10, maxbin = jb_fe_maxbin();
    double worst_mag = 0.0, worst_feat = 0.0, worst_act = 0.0;
    synth();
    jb_fe_init(&fe_new);
    jb_fe_init(&fe_ref);
    jb_lstm_init(&net_new);
    jb_lstm_init(&net_ref);
    for (int f = 0; f < frames; ++f) {
        features(&fe_new, f, 0, fn);
        features(&fe_ref, f, 1, fr);
        jb_fe_stage_window(&fe_new);                              /* the magnitudes of this frame, both ways */
        jb_fft_scalar(fe_new.buf);
        jb_fe_stage_split(fe_new.buf, mag_new);
        jb_fe_stage_split_ref(fe_new.buf, mag_ref);
        for (int k = 0; k < maxbin; ++k)
            if (mag_ref[k] > 0.0f)
                worst_mag = fmax(worst_mag, fabs((double)mag_new[k] - mag_ref[k]) / mag_ref[k]);
        for (int j = 0; j < BEAT_INPUTS; ++j)
            worst_feat = fmax(worst_feat, fabs((double)fn[j] - fr[j]));
        worst_act = fmax(worst_act, fabs((double)jb_lstm_step(&net_new, fn) - jb_lstm_step(&net_ref, fr)));
    }
    CHECK(worst_mag < 5e-6);
    CHECK(worst_feat < 1e-5);
    CHECK(worst_act < 1e-3);
    printf("front end, %d frames: magnitude relative error max %.3g; 81 log bands + 81 differences abs error max %.3g; "
           "network activation abs error max %.3g (bar 1e-3)\n", frames, worst_mag, worst_feat, worst_act);
}

int main(void)
{
    beatw_load();
    test_sqrt();
    test_log10();
    test_pipeline();
    printf("%d checks, %d failed\n", checks, fails);
    return fails;
}
