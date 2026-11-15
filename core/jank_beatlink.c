/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat tracker as the Link session's master (jank_beatlink.h). */
#include "jank_beatlink.h"
#include <math.h>
#include <string.h>

void jbl_init(struct jbl *c) { memset(c, 0, sizeof *c); }

static double bpm_of(double mpb_us) { return 60000000.0 / mpb_us; }

static void stop_session(struct jbl *c, struct lk_node *k, struct jst *start, int64_t now)
{
    if (!c->playing)
        return;
    jst_cancel(start);
    if (lk_playing(k))
        lk_set_playing(k, 0, lk_beat_at(k, now), now);
    c->playing = 0;
    c->stops++;
}

/* the audio stream closed (LINK mode): the session is left as it is, playing or not; a start not sent yet is dropped */
static void let_go(struct jbl *c, struct jst *start)
{
    if (!c->playing)
        return;
    jst_cancel(start);
    c->playing = 0;
    c->releases++;
}

/* the user's Stop with FOLLOW+LEAD: the session stops now, whoever started it; a start not sent yet is dropped */
static void hold_session(struct jbl *c, struct lk_node *k, struct jst *start, int64_t now)
{
    jst_cancel(start);
    if (lk_playing(k)) {
        lk_set_playing(k, 0, lk_beat_at(k, now), now);
        c->stops++;
    }
    c->playing = 0;
}

/* the tempo, when it moved more than the deadband */
static void follow_tempo(struct jbl *c, struct lk_node *k, int64_t now, double period_us)
{
    int32_t mpb = (int32_t)llround(period_us);
    double diff = bpm_of(period_us) - bpm_of((double)lk_mpb(k));
    if (mpb < LK_MPB_MIN || mpb > LK_MPB_MAX)
        return;
    if (fabs(diff) * 1000.0 <= JBL_TEMPO_DEADBAND_X1000)
        return;
    lk_set_tempo(k, mpb, now);
    c->tempo_sets++;
}

static int64_t floor_div(int64_t a, int64_t b) { return a / b - (a % b < 0); }

/* the session's beat nearest to the tracked one, and how far (us) the session's grid is from it; forces the grid when
 * that is more than JBL_PHASE_US */
static void follow_phase(struct jbl *c, struct lk_node *k, int64_t now, const struct jbl_in *in)
{
    double period = in->period_us;
    int64_t n = (int64_t)llround((double)(now - in->beat_us) / period);
    int64_t t = in->beat_us + (int64_t)llround((double)n * period);   /* a tracked beat near now */
    int64_t ub = lk_beat_at(k, t);
    int64_t whole = floor_div(ub + LK_UBEAT / 2, LK_UBEAT) * LK_UBEAT;   /* the nearest session beat */
    double err = (double)(ub - whole) * (double)lk_mpb(k) / (double)LK_UBEAT;
    c->err_us = (int32_t)llround(err);
    if (fabs(err) <= (double)JBL_PHASE_US)
        return;
    lk_force_beat(k, lk_mpb(k), whole, t);
    c->phase_forces++;
}

int32_t jbl_session_offset_us(int64_t ub_at, int32_t mpb)
{
    int64_t whole = floor_div(ub_at + LK_UBEAT / 2, LK_UBEAT) * LK_UBEAT;
    return (int32_t)llround(-(double)(ub_at - whole) * (double)mpb / (double)LK_UBEAT);
}

int jbl_bar_shift(int64_t ub_at, int32_t count, int phi)
{
    int64_t whole = floor_div(ub_at + LK_UBEAT / 4, LK_UBEAT);
    int64_t n = ((int64_t)count - phi - whole) % 4;
    if (n < 0)
        n += 4;
    return n == 3 ? -1 : (int)n;
}

void jbl_shift_bar(struct jbl *c, struct lk_node *k, struct jst *start, int n, int64_t t)
{
    if (!n)
        return;
    lk_force_beat(k, lk_mpb(k), lk_beat_at(k, t) + (int64_t)n * LK_UBEAT, t);
    jst_retarget(start, k, t);
    c->bar_moves++;
}

int jbl_bar_step(struct jbl *c, struct lk_node *k, struct jst *start, int settled, int64_t now, const struct jbl_in *in,
                 const struct jbl_bar *bar)
{
    int n;
    if (!settled || in->passive || in->held || !in->running || !bar->locked || (c->bar_seen && bar->seq == c->bar_seq))
        return 0;
    if (in->bar_mode == JBL_BAR_OFF)
        return 0;
    if (in->bar_mode == JBL_BAR_START && !in->bar_now && lk_playing(k) && !start->armed && lk_play_beat(k) <= lk_beat_at(k, now))
        return 0;                       /* AT START: the session plays (its start reached): the 1 waits for the next start */
    if (c->err_us > JBL_PHASE_US || c->err_us < -JBL_PHASE_US)
        return 0;
    c->bar_seq = bar->seq;
    c->bar_seen = 1;
    n = jbl_bar_shift(lk_beat_at(k, bar->beat_us), bar->count, bar->phi);
    jbl_shift_bar(c, k, start, n, now);
    return n;
}

/* Link's start on the next bar line, sent JST_LEAD_US before it (jank_start.h); the session playing already: kept */
static void start_session(struct jbl *c, struct lk_node *k, struct jst *start, int64_t now, uint32_t quantum)
{
    c->playing = 1;
    if (lk_playing(k))
        return;
    jst_arm(start, k, now, quantum);
    c->starts++;
}

void jbl_step(struct jbl *c, struct lk_node *k, struct jst *start, int settled, int64_t now, const struct jbl_in *in, uint32_t quantum)
{
    if (!settled)
        return;
    if (in->passive) {                  /* (c->held kept: a hold that lasts through LINK mode does not stop the session again) */
        let_go(c, start);
        return;
    }
    if (in->held) {
        if (!c->held)
            hold_session(c, k, start, now);
        c->held = 1;
        return;
    }
    c->held = 0;
    if (!in->running || in->period_us <= 0.0) {
        stop_session(c, k, start, now);
        return;
    }
    follow_tempo(c, k, now, in->period_us);
    follow_phase(c, k, now, in);
    if (!c->playing && !in->bar_wait)
        start_session(c, k, start, now, quantum);
}
