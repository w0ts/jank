/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat network's forward pass (jb_lstm.h): madmom's LSTMLayer.activate, frame by frame. */
#include "jb_lstm.h"
#include <math.h>
#include <string.h>

#define H BEAT_HIDDEN

struct layer {
    int n_in;
    const float *w, *r, *b, *p;
};

static const struct layer LAYERS[JB_LAYERS] = {
    {BEAT_INPUTS, beat_l0_w, beat_l0_r, beat_l0_b, beat_l0_p},
    {H, beat_l1_w, beat_l1_r, beat_l1_b, beat_l1_p},
    {H, beat_l2_w, beat_l2_r, beat_l2_b, beat_l2_p},
};

static float sigmoid(float x)
{
    return 0.5f + 0.5f * tanhf(0.5f * x);                      /* madmom's */
}

/* acc[j] = b[j] + sum_i x[i] W[i][j] + sum_i h[i] R[i][j] */
static void gate_sum(float *acc, const float *x, int n_in, const float *w, const float *r, const float *b, const float *h)
{
    memcpy(acc, b, H * sizeof(float));
    for (int i = 0; i < n_in; ++i) {
        const float xi = x[i], *row = w + i * H;
        for (int j = 0; j < H; ++j)
            acc[j] += xi * row[j];
    }
    for (int i = 0; i < H; ++i) {
        const float hi = h[i], *row = r + i * H;
        for (int j = 0; j < H; ++j)
            acc[j] += hi * row[j];
    }
}

static void layer_step(const struct layer *l, float *h, float *c, const float *x)
{
    const int n_in = l->n_in;
    const size_t wg = (size_t)n_in * H, rg = H * H;
    float ig[H], fg[H], cell[H], og[H];
    gate_sum(ig, x, n_in, l->w, l->r, l->b, h);
    gate_sum(fg, x, n_in, l->w + wg, l->r + rg, l->b + H, h);
    gate_sum(cell, x, n_in, l->w + 2 * wg, l->r + 2 * rg, l->b + 2 * H, h);
    for (int j = 0; j < H; ++j) {                              /* input and forget gates peek at the previous state */
        ig[j] = sigmoid(ig[j] + c[j] * l->p[j]);
        fg[j] = sigmoid(fg[j] + c[j] * l->p[H + j]);
        c[j] = tanhf(cell[j]) * ig[j] + c[j] * fg[j];
    }
    gate_sum(og, x, n_in, l->w + 3 * wg, l->r + 3 * rg, l->b + 3 * H, h);
    for (int j = 0; j < H; ++j)                                /* the output gate peeks at the new state */
        h[j] = tanhf(c[j]) * sigmoid(og[j] + c[j] * l->p[2 * H + j]);
}

void jb_lstm_init(struct jb_lstm *s)
{
    memset(s, 0, sizeof *s);
}

float jb_lstm_step(struct jb_lstm *s, const float *feat)
{
    const float *x = feat;
    for (int i = 0; i < JB_LAYERS; ++i) {
        layer_step(&LAYERS[i], s->h[i], s->c[i], x);
        x = s->h[i];
    }
    float a = beat_out_b[0];
    for (int j = 0; j < H; ++j)
        a += x[j] * beat_out_w[j];
    return sigmoid(a);
}
