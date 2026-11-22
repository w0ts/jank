/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat network's weights from a blob (beat_weights.h). */
#include "beat_weights.h"
#include "beat_model.h"
#include <string.h>

const float *beat_l0_w, *beat_l0_r, *beat_l0_b, *beat_l0_p;
const float *beat_l1_w, *beat_l1_r, *beat_l1_b, *beat_l1_p;
const float *beat_l2_w, *beat_l2_r, *beat_l2_b, *beat_l2_p;
const float *beat_out_w, *beat_out_b;

uint32_t beat_weights_crc32(const void *data, size_t n)
{
    const uint8_t *p = data;
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return c ^ 0xffffffffu;
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void beat_weights_unbind(void)
{
    beat_l0_w = beat_l0_r = beat_l0_b = beat_l0_p = NULL;
    beat_l1_w = beat_l1_r = beat_l1_b = beat_l1_p = NULL;
    beat_l2_w = beat_l2_r = beat_l2_b = beat_l2_p = NULL;
    beat_out_w = beat_out_b = NULL;
}

/* the layer's four arrays, in the blob's order, from *f on */
static void take_layer(const float **f, int n_in, const float **w, const float **r, const float **b, const float **p)
{
    *w = *f;
    *f += 4 * n_in * BEAT_HIDDEN;
    *r = *f;
    *f += 4 * BEAT_HIDDEN * BEAT_HIDDEN;
    *b = *f;
    *f += 4 * BEAT_HIDDEN;
    *p = *f;
    *f += 3 * BEAT_HIDDEN;
}

enum beat_weights_err beat_weights_bind(const void *blob, size_t n)
{
    const uint8_t *h = blob;
    beat_weights_unbind();
    if (!blob || n < BEAT_WEIGHTS_HEADER)
        return BW_SHORT;
    if (memcmp(h, BEAT_WEIGHTS_MAGIC, 4) != 0)
        return BW_MAGIC;
    if (le32(h + 4) != BEAT_WEIGHTS_COUNT)
        return BW_COUNT;
    if (n < BEAT_WEIGHTS_HEADER + 4u * BEAT_WEIGHTS_COUNT)
        return BW_SHORT;
    if (((uintptr_t)(h + BEAT_WEIGHTS_HEADER)) % sizeof(float) != 0)
        return BW_ALIGN;
    if (beat_weights_crc32(h + BEAT_WEIGHTS_HEADER, 4u * BEAT_WEIGHTS_COUNT) != le32(h + 8))
        return BW_CRC;
    const float *f = (const float *)(const void *)(h + BEAT_WEIGHTS_HEADER);
    take_layer(&f, BEAT_INPUTS, &beat_l0_w, &beat_l0_r, &beat_l0_b, &beat_l0_p);
    take_layer(&f, BEAT_HIDDEN, &beat_l1_w, &beat_l1_r, &beat_l1_b, &beat_l1_p);
    take_layer(&f, BEAT_HIDDEN, &beat_l2_w, &beat_l2_r, &beat_l2_b, &beat_l2_p);
    beat_out_w = f;
    beat_out_b = f + BEAT_HIDDEN;
    return BW_OK;
}

int beat_weights_ready(void)
{
    return beat_out_b != NULL;
}

const char *beat_weights_why(enum beat_weights_err e)
{
    switch (e) {
    case BW_OK: return "ok";
    case BW_SHORT: return "none";
    case BW_MAGIC: return "none";
    case BW_COUNT: return "wrong size";
    case BW_CRC: return "damaged";
    case BW_ALIGN: return "misaligned";
    }
    return "?";
}
