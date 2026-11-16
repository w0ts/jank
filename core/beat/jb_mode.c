/* SPDX-License-Identifier: GPL-3.0-only */
/* The estimator (jb_mode.h), a line-by-line port of eval/mode_bpm.py and eval/locked_bpm.py: names, constants and the
 * order of the steps are theirs, so that the two can be compared beat by beat. Python's round() and numpy's round()
 * round halves to even, as rint() does. */
#include "jb_mode.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* LockedBpm */
#define SHORT_BEATS 16
#define CHANGE_REL 0.005
#define CHANGE_BEATS 8
#define OUTLIER 0.25
#define MISS_RESTART 12
#define MIN_PERIOD 0.2
/* glide_bpm.quad_tempo */
#define GLIDE_BEATS 32
#define GLIDE_OUTLIER 0.2
/* ModeBpm */
#define CLASSIFY_BEATS 32
#define MIN_SIDE 8
#define RAMP_MIN_MOVE 0.004
#define RAMP_EXIT 0.002
#define EXIT_CHECKS 16
#define HOLD_S 30.0
#define STUCK_BEATS 32
#define RAMP_MARGIN 0.6
#define NEVER 1000000000

/* ---- deque ---- */
static void dq_init(struct jb_dq *q, int cap)
{
    q->n = 0;
    q->cap = (uint8_t)cap;
}

static void dq_push(struct jb_dq *q, double t)
{
    if (q->n == q->cap) {
        memmove(q->v, q->v + 1, (size_t)(q->n - 1) * sizeof(double));
        --q->n;
    }
    q->v[q->n++] = t;
}

static double dq_last(const struct jb_dq *q)
{
    return q->v[q->n - 1];
}

/* ---- numpy replacements ---- */
static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* np.median(np.diff(b)), n >= 2 */
static double median_diff(const double *b, int n)
{
    double d[JB_DQ_MAX];
    for (int i = 1; i < n; ++i)
        d[i - 1] = b[i] - b[i - 1];
    qsort(d, (size_t)(n - 1), sizeof(double), cmp_double);
    int m = n - 1;
    return m % 2 ? d[m / 2] : 0.5 * (d[m / 2 - 1] + d[m / 2]);
}

/* np.unique(k).size for small integer-valued k */
static int unique_count(const double *k, int n)
{
    int count = 0;
    for (int i = 0; i < n; ++i) {
        int seen = 0;
        for (int j = 0; j < i && !seen; ++j)
            seen = k[j] == k[i];
        count += !seen;
    }
    return count;
}

/* Least squares polynomial of degree 1 or 2 (np.polyfit), highest power first. Centred and scaled for conditioning.
 * Returns 0, or -1 when the points do not determine it. */
static int polyfit(const double *x, const double *y, int n, int deg, double *coef)
{
    double xm = 0, ym = 0, s = 0;
    for (int i = 0; i < n; ++i) {
        xm += x[i];
        ym += y[i];
    }
    xm /= n;
    ym /= n;
    for (int i = 0; i < n; ++i)
        if (fabs(x[i] - xm) > s)
            s = fabs(x[i] - xm);
    if (s == 0.0)
        return -1;
    const int m = deg + 1;
    double a[3][4] = {{0}};
    double pw[5] = {0};                                           /* sums of u^p, p = 0..4 */
    double rhs[3] = {0};
    for (int i = 0; i < n; ++i) {
        double u = (x[i] - xm) / s, yy = y[i] - ym, up = 1.0;
        for (int p = 0; p < 2 * deg + 1; ++p) {
            pw[p] += up;
            if (p <= deg)
                rhs[p] += up * yy;
            up *= u;
        }
    }
    for (int r = 0; r < m; ++r) {
        for (int c = 0; c < m; ++c)
            a[r][c] = pw[r + c];
        a[r][m] = rhs[r];
    }
    for (int c = 0; c < m; ++c) {                                 /* Gauss with partial pivoting */
        int piv = c;
        for (int r = c + 1; r < m; ++r)
            if (fabs(a[r][c]) > fabs(a[piv][c]))
                piv = r;
        if (fabs(a[piv][c]) < 1e-12)
            return -1;
        for (int k = 0; k <= m; ++k) {
            double tmp = a[c][k];
            a[c][k] = a[piv][k];
            a[piv][k] = tmp;
        }
        for (int r = c + 1; r < m; ++r) {
            double f = a[r][c] / a[c][c];
            for (int k = c; k <= m; ++k)
                a[r][k] -= f * a[c][k];
        }
    }
    double b[3];                                                  /* y - ym = b0 + b1 u + b2 u^2 */
    for (int r = m - 1; r >= 0; --r) {
        double v = a[r][m];
        for (int c = r + 1; c < m; ++c)
            v -= a[r][c] * b[c];
        b[r] = v / a[r][r];
    }
    if (deg == 1) {
        coef[0] = b[1] / s;
        coef[1] = ym + b[0] - b[1] * xm / s;
    } else {
        coef[0] = b[2] / (s * s);
        coef[1] = b[1] / s - 2.0 * b[2] * xm / (s * s);
        coef[2] = ym + b[0] - b[1] * xm / s + b[2] * xm * xm / (s * s);
    }
    return 0;
}

