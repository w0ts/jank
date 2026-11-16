/* SPDX-License-Identifier: GPL-3.0-only */
/* LOW CONFIDENCE: the grid-fit confidence and its gate (jank_conf.h). */
#include "jank_conf.h"
#include <math.h>
#include <string.h>

#define JCF_SLOTS_MAX 4096              /* a period under 2 ms (no tracker gives one): not a grid, confidence 0 */

/* the first index whose beat is >= x (Python's bisect_left) */
static size_t lower_bound(const double *beats, size_t n, double x)
{
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        if (beats[mid] < x)
            lo = mid + 1u;
        else
            hi = mid;
    }
    return lo;
}

/* slot k of the grid, computed as numpy does (t0 + period * k, the product rounded first: two statements, so no
 * compiler fuses them into one multiply-add) */
static double slot(double t0, double period, int64_t k)
{
    double step = period * (double)k;
    return t0 + step;
}

double jcf_grid_confidence(const double *beats, size_t n, int has_grid, double t0, double period, double now,
                           double window_s, double delay_s, double tol)
{
    double lo = now - window_s, hi = now - delay_s, margin, recall, precision;
    double k0d, k1d;
    int64_t k0, k1, k;
    size_t a, b, i, hits = 0, on_grid = 0;
    if (!has_grid || !(period > 0.0))
        return 0.0;
    if (hi <= lo)
        return 0.0;
    k0d = ceil((lo - t0) / period);
    k1d = floor((hi - t0) / period);
    if (!(k1d >= k0d))
        return 0.0;                     /* no slot in the window */
    if (k1d - k0d + 1.0 > (double)JCF_SLOTS_MAX)
        return 0.0;
    k0 = (int64_t)k0d;
    k1 = (int64_t)k1d;
    margin = tol * period;
    a = lower_bound(beats, n, lo - margin);
    b = lower_bound(beats, n, hi + margin);
    if (b <= a)
        return 0.0;
    /* Both sorted: a merge. The same test as the Python's (|beat - slot| <= margin, as rounded), on fewer pairs: a rounded
     * difference grows with the distance, so a beat too far before a slot is too far before every later one, and past
     * the slot the first one too far ends the search. Soft doubles on the device: ~50 pairs a step, not ~700 */
    i = a;
    for (k = k0; k <= k1; k++) {        /* a slot is hit: some recent beat within the margin */
        double s = slot(t0, period, k);
        while (i < b && beats[i] < s && fabs(beats[i] - s) > margin)
            i++;
        for (size_t j = i; j < b; j++) {
            if (fabs(beats[j] - s) <= margin) {
                hits++;
                break;
            }
            if (beats[j] > s)
                break;
        }
    }
    k = k0;
    for (i = a; i < b; i++) {           /* a beat is on the grid: some slot within the margin */
        while (k <= k1 && slot(t0, period, k) < beats[i] && fabs(beats[i] - slot(t0, period, k)) > margin)
            k++;
        for (int64_t q = k; q <= k1; q++) {
            double s = slot(t0, period, q);
            if (fabs(beats[i] - s) <= margin) {
                on_grid++;
                break;
            }
            if (s > beats[i])
                break;
        }
    }
    recall = (double)hits / (double)(k1 - k0 + 1);
    precision = (double)on_grid / (double)(b - a);
    return recall + precision == 0.0 ? 0.0 : 2.0 * recall * precision / (recall + precision);
}

void jcf_init(struct jcf *c) { memset(c, 0, sizeof *c); }

int jcf_samples(struct jcf *c, uint64_t sumsq, size_t n)
{
    int was = c->silent;
    if (!n)
        return 0;
    if ((double)sumsq < JCF_QUIET_MS_INT16 * (double)n) {
        uint64_t q = (uint64_t)c->quiet + n;
        c->quiet = q > JCF_SILENCE_SAMPLES ? JCF_SILENCE_SAMPLES : (uint32_t)q;
    } else
        c->quiet = 0;
    c->silent = (uint8_t)(c->quiet >= JCF_SILENCE_SAMPLES);
    if (!c->silent || was)
        return 0;
    c->n = 0;                           /* new music after silence: start over */
    c->lines = 0;
    c->silences++;
    return 1;
}

