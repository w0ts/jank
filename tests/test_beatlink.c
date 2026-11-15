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
static struct jst st;

static void start(int64_t now)
{
    struct lk_cfg cfg = {.ip = 0, .port = 0, .send = sent_nothing, .ctx = NULL};
    lk_init(&node, &cfg, 7, 500000);
    lk_enable(&node, 1, now);
    jbl_init(&jbl);
    jst_cancel(&st);
    st.commits = st.moves = 0;
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
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    CHECK(jbl.phase_forces == 1 && jbl.tempo_sets == 0);
    CHECK(jbl.err_us > 39000 && jbl.err_us < 41000);
    CHECK(abs64(grid_error(in.beat_us)) <= 2);
    CHECK(abs64(grid_error(in.beat_us + 5 * 500000)) <= 2);
    jbl_step(&jbl, &node, &st, 1, 10600000, &in, 4);      /* the same grid again: nothing to do */
    CHECK(jbl.phase_forces == 1);
    in.beat_us += 4000;                              /* 4 ms: inside */
    jbl_step(&jbl, &node, &st, 1, 11100000, &in, 4);
    CHECK(jbl.phase_forces == 1);
    in.beat_us += 4000;                              /* 8 ms from the grid: past 5 */
    jbl_step(&jbl, &node, &st, 1, 11600000, &in, 4);
    CHECK(jbl.phase_forces == 2 && abs64(grid_error(in.beat_us)) <= 2);
}

static void test_tempo_deadband(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    CHECK(jbl.tempo_sets == 0 && lk_mpb(&node) == 500000);
    in.period_us = 60e6 / 120.01;                    /* 0.01 BPM: inside the band */
    jbl_step(&jbl, &node, &st, 1, 10200000, &in, 4);
    CHECK(jbl.tempo_sets == 0 && lk_mpb(&node) == 500000);
    in.period_us = 60e6 / 120.05;                    /* 0.05 BPM */
    jbl_step(&jbl, &node, &st, 1, 10300000, &in, 4);
    CHECK(jbl.tempo_sets == 1 && lk_mpb(&node) == (int32_t)llround(60e6 / 120.05));
    in.period_us = 495000;                           /* 121.2 BPM */
    in.beat_us = 10300000;
    jbl_step(&jbl, &node, &st, 1, 10400000, &in, 4);
    CHECK(jbl.tempo_sets == 2 && lk_mpb(&node) == 495000);
    CHECK(abs64(grid_error(in.beat_us)) <= 5000 || jbl.phase_forces >= 1);   /* the tempo change kept the beat at the time: then the grid is followed */
    jbl_step(&jbl, &node, &st, 1, 10500000, &in, 4);
    CHECK(abs64(grid_error(in.beat_us)) <= 5000);
}

/* polls the start every 2 ms from t to t_end (as the bridge's task does): the time it went out, 0 none */
static int64_t poll_start(int64_t t, int64_t t_end, int allowed)
{
    for (; t <= t_end; t += 2000)
        if (jst_poll(&st, &node, t, allowed))
            return t;
    return 0;
}

static void test_transport(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    int64_t bar = 4 * (int64_t)LK_UBEAT, sent, t_bar;
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    CHECK(jbl.starts == 1 && jbl.playing && st.armed && !lk_playing(&node));    /* armed: nothing sent before its time */
    CHECK(st.ub % bar == 0 && st.ub - lk_beat_at(&node, 10100000) >= LK_UBEAT * 3 / 10);   /* a bar line, the lead ahead */
    t_bar = lk_time_at(&node, st.ub);
    sent = poll_start(10100000, 14000000, 1);
    CHECK(sent >= t_bar - JST_LEAD_US && sent < t_bar - JST_LEAD_US + 2000);   /* the lead before the bar, no sooner */
    CHECK(lk_playing(&node) && lk_play_beat(&node) == st.ub && !st.armed);   /* playing from bar B, set at `sent` */
    CHECK(node.ss.ts < lk_time_at(&node, st.ub) + node.icpt);                 /* its time stamp is the setting's, before B */
    jbl_step(&jbl, &node, &st, 1, sent + 200000, &in, 4);
    CHECK(jbl.starts == 1 && !st.armed);             /* running on: no second start */
    in.running = 0;
    jbl_step(&jbl, &node, &st, 1, 20000000, &in, 4);
    CHECK(jbl.stops == 1 && !jbl.playing && !lk_playing(&node));             /* stop: at once */
    jbl_step(&jbl, &node, &st, 1, 20200000, &in, 4);
    CHECK(jbl.stops == 1);                           /* stopped stays stopped */
    in.running = 1;
    in.beat_us = 20300000;
    jbl_step(&jbl, &node, &st, 1, 20400000, &in, 3);
    CHECK(jbl.starts == 2 && st.armed && st.ub % (3 * (int64_t)LK_UBEAT) == 0);
    in.running = 0;                                  /* stopped before its bar: the start is never sent */
    jbl_step(&jbl, &node, &st, 1, 20450000, &in, 3);
    CHECK(!st.armed && poll_start(20450000, 24000000, 1) == 0 && !lk_playing(&node));
}

