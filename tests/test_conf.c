/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of LOW CONFIDENCE (core/jank_conf.c) against beat-listener's live/confidence.py and live/engine.py's gate:
 * golden vectors in tests/conf_ref (beat-listener's scripts/confidence_golden.py). Exit status: the number of failed
 * checks.
 *   calls.txt   direct calls of grid_confidence: the same doubles in, the same F-measure out (to 1e-12)
 *   NAME.txt    runs: the tracked beats fed to ModeBpm (core/beat/jb_mode.c here) and the confidence every 0.25 s from
 *               the Python's grid (to 1e-12) and from the C ModeBpm's grid (to 1e-4), the gate at 0.8 and 0.5 */
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_conf.h"
#include "jb_mode.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define EXACT 1e-12
#define NEAR 1e-4
#define MAX_BEATS 256

/* one "c" line: window delay tol has_grid t0 period now expected n beats... */
static int check_call(char *line)
{
    double v[8], beats[MAX_BEATS], got;
    char *p = line + 1, *end;
    long n;
    for (int i = 0; i < 8; i++) {
        v[i] = strtod(p, &end);
        if (end == p)
            return -1;
        p = end;
    }
    n = strtol(p, &end, 10);
    if (end == p || n < 0 || n > MAX_BEATS)
        return -1;
    p = end;
    for (long i = 0; i < n; i++) {
        beats[i] = strtod(p, &end);
        if (end == p)
            return -1;
        p = end;
    }
    got = jcf_grid_confidence(beats, (size_t)n, (int)v[3], v[4], v[5], v[6], v[0], v[1], v[2]);
    if (fabs(got - v[7]) > EXACT)
        printf("  call: got %.17g, Python %.17g\n", got, v[7]);
    CHECK(fabs(got - v[7]) <= EXACT);
    return 0;
}

static void test_calls(const char *dir)
{
    char path[512], line[16384];
    int n = 0;
    snprintf(path, sizeof path, "%s/calls.txt", dir);
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    if (!f)
        return;
    while (fgets(line, sizeof line, f))
        if (line[0] == 'c') {
            CHECK(check_call(line) == 0);
            n++;
        }
    fclose(f);
    CHECK(n >= 50);
    printf("calls: %d against the Python\n", n);
}

struct run_stats {
    int queries, exact_bad, near_bad, grid_diff, gate_diff, gated80;
    double worst;
};

/* one query: the confidence from the Python's grid (exact) and from the C ModeBpm's (near), and the gates */
static void query(const struct jcf *c, struct jb_mode *m, const double *q, uint8_t *g80, uint8_t *g50, struct run_stats *s)
{
    double t0, period, mine, diff;
    int has = jb_mode_grid(m, &t0, &period);
    double ref = jcf_confidence(c, (int)q[1], q[2], q[3], q[0]);
    s->queries++;
    if (fabs(ref - q[4]) > EXACT)
        s->exact_bad++;
    if (has != (int)q[1] || (has && (fabs(t0 - q[2]) > 1e-9 || fabs(period - q[3]) > 1e-9)))
        s->grid_diff++;
    mine = jcf_confidence(c, has, t0, period, q[0]);
    diff = fabs(mine - q[4]);
    if (diff > s->worst)
        s->worst = diff;
    if (diff > NEAR)
        s->near_bad++;
    *g80 = jcf_gate(*g80, mine, JCF_STOP, 80);
    *g50 = jcf_gate(*g50, mine, JCF_STOP, 50);
    if (*g80 != (uint8_t)q[5] || *g50 != (uint8_t)q[6])
        s->gate_diff++;
    s->gated80 += *g80;
}

static void test_run(const char *dir, const char *name)
{
    char path[512], line[256];
    struct jcf c;
    struct jb_mode m;
    struct run_stats s = {0};
    uint8_t g80 = 0, g50 = 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    if (!f)
        return;
    jcf_init(&c);
    jb_mode_init(&m);
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'b') {
            double t = strtod(line + 1, NULL);
            jb_mode_update(&m, t);
            jcf_beat(&c, t);
        } else if (line[0] == 'q') {
            double q[7];
            char *p = line + 1, *end;
            int ok = 1;
            for (int i = 0; i < 7 && ok; i++) {
                q[i] = strtod(p, &end);
                ok = end != p;
                p = end;
            }
            CHECK(ok);
            if (ok)
                query(&c, &m, q, &g80, &g50, &s);
        }
    }
    fclose(f);
    printf("%s: %d queries, worst %.2g against the Python, %d of them gated at 80 %%, grids apart %d\n", name, s.queries,
           s.worst, s.gated80, s.grid_diff);
    CHECK(s.queries > 100);
    CHECK(s.exact_bad == 0);            /* the same grid in: the same F-measure out */
    CHECK(s.grid_diff == 0);            /* the C ModeBpm's grid is the Python's (to 1e-9 s) */
    CHECK(s.near_bad == 0);
    CHECK(s.gate_diff == 0);
}

