/* SPDX-License-Identifier: GPL-3.0-only */
/* The beat DBN's forward step (jb_dbn.h). */
#include "jb_dbn.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define TRANSITION_LAMBDA 100.0
#define OBSERVATION_LAMBDA 16.0

int jb_dbn_init(struct jb_dbn *d, double min_bpm, double max_bpm, double fps)
{
    memset(d, 0, sizeof *d);
    int lo = (int)rint(60.0 * fps / max_bpm), hi = (int)rint(60.0 * fps / min_bpm);
    if (lo < 2 || hi < lo || hi - lo + 1 > JB_DBN_MAX_INTERVALS)
        return -1;
    int states = 0;
    d->n_intervals = (uint16_t)(hi - lo + 1);
    for (int i = 0; i < d->n_intervals; ++i) {
        d->interval[i] = (uint16_t)(lo + i);
        d->first[i] = (uint16_t)states;
        states += lo + i;
        d->last[i] = (uint16_t)(states - 1);
    }
    if (states > JB_DBN_MAX_STATES)
        return -1;
    d->n_states = (uint16_t)states;
    for (int i = 0; i < d->n_intervals; ++i)
        for (int k = 0; k < d->interval[i]; ++k)
            if ((double)k / d->interval[i] < 1.0 / OBSERVATION_LAMBDA) {
                d->is_beat[d->first[i] + k] = 1;
                d->zone[i] = (uint16_t)(k + 1);
            }
    for (int j = 0; j < d->n_intervals; ++j) {                    /* from interval j, rows normalised */
        double sum = 0.0, p[JB_DBN_MAX_INTERVALS];
        for (int i = 0; i < d->n_intervals; ++i) {
            p[i] = exp(-TRANSITION_LAMBDA * fabs((double)d->interval[i] / d->interval[j] - 1.0));
            if (p[i] <= DBL_EPSILON)
                p[i] = 0.0;
            sum += p[i];
        }
        for (int i = 0; i < d->n_intervals; ++i)
            d->trans[i][j] = (float)(p[i] / sum);
    }
    d->fps = fps;
    d->min_gap = 60.0 / max_bpm;
    jb_dbn_reset(d);
    return 0;
}

void jb_dbn_reset(struct jb_dbn *d)
{
    for (int s = 0; s < d->n_states; ++s)
        d->fwd[s] = 1.0f / (float)d->n_states;
    d->counter = 0;
    d->last_beat = 0.0;
}

void jb_dbn_set_prior(struct jb_dbn *d, const float *weights)
{
    d->has_prior = (uint8_t)(weights != NULL);
    for (int i = 0; i < d->n_intervals; ++i)
        d->entry[i] = weights ? weights[i] : 1.0f;
}

float jb_dbn_mass_near(const struct jb_dbn *d, double bpm, double tol)
{
    const double lim = log2(1.0 + tol);
    float mass = 0.0f;
    for (int i = 0; i < d->n_intervals; ++i) {
        double lr = log2((60.0 * d->fps / d->interval[i]) / bpm);
        if (fabs(lr - rint(lr)) <= lim) {
            const int f = d->first[i], len = d->interval[i];
            float m = 0.0f;
            for (int k = 0; k < len; ++k)
                m += d->fwd[f + k];
            mass += m;
        }
    }
    return mass;
}

/* One frame, in place and with no double arithmetic (M2m: the profile on the S3 showed this step at 16.6 % of core 0, most of
 * it the soft-double sum of 1750 floats and a copy pass). Each state's value is the old step's: the first state of an interval is
 * the sum over the intervals' last states of last x transition (float, in the order of j), times the tempo prior, times the beat
 * density; the others are the previous state's value times the density of their position. Only the normalising sum differs: float
 * (a partial sum per interval, then their sum) instead of one double, so the scale can differ in the last bits; it scales every
 * state alike, so the most probable state, and the beats, do not move (checked against the golden beats). */
int jb_dbn_step(struct jb_dbn *d, float act, double *t)
{
    const int n = d->n_intervals;
    const float dens_beat = act, dens_off = (1.0f - act) / (float)(OBSERVATION_LAMBDA - 1.0);
    float lastv[JB_DBN_MAX_INTERVALS], first[JB_DBN_MAX_INTERVALS], total = 0.0f;
    float *fwd = d->fwd;
    for (int j = 0; j < n; ++j)
        lastv[j] = fwd[d->last[j]];
    for (int i = 0; i < n; ++i) {                                 /* the new first states, from the old last ones */
        const float *tr = d->trans[i];
        float v = 0.0f;
        for (int j = 0; j < n; ++j)
            v += lastv[j] * tr[j];
        first[i] = v;
    }
    for (int i = 0; i < n; ++i) {
        float *p = fwd + d->first[i], part = 0.0f;
        const int len = d->interval[i], zone = d->zone[i];
        for (int k = len - 1; k >= zone; --k) {                   /* no beat expected: from the end, in place */
            float v = p[k - 1] * dens_off;
            p[k] = v;
            part += v;
        }
        for (int k = zone - 1; k >= 1; --k) {                     /* the beat zone */
            float v = p[k - 1] * dens_beat;
            p[k] = v;
            part += v;
        }
        float v = first[i];                                       /* a new beat at this tempo */
        if (d->has_prior)
            v *= d->entry[i];
        v *= dens_beat;
        p[0] = v;
        total += part + v;
    }
    if (!(total > 0.0f)) {                                        /* all mass lost (an activation of exactly 0 or 1) */
        for (int s = 0; s < d->n_states; ++s)
            fwd[s] = 1.0f / (float)d->n_states;
        total = 1.0f;
    }
    const float scale = 1.0f / total;
    int best = 0;
    float best_v = -1.0f;
    for (int s = 0; s < d->n_states; ++s) {
        float v = fwd[s] * scale;
        fwd[s] = v;
        if (v > best_v) {
            best_v = v;
            best = s;
        }
    }
    const double cur = (double)d->counter++ / d->fps;
    if (d->is_beat[best] && cur >= d->last_beat + d->min_gap) {
        d->last_beat = cur;
        *t = cur;
        return 1;
    }
    return 0;
}
