/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the robust follower, all-d-f60 (M2m: core/beat/jb_robust.c, jb_comb.c, the DBN's tempo prior, the lock and jump
 * gate) against beat-listener's studies/follow-robust (golden vectors in tests/robust_ref, scripts/robust_golden.py).
 *   comb_units.txt  the comb pieces (ratio margin, tempo and phase agreement) on two activations, and the tempo prior
 *   NAME.track.txt  a synthetic track: its activation (the beats there are not used: the DBN makes them)
 *   NAME.txt        rchain.py's replay with the device's causal view, one line an operation (the header of the script). The
 *                   C runs the DBN frame by frame with its own tempo prior and must make the beats the Python's DBN made; the
 *                   beats it is given are those; each gate question and answer, each beat's follower state, each 5th tick's
 *                   confidences, agreement, gate, memory and prior are compared.
 * With the skip option (tests/skip_ref, scripts/robust_golden.py --skip-out): NAME.act.txt / NAME.pat.txt, the same replay with SKIP (jb_skip.h,
 * ACT / PAT) and the shown (graded) confidence; also compared: after each beat the beat path's state (k lines), after each tick the
 * activation path's (u lines: the run of bad ticks, the re-sync counts, the pattern, the gate, the shown value).
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
#include "jb_skip.h"
#include "jb_weak.h"

static int fails, checks, verbose;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define STATE_S 1e-9
#define CONF_NEAR 1e-12
#define AGREE_NEAR 1e-4                /* the comb sums are float */
#define MAX_FRAMES 20000
#define MAX_BEATS 2000
#define THRESHOLD 80

static char *slurp(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1u);
    if (buf && fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    if (buf)
        buf[n] = 0;
    fclose(fp);
    return buf;
}

static float s_act[MAX_FRAMES];
static int s_n_act;
static double s_dur;

static int read_track(const char *path)
{
    char *buf = slurp(path), *p, *end;
    if (!buf)
        return 0;
    s_n_act = 0;
    s_dur = 0.0;
    for (char *line = strtok(buf, "\n"); line; line = strtok(NULL, "\n")) {
        if (!strncmp(line, "dur ", 4))
            s_dur = strtod(line + 4, NULL);
        else if (!strncmp(line, "act ", 4)) {
            long n = strtol(line + 4, &p, 10);
            for (long i = 0; i < n && i < MAX_FRAMES; i++, p = end)
                s_act[s_n_act++] = (float)((double)strtol(p, &end, 10) / 1000.0);
        }
    }
    free(buf);
    return s_dur > 0.0 && s_n_act > 100;
}

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

/* ---- the run ---- */

#define MAX_ASKS 64
struct ask { int kind, ok; double now, period; };

struct run {
    struct jb_skip sk;
    double shown;
    int graded;                         /* the shown (graded) confidence is on: PAT's vectors */
    struct jb_follow f;
    struct jb_weak w;
    struct jcf conf;
    struct jb_robust r;
    struct jb_dbn dbn;
    uint8_t gated, was_running;
    double now;
    int jumps0;
    struct jrb_out out;
    int has_bpm, has_grid;
    double bpm, t0, p, strict, loose, c;
    /* the gate's questions the C asked, and those the Python did, to match at the beat */
    struct ask got[MAX_ASKS], want[MAX_ASKS];
    int n_got, n_want;
};

struct stats {
    int beats, state_bad, count_bad, ticks, conf_bad, gate_bad, run_bad, dbn_bad, dbn_beats, asks, asks_bad, agree_bad;
    int k_bad, u_bad, u_n;
    double worst_x, worst_conf, worst_agree, worst_shown;
};

static struct run s_run;
static struct jw_store s_store;
static struct jrb_store s_rstore;

static int hook(void *ctx, int kind, double now, double period)
{
    struct run *r = &s_run;
    int ok = jrb_gate(ctx, kind, now, period);
    if (r->n_got < MAX_ASKS)
        r->got[r->n_got++] = (struct ask){kind, ok, now, period};
    return ok;
}

static void attach(struct run *r)
{
    r->r.sk = jsk_on(&r->sk) ? &r->sk : NULL;
    jrb_attach(&r->r, &r->f, &r->w);
    r->f.m.gate = hook;                 /* (records each question) */
}

static void restart(struct run *r)
{
    jb_follow_init(&r->f);
    jw_reset(&r->w);
    jcf_init(&r->conf);
    jrb_reset(&r->r);
    jsk_reset(&r->sk);
    attach(r);
}

