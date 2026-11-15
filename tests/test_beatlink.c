/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the beat tracker as Link master (core/jank_beatlink.c). Exit status: the number of failed checks. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "jank_beatlink.h"
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

/* AUDIO mode's Stop with FOLLOW+LEAD (jank_hold.h): the session stops at the press, whoever started it, and the tracker
 * does not start it while held; Start: a start on the next bar line once the tracker runs */
static void test_held(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    poll_start(10100000, 14000000, 1);
    CHECK(lk_playing(&node) && jbl.playing && jbl.starts == 1);
    in.held = 1;                                     /* Stop: at once */
    jbl_step(&jbl, &node, &st, 1, 15000000, &in, 4);
    CHECK(!lk_playing(&node) && !jbl.playing && jbl.stops == 1 && jbl.held);
    jbl_step(&jbl, &node, &st, 1, 16000000, &in, 4);  /* still held, the tracker locked: no start */
    CHECK(!st.armed && poll_start(16000000, 20000000, 1) == 0 && !lk_playing(&node) && jbl.starts == 1);
    lk_set_playing(&node, 1, lk_beat_at(&node, 20000000), 20000000);   /* a peer starts the session while held: left alone */
    jbl_step(&jbl, &node, &st, 1, 20100000, &in, 4);
    CHECK(lk_playing(&node) && jbl.stops == 1);
    lk_set_playing(&node, 0, lk_beat_at(&node, 20200000), 20200000);
    in.passive = 1;                                  /* LINK mode (no sound) and back, still held: not stopped again */
    jbl_step(&jbl, &node, &st, 1, 20300000, &in, 4);
    in.passive = 0;
    jbl_step(&jbl, &node, &st, 1, 20400000, &in, 4);
    CHECK(jbl.stops == 1 && jbl.held);
    in.held = 0;                                     /* Start, the tracker not locked: nothing yet */
    in.running = 0;
    jbl_step(&jbl, &node, &st, 1, 21000000, &in, 4);
    CHECK(!st.armed && !jbl.held);
    in.running = 1;                                  /* locked: a start on the next bar line */
    jbl_step(&jbl, &node, &st, 1, 21500000, &in, 4);
    CHECK(st.armed && st.ub % (4 * (int64_t)LK_UBEAT) == 0 && jbl.starts == 2);
    CHECK(poll_start(21500000, 26000000, 1) != 0 && lk_playing(&node));
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

static int bar_pos(int64_t beat_us) { return (int)((((lk_beat_at(&node, beat_us) + LK_UBEAT / 4) / LK_UBEAT) % 4 + 4) % 4); }

/* M2h, the user: "beat is more phase. So zero is auto (again two layers)". PHASE (NUDGE BEAT's total) is an offset on top
 * of AUTO BAR's bar: AUTO BAR keeps placing the music's 1 (no MANUAL), the offset rides on it, neither changes the other;
 * back to 0, AUTO BAR's bar alone (no half-beat offset left, whatever folds the nudges were made at) */
static void test_phase_over_autobar(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    struct jbl_bar bar = {.locked = 1, .seq = 5, .count = 7, .phi = 2};
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    bar.beat_us = 10000000 + 4 * 500000;
    jbl_bar_step(&jbl, &node, &st, 1, 10200000, &in, &bar);
    CHECK(bar_pos(bar.beat_us) == (7 - 2) % 4);                   /* AUTO BAR: the counted beat at its place */
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 2, 0, 10300000) == 0);   /* PHASE +1 beat */
    CHECK(bar_pos(bar.beat_us) == (7 - 2 + 1) % 4);
    bar.seq = 6;                                                  /* the next decision, the same 1: no move back */
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10400000, &in, &bar) == 0 && bar_pos(bar.beat_us) == (7 - 2 + 1) % 4);
    bar.seq = 7;
    bar.phi = 3;                                                  /* the music's 1 moves: AUTO BAR follows, the phase on top */
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10500000, &in, &bar) == -1 && bar_pos(bar.beat_us) == (7 - 3 + 1) % 4);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -1, 0, 10600000) == 0);  /* +1/2 */
    bar.seq = 8;
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10700000, &in, &bar) == 0);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -2, 0, 10800000) == 0 && jbl.nudge == -1);   /* -1/2 */
    bar.seq = 9;
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10900000, &in, &bar) == 0);
    CHECK(bar_pos(bar.beat_us) == (7 - 3 + 3) % 4);               /* half a beat before the 1: counted as the beat before */
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 1, 11000000) == 0);   /* back to 0, the last half at fold x2 */
    CHECK(jbl.nudge == 0 && jbl.nudge_8ths == 0 && jbl_nudge_us(&jbl, 500000, 0) == 0);   /* AUTO: no offset left */
    bar.seq = 10;
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 11100000, &in, &bar) == 0 && bar_pos(bar.beat_us) == (7 - 3) % 4);
}