static void test_runs(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    CHECK(d != NULL);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 4, ".txt") || !strcmp(e->d_name, "calls.txt"))
            continue;
        test_run(dir, e->d_name);
        n++;
    }
    closedir(d);
    CHECK(n >= 8);
}

/* the gate: under the threshold it closes, it opens at the threshold + 5 points; KEEP RUNNING and 0 never close it */
static void test_gate(void)
{
    CHECK(jcf_gate(0, 0.79, JCF_STOP, 80) == 1);
    CHECK(jcf_gate(0, 0.80, JCF_STOP, 80) == 0);
    CHECK(jcf_gate(1, 0.80, JCF_STOP, 80) == 1);      /* in the band: stays closed */
    CHECK(jcf_gate(1, 0.84, JCF_STOP, 80) == 1);
    CHECK(jcf_gate(1, 0.85, JCF_STOP, 80) == 1);      /* 0.85 < 0.8 + 0.05 (= 0.8500000000000001), as the Python */
    CHECK(jcf_gate(1, 0.86, JCF_STOP, 80) == 0);
    CHECK(jcf_gate(0, 0.82, JCF_STOP, 80) == 0);      /* in the band: stays open */
    CHECK(jcf_gate(1, 0.99, JCF_STOP, 100) == 1 && jcf_gate(1, 1.0, JCF_STOP, 100) == 0);   /* capped at 100 % */
    CHECK(jcf_gate(1, 0.0, JCF_RUN, 80) == 0 && jcf_gate(0, 0.0, JCF_RUN, 80) == 0);
    CHECK(jcf_gate(1, 0.0, JCF_STOP, 0) == 0);
    CHECK(jcf_pct(0.0) == 0 && jcf_pct(0.844) == 84 && jcf_pct(0.845) == 85 && jcf_pct(1.0) == 100 && jcf_pct(1.5) == 100);
}

/* a grid that fits: 1.0; a beat out of order is dropped; the ring keeps the newest JCF_KEEP */
static void test_ring(void)
{
    struct jcf c;
    jcf_init(&c);
    for (int k = 0; k < 200; k++)
        jcf_beat(&c, 0.5 * k);
    CHECK(c.n == JCF_KEEP && c.t[JCF_KEEP - 1] == 99.5 && c.t[0] == 0.5 * (200 - JCF_KEEP));
    CHECK(jcf_confidence(&c, 1, 50.0, 0.5, 100.0) == 1.0);
    jcf_beat(&c, 10.0);
    CHECK(c.t[JCF_KEEP - 1] == 99.5);
    CHECK(jcf_confidence(&c, 1, 50.0, 0.5, 104.0) < 0.75);    /* 4 s with no beat: half the window's slots missed */
    CHECK(jcf_confidence(&c, 0, 0.0, 0.0, 100.0) == 0.0);
}