static double polyval(const double *coef, int deg, double x)
{
    double v = 0;
    for (int i = 0; i <= deg; ++i)
        v = v * x + coef[i];
    return v;
}

/* locked_bpm.short_period: the period from recent beats, missed ones bridged; <0 for None */
static double short_period(const double *b, int n)
{
    if (n < 4)
        return -1;
    double first = median_diff(b, n), k[JB_DQ_MAX], c[2];
    if (first <= 0.2)
        return -1;
    for (int i = 0; i < n; ++i)
        k[i] = rint((b[i] - b[n - 1]) / first);
    if (unique_count(k, n) < 4 || polyfit(k, b, n, 1, c))
        return -1;
    return c[0];
}

/* mode_bpm._bridge: beat numbers for beat times; 0 for None */
static int bridge(const double *b, int n, double *k)
{
    double first = median_diff(b, n);
    if (!(first > 0.2))
        return 0;
    for (int i = 0; i < n; ++i)
        k[i] = rint((b[i] - b[n - 1]) / first);
    return 1;
}

/* mode_bpm._rss */
static double rss(const double *k, const double *t, int n, int deg)
{
    double c[3], tot = 0.0;
    if (polyfit(k, t, n, deg, c)) {                               /* the points do not fix a curve: spread about the mean */
        double mean = 0;
        for (int i = 0; i < n; ++i)
            mean += t[i];
        mean /= n;
        for (int i = 0; i < n; ++i)
            tot += (t[i] - mean) * (t[i] - mean);
        return tot;
    }
    for (int i = 0; i < n; ++i) {
        double d = polyval(c, deg, k[i]) - t[i];
        tot += d * d;
    }
    return tot;
}

/* glide_bpm.quad_tempo: (period at the latest beat, relative tempo move across the window, last beat time on the curve) */
static int quad_tempo(const double *b, int n, double *period, double *move, double *last)
{
    if (n < GLIDE_BEATS / 2)
        return 0;
    double first = median_diff(b, n), k[JB_DQ_MAX], c[3];
    if (first <= 0.2)
        return 0;
    double kmin = 0;
    for (int i = 0; i < n; ++i) {
        k[i] = rint((b[i] - b[n - 1]) / first);
        if (k[i] < kmin)
            kmin = k[i];
    }
    if (unique_count(k, n) < GLIDE_BEATS / 2 || polyfit(k, b, n, 2, c))
        return 0;
    double kk[JB_DQ_MAX], bb[JB_DQ_MAX];
    int kept = 0;
    for (int i = 0; i < n; ++i)
        if (fabs(polyval(c, 2, k[i]) - b[i]) <= GLIDE_OUTLIER * first) {
            kk[kept] = k[i];
            bb[kept++] = b[i];
        }
    if (kept >= GLIDE_BEATS / 2 && kept != n && polyfit(kk, bb, kept, 2, c))
        return 0;
    if (c[1] <= 0.2)
        return 0;
    *period = c[1];
    *move = fabs(2.0 * c[0] * kmin) / c[1];
    *last = c[2];
    return 1;
}

