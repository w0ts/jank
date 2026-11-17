/* SPDX-License-Identifier: GPL-3.0-only */
/* SKIP (jb_skip.h): robust.py RobustBpm's names and order. Python's round() rounds halves to even, as rint() does; its `%` is the
 * floor modulo (pymod); the study's float32 arrays are float here. */
#include "jb_skip.h"
#include <math.h>
#include <string.h>
#include "jb_robust.h"

#define FPS JW_FPS
#define OUTLIER_NONE 0

void jsk_init(struct jb_skip *s, const struct jb_robust *r)
{
    memset(s, 0, sizeof *s);
    s->r = r;
    jsk_reset(s);
}

void jsk_set_mode(struct jb_skip *s, enum jsk_mode mode)
{
    s->mode = (uint8_t)mode;
    s->c = (struct jsk_cfg){.suspect = 1, .reopen = 1, .m = 2, .confirm = 3, .suspect_ticks = 2, .tpl_bars = 2, .pattern = 0,
                            .win = 0.08, .ratio = 0.5, .floor = 0.2, .phase_tol = 0.15, .tempo_tol = 0.06, .cool = 3.0,
                            .lag_s = 0.03, .every_s = 0.05, .decay = 0.7, .min_corr = 0.3};
    if (mode == JSK_ACT) {              /* all-act2-c1 */
        s->c.confirm = 1;
        s->c.suspect_ticks = 1;
    } else if (mode == JSK_PAT) {       /* a2-pat */
        s->c.pattern = 1;
    }
}

void jsk_reset(struct jb_skip *s)
{
    s->n_tracked = 0;
    s->off_run = 0;
    s->beat_suspect = 0;
    s->act_run = 0;
    s->reopen = -1;
    s->act_next = 0.0;
    s->act_last = -1e9;
    s->has_tpl = s->has_tpl_next = 0;
    s->tpl_n = 0;
    s->resyncs = s->act_resyncs = 0;
}

int jsk_on(const struct jb_skip *s) { return s->mode != JSK_OFF; }

int jsk_suspect(const struct jb_skip *s)
{
    return jsk_on(s) && (s->beat_suspect || s->act_run >= s->c.suspect_ticks);
}

/* Python's x % m for m > 0 */
static double pymod(double x, double m)
{
    double r = fmod(x, m);
    if (r != 0.0 && r < 0.0)
        r += m;
    return r;
}

static int imod(int a, int m) { return ((a % m) + m) % m; }

static int steps_dist(int a, int b)
{
    int d1 = imod(a - b, JSK_STEPS), d2 = imod(b - a, JSK_STEPS);
    return d1 < d2 ? d1 : d2;
}

/* ---- the beat path ---- */

static int last_outcome(const struct jb_follow *f)
{
    return f->out[(f->out_head + f->out_n - 1u) % JF_TRUST_BEATS];
}

static void clear_outcomes(struct jb_follow *f) { f->out_n = f->out_head = 0; }

/* the Kalman phase moved onto b with the variance var (robust.py's x[0] = b; c row 0 and column 0 cleared; c[0][0] = var) */
static void move_phase(struct jb_follow *f, double b, double var)
{
    f->x[0] = b;
    for (int i = 0; i < 3; i++)
        f->c[0][i] = f->c[i][0] = 0.0;
    f->c[0][0] = var;
}

/* RobustBpm._phase_resync */
static int phase_resync(struct jb_skip *s, struct jb_follow *f, double t)
{
    if (!f->has_kf || s->n_tracked < 2)
        return 0;
    double b1 = s->tracked[0], b2 = s->tracked[1], kp = f->x[1];
    if (fabs((b2 - b1) / kp - 1.0) > s->c.tempo_tol)
        return 0;
    double e1 = pymod(b1 - f->x[0] + kp / 2, kp) - kp / 2, e2 = pymod(b2 - f->x[0] + kp / 2, kp) - kp / 2;
    if (fmin(fabs(e1), fabs(e2)) <= s->c.phase_tol * kp || fabs(e1 - e2) > 0.1 * kp)
        return 0;
    move_phase(f, b2, (6e-3) * (6e-3));
    s->resyncs++;
    s->resyncs_total++;
    f->m.misses = 0;
    f->m.in_a_row = f->m.in_a_row > 1 ? f->m.in_a_row : 1;
    f->m.last_fit_time = t;
    f->m.has_last_fit = 1;
    clear_outcomes(f);
    s->reopen = 0;
    s->off_run = 0;
    s->beat_suspect = 0;
    return 1;
}