/* the audio stream closes (LINK mode): the session stays as it is; a start not sent is dropped */
static void test_passive(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    poll_start(10100000, 14000000, 1);
    CHECK(lk_playing(&node));
    in.passive = 1;
    in.running = 0;
    jbl_step(&jbl, &node, &st, 1, 15000000, &in, 4);
    CHECK(lk_playing(&node) && !jbl.playing && jbl.stops == 0 && jbl.releases == 1);
    in.passive = 0;                                  /* AUDIO again, the session playing: taken over, not restarted */
    in.running = 1;
    jbl_step(&jbl, &node, &st, 1, 16000000, &in, 4);
    CHECK(jbl.playing && !st.armed && jbl.starts == 1);
    start(30000000);                                 /* armed, then LINK mode before its bar: nothing is sent */
    jbl_step(&jbl, &node, &st, 1, 30100000, &in, 4);
    CHECK(st.armed);
    in.passive = 1;
    jbl_step(&jbl, &node, &st, 1, 30150000, &in, 4);
    CHECK(!st.armed && poll_start(30150000, 34000000, 1) == 0 && !lk_playing(&node));
}

/* the start's own rules */
static void test_start_rules(void)
{
    int64_t q = 4 * (int64_t)LK_UBEAT, t_bar, b;
    CHECK(jst_bar_at_or_after(0, 4) == 0);
    CHECK(jst_bar_at_or_after(1, 4) == q);
    CHECK(jst_bar_at_or_after(q, 4) == q);
    CHECK(jst_bar_at_or_after(-1, 4) == 0);
    CHECK(jst_bar_at_or_after(-q - 1, 4) == -q);
    CHECK(jst_bar_at_or_after(2500000, 3) == 3000000);
    start(10000000);
    jst_arm(&st, &node, 10000000, 4);
    b = st.ub;
    jst_arm(&st, &node, 10500000, 4);                /* armed already: the same bar */
    CHECK(st.ub == b);
    t_bar = lk_time_at(&node, b);
    CHECK(poll_start(10000000, t_bar, 0) == 0 && st.armed && st.ub == b + q);   /* not allowed (unsettled): the next bar */
    CHECK(st.moves == 1);
    t_bar = lk_time_at(&node, st.ub);
    CHECK(jst_poll(&st, &node, t_bar - JST_LATE_US + 1000, 1) == 0 && st.ub == b + 2 * q);   /* too late: the next bar */
    lk_set_playing(&node, 1, lk_beat_at(&node, t_bar), t_bar);    /* another peer started the session */
    CHECK(jst_poll(&st, &node, t_bar + 1000, 1) == 0 && !st.armed && st.commits == 0);
}

static void test_not_joined(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10040000, .period_us = 400000};
    start(10000000);
    jbl_step(&jbl, &node, &st, 0, 10100000, &in, 4);
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
        jbl_step(&jbl, &node, &st, 1, now, &in, 4);
    }
    CHECK(jbl.tempo_sets == 0);                      /* 0.0012 BPM */
    CHECK(jbl.phase_forces <= 1);                    /* 200 beats x 5 us = 1 ms in all */
}

