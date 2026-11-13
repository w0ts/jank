/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the beat tracker as Link master (core/jank_beatlink.c). Exit status: the number of failed checks. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "jank_beatlink.h"

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
static int sent_nothing(void *ctx, uint32_t ip, uint16_t port, const uint8_t *p, uint32_t n)
{
    (void)ctx, (void)ip, (void)port, (void)p, (void)n;
    return 0;
}

static struct lk_node node;
static struct jbl jbl;

static void start(int64_t now)
{
    struct lk_cfg cfg = {.ip = 0, .port = 0, .send = sent_nothing, .ctx = NULL};
    lk_init(&node, &cfg, 7, 500000);
    lk_enable(&node, 1, now);
    jbl_init(&jbl);
}

/* the session's beat grid against a tracked beat: us off, in (-mpb/2, mpb/2] */
static int64_t grid_error(int64_t beat_us)
{
    int64_t ub = lk_beat_at(&node, beat_us);
    int64_t r = ((ub % LK_UBEAT) + LK_UBEAT) % LK_UBEAT;
    if (r > LK_UBEAT / 2)
        r -= LK_UBEAT;
    return r * lk_mpb(&node) / LK_UBEAT;
}

static void test_phase_forced_past_5ms(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000 + 40000, .period_us = 500000};
    start(10000000);
    jbl_step(&jbl, &node, 1, 10100000, &in, 4);
    CHECK(jbl.phase_forces == 1 && jbl.tempo_sets == 0);
    CHECK(jbl.err_us > 39000 && jbl.err_us < 41000);
    CHECK(abs64(grid_error(in.beat_us)) <= 2);
    CHECK(abs64(grid_error(in.beat_us + 5 * 500000)) <= 2);
    jbl_step(&jbl, &node, 1, 10600000, &in, 4);      /* the same grid again: nothing to do */
    CHECK(jbl.phase_forces == 1);
    in.beat_us += 4000;                              /* 4 ms: inside */
    jbl_step(&jbl, &node, 1, 11100000, &in, 4);
    CHECK(jbl.phase_forces == 1);
    in.beat_us += 4000;                              /* 8 ms from the grid: past 5 */
    jbl_step(&jbl, &node, 1, 11600000, &in, 4);
    CHECK(jbl.phase_forces == 2 && abs64(grid_error(in.beat_us)) <= 2);
}

static void test_tempo_deadband(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    start(10000000);
    jbl_step(&jbl, &node, 1, 10100000, &in, 4);
    CHECK(jbl.tempo_sets == 0 && lk_mpb(&node) == 500000);
    in.period_us = 60e6 / 120.01;                    /* 0.01 BPM: inside the band */
    jbl_step(&jbl, &node, 1, 10200000, &in, 4);
    CHECK(jbl.tempo_sets == 0 && lk_mpb(&node) == 500000);
    in.period_us = 60e6 / 120.05;                    /* 0.05 BPM */
    jbl_step(&jbl, &node, 1, 10300000, &in, 4);
    CHECK(jbl.tempo_sets == 1 && lk_mpb(&node) == (int32_t)llround(60e6 / 120.05));
    in.period_us = 495000;                           /* 121.2 BPM */
    in.beat_us = 10300000;
    jbl_step(&jbl, &node, 1, 10400000, &in, 4);
    CHECK(jbl.tempo_sets == 2 && lk_mpb(&node) == 495000);
    CHECK(abs64(grid_error(in.beat_us)) <= 5000 || jbl.phase_forces >= 1);   /* the tempo change kept the beat at the time: then the grid is followed */
    jbl_step(&jbl, &node, 1, 10500000, &in, 4);
    CHECK(abs64(grid_error(in.beat_us)) <= 5000);
}

static void test_transport(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    int64_t bar = 4 * (int64_t)LK_UBEAT;
    start(10000000);
    jbl_step(&jbl, &node, 1, 10100000, &in, 4);
    CHECK(jbl.starts == 1 && jbl.playing && lk_playing(&node));
    CHECK(lk_play_beat(&node) % bar == 0 && lk_play_beat(&node) > lk_beat_at(&node, 10100000));
    CHECK(lk_play_beat(&node) - lk_beat_at(&node, 10100000) >= LK_UBEAT * 3 / 10 - 1);   /* at least the lead, 150 ms */
    jbl_step(&jbl, &node, 1, 10300000, &in, 4);
    CHECK(jbl.starts == 1);                          /* running on: no second start */
    in.running = 0;
    jbl_step(&jbl, &node, 1, 20000000, &in, 4);
    CHECK(jbl.stops == 1 && !jbl.playing && !lk_playing(&node));
    jbl_step(&jbl, &node, 1, 20200000, &in, 4);
    CHECK(jbl.stops == 1);                           /* stopped stays stopped */
    in.running = 1;
    in.beat_us = 20300000;
    jbl_step(&jbl, &node, 1, 20400000, &in, 3);
    CHECK(jbl.starts == 2 && lk_playing(&node) && lk_play_beat(&node) % (3 * (int64_t)LK_UBEAT) == 0);
}

static void test_not_joined(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10040000, .period_us = 400000};
    start(10000000);
    jbl_step(&jbl, &node, 0, 10100000, &in, 4);
    CHECK(jbl.tempo_sets == 0 && jbl.phase_forces == 0 && jbl.starts == 0 && lk_mpb(&node) == 500000);
}

static void test_drifting_tracked_grid(void)
{
    /* a tracker whose period is 10 ppm off the session's: a force now and then, never one at every step */
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500005};
    int64_t now = 10100000;
    start(10000000);
    for (int i = 0; i < 200; i++, now += 500000) {
        in.beat_us = 10000000 + (int64_t)i * 500005;
        jbl_step(&jbl, &node, 1, now, &in, 4);
    }
    CHECK(jbl.tempo_sets == 0);                      /* 0.0012 BPM */
    CHECK(jbl.phase_forces <= 1);                    /* 200 beats x 5 us = 1 ms in all */
}

int main(void)
{
    test_phase_forced_past_5ms();
    test_tempo_deadband();
    test_transport();
    test_not_joined();
    test_drifting_tracked_grid();
    printf("beatlink: %d checks, %d failed\n", checks, fails);
    return fails;
}
