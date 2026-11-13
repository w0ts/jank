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
            d->trans[j][i] = (float)(p[i] / sum);
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

int jb_dbn_step(struct jb_dbn *d, float act, double *t)
{
    const int n = d->n_intervals;
    const float dens_beat = act, dens_off = (1.0f - act) / (float)(OBSERVATION_LAMBDA - 1.0);
    for (int i = 0; i < n; ++i) {                                 /* the position steps on; interval i's first state is new */
        const int f = d->first[i], len = d->interval[i];
        float v = 0.0f;
        for (int j = 0; j < n; ++j)
            v += d->fwd[d->first[j] + d->interval[j] - 1] * d->trans[j][i];
        d->next[f] = v;
        for (int k = 1; k < len; ++k)
            d->next[f + k] = d->fwd[f + k - 1];
    }
    double sum = 0.0;
    int best = 0;
    for (int s = 0; s < d->n_states; ++s) {
        d->next[s] *= d->is_beat[s] ? dens_beat : dens_off;
        sum += d->next[s];
    }
    if (!(sum > 0.0)) {                                           /* all mass lost (an activation of exactly 0 or 1) */
        for (int s = 0; s < d->n_states; ++s)
            d->next[s] = 1.0f / (float)d->n_states;
        sum = 1.0;
    }
    const float scale = (float)(1.0 / sum);
    for (int s = 0; s < d->n_states; ++s) {
        d->fwd[s] = d->next[s] * scale;
        if (d->fwd[s] > d->fwd[best])
            best = s;
    }
    const double cur = (double)d->counter++ / d->fps;
    if (d->is_beat[best] && cur >= d->last_beat + d->min_gap) {
        d->last_beat = cur;
        *t = cur;
        return 1;
    }
    return 0;
}