/* mode_bpm.signed_quad: the move is + speeding up, - slowing down */
static int signed_quad(const double *b, int n, double *period, double *move, double *last)
{
    double k[JB_DQ_MAX], c[3];
    if (!quad_tempo(b, n, period, move, last) || !bridge(b, n, k) || polyfit(k, b, n, 2, c))
        return 0;
    if (c[0] > 0)
        *move = -*move;
    return 1;
}

/* mode_bpm.classify: returns 1 for a ramp, else 0 with the first beat after the break in *split */
static int classify(const double *hist, int n_all, int *split)
{
    int n = n_all < CLASSIFY_BEATS ? n_all : CLASSIFY_BEATS;
    const double *b = hist + (n_all - n);
    double k[JB_DQ_MAX];
    if (n < 2 * MIN_SIDE || !bridge(b, n, k) || unique_count(k, n) < 2 * MIN_SIDE) {
        *split = n > MIN_SIDE ? n - MIN_SIDE : 0;
        return 0;
    }
    double ramp = rss(k, b, n, 2), best = INFINITY;
    *split = n - MIN_SIDE;
    for (int s = MIN_SIDE; s <= n - MIN_SIDE; ++s) {
        double jump = rss(k, b, s, 1) + rss(k + s, b + s, n - s, 1);
        if (jump < best) {
            best = jump;
            *split = s;
        }
    }
    return ramp <= RAMP_MARGIN * best;
}

/* ---- LockedBpm ---- */
static int fit_line(const struct jb_mode *m, double *t0, double *period)
{
    if (m->fit_n < 3)
        return 0;
    double var = m->fit_n * m->skk - m->sk * m->sk;
    if (var <= 0)
        return 0;
    double p = (m->fit_n * m->skt - m->sk * m->st) / var;
    *period = p;
    *t0 = (m->st - p * m->sk) / m->fit_n;
    return 1;
}

static void fit_add(struct jb_mode *m, double k, double t)
{
    ++m->fit_n;
    m->sk += k;
    m->st += t;
    m->skk += k * k;
    m->skt += k * t;
}

/* LockedBpm.period: the fit's, else the short one; <0 for None */
static double locked_period(const struct jb_mode *m)
{
    double t0, p;
    if (fit_line(m, &t0, &p))
        return p;
    return short_period(m->recent.v, m->recent.n);
}

static double period_now(const struct jb_mode *m)                  /* ModeBpm.period */
{
    if ((m->mode == JB_SPEEDING_UP || m->mode == JB_SLOWING_DOWN) && m->has_ramp_period && m->ramp_period != 0.0)
        return m->ramp_period;
    return locked_period(m);
}

static void restart(struct jb_mode *m)                             /* LockedBpm._restart */
{
    m->fit_n = 0;
    m->sk = m->st = m->skk = m->skt = 0.0;
    ++m->restarts;
    double p = short_period(m->recent.v, m->recent.n);
    if (p < 0) {
        m->has_t_last = 0;
        return;
    }
    double last = dq_last(&m->recent);
    for (int i = 0; i < m->recent.n; ++i)
        fit_add(m, rint((m->recent.v[i] - last) / p), m->recent.v[i]);
    m->k_last = 0.0;
    m->t_last = last;
    m->has_t_last = 1;
    m->disagree = m->misses = 0;
}

static void set_mode(struct jb_mode *m, enum jb_mode_kind mode)
{
    m->mode = mode;
}

static int is_ramp(const struct jb_mode *m)
{
    return m->mode == JB_SPEEDING_UP || m->mode == JB_SLOWING_DOWN;
}

void jb_mode_init(struct jb_mode *m)
{
    memset(m, 0, sizeof *m);
    dq_init(&m->recent, SHORT_BEATS);
    dq_init(&m->history, CLASSIFY_BEATS);
    m->miss_limit = MISS_RESTART;          /* ModeBpm.__post_init__: hold on misses, the base restart never fires */
    m->lock_beats = JB_LOCK_BEATS;
    m->mode = JB_STOPPED;
}

enum jb_mode_kind jb_mode_at(struct jb_mode *m, double now)
{
    if (m->mode != JB_STOPPED && (!m->has_last_fit || now - m->last_fit_time > HOLD_S))
        set_mode(m, JB_STOPPED);
    return m->mode;
}

