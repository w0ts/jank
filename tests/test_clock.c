/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the tick maths. Exit status: the number of failed checks. */
#include <stdio.h>
#include <stdlib.h>
#include "jank_clock.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static int64_t abs64(int64_t v) { return v < 0 ? -v : v; }

static void test_map_exact(void)
{
    struct jc_map m;
    /* 120 BPM: 500000 us a beat, 20833.33 us a tick; at beat 0, time 1000 */
    jc_map_make(&m, 1000, 0, 500000, 0);
    CHECK(m.valid && m.n_ref == 0 && m.t_ref == 1000);
    CHECK(abs64(jc_time(&m, 24) - 501000) <= 1);
    CHECK(abs64(jc_time(&m, 12) - 251000) <= 1);
    /* just after tick 0: the next tick is 1 */
    jc_map_make(&m, 1000, 1, 500000, 0);
    CHECK(m.n_ref == 1);
    /* exactly on tick 24 (beat 1) */
    jc_map_make(&m, 5000, 1000000, 500000, 0);
    CHECK(m.n_ref == 24 && m.t_ref == 5000);
    /* negative beats: the count-in, floor not truncation */
    jc_map_make(&m, 0, -1000000, 500000, 0);
    CHECK(m.n_ref == -24 && m.t_ref == 0);
    jc_map_make(&m, 0, -1000001, 500000, 0);
    CHECK(m.n_ref == -24 && m.t_ref == 0 + 1);
    /* the clock's offset */
    jc_map_make(&m, 1000, 0, 500000, -300);
    CHECK(m.t_ref == 700);
}

static void test_tick_ub(void)
{
    CHECK(jc_tick_ub(0) == 0);
    CHECK(jc_tick_ub(24) == 1000000);
    CHECK(jc_tick_ub(-24) == -1000000);
    CHECK(jc_tick_ub(1) == 41667 || jc_tick_ub(1) == 41666);
    CHECK(jc_tick_ub(240000) == 10000000000ll);
}

/* a steady session for an hour, whatever the tempo: no drift against the exact rational time */
static void test_no_drift(void)
{
    static const int64_t tempos[] = {60060, 333333, 428571, 500000, 500001, 576923, 3000000};
    for (unsigned i = 0; i < sizeof tempos / sizeof tempos[0]; i++) {
        int64_t mpb = tempos[i];
        struct jc_map m;
        struct jc_ticker c;
        struct jc_fire f;
        int64_t ticks = 3600000000ll * 24 / mpb, t;
        jc_map_make(&m, 0, 0, mpb, 0);
        jc_ticker_init(&c, 4);
        CHECK(jc_begin(&c, &m, -1) == 0);
        t = c.t_due;
        for (int64_t k = 0; k < ticks; k++) {
            jc_step(&c, &m, t, &f);
            t = f.next;
        }
        CHECK(abs64(t - ticks * mpb / 24) <= 2);
        CHECK(c.sent == (uint32_t)ticks && c.resyncs == 0);
    }
}

/* the map republished every ~5 ms from a session whose true line is steady: the same schedule to the microsecond */
static void test_republish_is_stable(void)
{
    int64_t mpb = 512345, t, prev = 0, worst = 0;
    struct jc_map m;
    struct jc_ticker c;
    struct jc_fire f;
    jc_map_make(&m, 0, 0, mpb, 0);
    jc_ticker_init(&c, 4);
    CHECK(jc_begin(&c, &m, -1) == 0);
    t = c.t_due;
    for (int k = 0; k < 200000; k++) {
        int64_t pub = t + 3000;
        int64_t ub = pub * 1000000 / mpb;      /* what lk_beat_at says, rounded as it rounds */
        jc_map_make(&m, pub, ub, mpb, 0);
        jc_step(&c, &m, t, &f);
        if (k) {
            int64_t d = abs64((t - prev) - mpb / 24);
            worst = d > worst ? d : worst;
        }
        prev = t;
        t = f.next;
    }
    CHECK(worst <= 2);
    CHECK(c.resyncs == 0);
}

static void test_tempo_change(void)
{
    struct jc_map m;
    struct jc_ticker c;
    struct jc_fire f;
    int64_t t;
    jc_map_make(&m, 0, 0, 500000, 0);
    jc_ticker_init(&c, 4);
    CHECK(jc_begin(&c, &m, -1) == 0);
    t = c.t_due;
    for (int k = 0; k < 24; k++) {
        jc_step(&c, &m, t, &f);
        t = f.next;
    }
    /* the beat kept, the tempo doubled at t */
    jc_map_make(&m, t, jc_tick_ub(c.n), 250000, 0);
    int64_t t0 = t;
    for (int k = 0; k < 24; k++) {
        jc_step(&c, &m, t, &f);
        CHECK(f.next - t < 12000);    /* 10416 us a tick at 240 BPM */
        t = f.next;
    }
    CHECK(abs64(t - t0 - 250000) <= 2);
    CHECK(c.resyncs == 0);
}

