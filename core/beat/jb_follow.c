/* SPDX-License-Identifier: GPL-3.0-only */
/* The tempo follower (jb_follow.h), a line-by-line port of eval/follow_bpm.py: names, constants and the order of the
 * steps are the Python's, so that the two can be compared beat by beat. Python's round() rounds halves to even, as
 * rint() does. */
#include "jb_follow.h"
#include <math.h>
#include <string.h>

#define R_S 0.006               /* the tracker's beat jitter (s, one standard deviation) */
#define Q_PHASE_S 0.001         /* process noise a beat: phase (s) */
#define Q_RATE 0.00002          /* period random walk (relative) */
#define Q_DRIFT 0.00001         /* drift random walk (relative period change a beat) */
#define Q_DRIFT_SEED 0.0001
#define OPEN_PHASE 1.0          /* a manoeuvre opens the phase deviation to |innovation| / OPEN_PHASE */
#define OPEN_PERIOD 2.0         /* ... and the period's to |innovation| / OPEN_PERIOD */
#define MANOEUVRE_SIGMA 3.0
#define MANOEUVRE_RUN 3
#define OPEN_DRIFT 4.0          /* a second manoeuvre the same way within RAMP_BEATS opens the drift */
#define RAMP_BEATS 24
#define TEMPO_REL 0.04          /* the recent beats' own period this far off for CHANGE_BEATS beats: start over */
#define TRUST_SHARE 0.8
#define RATIO_SD 0.002          /* a measured ratio is good to about 0.2 % */
/* from LockedBpm / ModeBpm (jb_mode.c) */
#define OUTLIER 0.25
#define CHANGE_BEATS 8
#define MIN_PERIOD 0.2
#define OCTAVE_TOL 0.04264433740849372    /* log2(1.03): a fast jump's new tempo this near an octave is the fold's */
#define SINCE_NONE 1000000

enum { FIT, DOUBLE, MISS };

void jb_follow_init(struct jb_follow *f)
{
    memset(f, 0, sizeof *f);
    jb_mode_init(&f->m);
    f->last_ratio = 1.0;
}

/* Kalman.seed */
static void kf_seed(struct jb_follow *f, double t, double p, int n)
{
    n = n > 4 ? n : 4;
    memset(f->c, 0, sizeof f->c);
    f->x[0] = t;
    f->x[1] = p;
    f->x[2] = 0.0;
    f->c[0][0] = R_S * R_S;
    f->c[1][1] = 12.0 * (R_S * R_S) / ((double)n * n * n);
    f->c[2][2] = (Q_DRIFT_SEED * p) * (Q_DRIFT_SEED * p);
    f->run = f->manoeuvres = f->last_side = 0;
    f->since = SINCE_NONE;
    f->has_kf = 1;
}