/* ModeBpm._recent_consistent */
static int recent_consistent(const struct jb_mode *m)
{
    const int lb = m->lock_beats;
    if (lb < 2 || lb > JB_DQ_MAX || m->recent.n < lb)
        return 0;
    const double *b = m->recent.v + (m->recent.n - lb);
    double p = short_period(b, lb);
    if (p < 0)
        return 0;
    double k[JB_DQ_MAX], c[2], kmax = -1e30, kmin = 1e30;
    for (int i = 0; i < lb; ++i) {
        k[i] = rint((b[i] - b[lb - 1]) / p);
        kmax = k[i] > kmax ? k[i] : kmax;
        kmin = k[i] < kmin ? k[i] : kmin;
    }
    if (unique_count(k, lb) != lb || kmax - kmin != lb - 1 || polyfit(k, b, lb, 1, c))
        return 0;
    for (int i = 0; i < lb; ++i)
        if (fabs(b[i] - polyval(c, 1, k[i])) >= 0.08 * p)
            return 0;
    return 1;
}

/* ModeBpm._classify */
static void classify_change(struct jb_mode *m)
{
    double old = locked_period(m), shrt = short_period(m->recent.v, m->recent.n);
    int direction = (shrt > 0 && old > 0 && shrt < old) ? 1 : -1;
    int split, ramp = classify(m->history.v, m->history.n, &split);
    int soon = m->has_last_jump && m->beats_seen - m->last_jump_beat <= 2 * CLASSIFY_BEATS;
    if (soon && m->last_jump_dir == direction)
        ramp = 1;
    if (ramp) {
        double period, move, last;
        if (signed_quad(m->history.v, m->history.n, &period, &move, &last) && fabs(move) >= RAMP_MIN_MOVE && old > 0) {
            m->ramp_period = period;
            m->ramp_last = last;
            m->has_ramp_period = 1;
            m->exit_run = m->misses = 0;
            m->has_last_jump = 0;
            set_mode(m, move > 0 ? JB_SPEEDING_UP : JB_SLOWING_DOWN);
            return;
        }
    }
    ++m->jumps;
    m->last_jump_beat = m->beats_seen;
    m->last_jump_dir = direction;
    m->has_last_jump = 1;
    struct jb_dq keep = m->history;
    dq_init(&m->recent, SHORT_BEATS);
    for (int i = split; i < keep.n; ++i)
        dq_push(&m->recent, keep.v[i]);
    restart(m);
}

/* ModeBpm._check_change */
static void check_change(struct jb_mode *m, double period)
{
    if (is_ramp(m))
        return;
    double sp = short_period(m->recent.v, m->recent.n);
    if (sp < 0)
        return;
    m->disagree = fabs(sp / period - 1.0) > CHANGE_REL ? m->disagree + 1 : 0;
    if (m->disagree >= CHANGE_BEATS) {
        m->disagree = 0;
        if (m->mode == JB_STOPPED)
            restart(m);                    /* not running yet: reseed */
        else
            classify_change(m);
    }
}

/* LockedBpm.update, with ModeBpm's overrides */
static void locked_update(struct jb_mode *m, double t)
{
    dq_push(&m->recent, t);
    double p = period_now(m);
    if (!m->has_t_last || p < 0) {
        if (m->recent.n >= 4)
            restart(m);
        return;
    }
    double k = m->k_last + rint((t - m->t_last) / p);
    if (k == m->k_last)
        return;                            /* a double detection of the same beat */
    double t0, period;
    if (!fit_line(m, &t0, &period)) {
        t0 = m->t_last - p * m->k_last;
        period = p;
    }
    if (fabs(t - (t0 + period * k)) > OUTLIER * period) {
        if (++m->misses >= NEVER)
            restart(m);
        return;
    }
    m->misses = 0;
    fit_add(m, k, t);
    m->k_last = k;
    m->t_last = t;
    check_change(m, period);
}