/* the session moved a little (a new measurement): no doubled tick, no gap, it slews back */
static void test_slew(void)
{
    int64_t mpb = 500000, tick = mpb / 24;
    static const int64_t shifts[] = {+15000, -15000, +60000, -60000};
    for (unsigned s = 0; s < 4; s++) {
        struct jc_map m;
        struct jc_ticker c;
        struct jc_fire f;
        int64_t t, sh = shifts[s];
        jc_map_make(&m, 0, 0, mpb, 0);
        jc_ticker_init(&c, 4);
        CHECK(jc_begin(&c, &m, -1) == 0);
        t = c.t_due;
        for (int k = 0; k < 10; k++) {
            jc_step(&c, &m, t, &f);
            t = f.next;
        }
        jc_map_make(&m, t, jc_tick_ub(c.n), mpb, (int32_t)sh);   /* tick c.n now belongs at t + sh */
        for (int k = 0; k < 40; k++) {
            jc_step(&c, &m, t, &f);
            CHECK(f.next - t >= tick / 2 - 1 && f.next - t <= tick * 3 / 2 + 1);
            t = f.next;
        }
        CHECK(abs64(t - jc_time(&m, c.n)) <= 1);                 /* back on the line */
        CHECK(c.resyncs == 0);
    }
}

/* the session jumped far: one jump, on phase */
static void test_resync(void)
{
    struct jc_map m;
    struct jc_ticker c;
    struct jc_fire f;
    int64_t t;
    jc_map_make(&m, 0, 0, 500000, 0);
    jc_ticker_init(&c, 4);
    CHECK(jc_begin(&c, &m, -1) == 0);
    t = c.t_due;
    jc_step(&c, &m, t, &f);
    t = f.next;
    jc_map_make(&m, t, 37000000, 500000, 0);    /* beat 37 */
    jc_step(&c, &m, t, &f);
    CHECK(c.resyncs == 1);
    CHECK(c.n >= 37 * 24 && c.n < 37 * 24 + 3);
    CHECK(abs64(f.next - jc_time(&m, c.n)) <= 1);
    /* a stalled interrupt: late by far more than the limit: it does not machine-gun to catch up */
    t = f.next;
    jc_step(&c, &m, t + 500000, &f);
    CHECK(f.next >= t + 500000);
    CHECK(c.resyncs == 2);
}

static uint32_t count(const struct jc_fire *f, uint8_t b)
{
    uint32_t c = 0;
    for (unsigned i = 0; i < f->n; i++)
        c += f->bytes[i] == b;
    return c;
}

static void test_start_on_the_bar(void)
{
    struct jc_map m;
    struct jc_ticker c;
    struct jc_fire f;
    int64_t t;
    jc_map_make(&m, 0, 240000, 500000, 0);      /* just after beat 0.24: tick 6 */
    jc_ticker_init(&c, 4);
    CHECK(jc_begin(&c, &m, -1) == 0);
    t = c.t_due;
    jc_want(&c, 1);
    int started_at = -1;
    for (int k = 0; k < 200; k++) {
        int64_t n = c.n;
        jc_step(&c, &m, t, &f);
        if (count(&f, JC_START)) {
            CHECK(started_at < 0);
            started_at = (int)n;
            CHECK(f.n == 2 && f.bytes[0] == JC_START && f.bytes[1] == JC_CLOCK);  /* start, then the clock it counts from */
        } else {
            CHECK(f.n == 1 && f.bytes[0] == JC_CLOCK);
        }
        t = f.next;
    }
    CHECK(started_at == 96);                    /* bar 2 of quantum 4, not the middle of bar 1 */
    jc_want(&c, 1);                             /* no second start while playing */
    for (int k = 0; k < 200; k++) {
        jc_step(&c, &m, t, &f);
        CHECK(!count(&f, JC_START));
        t = f.next;
    }
    /* stop: at the next tick, before its clock, and the clock goes on */
    jc_want(&c, 0);
    jc_step(&c, &m, t, &f);
    CHECK(f.n == 2 && f.bytes[0] == JC_STOP && f.bytes[1] == JC_CLOCK);
    t = f.next;
    jc_step(&c, &m, t, &f);
    CHECK(f.n == 1 && f.bytes[0] == JC_CLOCK);
    /* a stop before a pending start went out: nothing is sent */
    jc_want(&c, 1);
    jc_want(&c, 0);
    t = f.next;
    for (int k = 0; k < 200; k++) {
        jc_step(&c, &m, t, &f);
        CHECK(f.n == 1);
        t = f.next;
    }
}

