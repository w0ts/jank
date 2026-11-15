/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the advanced beat follow (M2i) against beat-listener's Python: golden vectors in tests/follow_ref
 * (beat-listener's scripts/follow_golden.py). Exit status: the number of failed checks.
 *   NAME.txt       the follower (core/beat/jb_follow.c) against eval/follow_bpm.py FollowBpm beat by beat (its state to
 *                  1e-9), and the line confidence (core/jank_conf.c) against live/follow.py: from the Python's grid the
 *                  same F-measure (1e-12) and the same lines; from the C follower's grid the same gates
 *   rate_NAME.txt  the spectral rate (core/beat/jb_rate.c on core/beat/jb_fe.c's frames) against live/rate.py on a
 *                  synthetic signal made here again from its description: each estimate and each event */
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_conf.h"
#include "jb_fe.h"
#include "jb_follow.h"
#include "jb_rate.h"

static int fails, checks, verbose;
static int n_follow, n_rate;
static double all_x, all_ratio, all_peak;      /* the worst over all the runs */
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define STATE_S 1e-9                   /* the glide's state: seconds (both are doubles: only the sums' order differs) */
#define EXACT 1e-12
#define BPM_NEAR 1e-6
#define RATE_NEAR 1e-5                 /* a ratio: float spectra and the uint16 ring against numpy's doubles (measured 3e-7) */
#define PEAK_NEAR 1e-4

/* the numbers after the line's tag */
static int numbers(const char *line, double *v, int n)
{
    const char *p = line + 1;
    char *end;
    for (int i = 0; i < n; i++) {
        v[i] = strtod(p, &end);
        if (end == p)
            return 0;
        p = end;
    }
    return 1;
}

struct follow_stats {
    int beats, state_bad, count_bad, queries, conf_bad, lines_bad, gate_bad, bpm_bad, gated80;
    double worst_x, worst_conf;
};

/* the state after a beat: s kf mode t p d jumps misses in_a_row manoeuvres follow_beats ratios refused */
static void check_state(const struct jb_follow *f, const double *s, struct follow_stats *st)
{
    double dx = 0.0;
    st->beats++;
    if (f->has_kf != (int)s[0] || (int)f->m.mode != (int)s[1]) {
        st->state_bad++;
        return;
    }
    if (f->has_kf)
        for (int i = 0; i < 3; i++)
            dx = fmax(dx, fabs(f->x[i] - s[2 + i]));
    if (dx > st->worst_x)
        st->worst_x = dx;
    if (dx > STATE_S)
        st->state_bad++;
    if (f->m.jumps != (int)s[5] || f->m.misses != (int)s[6] || f->m.in_a_row != (int)s[7]
        || (f->has_kf ? f->manoeuvres : -1) != (int)s[8] || f->follow_beats != (int)s[9]
        || f->ratios_taken != (uint32_t)s[10] || f->ratios_refused != (uint32_t)s[11])
        st->count_bad++;
}

/* q now has_bpm bpm has_grid t0 p conf g80 g50 lines */
static void check_query(struct jb_follow *f, struct jcf *py, struct jcf *mine, const double *q, uint8_t *g80,
                        uint8_t *g50, struct follow_stats *st)
{
    double bpm = 0.0, t0 = 0.0, p = 0.0;
    double ref = jcf_line_confidence(py, (int)q[1], (int)q[3], q[4], q[5], q[0]);
    st->queries++;
    if (fabs(ref - q[6]) > EXACT)
        st->conf_bad++;
    if (py->lines != (uint16_t)q[9])
        st->lines_bad++;
    int has_bpm = jb_follow_bpm_at(f, q[0], &bpm), has_grid = jb_follow_grid(f, &t0, &p);
    if (has_bpm != (int)q[1] || (has_bpm && fabs(bpm - q[2]) > BPM_NEAR))
        st->bpm_bad++;
    double conf = jcf_line_confidence(mine, has_bpm, has_grid, t0, p, q[0]);
    if (fabs(conf - q[6]) > st->worst_conf)
        st->worst_conf = fabs(conf - q[6]);
    *g80 = jcf_gate(*g80, conf, JCF_STOP, 80);
    *g50 = jcf_gate(*g50, conf, JCF_STOP, 50);
    if (*g80 != (uint8_t)q[7] || *g50 != (uint8_t)q[8])
        st->gate_bad++;
    st->gated80 += *g80;
}

static void test_follow_run(const char *dir, const char *name)
{
    char path[512], line[512];
    static struct jb_follow f;
    static struct jcf py, mine;
    struct follow_stats st = {0};
    uint8_t g80 = 0, g50 = 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *fp = fopen(path, "r");
    CHECK(fp != NULL);
    if (!fp)
        return;
    jb_follow_init(&f);
    jcf_init(&py);
    jcf_init(&mine);
    while (fgets(line, sizeof line, fp)) {
        double v[12];
        if (line[0] == 'b' && numbers(line, v, 1)) {
            jb_follow_update(&f, v[0]);
            jcf_beat(&py, v[0]);
            jcf_beat(&mine, v[0]);
        } else if (line[0] == 's') {
            CHECK(numbers(line, v, 12));
            check_state(&f, v, &st);
        } else if (line[0] == 'r') {
            CHECK(numbers(line, v, 3));
            jb_follow_propose(&f, v[0], v[1], !isnan(v[2]), v[2]);
        } else if (line[0] == 'q') {
            CHECK(numbers(line, v, 10));
            check_query(&f, &py, &mine, v, &g80, &g50, &st);
        }
    }
    fclose(fp);
    if (verbose)
        printf("%s: %d beats (glide worst %.2g s), %d queries (line conf worst %.2g from the C grid), %d gated at 80 %%; "
               "%u manoeuvres, %u ratios taken, %u refused, %d jumps\n", name, st.beats, st.worst_x, st.queries,
               st.worst_conf, st.gated80, (unsigned)f.manoeuvres_total, (unsigned)f.ratios_taken,
               (unsigned)f.ratios_refused, f.m.jumps);
    n_follow++;
    all_x = fmax(all_x, st.worst_x);
    CHECK(st.beats > 40 && st.queries > 100);
    CHECK(st.state_bad == 0);
    CHECK(st.count_bad == 0);
    CHECK(st.conf_bad == 0);           /* the Python's grid in: the same lines and the same F-measure out */
    CHECK(st.lines_bad == 0);
    CHECK(st.bpm_bad == 0);
    CHECK(st.gate_bad == 0);           /* the C follower's grid: the same gate decisions */
}

/* ---- the rate ---- */

struct partial {
    double f, a;
};

#define MAX_PARTIALS 16
#define MAX_EST 256

struct rate_case {
    double dur, t0, ratio, ramp_s, tchord;
    int step, every;
    struct partial p[2][MAX_PARTIALS];
    int np[2];
    double est[MAX_EST][3], ev[8][2];
    int n_est, n_ev;
};

/* follow_golden.py synth(), term by term */
static float synth(const struct rate_case *c, long n)
{
    double t = (double)n / 44100.0, g, u = t - c->t0;
    if (c->ramp_s > 0.0)
        g = t < c->t0 ? 0.0 : t < c->t0 + c->ramp_s ? u * u / (2.0 * c->ramp_s) : c->ramp_s / 2.0 + (u - c->ramp_s);
    else
        g = t < c->t0 ? 0.0 : t - c->t0;
    double tau = t + (c->ratio - 1.0) * g;
    double env = 0.25 + 0.75 * exp(-fmod(tau, 0.46875) / 0.06);
    int set = t < c->tchord ? 0 : 1;
    double s = 0.0;
    for (int i = 0; i < c->np[set]; i++) {
        double w = 2.0 * M_PI * c->p[set][i].f;
        s = s + c->p[set][i].a * sin(w * tau + i);
    }
    return (float)(env * s);
}

static int read_rate(const char *path, struct rate_case *c)
{
    char line[256];
    double v[7];
    FILE *fp = fopen(path, "r");
    if (!fp)
        return 0;
    memset(c, 0, sizeof *c);
    while (fgets(line, sizeof line, fp)) {
        if (!strncmp(line, "case", 4)) {
            const char *p = line + 4;
            char *end;
            for (int i = 0; i < 7; i++) {
                v[i] = strtod(p, &end);
                p = end;
            }
            c->dur = v[0], c->t0 = v[1], c->ratio = v[2], c->ramp_s = v[3], c->tchord = v[4], c->step = (int)v[5];
            c->every = (int)v[6];
        } else if ((line[0] == 'p' || line[0] == 'q') && numbers(line, v, 2)) {
            int set = line[0] == 'q';
            if (c->np[set] < MAX_PARTIALS)
                c->p[set][c->np[set]++] = (struct partial){v[0], v[1]};
        } else if (line[0] == 'e' && numbers(line, v, 3) && c->n_est < MAX_EST) {
            memcpy(c->est[c->n_est++], v, sizeof(double) * 3);
        } else if (line[0] == 'v' && numbers(line, v, 2) && c->n_ev < 8) {
            memcpy(c->ev[c->n_ev++], v, sizeof(double) * 2);
        }
    }
    fclose(fp);
    return c->dur > 0.0;
}

struct rate_ctx {
    struct jb_rate r;
    double est[MAX_EST][3], ev[8][2];
    int n_est, n_ev;
    uint32_t seen;
};

static void rate_tap(void *ctx, int64_t index, const float *mag)
{
    struct rate_ctx *x = ctx;
    double t, ratio;
    jb_rate_frame(&x->r, index, mag);
    if (x->r.estimates != x->seen && x->n_est < MAX_EST) {
        x->seen = x->r.estimates;
        x->est[x->n_est][0] = (double)(index + 1) / 100.0;
        x->est[x->n_est][1] = x->r.est_ratio;
        x->est[x->n_est++][2] = x->r.est_peak;
    }
    if (jb_rate_take(&x->r, &t, &ratio) && x->n_ev < 8) {
        x->ev[x->n_ev][0] = t;
        x->ev[x->n_ev++][1] = ratio;
    }
}

static void test_rate_run(const char *dir, const char *name)
{
    char path[512];
    static struct rate_case c;
    static struct rate_ctx x;
    static struct jr_store store;
    static struct jb_fe fe;
    static float feat[BEAT_INPUTS], buf[1000];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    CHECK(read_rate(path, &c));
    memset(&x, 0, sizeof x);
    jb_rate_init(&x.r, &store, c.step, c.every);
    jb_rate_set_active(&x.r, 1);
    jb_fe_init(&fe);
    fe.tap = rate_tap;
    fe.tap_ctx = &x;
    long total = (long)(c.dur * 44100.0), n = 0;
    while (n < total) {
        size_t m = (size_t)(total - n < 1000 ? total - n : 1000), off = 0;
        for (size_t i = 0; i < m; i++)
            buf[i] = synth(&c, n + (long)i);
        while (off < m) {
            int ready;
            off += jb_fe_push(&fe, buf + off, m - off, feat, &ready);
        }
        n += (long)m;
    }
    double worst = 0.0, worst_peak = 0.0;
    int matched = 0;
    for (int i = 0, j = 0; i < c.n_est && j < x.n_est;) {
        if (fabs(c.est[i][0] - x.est[j][0]) < 1e-9) {
            worst = fmax(worst, fabs(c.est[i][1] - x.est[j][1]));
            worst_peak = fmax(worst_peak, fabs(c.est[i][2] - x.est[j][2]));
            matched++;
            i++, j++;
        } else if (c.est[i][0] < x.est[j][0])
            i++;
        else
            j++;
    }
    if (verbose) {
        printf("%s: %d estimates (Python %d), ratio worst %.2g, peak worst %.2g; events %d (Python %d)", name, x.n_est,
               c.n_est, worst, worst_peak, x.n_ev, c.n_ev);
        for (int i = 0; i < x.n_ev; i++)
            printf(" %.2f s x%.5f", x.ev[i][0], x.ev[i][1]);
        printf("\n");
    }
    n_rate++;
    all_ratio = fmax(all_ratio, worst);
    all_peak = fmax(all_peak, worst_peak);
    CHECK(c.n_est > 20);
    CHECK(matched == c.n_est && x.n_est == c.n_est);   /* the same estimates at the same times (refractory included) */
    CHECK(worst <= RATE_NEAR);
    CHECK(worst_peak <= PEAK_NEAR);
    CHECK(x.n_ev == c.n_ev);
    for (int i = 0; i < x.n_ev && i < c.n_ev; i++)
        CHECK(fabs(x.ev[i][0] - c.ev[i][0]) < 1e-9 && fabs(x.ev[i][1] - c.ev[i][1]) <= RATE_NEAR);
}

static void test_runs(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int nf = 0, nr = 0;
    CHECK(d != NULL);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 4, ".txt"))
            continue;
        if (!strncmp(e->d_name, "rate_", 5)) {
            test_rate_run(dir, e->d_name);
            nr++;
        } else {
            test_follow_run(dir, e->d_name);
            nf++;
        }
    }
    closedir(d);
    CHECK(nf >= 20 && nr >= 10);
}

