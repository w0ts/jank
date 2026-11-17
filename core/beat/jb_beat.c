/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat tracker's pipeline (jb_beat.h). */
#include "jb_beat.h"
#include <string.h>

int jb_beat_init(struct jb_beat *b)
{
    jb_fe_init(&b->fe);
    jb_lstm_init(&b->net);
    b->q16 = 0;
    b->tap = NULL;
    b->tap_ctx = NULL;
    return jb_dbn_init(&b->dbn, JB_MIN_BPM, JB_MAX_BPM, JB_FPS);
}

int jb_beat_init_q16(struct jb_beat *b)
{
    int r = jb_beat_init(b);
    jb_lstm_q16_build();
    b->q16 = 1;
    return r;
}

void jb_beat_reset(struct jb_beat *b)
{
    jb_fe_init(&b->fe);
    b->fe.tap = b->tap;
    b->fe.tap_ctx = b->tap_ctx;
    jb_lstm_init(&b->net);
    jb_dbn_reset(&b->dbn);
}

void jb_beat_set_tap(struct jb_beat *b, jb_fe_tap fn, void *ctx)
{
    b->tap = b->fe.tap = fn;
    b->tap_ctx = b->fe.tap_ctx = ctx;
}

void jb_beat_push(struct jb_beat *b, const float *x, size_t n, jb_frame_fn fn, void *ctx)
{
    float feat[BEAT_INPUTS];
    while (n) {
        int ready;
        const uint32_t c0 = b->clock ? b->clock() : 0u;
        size_t took = jb_fe_push(&b->fe, x, n, feat, &ready);
        if (b->clock) {
            uint32_t c = b->clock() - c0;
            b->cyc_fe += c;
            b->max_fe = c > b->max_fe ? c : b->max_fe;
        }
        x += took;
        n -= took;
        if (!ready)
            continue;
        struct jb_frame f = {.index = jb_fe_last_frame(&b->fe), .bands = feat};
        const uint32_t c1 = b->clock ? b->clock() : 0u;
        f.act = b->q16 ? jb_lstm_q16_step(&b->net, feat) : jb_lstm_step(&b->net, feat);
        const uint32_t c2 = b->clock ? b->clock() : 0u;
        f.beat = jb_dbn_step(&b->dbn, f.act, &f.time);
        if (b->clock) {
            uint32_t cd = b->clock() - c2;
            b->cyc_net += c2 - c1;
            b->cyc_dbn += cd;
            b->max_net = c2 - c1 > b->max_net ? c2 - c1 : b->max_net;
            b->max_dbn = cd > b->max_dbn ? cd : b->max_dbn;
        }
        if (fn)
            fn(ctx, &f);
    }
}

void jb_beat_push_s16(struct jb_beat *b, const int16_t *x, size_t n, jb_frame_fn fn, void *ctx)
{
    float tmp[256];
    while (n) {
        size_t m = n < 256 ? n : 256;
        for (size_t i = 0; i < m; ++i)
            tmp[i] = (float)x[i] * (1.0f / 32768.0f);
        jb_beat_push(b, tmp, m, fn, ctx);
        x += m;
        n -= m;
    }
}
