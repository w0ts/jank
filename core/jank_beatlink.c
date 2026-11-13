/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat tracker as the Link session's master (jank_beatlink.h). */
#include "jank_beatlink.h"
#include <math.h>
#include <string.h>

void jbl_init(struct jbl *c) { memset(c, 0, sizeof *c); }

static double bpm_of(double mpb_us) { return 60000000.0 / mpb_us; }

static void stop_session(struct jbl *c, struct lk_node *k, int64_t now)
{
    if (!c->playing)
        return;
    lk_set_playing(k, 0, lk_beat_at(k, now), now);
    c->playing = 0;
    c->stops++;
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

/* Link's start on the next bar line at least JBL_START_LEAD_US ahead */
static void start_session(struct jbl *c, struct lk_node *k, int64_t now, uint32_t quantum)
{
    int64_t bar = (int64_t)(quantum ? quantum : 1u) * LK_UBEAT;
    int64_t ub = lk_beat_at(k, now + JBL_START_LEAD_US);
    int64_t start = (ub / bar + 1) * bar;
    lk_set_playing(k, 1, start, now);
    c->playing = 1;
    c->starts++;
}

void jbl_step(struct jbl *c, struct lk_node *k, int joined, int64_t now, const struct jbl_in *in, uint32_t quantum)
{
    if (!joined)
        return;
    if (!in->running || in->period_us <= 0.0) {
        stop_session(c, k, now);
        return;
    }
    follow_tempo(c, k, now, in->period_us);
    follow_phase(c, k, now, in);
    if (!c->playing)
        start_session(c, k, now, quantum);
}