static void beat(struct run *r, double t)
{
    int jumps = r->f.m.jumps;
    uint32_t rs = r->sk.resyncs;
    jsk_update(&r->sk, &r->w, &r->f, t);
    jb_follow_fast_jump(&r->f, t, JF_FAST_JUMP_REL);
    if (r->f.m.jumps != jumps || r->sk.resyncs != rs)      /* (chain: clear_lines on a jump; rchain: also on a re-sync) */
        jcf_clear_lines(&r->conf);
    jcf_beat(&r->conf, t);
}

static void tick(struct run *r, int silent)
{
    double now = r->now;
    jsk_coast(&r->sk, &r->w, &r->f, now);
    r->has_bpm = jb_follow_bpm_at(&r->f, now, &r->bpm);
    r->has_grid = jb_follow_grid(&r->f, &r->t0, &r->p);
    if (!r->has_bpm)
        r->bpm = 0.0;
    if (!r->has_grid)
        r->t0 = r->p = 0.0;
    r->strict = r->loose = 0.0;
    if (silent)
        r->c = 0.0;
    else {
        jcf_line_tick(&r->conf, r->has_bpm ? r->has_grid : 0, r->t0, r->p, now);
        r->loose = jcf_weak_confidence(jw_strong(&r->w), r->w.n_strong, jw_seen(&r->w), r->w.n_seen, jw_weak(&r->w),
                                       r->w.n_weak, r->conf.line, r->conf.lines, r->has_grid, r->p, now, JW_MIN_OBS,
                                       &r->w.last_conf, NULL);
        r->strict = jcf_lines_confidence(r->conf.t, r->conf.n, r->conf.line, r->conf.lines, r->has_grid, r->p, now,
                                         JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
        r->c = jcf_weak_pick(r->strict, r->loose, r->gated, r->was_running);
    }
    struct jrb_in in = {.now = now, .silent = silent, .has_bpm = r->has_bpm, .bpm = r->bpm, .has_grid = r->has_grid,
                       .period = r->p, .conf = r->c, .strict = r->strict, .has_kf = r->f.has_kf,
                       .jumped = r->f.m.jumps != r->jumps0, .was_running = r->was_running, .lines = r->conf.line,
                       .n_lines = r->conf.lines, .threshold_pct = THRESHOLD, .keep_running = 0};
    jrb_tick(&r->r, &r->dbn, &in, &r->gated, &r->out);
    r->was_running = r->out.run;
    r->shown = NAN;
    if (!silent && r->graded)
        r->shown = jg_graded(r->has_grid ? jg_soft_f(jw_seen(&r->w), r->w.n_seen, r->conf.line, r->conf.lines, r->p, now) : 0.0,
                             jg_jitter(jw_seen(&r->w), r->w.n_seen), r->r.v_tempo, r->r.v_dbn);
}

/* s kf mode t p d jumps misses in_a_row manoeuvres follow_beats strong weak_hits coasted fast_jumps lines */
static void check_state(const struct run *r, const double *s, struct stats *st)
{
    const struct jb_follow *f = &r->f;
    double dx = 0.0;
    st->beats++;
    if (f->has_kf != (int)s[0] || (int)f->m.mode != (int)s[1]) {
        st->state_bad++;
        return;
    }
    if (f->has_kf)
        for (int i = 0; i < 3; i++)
            dx = fmax(dx, fabs(f->x[i] - s[2 + i]));
    st->worst_x = fmax(st->worst_x, dx);
    if (dx > STATE_S)
        st->state_bad++;
    if (f->m.jumps != (int)s[5] || f->m.misses != (int)s[6] || f->m.in_a_row != (int)s[7]
        || (f->has_kf ? f->manoeuvres : -1) != (int)s[8] || f->follow_beats != (int)s[9] || r->w.n_strong != (int)s[10]
        || r->w.n_weak != (int)s[11] || r->w.coasted != (uint32_t)s[12] || f->fast_jumps != (uint32_t)s[13]
        || r->conf.lines != (uint16_t)s[14]) {
        if (verbose && st->count_bad == 0)
            printf("  first count mismatch at beat %d: jumps %d/%g misses %d/%g in_a_row %d/%g strong %d/%g weak %d/%g "
                   "lines %u/%g\n", st->beats, f->m.jumps, s[5], f->m.misses, s[6], f->m.in_a_row, s[7], r->w.n_strong,
                   s[10], r->w.n_weak, s[11], r->conf.lines, s[14]);
        st->count_bad++;
    }
}

/* k off_run beat_suspect reopen resyncs: the beat path after a beat */
static void check_k(const struct run *r, const double *k, struct stats *st)
{
    if (r->sk.off_run != (int)k[0] || r->sk.beat_suspect != (uint8_t)k[1] || r->sk.reopen != (int)k[2]
        || r->sk.resyncs != (uint32_t)k[3])
        st->k_bad++;
}

/* u act_run reopen resyncs act_resyncs tpl_n suspect act_last tpl_sum gated shown: the skip state after a tick */
static void check_u(const struct run *r, const double *u, struct stats *st)
{
    float tpl = 0.0f;
    for (int i = 0; r->sk.has_tpl && i < JSK_BAR * JSK_STEPS; i++)
        tpl += r->sk.tpl[i];
    st->u_n++;
    if (r->sk.act_run != (int)u[0] || r->sk.reopen != (int)u[1] || r->sk.resyncs != (uint32_t)u[2]
        || r->sk.act_resyncs != (uint32_t)u[3] || r->sk.tpl_n != (int)u[4] || jsk_suspect(&r->sk) != (int)u[5]
        || fabs(r->sk.act_last - u[6]) > STATE_S || fabs(tpl - u[7]) > 1e-3 * (1.0 + fabs(u[7])) || r->gated != (uint8_t)u[8])
        st->u_bad++;
    if (u[9] == u[9] || r->shown == r->shown) {
        if ((u[9] == u[9]) != (r->shown == r->shown))
            st->u_bad++;
        else {
            st->worst_shown = fmax(st->worst_shown, fabs(r->shown - u[9]));
            if (fabs(r->shown - u[9]) > AGREE_NEAR)
                st->u_bad++;
        }
    }
}

/* q has_bpm bpm has_grid t0 p strict loose conf gconf gated running coasting confident has_mem mem_bpm mem_t prior_on
 *   v_tempo v_phase v_dbn lines coasted jumped */
static void check_tick(const struct run *r, const double *q, struct stats *st)
{
    double d = fmax(fabs(r->strict - q[5]), fmax(fabs(r->loose - q[6]), fabs(r->c - q[7])));
    double a = fmax(fabs(r->out.gconf - q[8]),
                    fmax(fabs(r->r.v_tempo - q[17]), fmax(fabs(r->r.v_phase - q[18]), fabs(r->r.v_dbn - q[19]))));
    st->ticks++;
    st->worst_conf = fmax(st->worst_conf, d);
    st->worst_agree = fmax(st->worst_agree, a);
    if (d > CONF_NEAR || r->has_bpm != (int)q[0] || r->has_grid != (int)q[2] || fabs(r->p - q[4]) > STATE_S)
        st->conf_bad++;
    if (a > AGREE_NEAR)
        st->agree_bad++;
    if (r->gated != (uint8_t)q[9])
        st->gate_bad++;
    if (r->out.run != (uint8_t)q[10] || r->out.coasting != (uint8_t)q[11] || r->out.confident != (uint8_t)q[12]
        || r->r.has_mem != (uint8_t)q[13] || (q[13] != 0.0 && (fabs(r->r.mem_bpm - q[14]) > STATE_S
                                                                  || fabs(r->r.mem_t - q[15]) > STATE_S))
        || r->out.prior_on != (uint8_t)q[16] || r->conf.lines != (uint16_t)q[20] || r->w.coasted != (uint32_t)q[21]
        || (r->f.m.jumps != r->jumps0) != (q[22] != 0.0))
        st->run_bad++;
}

/* d t ...: the DBN's beats up to this tick's frame (its frames ran first, with the prior the last tick left) */
static void dbn_line(struct run *r, const char *line, struct stats *st)
{
    double want[64], t;
    int n = 0;
    char *end;
    const char *p = line + 1;
    for (; n < 64; p = end) {
        double v = strtod(p, &end);
        if (end == p)
            break;
        want[n++] = v;
    }
    int got = 0;
    int64_t upto = (int64_t)rint(r->now * JW_FPS);
    for (int64_t k = r->dbn.counter; k <= upto && k < s_n_act; k++)
        if (jb_dbn_step(&r->dbn, s_act[k], &t)) {
            if (got < n && t == want[got])
                st->dbn_beats++;
            else
                st->dbn_bad++;
            got++;
        }
    if (got != n)
        st->dbn_bad++;
    r->jumps0 = r->f.m.jumps;
}

static int s_resyncs_all, s_act_resyncs_all;

/* tag NULL: the all-d-f60 vectors; "act" / "pat": the skip vectors */
static void test_run(const char *dir, const char *name, const char *tag)
{
    char path[512], line[1024];
    struct run *r = &s_run;
    struct stats st = {0};
    if (tag)
        snprintf(path, sizeof path, "%s/%s.%s.txt", dir, name, tag);
    else
        snprintf(path, sizeof path, "%s/%s.txt", dir, name);
    FILE *fp = fopen(path, "r");
    CHECK(fp != NULL);
    if (!fp)
        return;
    memset(r, 0, sizeof *r);
    jb_follow_init(&r->f);
    jcf_init(&r->conf);
    jw_init(&r->w, &s_store, NULL, 0, 1);
    jrb_init(&r->r, &s_rstore);
    jsk_init(&r->sk, &r->r);
    if (tag) {
        jsk_set_mode(&r->sk, !strcmp(tag, "pat") ? JSK_PAT : JSK_ACT);
        r->graded = !strcmp(tag, "pat");
    }
    CHECK(jb_dbn_init(&r->dbn, 90.0, 180.0, 100.0) == 0);
    attach(r);
    while (fgets(line, sizeof line, fp)) {
        double v[32];
        if (line[0] == 't' && numbers(line, v, 1)) {
            r->now = v[0];
            int64_t upto = (int64_t)rint(r->now * JW_FPS);
            for (int64_t k = r->w.act_n; k <= upto && k < s_n_act; k++) {
                jw_act(&r->w, k, s_act[k]);
                jrb_act(&r->r, k, s_act[k]);
            }
        } else if (line[0] == 'x')
            restart(r);
        else if (line[0] == 'd')
            dbn_line(r, line, &st);
        else if (line[0] == 'r' && numbers(line, v, 3))
            jb_follow_propose(&r->f, v[0], v[1], 1, v[2]);
        else if (line[0] == 'g' && numbers(line, v, 4)) {
            if (r->n_want < MAX_ASKS)
                r->want[r->n_want++] = (struct ask){(int)v[0], (int)v[1], v[2], v[3]};
        } else if (line[0] == 'b' && numbers(line, v, 1)) {
            r->n_got = 0;
            beat(r, v[0]);
            st.asks += r->n_want;
            if (r->n_got != r->n_want)
                st.asks_bad++;
            for (int i = 0; i < r->n_got && i < r->n_want; i++)
                if (r->got[i].kind != r->want[i].kind || r->got[i].ok != r->want[i].ok
                    || fabs(r->got[i].now - r->want[i].now) > STATE_S || fabs(r->got[i].period - r->want[i].period) > STATE_S)
                    st.asks_bad++;
            r->n_want = 0;
        } else if (line[0] == 's') {
            CHECK(numbers(line, v, 15));
            check_state(r, v, &st);
        } else if (line[0] == 'k' && numbers(line, v, 4))
            check_k(r, v, &st);
        else if (line[0] == 'u' && numbers(line, v, 10))
            check_u(r, v, &st);
        else if (line[0] == 'c' && numbers(line, v, 1))
            tick(r, (int)v[0]);
        else if (line[0] == 'q') {
            CHECK(numbers(line, v, 23));
            check_tick(r, v, &st);
        }
    }
    fclose(fp);
    if (verbose)
        printf("%-14s %d beats (glide %.2g s), %d ticks (conf %.2g, agreement %.2g); bad: state %d counts %d conf %d agree %d "
               "gate %d run %d dbn %d (%d beats) asks %d/%d; refused mem %u ratio %u, coast ticks %u, prior ticks %u, "
               "jumps %d\n", name, st.beats, st.worst_x, st.ticks, st.worst_conf, st.worst_agree, st.state_bad, st.count_bad,
               st.conf_bad, st.agree_bad, st.gate_bad, st.run_bad, st.dbn_bad, st.dbn_beats, st.asks_bad, st.asks,
               (unsigned)r->r.refused_memory, (unsigned)r->r.refused_ratio, (unsigned)r->r.coasting_ticks,
               (unsigned)r->r.prior_ticks, r->f.m.jumps);
    CHECK(st.beats > 80 && st.ticks > 200 && st.dbn_beats > 80);
    CHECK(st.state_bad == 0);
    CHECK(st.count_bad == 0);
    CHECK(st.conf_bad == 0);
    CHECK(st.agree_bad == 0);
    CHECK(st.gate_bad == 0);
    CHECK(st.run_bad == 0);
    CHECK(st.dbn_bad == 0);
    CHECK(st.asks_bad == 0);
    CHECK(st.k_bad == 0);
    CHECK(st.u_bad == 0);
    if (tag) {
        CHECK(st.u_n > 200);
        s_resyncs_all += (int)r->sk.resyncs;
        s_act_resyncs_all += (int)r->sk.act_resyncs;
        if (verbose)
            printf("   skip %s: beat path bad %d, activation path bad %d of %d ticks (shown worst %.2g); re-syncs %u (activation %u)\n",
                   tag, st.k_bad, st.u_bad, st.u_n, st.worst_shown, (unsigned)r->sk.resyncs, (unsigned)r->sk.act_resyncs);
    }
}

static int test_runs(const char *dir, int skip)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    char base[256], path[512];
    CHECK(d != NULL);
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len < 11 || strcmp(e->d_name + len - 10, ".track.txt"))
            continue;
        snprintf(base, sizeof base, "%.*s", (int)(len - 10), e->d_name);
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        CHECK(read_track(path));
        if (skip) {
            test_run(dir, base, "act");
            test_run(dir, base, "pat");
        } else
            test_run(dir, base, NULL);
        n++;
    }
    closedir(d);
    CHECK(n >= 10);
    if (skip)
        CHECK(s_act_resyncs_all >= 10 && s_resyncs_all >= s_act_resyncs_all);   /* the vectors do exercise the re-sync */
    return n;
}

