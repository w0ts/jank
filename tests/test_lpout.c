/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the Launchpad app's outgoing FIFO (core/jank_lpout.c): packets for the Digitone wait here, in order, until its
 * queue has room, so a Note Off is never lost to a full queue. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpout.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void pkt(uint8_t out[4], uint8_t tag)
{
    out[0] = 0x08;
    out[1] = 0x80;
    out[2] = tag;
    out[3] = 0;
}

static void test_order(void)
{
    struct jlo q;
    uint8_t p[4], out[4 * 8];
    jlo_init(&q);
    for (uint8_t i = 0; i < 5; i++) {
        pkt(p, i);
        CHECK(jlo_push(&q, p));
    }
    CHECK(jlo_count(&q) == 5);
    CHECK(jlo_drain(&q, 8, out) == 5);
    for (uint8_t i = 0; i < 5; i++)
        CHECK(out[4 * i + 2] == i);                              /* oldest first */
    CHECK(jlo_count(&q) == 0 && jlo_drain(&q, 8, out) == 0);
}

static void test_room_limits_the_drain(void)
{
    struct jlo q;
    uint8_t p[4], out[4 * 8];
    jlo_init(&q);
    for (uint8_t i = 0; i < 6; i++) {
        pkt(p, i);
        jlo_push(&q, p);
    }
    CHECK(jlo_drain(&q, 0, out) == 0 && jlo_count(&q) == 6);     /* no room: nothing leaves, nothing is lost */
    CHECK(jlo_drain(&q, 4, out) == 4 && out[2] == 0 && out[14] == 3);
    CHECK(jlo_count(&q) == 2);
    CHECK(jlo_drain(&q, 8, out) == 2 && out[2] == 4 && out[6] == 5);
}

static void test_full_and_wrap(void)
{
    struct jlo q;
    uint8_t p[4], out[4 * JLO_PACKETS];
    jlo_init(&q);
    for (uint32_t i = 0; i < JLO_PACKETS; i++) {
        pkt(p, (uint8_t)i);
        CHECK(jlo_push(&q, p));
    }
    pkt(p, 99);
    CHECK(!jlo_push(&q, p));                                     /* full: the new one is refused, the old ones stay */
    CHECK(jlo_count(&q) == JLO_PACKETS);
    CHECK(jlo_drain(&q, 10, out) == 10);
    for (uint8_t i = 0; i < 10; i++) {                           /* the ring wraps */
        pkt(p, (uint8_t)(100 + i));
        CHECK(jlo_push(&q, p));
    }
    CHECK(jlo_drain(&q, JLO_PACKETS, out) == JLO_PACKETS);
    CHECK(out[2] == 10 && out[4 * (JLO_PACKETS - 11) + 2] == JLO_PACKETS - 1 && out[4 * (JLO_PACKETS - 10) + 2] == 100);
    CHECK(out[4 * (JLO_PACKETS - 1) + 2] == 109);
}

static void test_clear(void)
{
    struct jlo q;
    uint8_t p[4], out[4];
    jlo_init(&q);
    pkt(p, 1);
    jlo_push(&q, p);
    jlo_clear(&q);
    CHECK(jlo_count(&q) == 0 && jlo_drain(&q, 1, out) == 0);
    CHECK(jlo_push(&q, p) && jlo_count(&q) == 1);                /* usable again */
}

int main(void)
{
    test_order();
    test_room_limits_the_drain();
    test_full_and_wrap();
    test_clear();
    printf("test_lpout: %d checks, %d failed\n", checks, fails);
    return fails;
}