/* ---- units ---- */

/* the Python's all-pairs line_confidence, as written there, against the module's neighbour search */
static double brute_lines(const double *beats, size_t n, const double *lines, size_t m, double period, double now)
{
    double lo = now - JCF_WINDOW_S, hi = now - JCF_DELAY_S, margin = JCF_TOL * period, r, p;
    size_t a = 0, b = 0, hits = 0, on = 0, nl = 0;
    while (a < n && beats[a] < lo - margin)
        a++;
    while (b < n && beats[b] < hi + margin)
        b++;
    for (size_t k = 0; k < m; k++)
        nl += lines[k] >= lo && lines[k] <= hi;
    if (!nl || b <= a)
        return 0.0;
    for (size_t k = 0; k < m; k++) {
        if (lines[k] < lo || lines[k] > hi)
            continue;
        double best = INFINITY;
        for (size_t i = a; i < b; i++)
            best = fmin(best, fabs(beats[i] - lines[k]));
        hits += best <= margin;
    }
    for (size_t i = a; i < b; i++) {
        double best = INFINITY;
        for (size_t k = 0; k < m; k++)
            if (lines[k] >= lo && lines[k] <= hi)
                best = fmin(best, fabs(beats[i] - lines[k]));
        on += best <= margin;
    }
    r = (double)hits / (double)nl;
    p = (double)on / (double)(b - a);
    return r + p == 0.0 ? 0.0 : 2.0 * r * p / (r + p);
}

