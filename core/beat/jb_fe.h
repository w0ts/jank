/* SPDX-License-Identifier: GPL-3.0-only */
/* The feature front end of madmom's online beat tracker, one frame at a time, no ESP-IDF and no allocation.
 *
 * 44.1 kHz mono in, 100 frames a second out: a 2048-point Hann-windowed frame centred on sample 441 x i (madmom's
 * origin 0: the frame holds 1024 samples of the future, 23 ms), the magnitude of its FFT (bins 0..1023), madmom's
 * logarithmic filterbank (12 bands an octave, 30 Hz to 17 kHz, 81 bands, each filter normalised to sum 1), log10(1 + x),
 * and the positive difference to the previous frame. A frame is the 81 log values then the 81 differences (162 inputs);
 * the first frame's differences are 0. The filterbank is generated into beat_model.c; everything else is computed at
 * start-up (the window and the FFT twiddles, 12 KB of RAM). */
#ifndef JB_FE_H
#define JB_FE_H
#include <stddef.h>
#include <stdint.h>
#include "beat_model.h"

#define JB_RATE 44100
#define JB_FRAME 2048
#define JB_HOP 441                     /* 100 frames a second */
#define JB_FPS 100

#define JB_ALIGN16 __attribute__((aligned(16)))

/* The FFT of the front end: 1024 complex points, interleaved re, im, in place, natural order in and out, forward (e^-i), no
 * scaling; z 16-byte aligned. jb_fft_scalar is the portable one. A build that defines JB_FFT_FAST (the firmware: esp-dsp's
 * assembly kernel, firmware/main/fft_dsp.c) provides jb_fft_fast and its one-time init instead, and the front end calls those. */
void jb_fft_scalar(float *z);
#ifdef JB_FFT_FAST
void jb_fft_fast_init(void);
void jb_fft_fast(float *z);
#endif

/* A frame's |FFT| (bins 0 .. the filterbank's highest), given to a tap during the frame's work: the spectral rate
 * (jb_rate.h) reads the tracker's own spectrum instead of computing a second one. index: the frame's (jb_fe_last_frame
 * once it is done). */
typedef void (*jb_fe_tap)(void *ctx, int64_t index, const float *mag);

struct jb_fe {
    float ring[JB_FRAME];              /* the last 2048 samples, sample n at n mod 2048 */
    float buf[JB_FRAME] JB_ALIGN16;    /* the FFT's work area (16-byte aligned: the S3's assembly FFT needs it) */
    float prev[BEAT_BANDS];            /* the previous frame's log spectrum */
    uint8_t have_prev;
    int64_t n;                         /* samples taken in */
    int64_t frame;                     /* the next frame to compute */
    jb_fe_tap tap;                     /* NULL: none (jb_fe_init clears it) */
    void *tap_ctx;
};

#ifdef JB_FE_STAGES
/* The frame's stages, public only with JB_FE_STAGES (tests, the heat lab): fe->frame's window into fe->buf; the magnitudes from
 * an FFT result (bins 0 .. the filterbank's highest, BEAT_BINS floats at most); the 162 features from magnitudes (updates fe->prev). */
void jb_fe_stage_window(struct jb_fe *fe);
void jb_fe_stage_split(const float *work, float *mag);
void jb_fe_stage_split_ref(const float *work, float *mag);     /* the same with libm's sqrtf: the reference */
void jb_fe_stage_bands(struct jb_fe *fe, const float *mag, float *feat);
void jb_fe_stage_bands_ref(struct jb_fe *fe, const float *mag, float *feat);   /* the same with the flash weights and libm's log10f */
float jb_fe_fast_log10(float v);       /* the bands' log10 for v >= 1 (absolute error under 6e-7 up to 1e5) */
float jb_fe_fast_sqrt(float x);        /* the magnitude stage's sqrt (relative error under 4.8e-6) */
const float *jb_fe_fb_values(void);    /* the weights the cheap bands stage reads (RAM copy or flash) */
int jb_fe_maxbin(void);                /* the highest bin any filter reads, plus one */
#endif

void jb_fe_init(struct jb_fe *fe);
/* Takes samples until one frame is complete or n are used up; returns how many it took. When *ready is set, feat holds
 * the frame (BEAT_INPUTS floats) and the call took exactly what that frame needed: call again with the rest. */
size_t jb_fe_push(struct jb_fe *fe, const float *x, size_t n, float *feat, int *ready);
/* The index of the frame that jb_fe_push just completed. */
int64_t jb_fe_last_frame(const struct jb_fe *fe);

#endif
