/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat tracker: 44.1 kHz mono audio in, beat times out. madmom's online tracker with its first network and the DBN
 * restricted to 90..180 BPM (the pieces: jb_fe.h, jb_lstm.h, jb_dbn.h). Portable C: no ESP-IDF, no allocation, no
 * threads. Push audio in any chunk size; the call that completes a frame does that frame's work (10 ms of audio apart).
 * RAM: sizeof struct jb_beat (42 KB) and 12 KB of shared tables (window, twiddles) in jb_fe.c; the weights are 123 KB of
 * flash (beat_model.c). On a host with -O2 a frame costs about 20 us. */
#ifndef JB_BEAT_H
#define JB_BEAT_H
#include <stddef.h>
#include "jb_dbn.h"
#include "jb_fe.h"
#include "jb_lstm.h"

#define JB_MIN_BPM 90.0
#define JB_MAX_BPM 180.0

struct jb_beat {
    struct jb_fe fe;
    struct jb_lstm net;
    struct jb_dbn dbn;
};

struct jb_frame {
    int64_t index;                     /* frame number: 10 ms each, frame i is centred on sample 441 i */
    float act;                         /* the network's beat activation */
    int beat;                          /* the DBN calls this frame a beat */
    double time;                       /* its time in seconds, when beat */
};
typedef void (*jb_frame_fn)(void *ctx, const struct jb_frame *f);

int jb_beat_init(struct jb_beat *b);   /* 0 ok */
void jb_beat_reset(struct jb_beat *b);
/* Audio in, mono 44.1 kHz in -1..1; fn is called once per completed frame (it may be NULL). */
void jb_beat_push(struct jb_beat *b, const float *x, size_t n, jb_frame_fn fn, void *ctx);
void jb_beat_push_s16(struct jb_beat *b, const int16_t *x, size_t n, jb_frame_fn fn, void *ctx);

#endif
