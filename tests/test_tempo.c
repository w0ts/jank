/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the tuned beat follow (M2j) against beat-listener's studies/tempo-changes chain (tuned-l16): golden vectors
 * in tests/tempo_ref (beat-listener's scripts/tempo_golden.py). Exit status: the number of failed checks.
 *   NAME.track.txt  a synthetic track: its tracked beats and the network's activation
 *   NAME.txt        chain.py's replay with the device's causal view (weak.py `causal`), one line an operation; the C does
 *                   the same operations with the device's 256-frame activation ring: the follower and the weak-signal
 *                   hold (core/beat/jb_follow.c, jb_weak.c), the fast jump, the lines cleared on a jump, the strict and
 *                   the weak confidence and the gate (core/jank_conf.c). Each beat's state (to STATE_S), each 5th tick's
 *                   confidences (to CONF_NEAR) and every gate decision (exact) are compared.
 *   NAME.nc.txt     the same with the study's look-ahead view (the whole activation pushed first): the port of weak.py
 *                   as it was measured */
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_conf.h"
#include "jb_follow.h"
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

#define STATE_S 1e-9                   /* the glide's state (s): both are doubles, only the sums' order differs */
#define CONF_NEAR 1e-12
#define MAX_FRAMES 20000
#define MAX_BEATS 2000
#define THRESHOLD 80                   /* chain.Config.threshold 0.8 */

struct track {
    double dur;
    double beats[MAX_BEATS];
    int n_beats;
    float act[MAX_FRAMES];
    int n_act;
};

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

/* tempo_golden.py track_lines: dur / beats ... / rms N ... / act N ... (x 1e-3, float32 as numpy makes it) */
static int read_track(const char *path, struct track *tr)
{
    char *buf = slurp(path), *p, *end;
    if (!buf)
        return 0;
    memset(tr, 0, sizeof *tr);
    for (char *line = strtok(buf, "\n"); line; line = strtok(NULL, "\n")) {
        if (!strncmp(line, "dur ", 4))
            tr->dur = strtod(line + 4, NULL);
        else if (!strncmp(line, "beats", 5))
            for (p = line + 5; tr->n_beats < MAX_BEATS; p = end) {
                double v = strtod(p, &end);
                if (end == p)
                    break;
                tr->beats[tr->n_beats++] = v;
            }
        else if (!strncmp(line, "act ", 4)) {
            long n = strtol(line + 4, &p, 10);
            for (long i = 0; i < n && i < MAX_FRAMES; i++, p = end)
                tr->act[tr->n_act++] = (float)((double)strtol(p, &end, 10) / 1000.0);
        }
    }
    free(buf);
    return tr->dur > 0.0 && tr->n_beats > 10 && tr->n_act > 100;
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

struct run {
    struct jb_follow f;
    struct jb_weak w;
    struct jcf conf;
    uint8_t gated, was_running;
    double now;
    /* the last tick's */
    int has_bpm, has_grid, running;
    double bpm, t0, p, strict, loose, c;
};

struct stats {
    int beats, state_bad, count_bad, ticks, conf_bad, gate_bad, run_bad, coasted;
    double worst_x, worst_conf;
};

static void restart(struct run *r)
{
    jb_follow_init(&r->f);              /* (lock_beats JB_LOCK_BEATS: 16) */
    jw_reset(&r->w);                    /* (the activation stays: the Python's track) */
    jcf_init(&r->conf);
}

/* chain._replay's beat: the proposals already in, CoastBpm.update, the fast jump, the lines cleared on a jump */
static void beat(struct run *r, double t)
{
    int jumps = r->f.m.jumps;
    jw_update(&r->w, &r->f, t);
    jb_follow_fast_jump(&r->f, t, JF_FAST_JUMP_REL);
    if (r->f.m.jumps != jumps)
        jcf_clear_lines(&r->conf);
    jcf_beat(&r->conf, t);
}

/* chain._replay's tick after its beats: coast, then the confidence (conf "weak") and the gate */
static void tick(struct run *r, int silent)
{
    double now = r->now;
    jw_coast(&r->w, &r->f, now);
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
                                       &r->w.last_conf);
        r->strict = jcf_lines_confidence(r->conf.t, r->conf.n, r->conf.line, r->conf.lines, r->has_grid, r->p, now,
                                         JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
        r->c = jcf_weak_pick(r->strict, r->loose, r->gated, r->was_running);
    }
    r->gated = jcf_gate(r->gated, r->c, JCF_STOP, THRESHOLD);
    r->running = r->has_bpm && !r->gated && !silent;
    r->was_running = (uint8_t)r->running;
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
                   "coasted %u/%g fast %u/%g lines %u/%g\n", st->beats, f->m.jumps, s[5], f->m.misses, s[6],
                   f->m.in_a_row, s[7], r->w.n_strong, s[10], r->w.n_weak, s[11], (unsigned)r->w.coasted, s[12],
                   (unsigned)f->fast_jumps, s[13], r->conf.lines, s[14]);
        st->count_bad++;
    }
}