int jsk_update(struct jb_skip *s, struct jb_weak *w, struct jb_follow *f, double t)
{
    if (!jsk_on(s))
        return jw_update(w, f, t);
    if (s->n_tracked == 2) {
        s->tracked[0] = s->tracked[1];
        s->tracked[1] = t;
    } else
        s->tracked[s->n_tracked++] = t;
    if (!f->has_kf) {
        int strong = jw_update(w, f, t);
        s->off_run = 0;
        s->beat_suspect = 0;
        return strong;
    }
    double kt = f->x[0], kp = f->x[1], n = rint((t - kt) / kp);
    int off = n >= 1.0 && fabs(t - (kt + n * kp)) > s->c.phase_tol * kp;
    int jumps = f->m.jumps;
    int strong = jw_update(w, f, t);
    if (f->m.jumps != jumps || !f->has_kf) {
        s->off_run = 0;
        s->beat_suspect = 0;
        return strong;
    }
    if (s->reopen >= 0 && f->out_n > 0)
        s->reopen = last_outcome(f) ? s->reopen + 1 : -1;
    s->off_run = off ? s->off_run + 1 : 0;
    s->beat_suspect = s->off_run >= s->c.suspect;
    if (s->off_run >= 1)
        phase_resync(s, f, t);            /* (the skip jump is not ported: skip.json's jump is false) */
    return strong;
}

/* ---- the activation path ---- */

static int64_t act_oldest(const struct jb_skip *s) { return s->r->act_n - JRB_ACT_RING; }

/* the activation's peak over frames [f - w, f + w], the newest frame in being the last (float, as numpy's float32 max) */
static int peak(const struct jb_skip *s, int64_t f, int w, float *out)
{
    int64_t lo = f - w, hi = f + w;
    if (lo < 0)
        lo = 0;
    if (hi > s->r->act_n - 1)
        hi = s->r->act_n - 1;
    if (lo < act_oldest(s) || hi < lo)
        return 0;
    float m = s->r->st->ring[(uint64_t)lo % JRB_ACT_RING];
    for (int64_t k = lo + 1; k <= hi; k++) {
        float v = s->r->st->ring[(uint64_t)k % JRB_ACT_RING];
        m = v > m ? v : m;
    }
    *out = m;
    return 1;
}

/* RobustBpm._tpl_update: the last bars folded onto 4 beats x steps phases (decay), kept while the grid is trusted */
static void tpl_update(struct jb_skip *s, const struct jb_follow *f, double now, int w)
{
    double p = f->x[1], kt = f->x[0];
    if (s->has_tpl && fabs(p / s->tpl_p - 1.0) > 0.03) {
        s->has_tpl = 0;
        s->tpl_n = 0;
        s->has_tpl_next = 0;
    }
    if (jsk_suspect(s) || s->act_run > 0)
        return;
    if (!s->has_tpl_next || s->tpl_next < kt - 8 * p) {
        s->tpl_next = kt + p;
        s->has_tpl_next = 1;
    }
    while (s->tpl_next + 4 * p + s->c.lag_s <= now) {
        double line = s->tpl_next;
        s->tpl_next = line + 4 * p;
        if (line > kt + p * 400)
            break;
        float v[JSK_BAR * JSK_STEPS];
        int ok = 1;
        for (int i = 0; i < JSK_BAR * JSK_STEPS && ok; i++) {
            int64_t fr = (int64_t)rint((line + ((double)i * p) / JSK_STEPS) * FPS);
            ok = peak(s, fr, w, &v[i]) && fr + w < s->r->act_n;
        }
        if (!ok)
            break;
        if (!s->has_tpl) {
            memcpy(s->tpl, v, sizeof v);
        } else {
            float d = (float)s->c.decay, e = (float)(1.0 - s->c.decay);
            for (int i = 0; i < JSK_BAR * JSK_STEPS; i++) {
                float a = d * s->tpl[i], b = e * v[i];
                s->tpl[i] = a + b;
            }
        }
        s->has_tpl = 1;
        s->tpl_p = p;
        s->tpl_n++;
    }
}

/* RobustBpm._pattern_scores: the correlation of vals (newest first, one sample a phase step) with the bar pattern, for each of the
 * 4 x steps positions of the newest sample in the bar */
static void pattern_scores(const struct jb_skip *s, const float *vals, int n_v, float *ps)
{
    const int n = JSK_BAR * JSK_STEPS;
    float xm = 0.0f, xx = 0.0f, x[JSK_MAX_M * JSK_STEPS];
    for (int j = 0; j < n_v; j++)
        xm += vals[j];
    xm /= (float)n_v;
    for (int j = 0; j < n_v; j++) {
        x[j] = vals[j] - xm;
        xx += x[j] * x[j];
    }
    for (int i = 0; i < n; i++) {
        float ym = 0.0f, yy = 0.0f, dot = 0.0f, y[JSK_MAX_M * JSK_STEPS];
        for (int j = 0; j < n_v; j++) {
            y[j] = s->tpl[imod(i - j, n)];
            ym += y[j];
        }
        ym /= (float)n_v;
        for (int j = 0; j < n_v; j++) {
            y[j] -= ym;
            yy += y[j] * y[j];
            dot += y[j] * x[j];
        }
        float den = sqrtf(xx * yy) + 1e-12f;
        ps[i] = dot / den;
    }
}