/* AUTO BAR AT START: the 1 is placed while the start is armed (it lands on it), never while the session plays; once at a
 * new start for the bar (bar_now); START WAIT holds the tracker's start; ALWAYS moves mid-play (test_bar_moves) */
static void test_bar_at_start(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000, .bar_mode = JBL_BAR_START};
    struct jbl_bar bar = {.locked = 1, .seq = 1, .count = 7, .phi = 2, .beat_us = 10000000 + 4 * 500000};
    int64_t ub;
    start(10000000);
    in.bar_wait = 1;                                             /* the estimator listening: the start waits */
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    CHECK(!st.armed && jbl.starts == 0 && !jbl.playing);
    in.bar_wait = 0;                                             /* found (or the wait is over): armed */
    jbl_step(&jbl, &node, &st, 1, 10150000, &in, 4);
    CHECK(st.armed && jbl.starts == 1);
    ub = lk_beat_at(&node, bar.beat_us);
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 10200000, &in, &bar) == jbl_bar_shift(ub, 7, 2));   /* armed: placed */
    CHECK(st.armed && st.ub % (4 * (int64_t)LK_UBEAT) == 0);     /* the start re-targeted onto the moved bar line */
    CHECK(poll_start(10200000, 16000000, 1) != 0 && lk_playing(&node));
    CHECK(((lk_beat_at(&node, bar.beat_us) / LK_UBEAT) % 4 + 4) % 4 == (7 - 2) % 4);
    bar.seq = 2;                                                 /* a later decision while playing: no move */
    bar.phi = 3;
    ub = lk_beat_at(&node, bar.beat_us);
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 17000000, &in, &bar) == 0 && lk_beat_at(&node, bar.beat_us) == ub);
    bar.seq = 3;
    in.bar_now = 1;                                              /* a new start for the bar (a fold change, a resync) */
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 17100000, &in, &bar) == jbl_bar_shift(ub, 7, 3));
    in.bar_now = 0;
    in.bar_mode = JBL_BAR_OFF;                                   /* OFF: nothing */
    bar.seq = 4;
    bar.phi = 1;
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 17200000, &in, &bar) == 0);
    in.bar_mode = JBL_BAR_START;                                 /* stopped: the next decision places it (the next start) */
    lk_set_playing(&node, 0, lk_beat_at(&node, 17300000), 17300000);
    bar.seq = 5;
    ub = lk_beat_at(&node, bar.beat_us);
    CHECK(jbl_bar_step(&jbl, &node, &st, 1, 17400000, &in, &bar) == jbl_bar_shift(ub, 7, 1));
}

/* the follower's input with the user's nudge kept, as beat.c's fold_step makes it (fold 1) */
static struct jbl_in nudged(struct jbl_in in)
{
    in.beat_us -= jbl_nudge_us(&jbl, in.period_us, 0);
    return in;
}

/* NUDGE BEAT: Link's beats jump half a beat now, and the follower keeps them there; halves add up to +-3 beats */
static void test_nudge_half(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    struct jbl_in n;
    int64_t t = 12000000, b0;
    uint32_t forces;
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    b0 = lk_beat_at(&node, t);                                    /* a tracked beat, on a session beat */
    CHECK(abs64(grid_error(t)) <= 2);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 0, 11000000) == 0);    /* UP: half a beat earlier */
    CHECK(jbl.nudge == 1 && jbl.nudges == 1 && jbl.nudge_8ths == 4);
    CHECK(llabs(lk_beat_at(&node, t) - b0 - LK_UBEAT / 2) <= 2);  /* the tracked beat is now the session's "and" */
    CHECK(jbl_nudge_us(&jbl, 500000, 0) == 250000);
    forces = jbl.phase_forces;
    n = nudged(in);
    for (int64_t now = 11100000; now < 20000000; now += 250000)  /* the follower keeps it: no force, in phase */
        jbl_step(&jbl, &node, &st, 1, now, &n, 4);
    CHECK(jbl.phase_forces == forces && abs64(jbl.err_us) <= 2);
    CHECK(llabs(lk_beat_at(&node, t) - b0 - LK_UBEAT / 2) <= 2);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 0, 20000000) == 0);    /* two halves: the bar's 1 a beat on, no offset */
    CHECK(jbl.nudge == 2 && jbl.nudge_8ths == 8 && jbl_nudge_us(&jbl, 500000, 0) == 0);
    CHECK(llabs(lk_beat_at(&node, t) - b0 - LK_UBEAT) <= 2);
    n = nudged(in);
    jbl_step(&jbl, &node, &st, 1, 20100000, &n, 4);
    CHECK(jbl.phase_forces == forces && abs64(jbl.err_us) <= 2);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -1, 0, 20200000) == 0);   /* DOWN: half a beat later, the same offset */
    CHECK(jbl.nudge == 1 && jbl.nudge_8ths == 4 && llabs(lk_beat_at(&node, t) - b0 - LK_UBEAT / 2) <= 2);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -2, 0, 20300000) == 0 && jbl.nudge == -1 && jbl_nudge_us(&jbl, 500000, 0) == 250000);  /* whole */
    CHECK(llabs(lk_beat_at(&node, t) - b0 + LK_UBEAT / 2) <= 2);
    CHECK(lk_mpb(&node) == 500000);                               /* the tempo untouched */
}

