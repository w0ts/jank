/* SPDX-License-Identifier: GPL-3.0-only */
/* The tuned follower's graded confidence (core/beat/jb_graded.h jg_tuned_step): the ONE number shown and judged (firmware/main/beat.c
 * follow_conf: the gate, threshold, hysteresis, COAST, colour and LED all judge it; graded off: the gate's value).
 *   - the parts on synthetic beat sequences: a clean steady beat is high, a jittery one in the middle, beats off the lines
 *     low, and the value falls as the jitter grows
 *   - the tuned chain of firmware/main/beat.c (activation -> DBN -> weak follower -> lines -> gate) replayed on the real
 *     activations of tests/beat_ref and the synthetic tracks of tests/robust_ref, twice: gated on the old gate value at 80 and on
 *     the graded value at the new default 60; the graded gate must follow the rule (closed under the threshold, open again at
 *     threshold + 5) at every step
 *   - `test_graded DIR... --report`: the distribution of the gate's value and of the graded one (percent at 0, at 100, between),
 *     seconds from the first lock to the outputs running, stops per hour of music, old versus new
 * Exit status: the number of failed checks. */
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_conf.h"
#include "jb_dbn.h"
#include "jb_follow.h"
#include "jb_graded.h"
#include "jb_robust.h"
#include "jb_weak.h"

static int fails, checks, report;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

/* ---- the parts on synthetic beats ---- */

#define N 64

/* beats on a grid of `period` s with a deterministic wobble of +-jit_s (a triangle wave), the lines on the grid, shifted by off_s */
static size_t make(double *beats, double *lines, double period, double jit_s, double off_s)
{
    size_t n = 0;
    for (size_t i = 0; i < N; i++) {
        double wob = jit_s * (i % 4 == 0 ? 1.0 : i % 4 == 1 ? -1.0 : i % 4 == 2 ? 0.5 : -0.5);
        beats[n] = 1.0 + (double)i * period + wob + off_s;
        lines[n] = 1.0 + (double)i * period;
        n++;
    }
    return n;
}

/* soft F and jitter at the 60th beat, the value with the tempo and DBN part td */
static double part_value(double period, double jit_s, double off_s, double td, double *soft, double *jit)
{
    double beats[N], lines[N];
    size_t use = 60;
    make(beats, lines, period, jit_s, off_s);
    double now = beats[use - 1] + 0.2;
    *soft = jg_soft_f(beats, use, lines, use, period, now);
    *jit = jg_jitter(beats, use);
    return jg_graded(*soft, *jit, td, td);
}

static void test_parts(void)
{
    double s, j, clean = part_value(0.5, 0.0, 0.0, 1.0, &s, &j);
    CHECK(s > 0.95 && j > 0.99 && clean > 0.97);
    double mid = part_value(0.5, 0.03, 0.0, 1.0, &s, &j);
    CHECK(mid > 0.6 && mid < 0.95);
    double off = part_value(0.5, 0.0, 0.25, 0.5, &s, &j);   /* every beat half a period from its line, the comb and the DBN half sure */
    CHECK(s < 0.05 && off < 0.6 && off < mid - 0.2);
    /* monotone in the jitter, soft F and the mean alike (never rising) */
    double prev = 2.0, prev_j = 2.0;
    for (int k = 0; k <= 12; k++) {
        double v = part_value(0.5, 0.005 * k, 0.0, 1.0, &s, &j);
        CHECK(v <= prev + 1e-12);
        CHECK(j <= prev_j + 1e-12);
        prev = v;
        prev_j = j;
    }
    CHECK(prev < clean - 0.2);
    /* monotone in the offset from the lines too */
    prev = 2.0;
    for (int k = 0; k <= 10; k++) {
        double v = part_value(0.5, 0.0, 0.025 * k, 1.0, &s, &j);
        CHECK(v <= prev + 1e-12);
        prev = v;
    }
    /* the mean is clipped, the parts are all 0..1 */
    CHECK(jg_graded(1, 1, 1, 1) == 1.0 && jg_graded(0, 0, 0, 0) == 0.0 && fabs(jg_graded(1, 0, 0, 0) - 0.25) < 1e-12);
}

/* ---- the tuned chain ---- */

#define MAX_FRAMES 20000
#define MAX_STEPS 4000
#define BEAT_DEFER JW_STRONG_HALF
#define STEP_EVERY 25
#define OLD_THRESHOLD 80
#define NEW_THRESHOLD 60

static float s_act[MAX_FRAMES];
static int s_n_act;
static struct jw_store s_wst;
static struct jrb_store s_rst;

