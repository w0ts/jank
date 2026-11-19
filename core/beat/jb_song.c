/* SPDX-License-Identifier: GPL-3.0-only */
#include "jb_song.h"
#include <math.h>
#include <string.h>
#include "beat_model.h"
#include "jb_fe.h"

void js_init(struct jb_song *s)
{
    for (int i = 0; i < JS_BANDS; i++) {
        const double f = (beat_fb_start[i] + (beat_fb_len[i] - 1) / 2.0) * (double)JB_RATE / (2.0 * BEAT_BINS);
        s->pc[i] = (int8_t)(f > 400.0 && f < 4000.0 ? ((long)lround(12.0 * log2(f / 440.0)) % 12 + 12) % 12 : -1);
    }
    s->news = 0;
    js_reset(s);
}

void js_reset(struct jb_song *s)
{
    memset(s->acc, 0, sizeof s->acc);
    s->n_acc = 0;
    s->n_blocks = s->head = 0;
    s->is_new = 0;
    s->last_dist = 0.0f;
}

/* the mean of `n` blocks ending `back` blocks before the newest (back 0: the newest) */
static void mean_of(const struct jb_song *s, int back, int n, double out[12])
{
    for (int c = 0; c < 12; c++)
        out[c] = 0.0;
    for (int j = back; j < back + n; j++) {
        const float *b = s->blocks[(s->head + JS_BLOCKS - 1 - j) % JS_BLOCKS];
        for (int c = 0; c < 12; c++)
            out[c] += b[c];
    }
    double m = 0.0;
    for (int c = 0; c < 12; c++)
        m += out[c] /= n;
    m /= 12.0;
    for (int c = 0; c < 12; c++)
        out[c] -= m;
}

static void decide(struct jb_song *s)
{
    const int have_ref = (int)s->n_blocks - JS_RECENT - JS_GAP;
    s->is_new = 0;
    if (have_ref < JS_MIN_REF)
        return;
    double r[12], f[12], rf = 0.0, rr = 0.0, ff = 0.0;
    mean_of(s, 0, JS_RECENT, r);
    mean_of(s, JS_RECENT + JS_GAP, have_ref < JS_REF ? have_ref : JS_REF, f);
    for (int c = 0; c < 12; c++) {
        rf += r[c] * f[c];
        rr += r[c] * r[c];
        ff += f[c] * f[c];
    }
    s->last_dist = (float)(1.0 - rf / (sqrt(rr * ff) + 1e-12));
    if (s->last_dist >= (float)JS_TH) {
        s->is_new = 1;
        s->news++;
    }
}

void js_frame(struct jb_song *s, const float *bands)
{
    float pcs[12] = {0}, cnt[12] = {0}, m = 0.0f;
    for (int i = 0; i < JS_BANDS; i++)
        if (s->pc[i] >= 0) {
            pcs[s->pc[i]] += bands[i];
            cnt[s->pc[i]] += 1.0f;
        }
    for (int c = 0; c < 12; c++) {
        pcs[c] = cnt[c] > 0.0f ? pcs[c] / cnt[c] : 0.0f;
        m += pcs[c];
    }
    m /= 12.0f;
    for (int c = 0; c < 12; c++)
        s->acc[c] += pcs[c] - m;
    if (++s->n_acc < JS_BLOCK)
        return;
    float *b = s->blocks[s->head];
    for (int c = 0; c < 12; c++) {
        b[c] = s->acc[c] / (float)JS_BLOCK;
        s->acc[c] = 0.0f;
    }
    s->n_acc = 0;
    s->head = (uint16_t)((s->head + 1u) % JS_BLOCKS);
    if (s->n_blocks < JS_BLOCKS)
        s->n_blocks++;
    decide(s);
}
