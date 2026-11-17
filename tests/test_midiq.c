/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of USB host mode's MIDI queue (core/jank_midiq.h). Exit status: 1 on a failed check. */
#include <stdio.h>
#include <string.h>
#include "jank_midiq.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const uint8_t F8[4] = {0x0F, 0xF8, 0, 0};
static const uint8_t FA_F8[8] = {0x0F, 0xFA, 0, 0, 0x0F, 0xF8, 0, 0};
static const uint8_t FC_F8[8] = {0x0F, 0xFC, 0, 0, 0x0F, 0xF8, 0, 0};

/* A device whose transfer stays out for several ticks: every F8 reaches it, in order, with the start and stop where
 * they were */
static void test_busy_device_gets_every_clock(void)
{
    struct jmq q;
    uint8_t out[64];
    size_t got, f8 = 0;
    memset(&q, 0, sizeof q);
    CHECK(jmq_add(&q, F8, 4) == 0);
    CHECK(jmq_add(&q, FA_F8, 8) == 0);
    CHECK(jmq_add(&q, F8, 4) == 0);
    CHECK(jmq_add(&q, FC_F8, 8) == 0);
    CHECK(q.n == 24);
    got = jmq_take(&q, out, sizeof out);
    CHECK(got == 24 && q.n == 0);
    for (size_t i = 0; i < got; i += 4)
        f8 += out[i + 1] == 0xF8;
    CHECK(f8 == 4);
    CHECK(out[5] == 0xFA && out[17] == 0xFC);                     /* the order kept */
}

/* a device with an 8-byte endpoint: two packets a transfer, the rest waits */
static void test_small_endpoint(void)
{
    struct jmq q;
    uint8_t out[64];
    memset(&q, 0, sizeof q);
    for (int i = 0; i < 5; i++)
        CHECK(jmq_add(&q, F8, 4) == 0);
    CHECK(jmq_take(&q, out, 8) == 8 && q.n == 12);
    CHECK(jmq_take(&q, out, 10) == 8 && q.n == 4);                /* whole packets only */
    CHECK(jmq_take(&q, out, 8) == 4 && q.n == 0);
    CHECK(jmq_take(&q, out, 8) == 0);
}

/* a device that never completes: 16 packets wait, the rest is counted lost; a part packet is never queued */
static void test_full(void)
{
    struct jmq q;
    uint8_t out[64];
    memset(&q, 0, sizeof q);
    for (int i = 0; i < 16; i++)
        CHECK(jmq_add(&q, F8, 4) == 0);
    CHECK(jmq_add(&q, FA_F8, 8) == 2 && q.n == 64);
    CHECK(jmq_take(&q, out, 64) == 64);
    CHECK(jmq_add(&q, FA_F8, 7) == 0 && q.n == 4);                /* 7 bytes: one whole packet */
}

int main(void)
{
    test_busy_device_gets_every_clock();
    test_small_endpoint();
    test_full();
    printf("midiq: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