/* RobustBpm._act_resync */
static void act_resync(struct jb_skip *s, struct jb_follow *f, double now)
{
    const struct jsk_cfg *a = &s->c;
    if (now < s->act_next)
        return;
    s->act_next = now + a->every_s;
    const double p = f->x[1];
    const int m = a->m, steps = JSK_STEPS;
    int w = (int)rint(a->win * p * FPS);
    w = w > 2 ? w : 2;
    if (a->pattern)
        tpl_update(s, f, now, w);
    if (now - s->act_last < a->cool * p)
        return;
    const double t_end = now - a->lag_s;
    const int64_t end_f = (int64_t)(t_end * FPS);
    if (end_f - (int64_t)((m + 1) * p * FPS) - w < 0)
        return;
    float vals[JSK_MAX_M * JSK_STEPS], scores[JSK_STEPS];
    for (int i = 0; i < m * steps; i++) {
        int64_t fr = (int64_t)rint((t_end - ((double)i * p) / steps) * FPS);
        if (!peak(s, fr, w, &vals[i]))
            return;
    }
    for (int k = 0; k < steps; k++) {
        float sum = vals[k];
        for (int j = 1; j < m; j++)
            sum += vals[k + j * steps];
        scores[k] = sum / (float)m;
    }
    const int cur_i = (int)rint(pymod(t_end - f->x[0], p) / (p / steps)) % steps;
    const double tol = a->phase_tol;
    int best;
    float cur, top;
    int ok_ev, lowcur;
    if (a->pattern && s->has_tpl && s->tpl_n >= a->tpl_bars) {
        float ps[JSK_BAR * JSK_STEPS], pb[JSK_STEPS];
        pattern_scores(s, vals, m * steps, ps);
        for (int i = 0; i < steps; i++) {
            pb[i] = ps[i];
            for (int b = 1; b < JSK_BAR; b++)
                pb[i] = ps[b * steps + i] > pb[i] ? ps[b * steps + i] : pb[i];
        }
        best = 0;
        for (int i = 1; i < steps; i++)
            if (pb[i] > pb[best])
                best = i;
        cur = pb[imod(cur_i - 1, steps)];
        for (int d = 0; d <= 1; d++)
            cur = pb[imod(cur_i + d, steps)] > cur ? pb[imod(cur_i + d, steps)] : cur;
        top = pb[best];
        ok_ev = top >= (float)a->min_corr && scores[best] >= (float)a->floor;
    } else {
        best = 0;
        for (int i = 1; i < steps; i++)
            if (scores[i] > scores[best])
                best = i;
        cur = scores[imod(cur_i - 1, steps)];
        for (int d = 0; d <= 1; d++)
            cur = scores[imod(cur_i + d, steps)] > cur ? scores[imod(cur_i + d, steps)] : cur;
        top = scores[best];
        ok_ev = top >= (float)a->floor;
    }
    lowcur = cur < (float)a->ratio * top;
    const double off = (double)steps_dist(best, cur_i) / steps;
    const int bad = ok_ev && lowcur && off > tol;       /* (`clear` is not used by ACT / PAT) */
    s->act_run = bad ? s->act_run + 1 : 0;
    if (s->act_run < a->confirm)
        return;
    move_phase(f, t_end - ((double)best * p) / steps, (8e-3) * (8e-3));
    s->resyncs++;
    s->resyncs_total++;
    s->act_resyncs++;
    s->act_resyncs_total++;
    s->act_last = now;
    s->act_run = 0;
    f->m.misses = 0;
    f->m.last_fit_time = now;
    f->m.has_last_fit = 1;
    clear_outcomes(f);
    s->reopen = 0;
    s->off_run = 0;
    s->beat_suspect = 0;
}

void jsk_coast(struct jb_skip *s, struct jb_weak *w, struct jb_follow *f, double now)
{
    jw_coast(w, f, now);
    if (jsk_on(s) && f->has_kf && jb_follow_at(f, now) == JB_STEADY)
        act_resync(s, f, now);
    else
        s->act_run = 0;
}

void jsk_gate(struct jb_skip *s, int has_bpm, double g, double threshold, uint8_t *gated)
{
    if (!jsk_on(s))
        return;
    if (s->reopen >= s->c.reopen && has_bpm) {
        *gated = 0;
        if (g >= threshold)
            s->reopen = -1;
    }
    if (jsk_suspect(s))
        *gated = 1;
    s->gated_ticks += *gated;
}
