/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the CLK / RST decision (jank_clockout.h). Exit status: the number of failed checks. */
#include <stdio.h>
#include "jank_clock.h"
#include "jank_clockout.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static struct jco_out tick(struct jco *s, unsigned rate, int stop, int start)
{
    uint8_t b[3];
    unsigned n = 0;
    if (stop)
        b[n++] = JC_STOP;
    if (start)
        b[n++] = JC_START;
    b[n++] = JC_CLOCK;
    return jco_fire(s, b, n, rate);
}

/* clock and reset pulses in `ticks` ticks after a Start (the Start tick is the first) */
static void count(unsigned rate, unsigned ticks, unsigned *clk, unsigned *rst, unsigned *first_clk_gap)
{
    struct jco s;
    jco_init(&s);
    *clk = *rst = 0;
    unsigned last = 0, gap = 0;
    for (unsigned t = 0; t < ticks; t++) {
        struct jco_out o = tick(&s, rate, 0, t == 0);
        if (o.clk) {
            if (*clk == 1)
                gap = t - last;
            last = t;
            (*clk)++;
        }
        *rst += o.rst;
    }
    *first_clk_gap = gap;
}

static void test_divisions(void)
{
    static const unsigned per_bar[JCO_RATES] = {4, 8, 16, 32, 96};   /* a 4 beat bar = 96 ticks */
    static const unsigned gap[JCO_RATES] = {24, 12, 6, 3, 1};
    for (unsigned r = 0; r < JCO_RATES; r++) {
        unsigned clk, rst, g;
        count(r, 96, &clk, &rst, &g);
        CHECK(clk == per_bar[r]);
        CHECK(rst == 1);
        CHECK(jco_ticks_per_step(r) == gap[r]);
        if (r < 4)
            CHECK(g == gap[r]);
    }
    CHECK(jco_ticks_per_step(7) == 6);               /* out of range: the default 1/16 */
}

static void test_start_pulses_both(void)
{
    struct jco s;
    jco_init(&s);
    struct jco_out o = tick(&s, 2, 0, 1);
    CHECK(o.clk == 1 && o.rst == 1);                 /* step 0: both */
    o = tick(&s, 2, 0, 0);
    CHECK(o.clk == 0 && o.rst == 0);
    for (int i = 0; i < 4; i++)
        o = tick(&s, 2, 0, 0);
    o = tick(&s, 2, 0, 0);                           /* tick 6 */
    CHECK(o.clk == 1 && o.rst == 0);                 /* the next step: clock only */
}

static void test_stopped(void)
{
    struct jco s;
    jco_init(&s);
    for (int i = 0; i < 100; i++) {
        struct jco_out o = tick(&s, 4, 0, 0);        /* clocks with no Start: nothing */
        CHECK(!o.clk && !o.rst);
    }
    tick(&s, 4, 0, 1);
    struct jco_out o = tick(&s, 4, 1, 0);            /* a Stop in the tick: its own clock is silent */
    CHECK(!o.clk && !o.rst);
    for (int i = 0; i < 100; i++) {
        o = tick(&s, 4, 0, 0);
        CHECK(!o.clk && !o.rst);
    }
}

static void test_start_while_running(void)
{
    struct jco s;
    jco_init(&s);
    tick(&s, 2, 0, 1);
    for (int i = 0; i < 9; i++)
        tick(&s, 2, 0, 0);                           /* mid step */
    struct jco_out o = tick(&s, 2, 0, 1);            /* Start again: the steps count from here, reset once more */
    CHECK(o.clk == 1 && o.rst == 1);
    o = tick(&s, 2, 0, 0);
    CHECK(!o.clk && !o.rst);
    /* the button: Stop and Start in one tick (resync): the pulse of step 0 once */
    for (int i = 0; i < 5; i++)
        tick(&s, 2, 0, 0);
    o = tick(&s, 2, 1, 1);
    CHECK(o.clk == 1 && o.rst == 1);
}

static void test_rate_change_keeps_grid(void)
{
    struct jco s;
    jco_init(&s);
    tick(&s, 0, 0, 1);
    for (int i = 1; i < 12; i++)
        CHECK(!tick(&s, 0, 0, 0).clk);               /* 1/4: nothing until the next beat */
    struct jco_out o = tick(&s, 1, 0, 0);            /* now 1/8: tick 12 is a step */
    CHECK(o.clk == 1 && o.rst == 0);
}

static void test_bytes_alone(void)
{
    struct jco s;
    jco_init(&s);
    uint8_t d[1] = {0x90};
    struct jco_out o = jco_fire(&s, d, 1, 4);
    CHECK(!o.clk && !o.rst);
    o = jco_fire(&s, d, 0, 4);
    CHECK(!o.clk && !o.rst);
    CHECK(JCO_PULSE_US == 1000);
}

int main(void)
{
    test_divisions();
    test_start_pulses_both();
    test_stopped();
    test_start_while_running();
    test_rate_change_keeps_grid();
    test_bytes_alone();
    printf("%d checks, %d failed\n", checks, fails);
    return fails;
}