static void test_lines_exact(void)
{
    unsigned seed = 777u;
    int bad = 0, mid = 0;
    for (int trial = 0; trial < 20000; trial++) {
        double beats[JCF_KEEP], lines[JCF_LINES], period, t = 0.0, l;
        size_t n = 0, m = 0;
        seed = seed * 1103515245u + 12345u;
        period = 0.3 + (double)(seed >> 8 & 1023u) / 2048.0;
        seed = seed * 1103515245u + 12345u;
        l = (double)(seed >> 8 & 1023u) / 512.0;
        while (m < JCF_LINES && l < 30.0) {          /* lines that wander a little, as a follower's */
            lines[m++] = l;
            seed = seed * 1103515245u + 12345u;
            l += period * (0.97 + (double)(seed >> 8 & 255u) / 4096.0);
        }
        while (n < JCF_KEEP) {
            seed = seed * 1103515245u + 12345u;
            switch (seed >> 28 & 3u) {
            case 0: t += period; break;
            case 1: t += period * (1.0 + JCF_TOL); break;
            case 2: t += period * 0.5; break;
            default: t += (double)(seed >> 8 & 255u) / 256.0; break;
            }
            beats[n++] = t;
        }
        seed = seed * 1103515245u + 12345u;
        double now = 8.0 + (double)(seed >> 8 & 4095u) / 4096.0 * 22.0;
        double f = jcf_lines_confidence(beats, n, lines, m, 1, period, now, JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
        mid += f > 0.0 && f < 1.0;
        bad += f != brute_lines(beats, n, lines, m, period, now);
    }
    CHECK(bad == 0 && mid > 10000);
}

/* a grid that moves a little does not double its lines; the oldest go after 10 s; silence clears them; no grid, no tick */
static void test_line_log(void)
{
    static struct jcf c;
    jcf_init(&c);
    for (int k = 1; k < 1000; k++)
        jcf_line_tick(&c, 1, 0.004 * (k % 2), 0.5, 0.01 * k);
    CHECK(c.lines >= 19 && c.lines <= 21);
    for (int i = 1; i < c.lines; i++)
        CHECK(c.line[i] - c.line[i - 1] > 0.4);
    CHECK(c.line[0] >= 9.99 - JCF_LINES_S);
    uint16_t before = c.lines;
    jcf_line_tick(&c, 0, 0.0, 0.5, 20.0);
    CHECK(c.lines == before);
    CHECK(jcf_line_confidence(&c, 1, 1, 0.0, 0.0, 10.0) == 0.0);     /* period 0: no confidence */
    for (int i = 0; i < 60; i++)
        jcf_samples(&c, 0, 441);
    CHECK(c.silent && c.lines == 0 && jcf_line_confidence(&c, 1, 1, 0.0, 0.5, 11.0) == 0.0 && c.lines == 0);
    jcf_init(&c);
    jcf_line_tick(&c, 1, 0.0, 0.2, 100.0);           /* the shortest period fills an empty log back 10 s: 51 lines */
    CHECK(c.lines >= 50 && c.lines <= 51 && fabs(c.line[c.lines - 1] - 100.0) < 1e-9 && c.line[0] > 90.0 - 1e-9);
}

/* the rate's log: within 3e-6 of log1p over the spectrum's range; the bins sit where the Python puts them */
static void test_rate_bins(void)
{
    static float mag[JR_MAG_BINS + 1];
    static struct jr_store store;
    struct jb_rate r;
    double worst = 0.0;
    jb_rate_init(&r, &store, JR_STEP, JR_EVERY);
    CHECK(r.step == 2 && r.every == 2);
    jb_rate_set_active(&r, 1);
    CHECK(r.active);
    for (double v = 0.0; v < 3000.0; v = v * 1.01 + 1e-4) {
        for (int i = 0; i <= JR_MAG_BINS; i++)
            mag[i] = (float)v;
        for (int i = 0; i < JR_BINS; i += 37)
            worst = fmax(worst, fabs(jb_rate_bin(&r, mag, i) - log1p((double)(float)v)));
    }
    printf("rate: log1p worst %.2g\n", worst);
    CHECK(worst < 3e-6);
    for (int i = 0; i <= JR_MAG_BINS; i++)
        mag[i] = (float)i;           /* a ramp: the bin's value is log1p of its place */
    for (int i = 0; i < JR_BINS; i += 11) {
        double pos = 300.0 * pow(2.0, i / 96.0) / (44100.0 / 2048.0);
        CHECK(pos + 1.0 < JR_MAG_BINS);
        CHECK(fabs(jb_rate_bin(&r, mag, i) - log1p(pos)) < 4e-6);
    }
    struct jb_rate none;                 /* no store (no PSRAM on the device): never active */
    jb_rate_init(&none, NULL, 2, 2);
    jb_rate_set_active(&none, 1);
    CHECK(!none.active);
    jb_rate_frame(&none, 9, mag);
    double t, ratio;
    CHECK(!jb_rate_take(&none, &t, &ratio));
}

/* the follower alone: the glide takes a step without a stop; a ratio needs a trusted glide and the next beat's backing */
static void test_follow_units(void)
{
    static struct jb_follow f;
    double t = 0.0, bpm, t0, p;
    jb_follow_init(&f);
    CHECK(jb_follow_state(&f, 0.0) == JF_STOPPED && !jb_follow_grid(&f, &t0, &p) && jb_follow_drift_ppm(&f) == 0);
    for (int k = 0; k < 160; k++)
        jb_follow_update(&f, t += 60.0 / 135.0);
    CHECK(jb_follow_state(&f, t) == JF_GLIDE && jb_follow_trusted(&f));
    jb_follow_propose(&f, 1.03, t + 0.1, 1, t + 0.05);
    jb_follow_update(&f, t += 60.0 / (135.0 * 1.03));
    CHECK(f.ratios_taken == 1 && f.last_ratio == 1.03);
    for (int k = 0; k < 20; k++)
        jb_follow_update(&f, t += 60.0 / (135.0 * 1.03));
    CHECK(jb_follow_bpm_at(&f, t, &bpm) && fabs(bpm - 135.0 * 1.03) < 0.05);
    jb_follow_init(&f);
    for (int k = 0; k < 10; k++)
        jb_follow_update(&f, t += 0.5);
    jb_follow_propose(&f, 1.03, t + 0.1, 0, 0.0);
    for (int k = 0; k < 3; k++)
        jb_follow_update(&f, t += 0.5);
    CHECK(f.has_kf && !jb_follow_trusted(&f));      /* a fresh glide: not trusted yet, the ratio is not taken */
    CHECK(f.ratios_taken == 0 && f.ratios_ignored == 1 && f.has_pending == 0);
    CHECK(jb_follow_at(&f, t + 31.0) == JB_STOPPED && jb_follow_state(&f, t + 31.0) == JF_STOPPED);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "tests/follow_ref";
    verbose = argc > 2 && !strcmp(argv[2], "-v");
    printf("sizeof struct jb_follow %zu, struct jb_rate %zu, struct jr_store %zu, struct jcf %zu\n", sizeof(struct jb_follow),
           sizeof(struct jb_rate), sizeof(struct jr_store), sizeof(struct jcf));
    test_lines_exact();
    test_line_log();
    test_rate_bins();
    test_follow_units();
    test_runs(dir);
    printf("follow: %d runs against FollowBpm + line_confidence (glide worst %.2g s, the Python's lines and gates exact); "
           "%d rate runs against RateProbe (ratio worst %.2g, peak worst %.2g)\n", n_follow, all_x, n_rate, all_ratio,
           all_peak);
    printf("follow: %d checks, %d failed\n", checks, fails);
    return fails;
}