/* the reports' session offset: + when the session's beat comes after the instant */
static void test_session_offset(void)
{
    const int32_t mpb = 500000;                                   /* 120 BPM */
    CHECK(jbl_session_offset_us(10 * (int64_t)LK_UBEAT, mpb) == 0);
    CHECK(jbl_session_offset_us(10 * (int64_t)LK_UBEAT - 20000, mpb) == 10000);     /* at 9.98 beats: next beat in 10 ms */
    CHECK(jbl_session_offset_us(10 * (int64_t)LK_UBEAT + 20000, mpb) == -10000);    /* at 10.02: it was 10 ms ago */
    CHECK(jbl_session_offset_us(-(int64_t)LK_UBEAT - 20000, mpb) == 10000);         /* at -1.02: beat -1 comes in 10 ms */
    CHECK(jbl_session_offset_us(10 * (int64_t)LK_UBEAT + 499999, mpb) == -250000);  /* the nearest beat is behind */
    CHECK(jbl_session_offset_us(10 * (int64_t)LK_UBEAT + 500001, mpb) == 250000);   /* ... ahead */
}

static void test_bar_shift(void)
{
    const int64_t B = LK_UBEAT;
    CHECK(jbl_bar_shift(100 * B, 7, 2) == 1);                     /* beat 7 is bar position 1: session 100 -> 101 */
    CHECK(jbl_bar_shift(100 * B, 7, 3) == 0);
    CHECK(jbl_bar_shift(100 * B, 7, 0) == -1);                    /* position 3: 100 -> 99 */
    CHECK(jbl_bar_shift(100 * B, 7, 1) == 2);
    CHECK(jbl_bar_shift(100 * B + B / 2, 7, 3) == 0);             /* half a beat off (a half nudge): the same beat */
    CHECK(jbl_bar_shift(100 * B - 20000, 7, 3) == 0);             /* 20 ms early: the same beat */
    CHECK(jbl_bar_shift(-3 * B, 1, 0) == 0);                      /* before the session's beat 0 */
}

/* AUTO BAR: the session's bar moves whole beats onto the 1, once a decision, the phase in tolerance, the tempo kept;
 * an armed start re-targets the moved grid's next bar line */
static void test_bar_moves(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    struct jbl_bar bar = {.locked = 1, .seq = 5, .count = 7, .phi = 2};
    int64_t bar_ub = 4 * (int64_t)LK_UBEAT, ub;
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);             /* in phase (the session started at the tracked beat) */
    CHECK(st.armed);
    bar.beat_us = 10000000 + 4 * 500000;                          /* a counted beat on the grid */
    ub = lk_beat_at(&node, bar.beat_us);
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10200000, &in, &bar) == jbl_bar_shift(ub, 7, 2));
    CHECK(((lk_beat_at(&node, bar.beat_us) / LK_UBEAT) % 4 + 4) % 4 == (7 - 2) % 4);   /* that beat is now position 1 */
    CHECK(lk_mpb(&node) == 500000 && abs64(grid_error(bar.beat_us)) <= 2);              /* tempo and phase kept */
    CHECK(st.armed && st.ub % bar_ub == 0 && st.ub - lk_beat_at(&node, 10200000) >= LK_UBEAT * 3 / 10);
    CHECK(jbl.bar_moves == (jbl_bar_shift(ub, 7, 2) != 0));
    ub = lk_beat_at(&node, bar.beat_us);
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10300000, &in, &bar) == 0);   /* the same decision: nothing more */
    CHECK(lk_beat_at(&node, bar.beat_us) == ub);
    bar.seq = 6;
    bar.phi = 3;                                                  /* a later move: position 0 */
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10400000, &in, &bar) == -1);        /* position 1 -> 0: a beat back */
    CHECK(((lk_beat_at(&node, bar.beat_us) / LK_UBEAT) % 4 + 4) % 4 == 0);
    bar.seq = 7;
    bar.phi = 0;
    bar.locked = 0;                                               /* listening / manual / off: no move */
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10500000, &in, &bar) == 0);
    bar.locked = 1;
    in.passive = 1;                                               /* LINK mode: the session's bar is not ours */
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10600000, &in, &bar) == 0);
    in.passive = 0;
    jbl.err_us = 9000;                                            /* the phase out of tolerance: wait */
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10700000, &in, &bar) == 0);
    jbl.err_us = 0;
    CHECK(jbl_bar_step(&jbl, &node, &st, 0, 10800000, &in, &bar) == 0);    /* not settled */
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10900000, &in, &bar) != 0);    /* the waiting decision, now */
}

int main(void)
{
    test_session_offset();
    test_bar_shift();
    test_bar_moves();
    test_phase_forced_past_5ms();
    test_tempo_deadband();
    test_transport();
    test_passive();
    test_start_rules();
    test_not_joined();
    test_drifting_tracked_grid();
    printf("beatlink: %d checks, %d failed\n", checks, fails);
    return fails;
}