/* +-3 beats in all: past it a nudge is refused whole; a clear forgets them (the session left where it is) */
static void test_nudge_clamp(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    int64_t ub;
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    for (int i = 0; i < 6; i++)
        CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 0, 10200000 + i * 1000) == 0);
    CHECK(jbl.nudge == JBL_NUDGE_MAX);
    ub = lk_beat_at(&node, 10300000);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 0, 10300000) == -1);
    CHECK(jbl.nudge == 6 && jbl.nudges == 6 && lk_beat_at(&node, 10300000) == ub);     /* nothing moved */
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -12, 0, 10300000) == 0 && jbl.nudge == -6);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -1, 0, 10300000) == -1);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 0, 0, 10300000) == -1);
    ub = lk_beat_at(&node, 10400000);
    jbl_nudge_clear(&jbl);
    CHECK(jbl.nudge == 0 && jbl.nudge_8ths == 0 && lk_beat_at(&node, 10400000) == ub);
}

/* a nudge while a start is armed: it re-targets the moved grid's next bar line; while playing, the session's playing
 * state is kept (its bar B is a bar line of the moved grid too) */
static void test_nudge_armed(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    int64_t bar = 4 * (int64_t)LK_UBEAT, sent;
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    CHECK(st.armed);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 0, 10200000) == 0);
    CHECK(st.armed && st.ub % bar == 0 && st.ub - lk_beat_at(&node, 10200000) >= LK_UBEAT * 3 / 10);
    CHECK(st.ub - lk_beat_at(&node, 10200000) <= bar + LK_UBEAT * 3 / 10);
    sent = poll_start(10200000, 16000000, 1);
    CHECK(sent && lk_playing(&node) && lk_play_beat(&node) % bar == 0);
    /* the start lands on the nudged grid: its bar line is half a tracked beat before a tracked beat */
    {
        int64_t t_bar = lk_time_at(&node, lk_play_beat(&node));
        int64_t r = ((t_bar - 10000000 + 250000) % 500000 + 500000) % 500000;
        CHECK(r <= 2 || r >= 499998);
    }
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -1, 0, sent + 1000000) == 0 && lk_playing(&node));
}

/* LINK mode: refused; not settled: no force, the offset kept for the follower */
static void test_nudge_passive_unsettled(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    struct jbl_in n;
    int64_t ub;
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    in.passive = 1;
    ub = lk_beat_at(&node, 10200000);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 0, 10200000) == -1 && jbl.nudge == 0 && lk_beat_at(&node, 10200000) == ub);
    in.passive = 0;
    CHECK(jbl_nudge(&jbl, &node, &st, 0, &in, 1, 0, 10200000) == 0 && jbl.nudge == 1);
    CHECK(lk_beat_at(&node, 10200000) == ub);                     /* no force without the session's state */
    n = nudged(in);
    jbl_step(&jbl, &node, &st, 1, 10300000, &n, 4);               /* the follower puts the session there */
    CHECK(abs64(grid_error(10000000 - 250000)) <= 2);
}

/* folded: half a beat of Link's; the offset in tracked time, so it means the same music at another fold */
static void test_nudge_fold(void)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = 500000};
    CHECK(jbl_half_8ths(-2) == 16 && jbl_half_8ths(-1) == 8 && jbl_half_8ths(0) == 4 && jbl_half_8ths(1) == 2 && jbl_half_8ths(2) == 1);
    start(10000000);
    jbl_step(&jbl, &node, &st, 1, 10100000, &in, 4);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 0, 10200000) == 0);    /* an off-beat lock fixed at fold 1 */
    CHECK(jbl_nudge_us(&jbl, 500000, 0) == 250000);
    CHECK(jbl_nudge_us(&jbl, 500000, 1) == 0);                    /* x2: a whole beat of the folded grid */
    CHECK(jbl_nudge_us(&jbl, 500000, -1) == 250000);              /* /2: still the on-beats */
    jbl_nudge_clear(&jbl);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, 1, 1, 10300000) == 0);    /* at x2: a quarter of a tracked beat */
    CHECK(jbl.nudge == 1 && jbl_nudge_us(&jbl, 500000, 1) == 125000 && jbl_nudge_us(&jbl, 500000, 0) == 125000);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -1, 1, 10400000) == 0 && jbl_nudge_us(&jbl, 500000, 1) == 0);
    CHECK(jbl_nudge(&jbl, &node, &st, 1, &in, -1, -2, 10500000) == 0);  /* at /4: two tracked beats */
    CHECK(jbl_nudge_us(&jbl, 500000, -2) == 1000000 && jbl_nudge_us(&jbl, 500000, 0) == 0);
}

