/* SPDX-License-Identifier: GPL-3.0-only */
/* The weak-signal hold (jb_weak.h), a line-by-line port of studies/tempo-changes/weak.py CoastBpm: names, constants and the
 * order of the steps are the Python's. Python's round() rounds halves to even, as rint() does; numpy's float32 median and
 * linear percentile are made the same way. */
#include "jb_weak.h"
#include <math.h>
#include <string.h>

#define OUTLIER 0.25                   /* LockedBpm.outlier */

void jw_reset(struct jb_weak *w)
{
    w->has_next = 0;
    w->next_line = 0.0;
    w->n_levels = w->levels_head = w->n_strong = w->n_seen = w->n_weak = 0;
    w->last_conf = 0.0;
    w->coasted = w->weak_beats = w->weak_misses = 0;
}

void jw_init(struct jb_weak *w, struct jw_store *st, float *act, uint32_t act_cap, int causal)
{
    memset(w, 0, sizeof *w);
    w->st = st;
    w->causal = (uint8_t)(causal != 0);
    if (!st)
        return;
    w->act = act ? act : st->act;
    w->act_cap = act ? act_cap : JW_ACT_RING;
    w->act_n = 0;
    jw_reset(w);
}

int jw_on(const struct jb_weak *w) { return w->st != NULL && w->act_cap > 0; }

void jw_act(struct jb_weak *w, int64_t index, float act)
{
    if (!jw_on(w) || index < w->act_n)
        return;
    if (index - w->act_n > (int64_t)w->act_cap)
        w->act_n = index - (int64_t)w->act_cap;     /* a long gap: only the ring's length of it matters */
    while (w->act_n < index)            /* (never on the device: frames come one by one) */
        w->act[(uint64_t)w->act_n++ % w->act_cap] = 0.0f;
    w->act[(uint64_t)index % w->act_cap] = act;
    w->act_n = index + 1;
}

/* the frames [lo, hi) are all in */
static int have(const struct jb_weak *w, int64_t lo, int64_t hi)
{
    return lo >= 0 && hi <= w->act_n && lo >= w->act_n - (int64_t)w->act_cap;
}

static float act_at(const struct jb_weak *w, int64_t k) { return w->act[(uint64_t)k % w->act_cap]; }

static int64_t frame_of(double t) { return (int64_t)rint(t * JW_FPS); }

/* deque.append with a maximum length: the oldest goes */
static void push(double *v, uint16_t *n, uint16_t cap, double t)
{
    if (*n == cap) {
        memmove(v, v + 1, sizeof v[0] * (size_t)(cap - 1u));
        (*n)--;
    }
    v[(*n)++] = t;
}

double jw_peak_near(const struct jb_weak *w, double t)
{
    int64_t f = frame_of(t), lo = f - JW_STRONG_HALF, hi = f + JW_STRONG_HALF + 1;
    int64_t oldest = w->act_n - (int64_t)w->act_cap;
    lo = lo > 0 ? lo : 0;
    lo = lo > oldest ? lo : oldest;
    hi = hi < w->act_n ? hi : w->act_n;
    if (hi <= lo)
        return 0.0;
    float m = act_at(w, lo);
    for (int64_t k = lo + 1; k < hi; k++)
        if (act_at(w, k) > m)
            m = act_at(w, k);
    return (double)m;
}

static void sort_floats(float *v, int n)
{
    for (int i = 1; i < n; ++i) {
        float x = v[i];
        int j = i;
        for (; j > 0 && v[j - 1] > x; --j)
            v[j] = v[j - 1];
        v[j] = x;
    }
}

/* np.percentile(levels, 75) (method linear, numpy's _lerp) */
static double level_p75(struct jb_weak *w)
{
    float *v = w->st->work;
    int n = w->n_levels;
    memcpy(v, w->st->levels, sizeof v[0] * (size_t)n);
    sort_floats(v, n);
    double virt = (double)n * 0.75 + (1.0 + 0.75 * (1.0 - 1.0 - 1.0)) - 1.0;
    double prev = floor(virt);
    int i = (int)prev, j = i + 1 < n ? i + 1 : n - 1;
    double gamma = virt - prev, a = v[i], b = v[j], diff = b - a;
    return gamma >= 0.5 ? b - diff * (1.0 - gamma) : a + diff * gamma;
}

/* CoastBpm.is_strong */
static int is_strong(struct jb_weak *w, double t)
{
    double a = jw_peak_near(w, t);
    double level = w->n_levels >= 8 ? level_p75(w) : 1.0;
    w->st->levels[w->levels_head] = (float)a;
    w->levels_head = (uint16_t)((w->levels_head + 1u) % JW_LEVEL_BEATS);
    if (w->n_levels < JW_LEVEL_BEATS)
        w->n_levels++;
    double th = fmax(JW_STRONG_ABS_MIN, fmin(JW_STRONG_ABS, JW_STRONG_REL * level));
    return a >= th;
}

