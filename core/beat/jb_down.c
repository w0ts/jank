/* SPDX-License-Identifier: GPL-3.0-only */
/* The downbeat estimator (jb_down.h), line for line beat-listener's live/downbeat.py. */
#include "jb_down.h"
#include <math.h>
#include <string.h>

#define LOW150 5
#define LOW100 3
static const uint8_t GROUPS[6][2] = {{0, 5}, {5, 17}, {17, 33}, {33, 52}, {52, 69}, {69, 81}};

static void reset_count(struct jb_down *d)
{
    d->n = 0;
    d->c_last = 0;
    memset(d->rec_count, 0, sizeof d->rec_count);
    memset(d->rec_time, 0, sizeof d->rec_time);
    memset(d->rec_seg, 0, sizeof d->rec_seg);
    memset(d->rec_frame, 0, sizeof d->rec_frame);
    memset(d->rec_peak_ok, 0, sizeof d->rec_peak_ok);
    memset(d->rec_peak, 0, sizeof d->rec_peak);
    memset(d->acc, 0, sizeof d->acc);
    memset(d->acc_n, 0, sizeof d->acc_n);
    d->state = d->auto_on ? JD_LISTENING : JD_OFF;
    d->phi = d->cand = -1;
    d->run = 0;
    d->has_scores = 0;
    d->lead = 0.0f;
    d->dec_count = 0;
    d->dec_time = 0.0;
}

void jd_init(struct jb_down *d, jd_seg_row *seg)
{
    memset(d, 0, sizeof *d);
    d->seg = seg;
    memset(seg, 0, sizeof(jd_seg_row) * JD_SEGS);
    d->first_frame = -1;
    for (int i = 0; i < JD_SEGS; i++)
        d->seg_start[i] = -1;
    d->auto_on = 1;
    reset_count(d);
}

void jd_reset(struct jb_down *d) { reset_count(d); }

void jd_set_auto(struct jb_down *d, int on)
{
    d->auto_on = (uint8_t)(on != 0);
    if (!d->auto_on)
        d->state = JD_OFF;
    else if (d->state == JD_OFF || d->state == JD_MANUAL)
        d->state = JD_LISTENING;
    else
        return;                        /* listening or locked already: nothing changes */
    d->phi = d->cand = -1;
    d->run = 0;
}

void jd_nudge(struct jb_down *d)
{
    if (!d->auto_on)
        return;
    d->state = JD_MANUAL;
    d->phi = d->cand = -1;
    d->run = 0;
}

int jd_counting(const struct jb_down *d) { return d->running || d->n > 0; }
int jd_playing(const struct jb_down *d) { return d->has_grid; }

static int64_t frame_at(double t) { return (int64_t)floor(t * JD_FPS + 0.5); }

int jd_grid(struct jb_down *d, double t, double tb, double period, int running)
{
    double k;
    if (!(period > 0.0))
        return -1;
    d->has_grid = 1;
    d->running = (uint8_t)(running != 0);
    d->tb = tb;
    d->period = period;
    k = ceil((t - tb) / period - 1e-9);
    while (d->has_last && tb + k * period <= d->b_last + 0.5 * period)
        k += 1.0;
    d->k = (int64_t)k;
    d->next = tb + k * period;
    d->next_frame = frame_at(d->next);
    return 0;
}

void jd_stop(struct jb_down *d)
{
    d->has_grid = d->running = d->open = 0;
    if (d->n)
        reset_count(d);
}

/* the 10 curves of a frame: e150, e100, flux150, flux_all, flux_g0..5 */
static void curves(const struct jb_down *d, const float *L, float *out)
{
    float lin150 = 0.0f, lin100 = 0.0f, all = 0.0f, g[6] = {0};
    for (int b = 0; b < LOW150; b++) {
        float v = powf(10.0f, L[b]) - 1.0f;
        lin150 += v;
        if (b < LOW100)
            lin100 += v;
    }
    out[0] = log10f(1.0f + lin150);
    out[1] = log10f(1.0f + lin100);
    if (d->have_prev) {
        for (int k = 0; k < 6; k++)
            for (int b = GROUPS[k][0]; b < GROUPS[k][1]; b++) {
                float p = L[b] - d->prev[b];
                if (p > 0.0f)
                    g[k] += p;
            }
        for (int k = 0; k < 6; k++)
            all += g[k];
    }
    out[2] = g[0];                     /* bands 0-4 are group 0 */
    out[3] = all;
    for (int k = 0; k < 6; k++)
        out[4 + k] = g[k];
}