/* Kalman.step: FIT, DOUBLE (a second detection of the beat already taken) or MISS (further off than outlier periods) */
static int kf_step(struct jb_follow *f, double obs, double outlier)
{
    double p = f->x[1], n = rint((obs - f->x[0]) / p);
    if (n < 1.0)
        return DOUBLE;
    double F[3][3] = {{1.0, n, n * (n + 1.0) / 2.0}, {0.0, 1.0, n}, {0.0, 0.0, 1.0}};
    double fc[3][3], c[3][3], xp[3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            fc[i][j] = F[i][0] * f->c[0][j] + F[i][1] * f->c[1][j] + F[i][2] * f->c[2][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            c[i][j] = fc[i][0] * F[j][0] + fc[i][1] * F[j][1] + fc[i][2] * F[j][2];
    c[0][0] += n * (Q_PHASE_S * Q_PHASE_S);
    c[1][1] += n * ((Q_RATE * p) * (Q_RATE * p));
    c[2][2] += n * ((Q_DRIFT * p) * (Q_DRIFT * p));
    for (int i = 0; i < 3; ++i)
        xp[i] = F[i][0] * f->x[0] + F[i][1] * f->x[1] + F[i][2] * f->x[2];
    double e = obs - xp[0];
    if (fabs(e) > outlier * p)
        return MISS;
    double s = c[0][0] + R_S * R_S, z = e / sqrt(s);
    if (fabs(z) > MANOEUVRE_SIGMA) {
        int side = z > 0 ? 1 : -1;
        f->run = f->run * side > 0 ? f->run + side : side;
    } else
        f->run = 0;
    f->since += (int)n;
    if (f->run >= MANOEUVRE_RUN || f->run <= -MANOEUVRE_RUN) {
        /* the tempo moved: open phase and period by what the innovation says; the drift only when the previous
         * manoeuvre went the same way less than RAMP_BEATS ago (a ramp, not a step) */
        int side = f->run > 0 ? 1 : -1;
        int ramp = side == f->last_side && f->since <= RAMP_BEATS;
        c[0][0] += (e / OPEN_PHASE) * (e / OPEN_PHASE);
        c[1][1] += (e / OPEN_PERIOD) * (e / OPEN_PERIOD);
        if (ramp)
            c[2][2] += (e / OPEN_DRIFT) * (e / OPEN_DRIFT);
        s = c[0][0] + R_S * R_S;
        f->manoeuvres++;
        f->manoeuvres_total++;
        f->last_side = side;
        f->since = 0;
        f->run = 0;
    }
    double k[3] = {c[0][0] / s, c[1][0] / s, c[2][0] / s};
    for (int i = 0; i < 3; ++i)
        f->x[i] = xp[i] + k[i] * e;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            f->c[i][j] = c[i][j] - k[i] * c[0][j];
    return FIT;
}

void jb_follow_outcome(struct jb_follow *f, uint8_t fitted)
{
    f->out[(f->out_head + f->out_n) % JF_TRUST_BEATS] = fitted;
    if (f->out_n < JF_TRUST_BEATS)
        f->out_n++;
    else
        f->out_head = (uint8_t)((f->out_head + 1u) % JF_TRUST_BEATS);
}

int jb_follow_trusted(const struct jb_follow *f)
{
    int sum = 0;
    for (int i = 0; i < f->out_n; ++i)
        sum += f->out[i];             /* (the ring's order does not matter for a sum) */
    return f->out_n >= JF_TRUST_BEATS / 2 && sum >= TRUST_SHARE * f->out_n;
}

/* FollowBpm._seed: the glide from ModeBpm's line, if there is one (else the glide stays as it was) */
static void seed(struct jb_follow *f)
{
    double t0, p;
    if (!jb_mode_line(&f->m, &t0, &p))
        return;
    kf_seed(f, t0, p, f->m.fit_n);
    f->out_n = f->out_head = 0;
    f->m.disagree = 0;
    f->seeds++;
}

/* FollowBpm._jump (sticky off): a new segment from the recent beats, and the glide seeded on it */
void jb_follow_jump(struct jb_follow *f, double t)
{
    f->m.jumps++;
    jb_mode_restart(&f->m);
    f->m.in_a_row = f->m.lock_beats;
    f->m.last_fit_time = t;
    f->m.has_last_fit = 1;
    seed(f);
}

/* FollowBpm.apply_ratio: the period divided by the ratio, the drift cleared, the next line moved as if the music had
 * run at the new rate from since; only on a trusted glide. 1 taken */
static int apply_ratio(struct jb_follow *f, double ratio, int has_since, double since)
{
    if (!f->has_kf || !jb_follow_trusted(f) || !(0.85 < ratio && ratio < 1.15))
        return 0;
    double t_last = f->x[0], p_old = f->x[1];
    f->x[1] = p_old / ratio;
    f->x[2] = 0.0;
    if (has_since && since > t_last) {
        double nxt = t_last + p_old;                      /* the next line at the old rate */
        if (nxt > since)
            f->x[0] = since + (nxt - since) / ratio - f->x[1];      /* re-timed from the step on */
    }
    f->c[1][1] += (RATIO_SD * f->x[1]) * (RATIO_SD * f->x[1]);
    return 1;
}

static double line_error(const double *x, double beat)
{
    double n = rint((beat - x[0]) / x[1]);
    return beat - (x[0] + (n > 1.0 ? n : 1.0) * x[1]);
}

/* FollowBpm._check_pending: the proposal judged by this beat; undone when the beat does not back it */
static void check_pending(struct jb_follow *f, double beat)
{
    f->has_pending = 0;
    if (!f->has_kf || beat <= f->pending_t - 1.0)
        return;
    double x_old[3], c_old[3][3];
    memcpy(x_old, f->x, sizeof x_old);
    memcpy(c_old, f->c, sizeof c_old);
    double e_old = line_error(x_old, beat);
    if (!apply_ratio(f, f->pending_ratio, f->pending_has_since, f->pending_since)) {
        f->ratios_ignored++;
        return;
    }
    if (fabs(line_error(f->x, beat)) >= fabs(e_old)) {
        memcpy(f->x, x_old, sizeof x_old);               /* the beat does not back it: undone */
        memcpy(f->c, c_old, sizeof c_old);
        f->ratios_refused++;
        return;
    }
    f->ratios_taken++;
    f->last_ratio = f->pending_ratio;
}

void jb_follow_propose(struct jb_follow *f, double ratio, double t, int has_since, double since)
{
    f->has_pending = 1;
    f->pending_ratio = ratio;
    f->pending_t = t;
    f->pending_has_since = (uint8_t)(has_since != 0);
    f->pending_since = since;
}

void jb_follow_update(struct jb_follow *f, double t)
{
    struct jb_mode *m = &f->m;
    if (f->has_kf && jb_mode_at(m, t) != JB_STEADY)
        f->has_kf = 0;                 /* stopped (no fitting beat for the hold): ModeBpm relocks */
    if (!f->has_kf) {
        jb_mode_update(m, t);
        if (m->mode == JB_STEADY)
            seed(f);
        return;
    }
    m->beats_seen++;
    jb_dq_push(&m->history, t);
    jb_dq_push(&m->recent, t);
    if (f->has_pending)
        check_pending(f, t);
    int res = kf_step(f, t, OUTLIER);
    if (res == DOUBLE)
        return;
    jb_follow_outcome(f, (uint8_t)(res == FIT));
    if (res == FIT) {
        m->misses = 0;
        m->last_fit_time = t;
        m->has_last_fit = 1;
        m->in_a_row++;
        f->follow_beats++;
    } else {
        m->misses++;
        m->in_a_row = 0;
        if (m->misses >= m->miss_limit && jb_mode_recent_consistent(m)) {
            jb_follow_jump(f, t);
            return;
        }
    }
    /* the beats' own tempo far from the glide's for CHANGE_BEATS beats (a lock on a 4:3 of the music, whose every
     * third beat fits, never misses enough to jump): start over from the recent beats */
    double sp = jb_short_period(m->recent.v, m->recent.n);
    if (sp < 0 || !f->has_kf)
        return;
    m->disagree = fabs(sp / f->x[1] - 1.0) > TEMPO_REL ? m->disagree + 1 : 0;
    if (m->disagree >= CHANGE_BEATS) {
        m->disagree = 0;
        jb_follow_jump(f, t);
    }
}

enum jb_mode_kind jb_follow_at(struct jb_follow *f, double now)
{
    return jb_mode_at(&f->m, now);
}

int jb_follow_bpm_at(struct jb_follow *f, double now, double *bpm)
{
    if (jb_mode_at(&f->m, now) == JB_STOPPED)
        return 0;
    double p = f->has_kf ? f->x[1] : jb_mode_period(&f->m);
    if (!(p > MIN_PERIOD))
        return 0;
    *bpm = 60.0 / p;
    return 1;
}

int jb_follow_grid(const struct jb_follow *f, double *t_beat, double *period)
{
    if (!f->has_kf)
        return jb_mode_grid(&f->m, t_beat, period);
    *t_beat = f->x[0];
    *period = f->x[1];
    return 1;
}

enum jb_follow_state jb_follow_state(struct jb_follow *f, double now)
{
    if (jb_mode_at(&f->m, now) == JB_STOPPED)
        return JF_STOPPED;
    return f->has_kf ? JF_GLIDE : JF_LOCK;
}

/* chain.fast_jump: the last LOCK_BEATS beats on one grid of their own, its tempo more than rel from the glide's and not an
 * octave of it (the fold's business): jump to it now instead of waiting for misses or CHANGE_BEATS disagreeing beats */
int jb_follow_fast_jump(struct jb_follow *f, double t, double rel)
{
    const struct jb_mode *m = &f->m;
    if (!(rel > 0.0) || !f->has_kf || m->lock_beats < 2 || m->recent.n < m->lock_beats)
        return 0;
    /* the tempo test before the grid's (the Python's order the other way: both pure, the same answer; the fit is the
     * dear one and the tempo is the glide's on nearly every beat) */
    double sp = jb_short_period(m->recent.v + (m->recent.n - m->lock_beats), m->lock_beats);
    if (sp < 0)
        return 0;
    double r = f->x[1] / sp, lr = log2(r), near = rint(lr);
    if (fabs(r - 1.0) <= rel || (near != 0.0 && fabs(lr - near) <= OCTAVE_TOL) || !jb_mode_recent_consistent(m))
        return 0;
    jb_follow_jump(f, t);
    f->fast_jumps++;
    return 1;
}

/* weak.weak_step: the prediction as kf_step, the measurement noise r_s, no manoeuvre; taken within gate periods */
int jb_follow_weak_obs(struct jb_follow *f, double obs, double r_s, double gate)
{
    if (!f->has_kf)
        return 0;
    double p = f->x[1], n = rint((obs - f->x[0]) / p);
    if (n < 1.0)
        return 0;
    double F[3][3] = {{1.0, n, n * (n + 1.0) / 2.0}, {0.0, 1.0, n}, {0.0, 0.0, 1.0}};
    double fc[3][3], c[3][3], xp[3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            fc[i][j] = F[i][0] * f->c[0][j] + F[i][1] * f->c[1][j] + F[i][2] * f->c[2][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            c[i][j] = fc[i][0] * F[j][0] + fc[i][1] * F[j][1] + fc[i][2] * F[j][2];
    c[0][0] += n * (Q_PHASE_S * Q_PHASE_S);
    c[1][1] += n * ((Q_RATE * p) * (Q_RATE * p));
    c[2][2] += n * ((Q_DRIFT * p) * (Q_DRIFT * p));
    for (int i = 0; i < 3; ++i)
        xp[i] = F[i][0] * f->x[0] + F[i][1] * f->x[1] + F[i][2] * f->x[2];
    double e = obs - xp[0];
    if (fabs(e) > gate * p)
        return 0;
    double s = c[0][0] + r_s * r_s;
    double k[3] = {c[0][0] / s, c[1][0] / s, c[2][0] / s};
    for (int i = 0; i < 3; ++i)
        f->x[i] = xp[i] + k[i] * e;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            f->c[i][j] = c[i][j] - k[i] * c[0][j];
    return 1;
}

int32_t jb_follow_drift_ppm(const struct jb_follow *f)
{
    if (!f->has_kf || !(f->x[1] > 0.0))
        return 0;
    double ppm = f->x[2] / f->x[1] * 1e6;
    return (int32_t)lrint(ppm > 1e9 ? 1e9 : ppm < -1e9 ? -1e9 : ppm);
}
