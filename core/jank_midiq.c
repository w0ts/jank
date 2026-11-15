/* SPDX-License-Identifier: GPL-3.0-only */
/* A device's waiting USB-MIDI packets (jank_midiq.h). */
#include "jank_midiq.h"
#include <string.h>

uint32_t jmq_add(struct jmq *q, const uint8_t *packets, size_t n)
{
    size_t room = sizeof q->b - q->n, k;
    n &= ~(size_t)3u;
    k = n < room ? n : room;
    memcpy(q->b + q->n, packets, k);
    q->n = (uint8_t)(q->n + k);
    return (uint32_t)((n - k) / 4u);
}

size_t jmq_take(struct jmq *q, uint8_t *out, size_t max)
{
    size_t k = q->n < max ? q->n : max;
    k &= ~(size_t)3u;
    memcpy(out, q->b, k);
    q->n = (uint8_t)(q->n - k);
    memmove(q->b, q->b + k, q->n);
    return k;
}