/* SILENCE: 0.5 s under -60 dBFS RMS: silent, the beats go, confidence 0, no beat kept until sound */
static void test_silence(void)
{
    struct jcf c;
    uint64_t loud = 441ull * 2000ull * 2000ull, quiet = 441ull * 30ull * 30ull;   /* RMS 2000 (-24 dB), 30 (-61 dB) */
    int onsets = 0;
    jcf_init(&c);
    for (int k = 0; k < 40; k++)
        jcf_beat(&c, 0.5 * k);
    CHECK(jcf_samples(&c, loud, 441) == 0 && !c.silent);
    for (int i = 0; i < 49; i++)
        onsets += jcf_samples(&c, quiet, 441);
    CHECK(onsets == 0 && !c.silent && c.n == 40);            /* 0.49 s */
    CHECK(jcf_samples(&c, quiet, 441) == 1 && c.silent && c.n == 0 && c.silences == 1);   /* 0.5 s */
    CHECK(jcf_samples(&c, 0, 441) == 0 && c.silent);          /* (a stalled stream: quiet too) */
    jcf_beat(&c, 30.0);
    CHECK(c.n == 0 && jcf_confidence(&c, 1, 0.0, 0.5, 31.0) == 0.0);
    CHECK(jcf_samples(&c, 441ull * 33ull * 33ull, 441) == 0 && !c.silent);   /* -60 dB and up is sound */
    jcf_beat(&c, 30.5);
    CHECK(c.n == 1);
    CHECK(jcf_samples(&c, quiet, 0) == 0 && !c.silent);       /* an empty block: nothing */
}

/* the Python's all-pairs F-measure, as written there (numpy's distance matrix), against the module's merge */
static double brute(const double *beats, size_t n, double t0, double period, double now)
{
    double lo = now - JCF_WINDOW_S, hi = now - JCF_DELAY_S, margin = JCF_TOL * period, r, p;
    int64_t k0 = (int64_t)ceil((lo - t0) / period), k1 = (int64_t)floor((hi - t0) / period);
    size_t a = 0, b = 0, hits = 0, on = 0;
    while (a < n && beats[a] < lo - margin)
        a++;
    while (b < n && beats[b] < hi + margin)
        b++;
    if (k1 < k0 || b <= a)
        return 0.0;
    for (int64_t k = k0; k <= k1; k++) {
        double step = period * (double)k, s = t0 + step, best = INFINITY;
        for (size_t i = a; i < b; i++)
            best = fmin(best, fabs(beats[i] - s));
        hits += best <= margin;
    }
    for (size_t i = a; i < b; i++) {
        double best = INFINITY;
        for (int64_t k = k0; k <= k1; k++) {
            double step = period * (double)k;
            best = fmin(best, fabs(beats[i] - (t0 + step)));
        }
        on += best <= margin;
    }
    r = (double)hits / (double)(k1 - k0 + 1);
    p = (double)on / (double)(b - a);
    return r + p == 0.0 ? 0.0 : 2.0 * r * p / (r + p);
}

/* the merge gives the all-pairs answer exactly: random grids, beats on and off them, some exactly a margin away */
static void test_merge_is_exact(void)
{
    unsigned seed = 12345u;
    int bad = 0, mid = 0;
    for (int trial = 0; trial < 20000; trial++) {
        double beats[JCF_KEEP], period, t0, now, t = 0.0;
        size_t n = 0;
        seed = seed * 1103515245u + 12345u;
        period = 0.3 + (double)(seed >> 8 & 1023u) / 2048.0;
        seed = seed * 1103515245u + 12345u;
        t0 = (double)(seed >> 8 & 4095u) / 256.0;
        while (n < JCF_KEEP) {
            seed = seed * 1103515245u + 12345u;
            switch (seed >> 28 & 3u) {
            case 0: t += period; break;                                    /* on the grid's step */
            case 1: t += period * (1.0 + JCF_TOL); break;                   /* a margin off */
            case 2: t += period * 0.5; break;                               /* an off-beat */
            default: t += (double)(seed >> 8 & 255u) / 256.0; break;        /* anywhere */
            }
            beats[n++] = t;
        }
        seed = seed * 1103515245u + 12345u;
        now = 8.0 + (double)(seed >> 8 & 4095u) / 4096.0 * (t - 4.0);
        double f = jcf_grid_confidence(beats, n, 1, t0, period, now, JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
        mid += f > 0.0 && f < 1.0;
        if (f != brute(beats, n, t0, period, now))
            bad++;
    }
    CHECK(bad == 0 && mid > 10000);   /* (most cases between 0 and 1: the tests bite) */
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "tests/conf_ref";
    test_merge_is_exact();
    test_gate();
    test_ring();
    test_silence();
    test_calls(dir);
    test_runs(dir);
    printf("conf: %d checks, %d failed\n", checks, fails);
    return fails;
}
