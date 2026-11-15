/* SPDX-License-Identifier: GPL-3.0-only */
/* AUDIO mode's fold and AUTO RANGE (jank_fold.h). */
#include "jank_fold.h"
#include <math.h>
#include <string.h>

void jfo_init(struct jfo *f) { memset(f, 0, sizeof *f); }

static void set_exp(struct jfo *f, int exp)
{
    exp = exp < JFO_EXP_MIN ? JFO_EXP_MIN : exp > JFO_EXP_MAX ? JFO_EXP_MAX : exp;
    if (exp != f->exp)
        f->anchored = 0;
    f->exp = (int8_t)exp;
}

int jfo_user(struct jfo *f, int dir)
{
    set_exp(f, dir ? f->exp + (dir > 0 ? 1 : -1) : 0);
    f->manual = 1;
    f->manuals++;
    return f->exp;
}

void jfo_reset(struct jfo *f)
{
    set_exp(f, 0);
    f->manual = 0;
    f->had_tempo = 0;
}

const char *jfo_name(int exp)
{
    static const char *const names[] = {"0.25", "0.5", "1", "2", "4"};
    return exp >= JFO_EXP_MIN && exp <= JFO_EXP_MAX ? names[exp - JFO_EXP_MIN] : "1";
}

int jfo_best(double bpm, double ref)
{
    int best = 0;
    double best_d = 1e9;
    if (!(bpm > 0.0) || !(ref > 0.0))
        return 0;
    for (int e = JFO_EXP_MIN; e <= JFO_EXP_MAX; e++) {
        double d = fabs(log2(bpm * pow(2.0, e) / ref));
        if (d < best_d - 1e-9 || (fabs(d - best_d) <= 1e-9 && e < best)) {
            best = e;
            best_d = d;
        }
    }
    return best;
}

int jfo_range(double bpm, double ref)
{
    int e = jfo_best(bpm, ref);
    double r = bpm * pow(2.0, e) / ref;
    return e != 0 && fabs(r - 1.0) <= JFO_RANGE_TOL ? e : 0;
}

int jfo_step(struct jfo *f, int have, double bpm, int32_t jumps, double session_bpm, uint32_t now_ms)
{
    int before = f->exp;
    if (have && bpm > 0.0) {
        if (!f->had_tempo || jumps != f->jumps) {            /* a new tempo: a lock after none, or a jump */
            double ref = session_bpm > 0.0 ? session_bpm : (f->out_bpm > 0.0 && now_ms - f->out_ms < JFO_RECENT_MS) ? f->out_bpm : 0.0;
            int e = ref > 0.0 ? jfo_range(bpm, ref) : 0;
            if (e || !f->manual) {           /* a range match, or no user's fold to keep: AUTO RANGE decides */
                set_exp(f, e);
                f->manual = 0;
                f->autos += f->exp != before;
            }
        }
        f->out_bpm = bpm * jfo_factor(f);
        f->out_ms = now_ms;
    }
    f->had_tempo = (uint8_t)(have && bpm > 0.0);
    f->jumps = jumps;
    return f->exp != before;
}

static int64_t mod64(int64_t a, int64_t m) { return ((a % m) + m) % m; }

/* /2, /4: of the m tracked beats t - r p, the one nearest a multiple of K output beats, K the folded period in them */
static int64_t anchor(int64_t t, double p, int64_t m, double pos_at_t, double out_period)
{
    double k = round((double)m * p / out_period), best_d = 1e9;
    int64_t best = 0;
    if (k < 1.0)
        k = 1.0;
    for (int64_t r = 0; r < m; r++) {
        double pos = pos_at_t - (double)r * p / out_period;
        double d = fabs(pos - k * round(pos / k));
        if (d < best_d - 1e-9) {
            best_d = d;
            best = r;
        }
    }
    return t - (int64_t)llround((double)best * p);
}

void jfo_grid(struct jfo *f, int64_t t, double p, int have_out, double pos_at_t, double out_period, int64_t *beat, double *period)
{
    int64_t m, j;
    if (f->exp >= 0) {
        *beat = t;
        *period = p / (double)(1 << f->exp);
        return;
    }
    m = (int64_t)1 << -f->exp;
    if (!f->anchored) {
        f->anchor_us = have_out && out_period > 0.0 ? anchor(t, p, m, pos_at_t, out_period) : t;
        f->anchored = 1;
    }
    j = (int64_t)llround((double)(t - f->anchor_us) / p);
    *beat = t - (int64_t)llround((double)mod64(j, m) * p);
    f->anchor_us = *beat;               /* moved along: the tracked period's small changes never add up */
    *period = p * (double)m;
}