/* q has_bpm bpm has_grid t0 p strict loose conf gated running lines coasted */
static void check_tick(const struct run *r, const double *q, struct stats *st)
{
    double d = fmax(fabs(r->strict - q[5]), fmax(fabs(r->loose - q[6]), fabs(r->c - q[7])));
    st->ticks++;
    st->worst_conf = fmax(st->worst_conf, d);
    if (d > CONF_NEAR || r->has_bpm != (int)q[0] || r->has_grid != (int)q[2] || fabs(r->p - q[4]) > STATE_S)
        st->conf_bad++;
    if (r->gated != (uint8_t)q[8])
        st->gate_bad++;
    if (r->running != (int)q[9] || r->conf.lines != (uint16_t)q[10] || r->w.coasted != (uint32_t)q[11])
        st->run_bad++;
}

static struct track s_track;
static struct jw_store s_store;
static float s_whole[MAX_FRAMES];

static void test_run(const char *dir, const char *name, int causal)
{
    char path[512], line[512];
    static struct run r;
    struct stats st = {0};
    struct track *tr = &s_track;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *fp = fopen(path, "r");
    CHECK(fp != NULL);
    if (!fp)
        return;
    memset(&r, 0, sizeof r);
    jb_follow_init(&r.f);
    jcf_init(&r.conf);
    if (causal)
        jw_init(&r.w, &s_store, NULL, 0, 1);            /* the device's ring */
    else {
        jw_init(&r.w, &s_store, s_whole, MAX_FRAMES, 0);
        for (int k = 0; k < tr->n_act; k++)
            jw_act(&r.w, k, tr->act[k]);
    }
    while (fgets(line, sizeof line, fp)) {
        double v[16];
        if (line[0] == 't' && numbers(line, v, 1)) {
            r.now = v[0];
            if (causal) {
                int64_t upto = (int64_t)rint(r.now * JW_FPS);
                for (int64_t k = r.w.act_n; k <= upto && k < tr->n_act; k++)
                    jw_act(&r.w, k, tr->act[k]);
            }
        } else if (line[0] == 'x')
            restart(&r);
        else if (line[0] == 'r' && numbers(line, v, 3))
            jb_follow_propose(&r.f, v[0], v[1], 1, v[2]);
        else if (line[0] == 'b' && numbers(line, v, 1))
            beat(&r, v[0]);
        else if (line[0] == 's') {
            CHECK(numbers(line, v, 15));
            check_state(&r, v, &st);
        } else if (line[0] == 'c' && numbers(line, v, 1))
            tick(&r, (int)v[0]);
        else if (line[0] == 'q') {
            CHECK(numbers(line, v, 12));
            check_tick(&r, v, &st);
        }
    }
    fclose(fp);
    if (verbose)
        printf("%-20s %d beats (glide worst %.2g s), %d ticks (conf worst %.2g); bad: state %d counts %d conf %d gate %d "
               "run %d; coasted %u, weak beats %u, fast jumps %u, jumps %d\n", name, st.beats, st.worst_x, st.ticks,
               st.worst_conf, st.state_bad, st.count_bad, st.conf_bad, st.gate_bad, st.run_bad, (unsigned)r.w.coasted,
               (unsigned)r.w.weak_beats, (unsigned)r.f.fast_jumps, r.f.m.jumps);
    CHECK(st.beats > 80 && st.ticks > 200);
    CHECK(st.state_bad == 0);
    CHECK(st.count_bad == 0);
    CHECK(st.conf_bad == 0);
    CHECK(st.gate_bad == 0);
    CHECK(st.run_bad == 0);
}

static int test_runs(const char *dir)
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
        CHECK(read_track(path, &s_track));
        snprintf(path, sizeof path, "%s.txt", base);
        test_run(dir, path, 1);
        snprintf(path, sizeof path, "%s.nc.txt", base);
        test_run(dir, path, 0);
        n++;
    }
    closedir(d);
    CHECK(n >= 10);
    return n;
}

/* ---- units ---- */

/* np.percentile(x, 75) and np.median on float32, as the module makes them (weak.is_strong, weak.weak_peak) */
static void test_numpy_pieces(void)
{
    static struct jb_weak w;
    double obs = 0.0;
    jw_init(&w, &s_store, NULL, 0, 1);
    for (int k = 0; k < 700; k++)
        jw_act(&w, k, k == 503 ? 0.2f : k == 504 ? 0.1f : k == 502 ? 0.05f : 0.01f);
    CHECK(jw_weak_peak(&w, 5.0, 0.46, &obs) && fabs(obs - 5.03) < 0.006);
    CHECK(!jw_weak_peak(&w, 6.0, 0.46, &obs));                 /* nothing salient there */
    CHECK(!jw_weak_peak(&w, 6.5, 0.46, &obs));                 /* its evidence not all in yet (the causal view) */
    CHECK(fabs(jw_peak_near(&w, 5.0) - 0.2f) < 1e-9 && jw_peak_near(&w, 4.9) == 0.01f);
    jw_act(&w, 1000000, 0.5f);                                  /* a long gap: only the ring's length is walked */
    CHECK(w.act_n == 1000001 && jw_peak_near(&w, 10000.0) == 0.5 && jw_peak_near(&w, 9990.0) == 0.0);
    jw_init(&w, NULL, NULL, 0, 1);
    CHECK(!jw_on(&w));
}