/* ---- the units ---- */

static void test_units(const char *dir)
{
    char path[512];
    static float act[4096], scratch[JC_MAX_SPAN];
    snprintf(path, sizeof path, "%s/comb_units.txt", dir);
    char *buf = slurp(path);
    CHECK(buf != NULL);
    if (!buf)
        return;
    int n_act = 0, n_checked = 0;
    double worst = 0.0;
    for (char *line = strtok(buf, "\n"); line; line = strtok(NULL, "\n")) {
        char *p, *end;
        if (line[0] == 'a') {
            long n = strtol(line + 2, &p, 10);
            for (long i = 0; i < n; i++, p = end)
                act[i] = (float)((double)strtol(p, &end, 10) / 1000.0);
            n_act = (int)n;
        } else if (line[0] == 'm' || line[0] == 't' || line[0] == 'p') {
            long e = strtol(line + 2, &p, 10);
            double per = strtod(p, &p), want = strtod(p, NULL), got;
            struct jc_curve c;
            if (line[0] == 'm') {
                got = jc_curve(&c, scratch, act, 4096, n_act, e, JRB_RATIO_SPAN) ? jc_ratio_margin(&c, per) : NAN;
            } else if (line[0] == 't') {
                got = jc_curve(&c, scratch, act, 4096, n_act, e, JRB_AGREE_SPAN) ? jc_tempo_agreement(&c, per) : 0.0;
            } else {
                double ls[64];
                int n = 0;
                const double start = e / 100.0 - 7.9, delta = (start + per / 100.0) - start;
                for (; n < 64; n++) {                          /* (np.arange: start + i x ((start + step) - start)) */
                    double x = start + n * delta;
                    if (!(x < e / 100.0 - 0.1))
                        break;
                    ls[n] = x;
                }
                got = jc_phase_agreement(act, 4096, n_act, ls, n, per / 100.0, e);
            }
            if (want != want || got != got) {
                CHECK((want != want) == (got != got));
            } else {
                worst = fmax(worst, fabs(got - want));
                if (fabs(got - want) >= AGREE_NEAR)
                    printf("  unit %c end %ld period %g: C %.9g, Python %.9g\n", line[0], e, per, got, want);
                CHECK(fabs(got - want) < AGREE_NEAR);
            }
            n_checked++;
        } else if (line[0] == 'w') {
            static struct jb_dbn d;
            float w[JB_DBN_MAX_INTERVALS];
            double mem = strtod(line + 2, &p), width = strtod(p, &p), floor_ = strtod(p, &p);
            CHECK(jb_dbn_init(&d, 90.0, 180.0, 100.0) == 0);
            jrb_tempo_prior(&d, mem, width, floor_, w);
            for (int i = 0; i < d.n_intervals; i++, p = end) {
                double want = strtod(p, &end);
                CHECK(fabs(w[i] - want) < 1e-6);
            }
        }
    }
    free(buf);
    if (verbose)
        printf("units: %d comb checks, worst %.2g\n", n_checked, worst);
    CHECK(n_checked > 100);
}

