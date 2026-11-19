/* SPDX-License-Identifier: GPL-3.0-only */
#include "jb_resume.h"
#include <math.h>

void jre_init(struct jb_resume *r)
{
    r->period = 0.0;
    r->has = r->armed = 0;
    r->mark = 0;
    r->last_r = 0.0f;
    r->resumes = 0;
    for (int i = 0; i < JRE_BINS; i++)
        r->shape[i] = 0.0f;
}

/* the activation at a fractional frame (linear), -1 when not in the ring */
static float act_at(const float *ring, uint32_t cap, int64_t act_n, double f)
{
    const long a = (long)floor(f);
    if (a < 0 || a + 1 > act_n - 1 || a < act_n - (int64_t)cap)
        return -1.0f;
    const float w = (float)(f - (double)a);
    return (1.0f - w) * ring[(uint64_t)a % cap] + w * ring[(uint64_t)(a + 1) % cap];
}

/* the activation folded at the period from the origin o (frames), JRE_LINES periods: y[i] at o + i P / JRE_BINS + k P;
 * 0 when not all in the ring */
static int fold(const float *ring, uint32_t cap, int64_t act_n, double o, double p, float *y)
{
    for (int i = 0; i < JRE_BINS; i++) {
        float s = 0.0f;
        for (int k = 0; k < JRE_LINES; k++) {
            const float v = act_at(ring, cap, act_n, o + (double)i * p / JRE_BINS + (double)k * p);
            if (v < 0.0f)
                return 0;
            s += v;
        }
        y[i] = s / JRE_LINES;
    }
    return 1;
}

void jre_remember(struct jb_resume *r, double period, double t_line, const float *ring, uint32_t cap, int64_t act_n)
{
    if (!(period > 0.0))
        return;
    if (r->has && fabs(period / r->period - 1.0) > 0.03)
        r->has = 0;                    /* another tempo: its beat is learnt afresh */
    float y[JRE_BINS];
    const double p = period * 100.0, newest = (double)(act_n - 2);
    /* the published line's frame, moved back to the last one whose JRE_LINES periods are all in */
    double o = t_line * 100.0;
    o -= ceil((o + JRE_LINES * p - newest) / p) * p;
    if (!ring || !fold(ring, cap, act_n, o, p, y))
        return;
    if (!r->has) {
        for (int i = 0; i < JRE_BINS; i++)
            r->shape[i] = y[i];
        r->has = 1;
    } else
        for (int i = 0; i < JRE_BINS; i++)
            r->shape[i] += (float)JRE_LEARN * (y[i] - r->shape[i]);
    r->period = period;
}

void jre_arm(struct jb_resume *r, uint32_t mark)
{
    if (!r->has || r->armed)
        return;
    r->armed = 1;
    r->mark = mark;
    r->sound_since = -1.0;
}

void jre_sound(struct jb_resume *r, double now)
{
    if (r->armed && r->sound_since < 0.0)
        r->sound_since = now;
}

int jre_holds(const struct jb_resume *r, uint32_t mark, double now)
{
    return r->armed && mark == r->mark && (r->sound_since < 0.0 || now - r->sound_since < JRE_WAIT_S);
}

void jre_disarm(struct jb_resume *r) { r->armed = 0; }

/* Pearson r of a and b circularly shifted by s */
static float corr_at(const float *a, const float *b, int s)
{
    float ma = 0.0f, mb = 0.0f;
    for (int i = 0; i < JRE_BINS; i++) {
        ma += a[i];
        mb += b[i];
    }
    ma /= JRE_BINS;
    mb /= JRE_BINS;
    float ab = 0.0f, aa = 0.0f, bb = 0.0f;
    for (int i = 0; i < JRE_BINS; i++) {
        const float x = a[i] - ma, y = b[(i + s) % JRE_BINS] - mb;
        ab += x * y;
        aa += x * x;
        bb += y * y;
    }
    return aa > 0.0f && bb > 0.0f ? ab / sqrtf(aa * bb) : 0.0f;
}

int jre_find(struct jb_resume *r, const float *ring, uint32_t cap, int64_t act_n, double *t_line)
{
    if (!r->armed || !ring || !(r->period > 0.0))
        return 0;
    const double p = r->period * 100.0, o = (double)(act_n - 2) - JRE_LINES * p;
    float y[JRE_BINS];
    if (!fold(ring, cap, act_n, o, p, y))
        return 0;
    int with = 0, newest = 0;          /* periods with a peak: at least JRE_SOUND, the newest among them */
    for (int k = 0; k < JRE_LINES; k++) {
        float top = 0.0f;
        for (int i = 0; i < JRE_BINS; i++) {
            const float v = act_at(ring, cap, act_n, o + (double)i * p / JRE_BINS + (double)k * p);
            top = v > top ? v : top;
        }
        if (top >= (float)JRE_PEAK) {
            with++;
            newest = k == JRE_LINES - 1;
        }
    }
    if (with < JRE_SOUND || !newest)
        return 0;                      /* not enough beat in the window yet (a cut's own onset is one peak) */
    float fold_top = 0.0f;
    for (int i = 0; i < JRE_BINS; i++)
        fold_top = y[i] > fold_top ? y[i] : fold_top;
    if (fold_top < (float)JRE_FOLD_PEAK)
        return 0;                      /* the peaks are not at one phase of the period */
    int best = 0;
    float rb = -2.0f;
    for (int s = 0; s < JRE_BINS; s++) {
        const float c = corr_at(r->shape, y, s);
        if (c > rb) {
            rb = c;
            best = s;
        }
    }
    r->last_r = rb;
    if (rb < (float)JRE_CORR)
        return 0;                      /* not the song's beat (yet) */
    /* the beat (the shape's bin 0) sits at bin `best` of the window: its newest line in the window */
    double t = (o + (double)best * p / JRE_BINS + (JRE_LINES - 1) * p) / 100.0;
    *t_line = t;
    r->resumes++;
    jre_disarm(r);
    return 1;
}

void jre_seed(struct jb_follow *f, double t, double period)
{
    /* a clean ModeBpm: the beats it kept from before (the cut's own onset, a stray beat) would bend the fit off the grid */
    struct jb_mode *m = &f->m;
    int (*gate)(void *, int, double, double) = m->gate;
    void *gate_ctx = m->gate_ctx;
    const int jumps = m->jumps, lock_beats = m->lock_beats;
    jb_mode_init(m);
    m->gate = gate;
    m->gate_ctx = gate_ctx;
    m->jumps = jumps;
    m->lock_beats = lock_beats;
    f->has_kf = 0;
    /* (a clean ModeBpm decides its mode on up to 2 x lock_beats beats: as many virtual beats as it takes, ending on t) */
    const int n = 2 * (lock_beats > 1 ? lock_beats : 2);
    int k = n - 1;
    for (; k >= 0 && !f->has_kf; k--)
        jb_follow_update(f, t - k * period);
    for (; k >= 0; k--)                /* locked before t: the rest of the grid up to t through the glide */
        jb_follow_update(f, t - k * period);
}