/* off (no store: no PSRAM on the device), the weak follower is the follower */
static void test_off_is_m2i(void)
{
    static struct jb_follow a, b;
    static struct jb_weak w;
    double t = 0.3;
    jb_follow_init(&a);
    jb_follow_init(&b);
    jw_init(&w, NULL, NULL, 0, 1);
    for (int k = 0; k < 200; k++) {
        t += 60.0 / (k < 100 ? 128.0 : 133.0) + 0.002 * sin(k * 1.3);
        jb_follow_update(&a, t);
        CHECK(jw_update(&w, &b, t) == 1);
        jw_coast(&w, &b, t + 0.1);
    }
    CHECK(memcmp(a.x, b.x, sizeof a.x) == 0 && a.m.jumps == b.m.jumps && a.has_kf == b.has_kf);
}

/* the fast jump: a consistent new grid 7 % away is taken at once; an octave of the glide is not */
static void test_fast_jump(void)
{
    static struct jb_follow f;
    double t = 0.5;
    jb_follow_init(&f);
    for (int k = 0; k < 64; k++)
        jb_follow_update(&f, t += 0.5);
    CHECK(f.has_kf && fabs(f.x[1] - 0.5) < 1e-6);
    CHECK(!jb_follow_fast_jump(&f, t, JF_FAST_JUMP_REL));      /* on its own grid */
    struct jb_follow g = f;
    for (int k = 0; k < 16; k++)                                /* the recent beats at 1 s: an octave down */
        jb_dq_push(&g.m.recent, t += 1.0);
    CHECK(!jb_follow_fast_jump(&g, t, JF_FAST_JUMP_REL) && g.fast_jumps == 0);
    for (int k = 0; k < 16; k++)                                /* at 0.5 / 1.07 s: a new tempo */
        jb_dq_push(&f.m.recent, t += 0.5 / 1.07);
    int jumps = f.m.jumps;
    CHECK(jb_follow_fast_jump(&f, t, JF_FAST_JUMP_REL) && f.fast_jumps == 1 && f.m.jumps == jumps + 1);
    CHECK(f.has_kf && fabs(f.x[1] - 0.5 / 1.07) < 1e-3);
    CHECK(!jb_follow_fast_jump(&f, t, 0.0));                    /* rel 0: off */
}

/* the weak confidence: unobserved lines do not count; a strong beat off the lines does; too few observed: held */
static void test_weak_confidence(void)
{
    double lines[20], strong[20], last = 0.42;
    for (int i = 0; i < 20; i++)
        lines[i] = 1.0 + 0.5 * i;      /* the window at 10.5 s: 2.5 .. 10.4 */
    for (int i = 0; i < 8; i++)
        strong[i] = lines[4 + i];       /* beats on 3.0 .. 6.5, then nothing: the later lines are not observed */
    double c = jcf_weak_confidence(strong, 8, strong, 8, NULL, 0, lines, 20, 1, 0.5, 10.5, JW_MIN_OBS, &last);
    CHECK(fabs(c - 1.0) < 1e-12 && last == c);
    double off[9] = {3.0, 3.5, 4.0, 4.25, 4.5, 5.0, 5.5, 6.0, 6.5};    /* a strong beat off the lines */
    c = jcf_weak_confidence(off, 9, off, 9, NULL, 0, lines, 20, 1, 0.5, 10.5, JW_MIN_OBS, &last);
    CHECK(fabs(c - 2.0 * (8.0 / 9.0) / (1.0 + 8.0 / 9.0)) < 1e-12);
    last = 0.42;
    c = jcf_weak_confidence(strong, 2, strong, 2, NULL, 0, lines, 20, 1, 0.5, 10.5, JW_MIN_OBS, &last);
    CHECK(c == 0.42);                                           /* 2 observed lines: the last value holds */
    CHECK(jcf_weak_confidence(strong, 6, strong, 6, NULL, 0, lines, 20, 0, 0.0, 10.5, JW_MIN_OBS, &last) == 0.0
          && last == 0.0);
    CHECK(jcf_weak_pick(0.3, 0.9, 1, 1) == 0.3 && jcf_weak_pick(0.3, 0.9, 0, 0) == 0.3 && jcf_weak_pick(0.3, 0.9, 0, 1) == 0.9);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "tests/tempo_ref";
    verbose = !strcmp(argv[argc - 1], "-v");
    printf("sizeof struct jb_weak %zu, struct jw_store %zu, struct jb_follow %zu\n", sizeof(struct jb_weak),
           sizeof(struct jw_store), sizeof(struct jb_follow));
    test_numpy_pieces();
    test_off_is_m2i();
    test_fast_jump();
    test_weak_confidence();
    int n = test_runs(dir);
    printf("tempo: %d tracks x 2 views against studies/tempo-changes tuned-l16 (state %.0g s, confidences %.0g, gates "
           "exact)\n", n, STATE_S, CONF_NEAR);
    printf("tempo: %d checks, %d failed\n", checks, fails);
    return fails;
}