static void peaks(const struct jb_down *d, int64_t index, float *pk)
{
    int64_t lo = index - JD_POST - JD_PRE;
    float mx[JD_CURVES], sum = 0.0f;
    int n = 0;
    if (lo < d->first_frame)
        lo = d->first_frame;
    for (int c = 0; c < JD_CURVES; c++)
        mx[c] = -INFINITY;
    for (int64_t f = lo; f <= index; f++) {
        const float *r = d->ring[f % JD_RING];
        for (int c = 0; c < JD_CURVES; c++)
            if (r[c] > mx[c])
                mx[c] = r[c];
        sum += r[0];
        n++;
    }
    pk[0] = mx[0];
    pk[1] = mx[1];
    pk[2] = sum / (float)n;
    pk[3] = mx[2];
    pk[4] = mx[3];
    for (int k = 0; k < 6; k++)
        pk[5 + k] = mx[4 + k];
}

/* segment m ends where m + 1 starts, both still in the ring */
static int joined(const struct jb_down *d, int64_t m)
{
    int a, b;
    if (m < 0 || m < (int64_t)d->sn - JD_SEGS)
        return 0;
    a = (int)(m % JD_SEGS);
    b = (int)((m + 1) % JD_SEGS);
    return d->seg_start[a] >= 0 && d->seg_start[a] + d->seg_n[a] == d->seg_start[b];
}

/* the mean bands of segments m0 .. m0 + n - 1 */
static void seg_mean(const struct jb_down *d, int64_t m0, int n, float *out)
{
    int32_t frames = 0;
    for (int b = 0; b < JD_BANDS; b++)
        out[b] = 0.0f;
    for (int i = 0; i < n; i++) {
        int s = (int)((m0 + i) % JD_SEGS);
        for (int b = 0; b < JD_BANDS; b++)
            out[b] += d->seg[s][b];
        frames += d->seg_n[s];
    }
    if (frames < 1)
        frames = 1;
    for (int b = 0; b < JD_BANDS; b++)
        out[b] /= (float)frames;
}

/* the 22 features of the counted beat whose segment is m (segments m .. m + 3 closed) */
static void features(const struct jb_down *d, int64_t m, const float *pk, float *x)
{
    float after[JD_BANDS], other[JD_BANDS];
    float nov[3] = {0}, nb[2] = {0};
    int nbefore = 0;
    seg_mean(d, m, 1, after);
    if (joined(d, m - 1)) {
        seg_mean(d, m - 1, 1, other);
        for (int b = 0; b < JD_BANDS; b++) {
            float dm = after[b] - other[b];
            nov[0] += fabsf(dm);
            if (b < LOW150)
                nov[1] += fabsf(dm);
            if (dm > 0.0f)
                nov[2] += dm;
        }
    }
    x[0] = pk[0];
    x[1] = pk[1];
    x[2] = pk[2];
    x[3] = pk[3];
    x[4] = pk[4];
    x[5] = nov[0];
    x[6] = nov[1];
    x[7] = nov[2];
    for (int k = 0; k < 6; k++)
        x[8 + k] = pk[5 + k];
    for (int k = 0; k < 6; k++) {
        float s = 0.0f;
        for (int b = GROUPS[k][0]; b < GROUPS[k][1]; b++)
            s += after[b];
        x[14 + k] = s / (float)(GROUPS[k][1] - GROUPS[k][0]);
    }
    while (nbefore < 4 && joined(d, m - 1 - nbefore))
        nbefore++;
    if (nbefore) {
        seg_mean(d, m, 4, after);
        seg_mean(d, m - nbefore, nbefore, other);
        for (int b = 0; b < JD_BANDS; b++) {
            float db = fabsf(after[b] - other[b]);
            nb[0] += db;
            if (b < LOW150)
                nb[1] += db;
        }
    }
    x[20] = nb[0];
    x[21] = nb[1];
}

static void enter(struct jb_down *d, int32_t count, const float *x)
{
    int q = (int)(((count % 4) + 4) % 4);
    const float decay = expf(-1.0f / DOWN_BARS);
    for (int f = 0; f < JD_NF; f++)
        d->acc[f][q] = x[f] + decay * d->acc[f][q];
    d->acc_n[q]++;
}

/* the 4 phases' scores; 0 until every bar position has a beat */
static int score(const struct jb_down *d, float *s)
{
    float rel[JD_NF][4];
    for (int q = 0; q < 4; q++)
        if (!d->acc_n[q])
            return 0;
    for (int f = 0; f < JD_NF; f++) {
        float m = (d->acc[f][0] + d->acc[f][1] + d->acc[f][2] + d->acc[f][3]) / 4.0f;
        for (int q = 0; q < 4; q++)
            rel[f][q] = (d->acc[f][q] - m) / (fabsf(m) + 1e-6f);
    }
    for (int r = 0; r < 4; r++) {
        float t = 0.0f;
        for (int f = 0; f < JD_NF; f++)
            for (int p = 0; p < 4; p++)
                t += down_w[f * 4 + p] * rel[f][(r + p) % 4];
        s[r] = t;
    }
    return 1;
}

static float best_other(const float *s, int skip)
{
    float m = -INFINITY;
    for (int q = 0; q < 4; q++)
        if (q != skip && s[q] > m)
            m = s[q];
    return m;
}