struct rec {                            /* what the gate and the follower decided at a step */
    double now, bpm, period, tb, conf;
    int has_bpm, has_grid, jumps, gated, ran, lines;
    double lines_sum;
};

struct trace {
    struct rec r[MAX_STEPS];
    double graded[MAX_STEPS];
    int n;
};

/* judge_graded 0: the gate on the gate value at OLD_THRESHOLD (before); 1: on the graded value at NEW_THRESHOLD (now) */
static void run_chain(struct trace *tr, int judge_graded)
{
    static struct jb_dbn dbn;
    struct jb_follow f;
    struct jb_weak w;
    struct jb_robust rb;
    struct jcf cf;
    struct jg_tuned g;
    uint8_t gated = 0, ran = 0, pend = 0;
    int64_t pend_index = 0;
    double pend_t = 0.0;
    tr->n = 0;
    jb_follow_init(&f);
    jcf_init(&cf);
    jw_init(&w, &s_wst, NULL, 0, 1);
    jrb_init(&rb, &s_rst);              /* (its ring only: no attach, no tick: the tuned follower) */
    jg_tuned_reset(&g);
    CHECK(jb_dbn_init(&dbn, 90.0, 180.0, 100.0) == 0);
    for (int k = 0; k < s_n_act; k++) {
        double now = (double)k / 100.0, t = 0.0;
        int fed = 0;
        jw_act(&w, k, s_act[k]);
        jrb_act(&rb, k, s_act[k]);
        if (jb_dbn_step(&dbn, s_act[k], &t)) {
            if (pend) {
                jw_update(&w, &f, pend_t);
                jb_follow_fast_jump(&f, pend_t, JF_FAST_JUMP_REL);
                jcf_beat(&cf, pend_t);
            }
            pend = 1;
            pend_index = k;
            pend_t = t;
        }
        if (pend && k >= pend_index + BEAT_DEFER) {
            int jumps = f.m.jumps;
            pend = 0;
            jw_update(&w, &f, pend_t);
            jb_follow_fast_jump(&f, pend_t, JF_FAST_JUMP_REL);
            if (f.m.jumps != jumps)
                jcf_clear_lines(&cf);
            jcf_beat(&cf, pend_t);
            fed = 1;
        }
        jw_coast(&w, &f, now);
        if (!fed && k % STEP_EVERY)
            continue;
        /* follow_conf, the tuned branch */
        double tb = 0.0, period = 0.0, bpm = 0.0;
        int has = jb_follow_grid(&f, &tb, &period), has_bpm = jb_follow_bpm_at(&f, now, &bpm);
        jcf_line_tick(&cf, has_bpm ? has : 0, tb, period, now);
        double strict = jcf_lines_confidence(cf.t, cf.n, cf.line, cf.lines, has, period, now, JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
        double conf = strict;
        uint8_t held = 0;
        double weak = jcf_weak_confidence(jw_strong(&w), w.n_strong, jw_seen(&w), w.n_seen, jw_weak(&w), w.n_weak, cf.line,
                                          cf.lines, has, period, now, JW_MIN_OBS, &w.last_conf, &held);
        conf = jcf_weak_pick(strict, weak, gated, ran);
        double graded = jg_tuned_step(&g, &rb, &dbn, jw_seen(&w), w.n_seen, cf.line, cf.lines, has, period, now);
        uint8_t thr = judge_graded ? NEW_THRESHOLD : OLD_THRESHOLD, prev = gated;
        double judged = judge_graded ? graded : conf;
        gated = jcf_gate(gated, judged, JCF_STOP, thr);
        if (judge_graded) {             /* the rule, at every step: closed under the threshold, open again at threshold + 5, else unchanged */
            if (judged < thr / 100.0)
                CHECK(gated);
            else if (judged >= thr / 100.0 + JCF_HYST_PCT / 100.0)
                CHECK(!gated);
            else
                CHECK(gated == prev);
        }
        ran = (uint8_t)(has_bpm && !gated);
        if (tr->n < MAX_STEPS) {
            struct rec *r = &tr->r[tr->n];
            double ls = 0.0;
            for (size_t i = 0; i < cf.lines; i++)
                ls += cf.line[i];
            *r = (struct rec){.now = now, .bpm = has_bpm ? bpm : 0.0, .period = period, .tb = tb, .conf = conf, .has_bpm = has_bpm,
                              .has_grid = has, .jumps = f.m.jumps, .gated = gated, .ran = ran, .lines = (int)cf.lines, .lines_sum = ls};
            tr->graded[tr->n] = graded;
            tr->n++;
        }
    }
}

/* the gate judges the shown number: open, closes at the threshold, reopens at threshold + 5, graded off = the gate's value */
static void test_gate_rule(void)
{
    uint8_t g = 0;
    CHECK(JCF_THRESHOLD_DEFAULT == 60);
    g = jcf_gate(g, 0.60, JCF_STOP, 60);
    CHECK(g == 0);                      /* at the threshold: open */
    g = jcf_gate(g, 0.59, JCF_STOP, 60);
    CHECK(g == 1);                      /* under it: closed */
    g = jcf_gate(g, 0.64, JCF_STOP, 60);
    CHECK(g == 1);                      /* in the band: stays closed */
    g = jcf_gate(g, 0.65, JCF_STOP, 60);
    CHECK(g == 0);                      /* threshold + 5: open again */
    g = jcf_gate(g, 0.62, JCF_STOP, 60);
    CHECK(g == 0);                      /* in the band: stays open */
    CHECK(jcf_gate(1, 0.10, JCF_RUN, 60) == 0 && jcf_gate(1, 0.10, JCF_STOP, 0) == 0);   /* KEEP RUNNING / threshold 0: never gated */
}

/* ---- inputs ---- */

static int read_act_lines(const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[64];
    if (!fp)
        return 0;
    s_n_act = 0;
    while (fgets(line, sizeof line, fp) && s_n_act < MAX_FRAMES)
        s_act[s_n_act++] = (float)atof(line);
    fclose(fp);
    return s_n_act > 500;
}

static int read_track(const char *path)
{
    FILE *fp = fopen(path, "r");
    static char buf[1 << 20];
    if (!fp)
        return 0;
    s_n_act = 0;
    while (fgets(buf, sizeof buf, fp))
        if (!strncmp(buf, "act ", 4)) {
            char *p = buf + 4, *end;
            long n = strtol(p, &end, 10);
            p = end;
            for (long i = 0; i < n && s_n_act < MAX_FRAMES; i++, p = end)
                s_act[s_n_act++] = (float)((double)strtol(p, &end, 10) / 1000.0);
        }
    fclose(fp);
    return s_n_act > 500;
}

struct dist { double secs; long stops_a, stops_b; long n, g0, g100, gmid, v0, v100, vmid, lo, mid, hi, top; };   /* graded: under 50, 50..79, 80..89, 90 and over */

/* the steps after the first lock only: the gate is judged while the outputs are or could be running */
static void run_one(const char *name, struct dist *all, int *runs)
{
    static struct trace a, b;
    run_chain(&a, 1);                    /* now: graded, 60 */
    run_chain(&b, 0);                    /* before: the gate value, 80 */
    CHECK(a.n > 20 && a.n == b.n);
    if (report && a.n > 20) {
        double secs = a.r[a.n - 1].now, la = -1.0, lb = -1.0, da = -1.0, db = -1.0;
        int sa = 0, sb = 0;
        for (int i = 0; i < a.n; i++) {
            sa += i && a.r[i - 1].ran && !a.r[i].ran;
            sb += i && b.r[i - 1].ran && !b.r[i].ran;
            if (a.r[i].has_bpm && la < 0.0)
                la = a.r[i].now;
            if (la >= 0.0 && a.r[i].ran && da < 0.0)
                da = a.r[i].now - la;
            if (b.r[i].has_bpm && lb < 0.0)
                lb = b.r[i].now;
            if (lb >= 0.0 && b.r[i].ran && db < 0.0)
                db = b.r[i].now - lb;
        }
        printf("RUN %-16s lock->running s: gate@80 %6.1f  graded@60 %6.1f | stops: gate@80 %d  graded@60 %d | music %.0f s\n", name, db, da, sb, sa, secs);
        all->stops_a += sa;
        all->stops_b += sb;
        all->secs += secs;
    }
    struct dist d = {0};
    double gsum = 0.0, vsum = 0.0, vmin = 1.0, vmax = 0.0;
    int locked = 0;
    for (int i = 0; i < a.n; i++) {
        locked |= a.r[i].has_bpm;
        if (!locked)                     /* (from the first lock on: the steps before it are 0 for both) */
            continue;
        int gp = jcf_pct(a.r[i].conf), vp = jcf_pct(a.graded[i]);
        CHECK(a.graded[i] >= 0.0 && a.graded[i] <= 1.0);
        d.n++;
        d.g0 += gp == 0;
        d.g100 += gp == 100;
        d.gmid += gp > 0 && gp < 100;
        d.v0 += vp == 0;
        d.v100 += vp == 100;
        d.vmid += vp > 0 && vp < 100;
        d.lo += vp < 50;
        d.mid += vp >= 50 && vp < 80;
        d.hi += vp >= 80 && vp < 90;
        d.top += vp >= 90;
        gsum += gp;
        vsum += vp;
        vmin = fmin(vmin, vp);
        vmax = fmax(vmax, vp);
    }
    if (report && d.n)
        printf("%-16s %4ld steps after the first lock | gate: 0 %3ld%%  100 %3ld%%  between %3ld%%  mean %3.0f | graded: 0 %3ld%%  100 %3ld%%  between %3ld%%  mean %3.0f  (%g..%g)  <50 %3ld%%  50-79 %3ld%%  80-89 %3ld%%  90+ %3ld%%\n",
               name, d.n, 100 * d.g0 / d.n, 100 * d.g100 / d.n, 100 * d.gmid / d.n, gsum / (double)d.n, 100 * d.v0 / d.n,
               100 * d.v100 / d.n, 100 * d.vmid / d.n, vsum / (double)d.n, vmin, vmax, 100 * d.lo / d.n, 100 * d.mid / d.n,
               100 * d.hi / d.n, 100 * d.top / d.n);
    all->n += d.n;
    all->g0 += d.g0;
    all->g100 += d.g100;
    all->gmid += d.gmid;
    all->v0 += d.v0;
    all->v100 += d.v100;
    all->vmid += d.vmid;
    all->lo += d.lo;
    all->mid += d.mid;
    all->hi += d.hi;
    all->top += d.top;
    (*runs)++;
}

static void test_dir(const char *dir, int tracks, struct dist *all, int *runs)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    char path[512], name[256];
    const char *suffix = tracks ? ".track.txt" : ".act.txt";
    size_t sl = strlen(suffix);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len <= sl || strcmp(e->d_name + len - sl, suffix))
            continue;
        snprintf(name, sizeof name, "%.*s", (int)(len - sl), e->d_name);
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (tracks ? read_track(path) : read_act_lines(path))
            run_one(name, all, runs);
    }
    closedir(d);
}