void jcf_beat(struct jcf *c, double t)
{
    if (c->silent)
        return;
    if (c->n && t < c->t[c->n - 1u])
        return;                         /* out of order: the window's search needs them ascending */
    if (c->n == JCF_KEEP) {
        memmove(c->t, c->t + 1, sizeof c->t[0] * (JCF_KEEP - 1u));
        c->n--;
    }
    c->t[c->n++] = t;
}

double jcf_confidence(const struct jcf *c, int has_grid, double t0, double period, double now)
{
    if (c->silent)
        return 0.0;
    return jcf_grid_confidence(c->t, c->n, has_grid, t0, period, now, JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
}

/* LineLog.tick: Python's line arithmetic (the latest line at or before now, then whole periods back one subtraction at a
 * time), the new lines appended oldest first, then those older than JCF_LINES_S dropped */
void jcf_line_tick(struct jcf *c, int has_grid, double t0, double period, double now)
{
    double first, line, floor_line;
    size_t m = 0, drop;
    if (!has_grid || !(period > 0.0))
        return;
    floor_line = t0 + floor((now - t0) / period) * period;
    first = c->lines ? c->line[c->lines - 1u] + 0.5 * period : now - JCF_LINES_S;
    for (line = floor_line; line > first && line <= now && m < JCF_LINES; line -= period)
        m++;                            /* how many: the same walk as below */
    if (c->lines + m > JCF_LINES) {     /* (never at a period over 0.2 s: the trim below keeps it short) */
        drop = c->lines + m - JCF_LINES;
        memmove(c->line, c->line + drop, sizeof c->line[0] * (c->lines - drop));
        c->lines = (uint16_t)(c->lines - drop);
    }
    line = floor_line;
    for (size_t i = 0; i < m; i++, line -= period)
        c->line[c->lines + m - 1u - i] = line;
    c->lines = (uint16_t)(c->lines + m);
    for (drop = 0; drop < c->lines && c->line[drop] < now - JCF_LINES_S; drop++)
        ;
    if (drop) {
        memmove(c->line, c->line + drop, sizeof c->line[0] * (c->lines - drop));
        c->lines = (uint16_t)(c->lines - drop);
    }
}

/* the distance from x to the nearest of v[a..b) (ascending) is within margin: only the neighbours of x can be nearest
 * (a rounded difference grows with the distance) */
static int near_one(const double *v, size_t a, size_t b, double x, double margin)
{
    size_t i = a + lower_bound(v + a, b - a, x);
    return (i < b && fabs(v[i] - x) <= margin) || (i > a && fabs(v[i - 1u] - x) <= margin);
}

double jcf_lines_confidence(const double *beats, size_t n, const double *lines, size_t m, int has_period, double period,
                            double now, double window_s, double delay_s, double tol)
{
    double lo = now - window_s, hi = now - delay_s, margin, recall, precision;
    size_t la, lb, a, b, hits = 0, on = 0;
    if (!has_period || !(period > 0.0))
        return 0.0;
    la = lower_bound(lines, m, lo);     /* the lines lo <= x <= hi */
    for (lb = la; lb < m && lines[lb] <= hi; lb++)
        ;
    if (lb <= la)
        return 0.0;
    margin = tol * period;
    a = lower_bound(beats, n, lo - margin);
    b = lower_bound(beats, n, hi + margin);
    if (b <= a)
        return 0.0;
    for (size_t i = la; i < lb; i++)    /* a line is hit: some recent beat within the margin */
        hits += (size_t)near_one(beats, a, b, lines[i], margin);
    for (size_t i = a; i < b; i++)      /* a beat is on a line: some line within the margin */
        on += (size_t)near_one(lines, la, lb, beats[i], margin);
    recall = (double)hits / (double)(lb - la);
    precision = (double)on / (double)(b - a);
    return recall + precision == 0.0 ? 0.0 : 2.0 * recall * precision / (recall + precision);
}

double jcf_line_confidence(struct jcf *c, int tick, int has_grid, double t0, double period, double now)
{
    if (c->silent)
        return 0.0;
    if (tick)
        jcf_line_tick(c, has_grid, t0, period, now);
    return jcf_lines_confidence(c->t, c->n, c->line, c->lines, has_grid, period, now, JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
}

void jcf_clear_lines(struct jcf *c) { c->lines = 0; }

/* the distance from x to the nearest of v[a..b), by the neighbours of x when the slice is ascending (always, but for a
 * weak observation taken out of order), else over them all: the minimum the Python takes either way */
static double nearest(const double *v, size_t a, size_t b, int ascending, double x)
{
    double best = INFINITY;
    if (b <= a)
        return best;
    if (ascending) {
        size_t i = a + lower_bound(v + a, b - a, x);
        if (i < b)
            best = fabs(v[i] - x);
        if (i > a && fabs(v[i - 1u] - x) < best)
            best = fabs(v[i - 1u] - x);
        return best;
    }
    for (size_t i = a; i < b; i++)
        if (fabs(v[i] - x) < best)
            best = fabs(v[i] - x);
    return best;
}

static int ascending(const double *v, size_t a, size_t b)
{
    for (size_t i = a + 1u; i < b; i++)
        if (v[i] < v[i - 1u])
            return 0;
    return 1;
}

double jcf_weak_confidence(const double *strong, size_t ns, const double *tracked, size_t nt, const double *weak, size_t nw,
                           const double *lines, size_t m, int has_period, double period, double now, int min_obs,
                           double *last, uint8_t *held)
{
    double lo = now - JCF_WINDOW_S, hi = now - JCF_DELAY_S, margin, recall, precision, f;
    size_t la, lb, sa, sb, ta, tb, wa, wb, hits = 0, observed = 0, in_win = 0, on = 0;
    int s_up, t_up, w_up;
    if (held)
        *held = 0;
    if (!has_period || !(period > 0.0)) {
        *last = 0.0;
        return 0.0;
    }
    la = lower_bound(lines, m, lo);     /* the lines lo <= x <= hi */
    for (lb = la; lb < m && lines[lb] <= hi; lb++)
        ;
    if (lb <= la) {
        *last = 0.0;
        return 0.0;
    }
    sa = lower_bound(strong, ns, lo - period);
    sb = lower_bound(strong, ns, hi + period);
    ta = lower_bound(tracked, nt, lo - period);
    tb = lower_bound(tracked, nt, hi + period);
    wa = lower_bound(weak, nw, lo - period);
    wb = lower_bound(weak, nw, hi + period);
    s_up = ascending(strong, sa, sb);
    t_up = ascending(tracked, ta, tb);
    w_up = ascending(weak, wa, wb);
    margin = JCF_TOL * period;
    for (size_t i = la; i < lb; i++) {
        int hit = nearest(tracked, ta, tb, t_up, lines[i]) <= margin || nearest(weak, wa, wb, w_up, lines[i]) <= margin;
        hits += (size_t)hit;
        observed += (size_t)(hit || nearest(strong, sa, sb, s_up, lines[i]) <= 0.5 * period);
    }
    if (observed < (size_t)min_obs) {
        if (held)
            *held = 1;
        return *last;
    }
    for (size_t i = sa; i < sb; i++) {  /* a strong beat at the window's edge is judged against all the lines */
        if (strong[i] < lo || strong[i] > hi)
            continue;
        in_win++;
        on += (size_t)(nearest(lines, 0, m, 1, strong[i]) <= margin);
    }
    precision = in_win ? (double)on / (double)in_win : 1.0;
    recall = (double)hits / (double)observed;
    f = recall + precision == 0.0 ? 0.0 : 2.0 * recall * precision / (recall + precision);
    *last = f;
    return f;
}

double jcf_show_step(double shown, double conf, int held, double threshold, double dt_s)
{
    if (!held)
        return conf;
    if (shown > conf)
        shown = conf;                   /* (never above what is held) */
    if (shown <= threshold || !(dt_s > 0.0))
        return shown;
    return threshold + (shown - threshold) * exp(-dt_s * 3.0 / JCF_HOLD_EASE_S);
}

uint8_t jcf_pct(double conf)
{
    double p = floor(conf * 100.0 + 0.5);
    return (uint8_t)(p < 0.0 ? 0.0 : p > 100.0 ? 100.0 : p);
}

uint8_t jcf_gate(uint8_t gated, double conf, uint8_t lowconf, uint8_t threshold_pct)
{
    double th = (double)threshold_pct / 100.0, open;
    if (lowconf != JCF_STOP || threshold_pct == 0u)
        return 0;
    if (!gated)
        return (uint8_t)(conf < th);
    open = th + (double)JCF_HYST_PCT / 100.0;      /* as the Python adds them: 0.8 + 0.05 (= 0.8500000000000001) */
    return (uint8_t)(conf < (open < 1.0 ? open : 1.0));
}