/* one click: exactly what goes out, and when */
static void test_click_resync(void)
{
    struct jc_map m;
    struct jc_ticker c;
    struct jc_fire f;
    int64_t t, stop_n = -1, start_n = -1;
    jc_map_make(&m, 0, 10 * 1000000 + 500000, 500000, 0);    /* beat 10.5: tick 252 */
    jc_ticker_init(&c, 4);
    CHECK(jc_begin(&c, &m, -1) == 0);
    t = c.t_due;
    jc_resync(&c);
    for (int k = 0; k < 200; k++) {
        int64_t n = c.n;
        jc_step(&c, &m, t, &f);
        if (count(&f, JC_STOP)) {
            stop_n = n;
            CHECK(f.bytes[0] == JC_STOP && f.bytes[f.n - 1u] == JC_CLOCK);
        }
        if (count(&f, JC_START)) {
            start_n = n;
            CHECK(f.n == 2 && f.bytes[0] == JC_START && f.bytes[1] == JC_CLOCK);
        }
        CHECK(count(&f, JC_CLOCK) == 1);                     /* clocks never stop */
        t = f.next;
    }
    CHECK(stop_n == 252);
    CHECK(start_n == 288);                                    /* beat 12, the next bar line of quantum 4 */
    CHECK(c.playing == 1);
    /* a click exactly on a bar line waits for the next one: stop and start are never in one tick */
    jc_map_make(&m, t, jc_tick_ub(c.n), 500000, 0);
    while (c.n % 96) {
        jc_step(&c, &m, t, &f);
        t = f.next;
    }
    int64_t bar = c.n;
    jc_resync(&c);
    jc_step(&c, &m, t, &f);
    CHECK(f.n == 2 && f.bytes[0] == JC_STOP && f.bytes[1] == JC_CLOCK);
    t = f.next;
    for (int k = 0; k < 95; k++) {
        jc_step(&c, &m, t, &f);
        CHECK(f.n == 1);
        t = f.next;
    }
    CHECK(c.n == bar + 96);
    jc_step(&c, &m, t, &f);
    CHECK(f.n == 2 && f.bytes[0] == JC_START);
    /* a click starts a stopped session's gear too, and stays started until the session's next stop */
    CHECK(c.playing == 1);
    jc_want(&c, 0);
    t = f.next;
    jc_step(&c, &m, t, &f);
    CHECK(f.bytes[0] == JC_STOP);
}

/* the scan: no new maps for a while: the same tempo and phase, to the microsecond */
static void test_free_run_through_a_scan(void)
{
    struct jc_map m;
    struct jc_ticker c;
    struct jc_fire f;
    int64_t t;
    jc_map_make(&m, 0, 0, 511111, 0);
    jc_ticker_init(&c, 4);
    CHECK(jc_begin(&c, &m, -1) == 0);
    t = c.t_due;
    while (t < 1000000) {                 /* a second with no republish at all, far more than a scan */
        jc_step(&c, &m, t, &f);
        t = f.next;
    }
    CHECK(c.resyncs == 0);
    CHECK(abs64(t - c.n * 511111 / 24) <= 1);
}

static void test_negative_beats_bar(void)
{
    struct jc_map m;
    struct jc_ticker c;
    struct jc_fire f;
    int64_t t;
    jc_map_make(&m, 0, -2000000, 500000, 0);    /* the count-in: bar lines still at multiples of 96 */
    jc_ticker_init(&c, 1);
    CHECK(c.bar_ticks == 24);
    CHECK(jc_begin(&c, &m, -1) == 0);
    t = c.t_due;
    for (int k = 0; k < 2; k++) {
        jc_step(&c, &m, t, &f);
        t = f.next;
    }
    jc_want(&c, 1);
    int found = 0;
    for (int k = 0; k < 24 && !found; k++) {
        int64_t n = c.n;
        jc_step(&c, &m, t, &f);
        if (count(&f, JC_START)) {
            found = 1;
            CHECK(n == -24);
        }
        t = f.next;
    }
    CHECK(found);
}

static void test_begin_needs_map(void)
{
    struct jc_map m = {0};
    struct jc_ticker c;
    jc_ticker_init(&c, 4);
    CHECK(jc_begin(&c, &m, 0) == -1);
    jc_ticker_init(&c, 0);
    CHECK(c.bar_ticks == 24);                   /* quantum 0 is treated as 1 */
}

static void test_transport(void)
{
    CHECK(jc_should_play(0, 1, 0) == 1);
    CHECK(jc_should_play(0, 0, 5) == 0);
    CHECK(jc_should_play(1, 0, 1) == 1);
    CHECK(jc_should_play(1, 1, 0) == 0);
}

int main(void)
{
    test_map_exact();
    test_tick_ub();
    test_no_drift();
    test_republish_is_stable();
    test_tempo_change();
    test_slew();
    test_resync();
    test_start_on_the_bar();
    test_click_resync();
    test_free_run_through_a_scan();
    test_negative_beats_bar();
    test_begin_needs_map();
    test_transport();
    printf("clock: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
