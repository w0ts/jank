/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat network with its weights in RAM as int16 (jb_lstm.h, jb_lstm_q16_*): one float scale per gate unit, float
 * activations, a 2 KB tanh table instead of libm's tanhf. The float weights in beat_model.c stay in flash and are read
 * once, at the build. Laid out for one dot product per gate unit: row r = gate * 25 + unit, over [x; h] padded to a
 * multiple of 4. Measured on the timing bench (worktrees/timing-scratch, bench/main/opt/jo_q16.c, the same maths):
 * activation error < 1e-3 and the same beats as the float network; 0.22 M cycles a frame on the S3 against 0.39 M. */
#include "jb_lstm.h"
#include <math.h>
#include <string.h>

#define H BEAT_HIDDEN
#define ROWS (4 * H)
#define PAD0 188                        /* 162 + 25 = 187, padded to a multiple of 4 */
#define PAD1 52                         /* 25 + 25 = 50, padded */
#define TT_N 512
#define TT_LO (-8.0f)
#define TT_SCALE ((float)TT_N / 16.0f)

struct qlayer {
    int n_in, n_pad;
    int16_t *w;                         /* [ROWS][n_pad] */
    const float *b, *p;                 /* biases [ROWS] and peepholes [3 * H]: small, read from beat_model.c */
};

static int16_t s_w0[ROWS * PAD0], s_w1[ROWS * PAD1], s_w2[ROWS * PAD1];
static float s_scale[JB_LAYERS][ROWS];
static float s_tt[TT_N + 1];
static struct qlayer s_layer[JB_LAYERS];
static int s_built;

static float ftanh(float x)
{
    float u = (x - TT_LO) * TT_SCALE;
    if (u <= 0.0f)
        return -1.0f;
    if (u >= (float)TT_N)
        return 1.0f;
    int i = (int)u;
    float f = u - (float)i;
    return s_tt[i] + f * (s_tt[i + 1] - s_tt[i]);
}

static float fsigmoid(float x) { return 0.5f + 0.5f * ftanh(0.5f * x); }

/* the float weight of gate unit row r (gate r / H, unit r % H) for input i of [x; h] */
static float weight_at(const float *w, const float *rec, int n_in, int r, int i)
{
    const int g = r / H, j = r % H;
    if (i < n_in)
        return w[((size_t)g * n_in + i) * H + j];
    if (i < n_in + H)
        return rec[((size_t)g * H + (i - n_in)) * H + j];
    return 0.0f;
}

static void build_layer(int l, int n_in, int n_pad, int16_t *dst, const float *w, const float *rec, const float *b, const float *p)
{
    s_layer[l] = (struct qlayer){n_in, n_pad, dst, b, p};
    for (int r = 0; r < ROWS; ++r) {
        float m = 0.0f;
        for (int i = 0; i < n_pad; ++i)
            m = fmaxf(m, fabsf(weight_at(w, rec, n_in, r, i)));
        const float s = m > 0.0f ? m / 32767.0f : 1.0f;
        s_scale[l][r] = s;
        for (int i = 0; i < n_pad; ++i)
            dst[(size_t)r * n_pad + i] = (int16_t)lrintf(weight_at(w, rec, n_in, r, i) / s);
    }
}

void jb_lstm_q16_build(void)
{
    if (s_built)
        return;
    for (int i = 0; i <= TT_N; ++i)
        s_tt[i] = tanhf(TT_LO + (float)i / TT_SCALE);
    build_layer(0, BEAT_INPUTS, PAD0, s_w0, beat_l0_w, beat_l0_r, beat_l0_b, beat_l0_p);
    build_layer(1, H, PAD1, s_w1, beat_l1_w, beat_l1_r, beat_l1_b, beat_l1_p);
    build_layer(2, H, PAD1, s_w2, beat_l2_w, beat_l2_r, beat_l2_b, beat_l2_p);
    s_built = 1;
}

size_t jb_lstm_q16_ram(void)
{
    return sizeof s_w0 + sizeof s_w1 + sizeof s_w2 + sizeof s_scale + sizeof s_tt;
}

static float dot_q(const int16_t *w, const float *z, int n)
{
    float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    for (int i = 0; i < n; i += 4) {
        a0 += (float)w[i] * z[i];
        a1 += (float)w[i + 1] * z[i + 1];
        a2 += (float)w[i + 2] * z[i + 2];
        a3 += (float)w[i + 3] * z[i + 3];
    }
    return (a0 + a1) + (a2 + a3);
}

float jb_lstm_q16_step(struct jb_lstm *s, const float *feat)
{
    float z[PAD0], a[ROWS];
    const float *x = feat;
    for (int l = 0; l < JB_LAYERS; ++l) {
        const struct qlayer *ly = &s_layer[l];
        float *h = s->h[l], *c = s->c[l];
        memcpy(z, x, (size_t)ly->n_in * sizeof(float));
        memcpy(z + ly->n_in, h, H * sizeof(float));
        memset(z + ly->n_in + H, 0, (size_t)(ly->n_pad - ly->n_in - H) * sizeof(float));
        for (int r = 0; r < ROWS; ++r)
            a[r] = dot_q(ly->w + (size_t)r * ly->n_pad, z, ly->n_pad) * s_scale[l][r] + ly->b[r];
        for (int j = 0; j < H; ++j) {          /* the gates as in jb_lstm.c: input, forget, cell, output; peepholes */
            float ig = fsigmoid(a[j] + c[j] * ly->p[j]);
            float fg = fsigmoid(a[H + j] + c[j] * ly->p[H + j]);
            c[j] = ftanh(a[2 * H + j]) * ig + c[j] * fg;
            h[j] = ftanh(c[j]) * fsigmoid(a[3 * H + j] + c[j] * ly->p[2 * H + j]);
        }
        x = h;
    }
    float o = beat_out_b[0];
    for (int j = 0; j < H; ++j)
        o += x[j] * beat_out_w[j];
    return fsigmoid(o);
}
