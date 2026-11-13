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

struct jb_fe {
    float ring[JB_FRAME];              /* the last 2048 samples, sample n at n mod 2048 */
    float buf[JB_FRAME];               /* the FFT's work area */
    float prev[BEAT_BANDS];            /* the previous frame's log spectrum */
    uint8_t have_prev;
    int64_t n;                         /* samples taken in */
    int64_t frame;                     /* the next frame to compute */
};

void jb_fe_init(struct jb_fe *fe);
/* Takes samples until one frame is complete or n are used up; returns how many it took. When *ready is set, feat holds
 * the frame (BEAT_INPUTS floats) and the call took exactly what that frame needed: call again with the rest. */
size_t jb_fe_push(struct jb_fe *fe, const float *x, size_t n, float *feat, int *ready);
/* The index of the frame that jb_fe_push just completed. */
int64_t jb_fe_last_frame(const struct jb_fe *fe);

#endif