static void test_related_and_consistent(void)
{
    CHECK(jrb_related(96.0, 128.0, 0.04) && jrb_related(170.0, 128.0, 0.04) && jrb_related(64.0 * 1.5, 128.0, 0.04));
    CHECK(!jrb_related(128.0, 128.0, 0.04) && !jrb_related(100.0, 128.0, 0.04) && !jrb_related(64.0, 128.0, 0.04));
    double b[60];
    for (int i = 0; i < 60; i++)
        b[i] = 1.0 + 0.5 * i + (i % 7 == 3 ? 0.01 : 0.0);
    CHECK(jrb_consistent(b, 60, 0.5, 48));
    CHECK(!jrb_consistent(b, 40, 0.5, 48));                       /* too few */
    CHECK(!jrb_consistent(b, 60, 0.4, 48));                       /* another grid */
    for (int i = 30; i < 60; i++)
        b[i] += (i % 2) * 0.2;                                   /* wandering */
    CHECK(!jrb_consistent(b, 60, 0.5, 48));
}

/* the switch on tuned-l16 (no gate, no prior, no robust tick) is today's follower: the same beats give the same state */
static void test_off_is_m2j(void)
{
    static struct jb_follow a, b;
    static struct jb_dbn d1, d2;
    double t = 0.3, bt;
    jb_follow_init(&a);
    jb_follow_init(&b);
    CHECK(a.m.gate == NULL && b.m.gate == NULL);
    for (int k = 0; k < 300; k++) {
        t += 60.0 / (k < 150 ? 128.0 : 133.0) + 0.003 * sin(k * 1.7);
        jb_follow_update(&a, t);
        jb_follow_update(&b, t);
        if (k == 200) {
            jb_follow_try_jump(&b, t, 0.45);                   /* no gate: the plain jump */
            jb_follow_jump(&a, t);
        }
    }
    CHECK(memcmp(a.x, b.x, sizeof a.x) == 0 && a.m.jumps == b.m.jumps && a.m.in_a_row == b.m.in_a_row);
    CHECK(jb_dbn_init(&d1, 90.0, 180.0, 100.0) == 0 && jb_dbn_init(&d2, 90.0, 180.0, 100.0) == 0);
    float ones[JB_DBN_MAX_INTERVALS], tw[JB_DBN_MAX_INTERVALS];
    for (int i = 0; i < JB_DBN_MAX_INTERVALS; i++)
        ones[i] = 1.0f;
    jb_dbn_set_prior(&d2, ones);                                /* a prior of ones is the plain DBN, bit for bit */
    for (int k = 0; k < 3000; k++) {
        float act = (k % 47 == 3) ? 0.9f : 0.02f;
        int r1 = jb_dbn_step(&d1, act, &bt), r2 = jb_dbn_step(&d2, act, &bt);
        CHECK(r1 == r2);
    }
    CHECK(memcmp(d1.fwd, d2.fwd, sizeof(float) * d1.n_states) == 0);
    jrb_tempo_prior(&d1, 128.0, 0.04, 0.1, tw);                  /* a real prior moves the mass toward 128 and its octaves */
    jb_dbn_set_prior(&d1, tw);
    for (int k = 0; k < 400; k++)
        jb_dbn_step(&d1, 0.02f, &bt);
    CHECK(d1.has_prior);
    jb_dbn_set_prior(&d1, NULL);
    CHECK(!d1.has_prior);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 && argv[1][0] != '-' ? argv[1] : "tests/robust_ref";
    verbose = !strcmp(argv[argc - 1], "-v");
    printf("sizeof struct jb_robust %zu, struct jrb_store %zu, struct jb_dbn %zu, struct jb_follow %zu\n",
           sizeof(struct jb_robust), sizeof(struct jrb_store), sizeof(struct jb_dbn), sizeof(struct jb_follow));
    test_related_and_consistent();
    test_off_is_m2j();
    test_units(dir);
    int n = test_runs(dir, 0);
    printf("robust: %d tracks against studies/follow-robust all-d-f60 (state %.0g s, confidences %.0g, agreement %.0g, gates "
           "and DBN beats exact)\n", n, STATE_S, CONF_NEAR, AGREE_NEAR);
    const char *sdir = argc > 2 && argv[2][0] != '-' ? argv[2] : "tests/skip_ref";
    int ns = test_runs(sdir, 1);
    printf("skip: %d tracks x ACT and PAT against rchain.py with the skip configs (causal view): %d re-syncs, %d from the activation\n",
           ns, s_resyncs_all, s_act_resyncs_all);
    printf("robust: %d checks, %d failed\n", checks, fails);
    return fails;
}