int jw_update(struct jb_weak *w, struct jb_follow *f, double t)
{
    struct jb_mode *m = &f->m;
    if (!jw_on(w)) {
        jb_follow_update(f, t);
        return 1;
    }
    push(w->st->seen, &w->n_seen, JW_SEEN_KEEP, t);
    if (is_strong(w, t)) {
        push(w->st->strong, &w->n_strong, JW_STRONG_KEEP, t);
        jb_follow_update(f, t);
        return 1;
    }
    if (!f->has_kf) {
        jb_follow_update(f, t);         /* before the first lock every beat counts (weak.py filter off) */
        return 0;
    }
    /* every tracked beat stays in the recent beats: their own tempo stays at the beat level and a grid that slid off the
     * beats is still caught by the jump rule */
    jb_dq_push(&m->recent, t);
    jb_dq_push(&m->history, t);
    if (jb_follow_weak_obs(f, t, JW_R_WEAK, JW_WINDOW)) {
        push(w->st->weak, &w->n_weak, JW_WEAK_KEEP, t);
        m->last_fit_time = t;
        m->has_last_fit = 1;
        jb_follow_outcome(f, 1);
        w->weak_beats++;
        return 0;
    }
    double n = rint((t - f->x[0]) / f->x[1]);
    if (n >= 1.0 && fabs(t - (f->x[0] + n * f->x[1])) > OUTLIER * f->x[1]) {
        /* off the grid: a miss like a strong one (wandering beats of a breakdown are not consistent: no jump) */
        m->misses++;
        w->weak_misses++;
        if (m->misses >= m->miss_limit && jb_mode_recent_consistent(m))
            jb_follow_jump(f, t);
    }
    return 0;
}

/* np.median of the float32 frames [lo, hi): the middle one, or the float32 mean of the two */
static float median_frames(struct jb_weak *w, int64_t lo, int64_t hi)
{
    float *v = w->st->work;
    int n = (int)(hi - lo);
    for (int i = 0; i < n; i++)
        v[i] = act_at(w, lo + i);
    /* quickselect the upper middle, then the lower middle is the largest below it */
    int k = n / 2, l = 0, r = n - 1;
    while (l < r) {
        float pivot = v[(l + r) / 2];
        int i = l, j = r;
        while (i <= j) {
            while (v[i] < pivot)
                i++;
            while (v[j] > pivot)
                j--;
            if (i <= j) {
                float x = v[i];
                v[i++] = v[j];
                v[j--] = x;
            }
        }
        if (k <= j)
            r = j;
        else if (k >= i)
            l = i;
        else
            break;
    }
    if (n % 2)
        return v[k];
    float below = v[0];
    for (int i = 1; i < k; i++)
        below = v[i] > below ? v[i] : below;
    float s = below + v[k];
    return s / 2.0f;
}

int jw_weak_peak(struct jb_weak *w, double line, double period, double *obs)
{
    int64_t lo = frame_of(line - JW_WINDOW * period), hi = frame_of(line + JW_WINDOW * period) + 1;
    int64_t blo = frame_of(line - period), bhi = frame_of(line + period) + 1;
    if (lo < 1 || hi >= w->act_n || blo < 0 || bhi > w->act_n || !have(w, blo, bhi) || !have(w, lo - 1, hi + 1)
        || bhi - blo > JW_ACT_RING)
        return 0;
    int64_t at = lo;
    float pk = act_at(w, lo);
    for (int64_t k = lo + 1; k < hi; k++)
        if (act_at(w, k) > pk) {        /* (np.argmax: the first of equal maxima) */
            pk = act_at(w, k);
            at = k;
        }
    double base = (double)median_frames(w, blo, bhi);
    if ((double)pk < JW_FLOOR || (double)pk < JW_SALIENCE * fmax(base, 1e-6))
        return 0;
    double frac = 0.0;
    if (0 < at && at < w->act_n - 1) {
        double a = act_at(w, at - 1), b = act_at(w, at), c = act_at(w, at + 1);
        double d = a - 2 * b + c;
        frac = d < 0 ? 0.5 * (a - c) / d : 0.0;
    }
    *obs = ((double)at + frac) / JW_FPS;
    return 1;
}

/* a tracked beat within margin of the line: the last one (the look-ahead view), or any (the causal view, whose later
 * beats may already be in) */
static int tracked_near(const struct jb_weak *w, double line, double margin)
{
    const double *v = w->st->seen;
    size_t n = w->n_seen, lo = 0, hi = n;
    if (!n)
        return 0;
    if (!w->causal)
        return fabs(v[n - 1u] - line) <= margin;
    while (lo < hi) {                   /* bisect_left */
        size_t mid = (lo + hi) / 2u;
        if (v[mid] < line)
            lo = mid + 1u;
        else
            hi = mid;
    }
    return (lo < n && fabs(v[lo] - line) <= margin) || (lo > 0 && fabs(v[lo - 1u] - line) <= margin);
}

void jw_coast(struct jb_weak *w, struct jb_follow *f, double now)
{
    if (!jw_on(w))
        return;
    if (!f->has_kf || jb_mode_at(&f->m, now) == JB_STOPPED) {
        w->has_next = 0;
        return;
    }
    double p = f->x[1], obs;
    if (!w->has_next || w->next_line < f->x[0] + 0.5 * p) {
        w->next_line = f->x[0] + p;
        w->has_next = 1;
    }
    while (w->next_line + JW_WINDOW * p + JW_LAG_S <= now
           && (!w->causal || frame_of(w->next_line + p) <= frame_of(now))) {
        double line = w->next_line;
        w->next_line = line + p;
        if (tracked_near(w, line, JW_WINDOW * p))
            continue;                   /* a tracked beat is there (the follower took it or missed it) */
        if (jw_weak_peak(w, line, p, &obs) && jb_follow_weak_obs(f, obs, JW_R_WEAK, JW_WINDOW)) {
            push(w->st->weak, &w->n_weak, JW_WEAK_KEEP, obs);
            f->m.last_fit_time = obs;
            f->m.has_last_fit = 1;
            jb_follow_outcome(f, 1);
            w->coasted++;
            w->next_line = f->x[0] + f->x[1];
        }
    }
}