static void lock(struct jb_down *d, int phi)
{
    d->state = JD_LOCKED;
    d->phi = (int8_t)phi;
    d->cand = -1;
    d->run = 0;
    d->moves++;
    d->lead = d->scores[phi] - best_other(d->scores, phi);
}

static void decide(struct jb_down *d, int32_t count, double t)
{
    int best = 0;
    float *s = d->scores;
    d->seq++;
    d->dec_count = count;
    d->dec_time = t;
    d->has_scores = (uint8_t)score(d, s);
    if (!d->has_scores) {
        d->lead = 0.0f;
        return;
    }
    for (int q = 1; q < 4; q++)
        if (s[q] > s[best])
            best = q;
    d->lead = d->state == JD_LOCKED ? s[d->phi] - best_other(s, d->phi) : s[best] - best_other(s, best);
    if (d->state == JD_LISTENING) {
        if (d->lead >= DOWN_COMMIT)
            lock(d, best);
    } else if (d->state == JD_LOCKED) {
        if (best != d->phi && s[best] - s[d->phi] >= DOWN_SWITCH) {
            d->run = best == d->cand ? d->run + 1 : 1;
            d->cand = (int8_t)best;
            if (d->run >= DOWN_HOLD)
                lock(d, best);
        } else {
            d->cand = -1;
            d->run = 0;
        }
    }
}

/* a grid beat at frame index: a segment opens; counted: a decision, then beat i - 4 enters */
static void beat(struct jb_down *d, int64_t index)
{
    double b = d->next, p = d->period;
    int64_t gap = 0;
    int s;
    uint32_t i;
    int r;
    if (d->has_last) {
        gap = (int64_t)floor((b - d->b_last) / p + 0.5);
        if (gap < 1)
            gap = 1;
    }
    d->b_last = b;
    d->k++;
    d->next = d->tb + (double)d->k * p;
    d->next_frame = frame_at(d->next);
    s = (int)(d->sn % JD_SEGS);
    memset(d->seg[s], 0, sizeof(jd_seg_row));
    d->seg_n[s] = 0;
    d->seg_start[s] = index;
    d->sn++;
    d->open = 1;
    if (!d->running) {
        d->has_last = 1;
        return;                        /* a shadow beat: segments only */
    }
    d->c_last = d->n == 0 || !d->has_last ? 0 : d->c_last + (int32_t)gap;
    d->has_last = 1;
    i = d->n++;
    r = (int)(i % JD_BEATS);
    d->rec_count[r] = d->c_last;
    d->rec_time[r] = b;
    d->rec_seg[r] = d->sn - 1;
    d->rec_frame[r] = index;
    d->rec_peak_ok[r] = 0;
    decide(d, d->c_last, b);
    if (i >= JD_DELAY) {
        int rj = (int)((i - JD_DELAY) % JD_BEATS);
        features(d, d->rec_seg[rj], d->rec_peak[rj], d->x);
        enter(d, d->rec_count[rj], d->x);
        d->entered++;
        d->x_count = d->rec_count[rj];
        d->x_time = d->rec_time[rj];
    }
}

int jd_frame(struct jb_down *d, int64_t index, const float *bands)
{
    int decided = 0;
    uint32_t seq = d->seq;
    if (d->first_frame < 0)
        d->first_frame = index;
    curves(d, bands, d->ring[index % JD_RING]);
    memcpy(d->prev, bands, sizeof d->prev);
    d->have_prev = 1;
    if (d->has_grid && d->next_frame <= index)
        beat(d, index);
    if (d->open) {
        int s = (int)((d->sn - 1) % JD_SEGS);
        for (int b = 0; b < JD_BANDS; b++)
            d->seg[s][b] += bands[b];
        d->seg_n[s]++;
    }
    for (uint32_t j = d->n >= 2 ? d->n - 2 : 0; j < d->n; j++) {     /* a beat's peak window ends JD_POST frames after it */
        int r = (int)(j % JD_BEATS);
        if (!d->rec_peak_ok[r] && d->rec_frame[r] + JD_POST <= index) {
            peaks(d, index, d->rec_peak[r]);
            d->rec_peak_ok[r] = 1;
        }
    }
    decided = d->seq != seq;
    return decided;
}

void jd_view(const struct jb_down *d, struct jd_view *v)
{
    memset(v, 0, sizeof *v);
    v->state = d->state;
    v->phi = d->state == JD_LOCKED ? d->phi : -1;
    v->ready = d->has_scores;
    v->lead = d->lead;
    v->conf = d->has_scores ? fminf(fmaxf(d->lead / DOWN_COMMIT, 0.0f), 1.0f) : 0.0f;
    v->seq = d->seq;
    v->moves = d->moves;
    v->count = d->dec_count;
    v->t_beat = d->dec_time;
    memcpy(v->scores, d->scores, sizeof v->scores);
}