/* the MIDI clock from the session around a nudge (bridge.c publishes a map every few ms, midi.c ticks on it): one clock
 * per grid time, never two close together, never one missing; the clock's beat (tick 24 n) moves with the session */
static uint32_t clock_around_nudge(int halves, int32_t mpb, int64_t *min_gap, uint32_t *resyncs, int64_t *tick0_err)
{
    struct jbl_in in = {.running = 1, .beat_us = 10000000, .period_us = (double)mpb};
    struct jc_map m;
    struct jc_ticker c;
    struct jc_fire f;
    int64_t t, last = -1, gap = INT64_MAX, next_map = 0, t_nudge = 13000000 + 7 * (mpb / 24) + 333, t_end = 17000000;
    uint32_t clocks = 0;
    int nudged_yet = 0;
    start(10000000);
    lk_set_tempo(&node, mpb, 10000000);
    jbl_step(&jbl, &node, &st, 1, 10050000, &in, 4);
    jc_map_make(&m, 12000000, lk_beat_at(&node, 12000000), lk_mpb(&node), 0);
    jc_ticker_init(&c, 4);
    if (jc_begin(&c, &m, 12000000) != 0)
        return 0;
    t = c.t_due;
    *tick0_err = 0;
    while (t < t_end) {
        if (!nudged_yet && t >= t_nudge) {
            jbl_nudge(&jbl, &node, &st, 1, &in, halves, 0, t_nudge);
            nudged_yet = 1;
        }
        if (t >= next_map) {                                      /* every 5 ms, as bridge.c's PUBLISH_US */
            jc_map_make(&m, t, lk_beat_at(&node, t), lk_mpb(&node), 0);
            next_map = t + 5000;
        }
        if (t >= 13500000) {                                      /* after it: the ticks are the session's */
            int64_t e = llabs(lk_beat_at(&node, t) - jc_tick_ub(c.n)) * mpb / LK_UBEAT;
            if (e > *tick0_err)
                *tick0_err = e;
        }
        jc_step(&c, &m, t, &f);
        for (unsigned i = 0; i < f.n; i++)
            if (f.bytes[i] == JC_CLOCK) {
                clocks++;
                if (last >= 0 && t - last < gap)
                    gap = t - last;
                last = t;
            }
        t = f.next;
    }
    *min_gap = gap;
    *resyncs = c.resyncs;
    return clocks;
}

static void test_nudge_clock(void)
{
    static const int halves[] = {1, -1, 2, -2, 3, -6};
    static const int32_t mpbs[] = {500000, 468750, 461538};     /* 120, 128, 130 BPM */
    for (unsigned j = 0; j < sizeof mpbs / sizeof mpbs[0]; j++)
        for (unsigned i = 0; i < sizeof halves / sizeof halves[0]; i++) {
            int64_t gap, err, tick = mpbs[j] / 24;
            uint32_t resyncs, n = clock_around_nudge(halves[i], mpbs[j], &gap, &resyncs, &err);
            int64_t grid = (int64_t)llround(5000000.0 / ((double)mpbs[j] / 24.0));   /* the grid times in 5 s */
            CHECK(n >= grid - 1 && n <= grid + 1);                                  /* (the window's ends) */
            CHECK(gap >= tick - 2);                                                 /* never two in a row, none skipped */
            CHECK(resyncs == 1);                                                    /* one jump, at the nudge */
            CHECK(err <= 2);                                                        /* tick 24 n on session beat n */
            printf("beatlink: nudge %+d/2 at %.2f BPM: %u clocks (grid %lld), min gap %lld us of %lld, %u resync\n", halves[i],
                   60e6 / mpbs[j], n, (long long)grid, (long long)gap, (long long)tick, resyncs);
        }
}

int main(void)
{
    test_nudge_half();
    test_nudge_clamp();
    test_nudge_armed();
    test_nudge_passive_unsettled();
    test_nudge_fold();
    test_nudge_clock();
    test_session_offset();
    test_bar_shift();
    test_bar_moves();
    test_phase_over_autobar();
    test_phase_forced_past_5ms();
    test_tempo_deadband();
    test_transport();
    test_passive();
    test_held();
    test_bar_at_start();
    test_start_rules();
    test_not_joined();
    test_drifting_tracked_grid();
    printf("beatlink: %d checks, %d failed\n", checks, fails);
    return fails;
}