int main(int argc, char **argv)
{
    struct dist real = {0}, synth = {0};
    int runs_real = 0, runs_synth = 0;
    const char *real_dir = "tests/beat_ref", *synth_dir = "tests/robust_ref";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--report"))
            report = 1;
        else if (!real_dir || i == 1)
            real_dir = argv[i];
        else
            synth_dir = argv[i];
    }
    test_parts();
    test_gate_rule();
    test_dir(real_dir, 0, &real, &runs_real);
    test_dir(synth_dir, 1, &synth, &runs_synth);
    CHECK(runs_real >= 3 && runs_synth >= 10);
    /* it spreads: on the real activations the graded value is not a wall and a cliff */
    CHECK(real.n > 0 && real.vmid * 100 / real.n >= 40);
    if (report) {
        struct dist t[2] = {real, synth};
        const char *nm[2] = {"REAL (beat_ref)", "SYNTHETIC (robust_ref)"};
        for (int i = 0; i < 2; i++)
            if (t[i].n)
                printf("TOTAL %-22s %5ld steps after the first lock | gate: 0 %3ld%%  100 %3ld%%  between %3ld%% | graded: 0 %3ld%%  100 %3ld%%  between %3ld%%  <50 %3ld%%  50-79 %3ld%%  80-89 %3ld%%  90+ %3ld%%\n",
                       nm[i], t[i].n, 100 * t[i].g0 / t[i].n, 100 * t[i].g100 / t[i].n, 100 * t[i].gmid / t[i].n,
                       100 * t[i].v0 / t[i].n, 100 * t[i].v100 / t[i].n, 100 * t[i].vmid / t[i].n, 100 * t[i].lo / t[i].n,
                       100 * t[i].mid / t[i].n, 100 * t[i].hi / t[i].n, 100 * t[i].top / t[i].n);
    }
    if (report) {
        struct dist t[2] = {real, synth};
        for (int i = 0; i < 2; i++)
            if (t[i].secs > 0.0)
                printf("STOPS PER HOUR %-6s gate@80 %.1f  graded@60 %.1f  (%.0f s of music)\n", i ? "SYNTH" : "REAL",
                       (double)t[i].stops_b * 3600.0 / t[i].secs, (double)t[i].stops_a * 3600.0 / t[i].secs, t[i].secs);
    }
    printf("graded: %d checks, %d failed\n", checks, fails);
    return fails;
}
