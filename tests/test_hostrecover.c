/* SPDX-License-Identifier: GPL-3.0-only */
/* The software replug (core/jank_hostrecover.h): when, how often, and back on. */
#include <stdio.h>
#include "jank_hostrecover.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_a_cycle_after_three_failed_claims_then_power_back_on(void)
{
    struct jhr s;
    jhr_init(&s);
    CHECK(jhr_step(&s, 0, 1000) == JHR_NONE && jhr_step(&s, 2, 5000) == JHR_NONE);   /* fewer than three: the retries go on */
    CHECK(jhr_wait_ms(&s, 5000) == UINT32_MAX);
    CHECK(jhr_step(&s, 3, 7000) == JHR_POWER_OFF && s.count == 1);
    CHECK(jhr_wait_ms(&s, 7000) == JHR_OFF_MS && jhr_wait_ms(&s, 7600) == 400 && jhr_wait_ms(&s, 9000) == 0);
    CHECK(jhr_step(&s, 3, 7999) == JHR_NONE);                      /* off for a second */
    CHECK(jhr_step(&s, 3, 8000) == JHR_POWER_ON && jhr_wait_ms(&s, 8000) == UINT32_MAX);
    CHECK(jhr_step(&s, 0, 8100) == JHR_NONE);                      /* (the caller counts its fails again from 0) */
}

static void test_at_most_three_cycles_a_minute_apart(void)
{
    struct jhr s;
    uint32_t t = 10000;
    jhr_init(&s);
    CHECK(jhr_step(&s, 3, t) == JHR_POWER_OFF);
    CHECK(jhr_step(&s, 3, t + 1000) == JHR_POWER_ON);
    CHECK(jhr_step(&s, 5, t + 30000) == JHR_NONE);                 /* failing again within a minute: wait */
    CHECK(jhr_step(&s, 5, t + JHR_GAP_MS) == JHR_POWER_OFF && s.count == 2);
    CHECK(jhr_step(&s, 5, t + JHR_GAP_MS + 1000) == JHR_POWER_ON);
    CHECK(jhr_step(&s, 5, t + 2u * JHR_GAP_MS) == JHR_POWER_OFF && s.count == 3);
    CHECK(jhr_step(&s, 5, t + 2u * JHR_GAP_MS + 1000) == JHR_POWER_ON);
    CHECK(jhr_step(&s, 50, t + 10u * JHR_GAP_MS) == JHR_NONE && s.count == 3);   /* three in a start, never more */
}

static void test_the_clock_wraps(void)
{
    struct jhr s;
    jhr_init(&s);
    CHECK(jhr_step(&s, 3, UINT32_MAX - 500u) == JHR_POWER_OFF);
    CHECK(jhr_step(&s, 3, 400u) == JHR_NONE && jhr_step(&s, 3, 500u) == JHR_POWER_ON);   /* 1001 ms later, across the wrap */
}

int main(void)
{
    test_a_cycle_after_three_failed_claims_then_power_back_on();
    test_at_most_three_cycles_a_minute_apart();
    test_the_clock_wraps();
    printf("test_hostrecover: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