/* ModeBpm._ramp_update */
static void ramp_update(struct jb_mode *m, double t)
{
    dq_push(&m->recent, t);
    if (m->has_ramp_period && m->ramp_period != 0.0) {
        double n = rint((t - m->ramp_last) / m->ramp_period);
        if (n >= 1 && fabs(t - (m->ramp_last + n * m->ramp_period)) > OUTLIER * m->ramp_period) {
            --m->history.n;                /* off the curve: not fitted */
            ++m->misses;
            return;                        /* (the miss limit of 10^9 never ends a ramp here: see __post_init__) */
        }
    }
    m->misses = 0;
    m->last_fit_time = t;
    m->has_last_fit = 1;
    double period, change, last;
    if (!signed_quad(m->history.v, m->history.n, &period, &change, &last))
        return;
    m->ramp_period = period;
    m->ramp_last = last;
    m->has_ramp_period = 1;
    int reversed = (change > 0) != (m->mode == JB_SPEEDING_UP);
    if (reversed && fabs(change) >= RAMP_MIN_MOVE) {
        m->has_ramp_period = 0;            /* a ramp that turns around is not a ramp */
        set_mode(m, JB_STEADY);
        restart(m);
        return;
    }
    m->exit_run = fabs(change) < RAMP_EXIT ? m->exit_run + 1 : 0;
    if (m->exit_run >= EXIT_CHECKS) {
        m->has_ramp_period = 0;
        set_mode(m, JB_STEADY);
        restart(m);
    }
}

void jb_mode_update(struct jb_mode *m, double t)
{
    ++m->beats_seen;
    jb_mode_at(m, t);
    dq_push(&m->history, t);
    if (is_ramp(m)) {
        ramp_update(m, t);
        return;
    }
    int fitted_before = m->fit_n, restarts_before = m->restarts;
    locked_update(m, t);
    int restarted = m->restarts > restarts_before;
    if (m->fit_n != fitted_before) {
        m->last_fit_time = t;
        m->has_last_fit = 1;
    }
    m->in_a_row = (restarted || m->misses) ? 0 : m->in_a_row + (m->fit_n == fitted_before + 1);
    if (m->misses >= m->miss_limit && recent_consistent(m)) {
        ++m->jumps;                        /* the beats moved to a new grid */
        restart(m);
        m->in_a_row = m->lock_beats;
        m->last_fit_time = t;
        m->has_last_fit = 1;
    }
    if (m->mode == JB_STOPPED) {
        m->stuck = restarted ? 0 : m->stuck + 1;
        if (m->in_a_row >= m->lock_beats) {
            set_mode(m, JB_STEADY);
        } else if (m->stuck >= STUCK_BEATS) {
            m->stuck = 0;
            restart(m);                    /* no lock after STUCK_BEATS beats: the grid is wrong, reseed */
        }
    }
}

int jb_mode_bpm_at(struct jb_mode *m, double now, double *bpm)
{
    if (jb_mode_at(m, now) == JB_STOPPED)
        return 0;
    double p = period_now(m);
    if (!(p > MIN_PERIOD))
        return 0;
    *bpm = 60.0 / p;
    return 1;
}

/* ---- for the follower (jb_follow.c): LockedBpm's pieces it calls ---- */
void jb_dq_push(struct jb_dq *q, double t) { dq_push(q, t); }

double jb_short_period(const double *b, int n) { return short_period(b, n); }

void jb_mode_restart(struct jb_mode *m) { restart(m); }

int jb_mode_recent_consistent(const struct jb_mode *m) { return recent_consistent(m); }

double jb_mode_period(const struct jb_mode *m) { return period_now(m); }

int jb_mode_line(const struct jb_mode *m, double *t_beat, double *period)       /* LockedBpm.grid */
{
    double t0, p;
    if (!fit_line(m, &t0, &p) || !m->has_t_last)
        return 0;
    *t_beat = t0 + p * m->k_last;
    *period = p;
    return 1;
}

int jb_mode_grid(const struct jb_mode *m, double *t_beat, double *period)
{
    if (is_ramp(m) && m->has_ramp_period && m->ramp_period != 0.0) {
        *t_beat = m->ramp_last;
        *period = m->ramp_period;
        return 1;
    }
    return jb_mode_line(m, t_beat, period);
}
