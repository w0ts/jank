/* SPDX-License-Identifier: GPL-3.0-only */
#include "jb_kick.h"
#include <math.h>
#include "jb_fe.h"

void jbk_init(struct jb_kick *k)
{
    k->a = (float)(1.0 - exp(-2.0 * M_PI * JBK_CUTOFF_HZ / (double)JB_RATE));
    jbk_reset(k);
}

void jbk_reset(struct jb_kick *k)
{
    k->y1 = k->y2 = k->acc_k = k->acc_x = 0.0f;
    k->n_acc = 0;
    k->frames = 0;
    for (int i = 0; i < JBK_RING; i++)
        k->kick[i] = k->level[i] = JBK_NONE;
    for (int i = 0; i < JBK_MAX_BLOCKS; i++)
        k->block_max[i] = JBK_NONE;
    k->cur_max = JBK_NONE;
    k->blocks = k->block_head = 0;
}

static void hop_done(struct jb_kick *k, float acc_k, float acc_x)
{
    const uint32_t i = (uint32_t)((uint64_t)k->frames % JBK_RING);
    const float kd = 10.0f * log10f(acc_k / (float)JB_HOP + 1e-20f);
    k->kick[i] = kd;
    k->level[i] = 10.0f * log10f(acc_x / (float)JB_HOP + 1e-20f);
    if (kd > k->cur_max)
        k->cur_max = kd;
    k->frames++;
    if (k->frames % JBK_BLOCK == 0) {
        k->block_max[k->block_head] = k->cur_max;
        k->block_head = (uint8_t)((k->block_head + 1u) % JBK_MAX_BLOCKS);
        if (k->blocks < JBK_MAX_BLOCKS)
            k->blocks++;
        k->cur_max = JBK_NONE;
    }
}

void jbk_push_s16(struct jb_kick *k, const int16_t *x, size_t n)
{
    /* (the state in locals: the struct may sit in PSRAM) */
    const float a = k->a, s = 1.0f / 32768.0f;
    float y1 = k->y1, y2 = k->y2, acc_k = k->acc_k, acc_x = k->acc_x;
    unsigned n_acc = k->n_acc;
    for (size_t i = 0; i < n; i++) {
        const float v = (float)x[i] * s;
        y1 += a * (v - y1);
        y2 += a * (y1 - y2);
        acc_k += y2 * y2;
        acc_x += v * v;
        if (++n_acc == JB_HOP) {
            hop_done(k, acc_k, acc_x);
            acc_k = acc_x = 0.0f;
            n_acc = 0;
        }
    }
    k->y1 = y1;
    k->y2 = y2;
    k->acc_k = acc_k;
    k->acc_x = acc_x;
    k->n_acc = (uint16_t)n_acc;
}

/* the frames a .. b are in the ring */
static int have(const struct jb_kick *k, long a, long b) { return a >= 0 && b < k->frames && k->frames - a <= JBK_RING; }

static float at(const float *r, long i) { return r[(uint64_t)i % JBK_RING]; }

int jbk_on_line(const struct jb_kick *k, double line, double period, double tol)
{
    const long a = lround((line - tol * period) * JB_FPS), b = lround((line + tol * period) * JB_FPS) + JBK_BODY;
    const long va = lround((line + 0.35 * period) * JB_FPS), vb = lround((line + 0.85 * period) * JB_FPS);
    if (!have(k, a, vb > b ? vb : b))
        return 0;
    float pk = JBK_NONE, v = -JBK_NONE;
    for (long i = a; i <= b; i++)
        if (at(k->kick, i) > pk)
            pk = at(k->kick, i);
    for (long i = va; i <= vb; i++)
        if (at(k->kick, i) < v)
            v = at(k->kick, i);
    float mx = k->cur_max > pk ? k->cur_max : pk;
    for (int i = 0; i < k->blocks; i++)
        if (k->block_max[i] > mx)
            mx = k->block_max[i];
    return pk >= (float)JBK_FLOOR_DB && pk - v >= (float)JBK_CONTRAST_DB && pk >= mx - (float)JBK_REL_DB;
}

double jbk_line_level(const struct jb_kick *k, double line, double period)
{
    const long a = lround(line * JB_FPS), b = lround((line + period) * JB_FPS) - 1;
    if (b < a || !have(k, a, b))
        return JBK_NONE;
    double acc = 0.0;
    for (long i = a; i <= b; i++)
        acc += pow(10.0, (double)at(k->level, i) / 10.0);
    return 10.0 * log10(acc / (double)(b - a + 1) + 1e-20);
}
