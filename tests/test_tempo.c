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
 *                   as it was measured
 * And LOST (core/beat/jb_lost.h, applied as firmware/main/beat.c follow_conf does, the gate held once the outputs run):
 * synthetic audio (a kick on the beats over a pad) and activation: a drop of the same song holds, near silence stops within
 * 2 lines, the beat back runs again after 3; a weak activation with the kick there holds; jb_kick.h, the off-grid count
 * and jb_song.h on their own. */
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jank_conf.h"
#include "jb_follow.h"
#include "jb_kick.h"
#include "jb_lost.h"
#include "jb_song.h"
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
    struct jb_lost lost;
    const struct jb_kick *kick;         /* LOST's kick band and level (NULL: none) */
    uint8_t use_lost;                   /* LOST as beat.c (the golden runs: 0, as the Python) */
    uint8_t gated, was_running, is_lost;
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
                                       &r->w.last_conf, NULL);
        r->strict = jcf_lines_confidence(r->conf.t, r->conf.n, r->conf.line, r->conf.lines, r->has_grid, r->p, now,
                                         JCF_WINDOW_S, JCF_DELAY_S, JCF_TOL);
        if (r->use_lost) {
            uint8_t back = 0;
            r->is_lost = (uint8_t)jl_step(&r->lost, jw_strong(&r->w), r->w.n_strong, r->kick, r->has_bpm && r->has_grid, r->t0,
                                          r->p, now, r->was_running, 0, &back);
            if (back) {
                r->gated = 0;
                r->was_running = 1;
            }
        }
        r->c = jcf_weak_pick(r->strict, r->loose, r->gated, r->was_running);
    }
    if (silent && r->use_lost) {
        jl_clear(&r->lost);
        r->is_lost = 0;
    }
    {
        const uint8_t was = r->gated;
        r->gated = jcf_gate(r->gated, r->c, JCF_STOP, THRESHOLD);
        if (r->use_lost && !was && r->was_running)
            r->gated = 0;               /* (beat.c: once running, the confidence no longer stops the outputs; LOST does) */
    }
    if (r->is_lost)
        r->gated = 1;
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

/* the HOLD on show (M2k): a held 100 % eases toward the threshold (most of the way in JCF_HOLD_EASE_S), never above what
 * is held, never raised from under the threshold; evidence back: the measured value at once */
static void test_hold_shown(void)
{
    double shown = 1.0, th = 0.8;
    for (int k = 0; k < 4; k++)                                 /* 1 s of 0.25 s steps: down a little */
        shown = jcf_show_step(shown, 1.0, 1, th, 0.25);
    CHECK(shown < 0.96 && shown > 0.9);
    for (int k = 0; k < 28; k++)                                /* 8 s in all: within 5 % of the way to the threshold */
        shown = jcf_show_step(shown, 1.0, 1, th, 0.25);
    CHECK(shown > th && shown - th < 0.05 * (1.0 - th) + 1e-12);
    CHECK(jcf_show_step(shown, 0.02, 0, th, 0.25) == 0.02);    /* the first real recount: snapped to it */
    CHECK(jcf_show_step(0.5, 0.5, 1, th, 1.0) == 0.5);          /* held under the threshold: kept */
    CHECK(jcf_show_step(0.95, 0.9, 1, th, 0.0) == 0.9);         /* never above the held value */
    CHECK(jcf_pct(jcf_show_step(1.0, 1.0, 1, th, 8.0)) == 81);  /* 8 s in one step: 95 % of the way: 81 % */
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
    double c = jcf_weak_confidence(strong, 8, strong, 8, NULL, 0, lines, 20, 1, 0.5, 10.5, JW_MIN_OBS, &last, NULL);
    CHECK(fabs(c - 1.0) < 1e-12 && last == c);
    double off[9] = {3.0, 3.5, 4.0, 4.25, 4.5, 5.0, 5.5, 6.0, 6.5};    /* a strong beat off the lines */
    c = jcf_weak_confidence(off, 9, off, 9, NULL, 0, lines, 20, 1, 0.5, 10.5, JW_MIN_OBS, &last, NULL);
    CHECK(fabs(c - 2.0 * (8.0 / 9.0) / (1.0 + 8.0 / 9.0)) < 1e-12);
    last = 0.42;
    uint8_t held = 9;
    c = jcf_weak_confidence(strong, 2, strong, 2, NULL, 0, lines, 20, 1, 0.5, 10.5, JW_MIN_OBS, &last, &held);
    CHECK(c == 0.42 && held == 1);                              /* 2 observed lines: the last value holds: a HOLD */
    c = jcf_weak_confidence(strong, 8, strong, 8, NULL, 0, lines, 20, 1, 0.5, 10.5, JW_MIN_OBS, &last, &held);
    CHECK(fabs(c - 1.0) < 1e-12 && held == 0);                  /* evidence: measured, no hold */
    held = 9;
    CHECK(jcf_weak_confidence(strong, 8, strong, 8, NULL, 0, lines, 20, 0, 0.0, 10.5, JW_MIN_OBS, &last, &held) == 0.0
          && held == 0);
    CHECK(jcf_weak_confidence(strong, 6, strong, 6, NULL, 0, lines, 20, 0, 0.0, 10.5, JW_MIN_OBS, &last, NULL) == 0.0
          && last == 0.0);
    CHECK(jcf_weak_pick(0.3, 0.9, 1, 1) == 0.3 && jcf_weak_pick(0.3, 0.9, 0, 0) == 0.3 && jcf_weak_pick(0.3, 0.9, 0, 1) == 0.9);
}

/* LOST on a synthetic track at 120 BPM (beats every 0.5 s from 1.0 s). Audio: a pad (330 Hz, -23 dBFS) and a kick (60 Hz,
 * 50 ms decay) on the beats; the activation 0.8 on the beats, 0.01 between. From `from` to `to` the passage KIND:
 *   DROP  no kick, activation 0.01 (the DBN keeps ticking on the grid from its tempo model): a drop of the same song
 *   QUIET everything 40 dB down (over the -60 dBFS silence gate, under the song by more than JL_QUIET_REL)
 *   WEAK  the kick plays, the activation is 0.01 (the online network deaf to it, as on Rhyw - Driptych)
 * Fed as beat.c does: the samples to the kick band, the activation per frame, a beat 3 frames late, the coast, the step
 * every 0.25 s and after each beat. stop / back: the first stop at or after `from`, the first run again after it (-1:
 * none); ran_before: the share of the steps from 20 s to `from` that ran. */
enum lost_kind { LK_DROP, LK_QUIET, LK_WEAK };

struct lost_res {
    double stop, back, ran_before;
    int stops;
};

static struct jb_kick s_kick;

/* one hop of the synthetic audio: the pad, and the kick that began at kick_t (< 0: none) */
static void synth_hop(int16_t *x, double now, double kick_t, double gain)
{
    for (int i = 0; i < 441; i++) {
        const double ts = now + i / 44100.0;
        double v = 0.1 * sin(2 * M_PI * 330.0 * ts);
        if (kick_t >= 0 && ts >= kick_t)
            v += 0.7 * exp(-(ts - kick_t) / 0.05) * sin(2 * M_PI * 60.0 * (ts - kick_t));
        x[i] = (int16_t)lrint(32767.0 * gain * v);
    }
}

static struct lost_res lost_run(int use_lost, enum lost_kind kind, double from, double to)
{
    static struct run r;
    struct lost_res o = {-1.0, -1.0, 0.0, 0};
    memset(&r, 0, sizeof r);
    jb_follow_init(&r.f);
    jcf_init(&r.conf);
    jw_init(&r.w, &s_store, NULL, 0, 1);
    jl_init(&r.lost);
    jbk_init(&s_kick);
    r.kick = &s_kick;
    r.use_lost = (uint8_t)use_lost;
    int64_t pend = -1;
    double last_step = 0.0, kick_t = -1.0;
    int was = 0, before = 0, before_n = 0;
    int16_t x[441];
    for (int64_t k = 0; k < 8000; k++) {
        const double now = (double)k / JW_FPS;
        const int beat_frame = k >= 100 && (k - 100) % 50 == 0;
        const int in = now >= from && now < to;
        if (beat_frame)
            kick_t = in && kind == LK_DROP ? -1.0 : now;
        synth_hop(x, now, kick_t, in && kind == LK_QUIET ? 0.01 : 1.0);
        jbk_push_s16(&s_kick, x, 441);
        jw_act(&r.w, k, beat_frame && !in ? 0.8f : 0.01f);
        if (beat_frame)
            pend = k;
        int fed = 0;
        if (pend >= 0 && k >= pend + JW_STRONG_HALF) {
            beat(&r, (double)pend / JW_FPS);
            pend = -1;
            fed = 1;
        }
        jw_coast(&r.w, &r.f, now);
        if (!fed && now - last_step < 0.25)
            continue;
        last_step = now;
        r.now = now;
        tick(&r, 0);
        if (now >= 20.0 && now < from) {
            before += r.running;
            before_n++;
        }
        if (was && !r.running && now >= 20.0)
            o.stops++;
        if (o.stop < 0 && now >= from && !r.running)
            o.stop = now;
        if (o.stop >= 0 && o.back < 0 && r.running)
            o.back = now;
        was = r.running;
    }
    o.ran_before = before_n ? (double)before / before_n : 0.0;
    return o;
}

static void test_kick_unit(void)
{
    /* kicks at 10.0 s and 10.5 s over the pad: evidence on those lines, none a quarter period off, none with no kick */
    jbk_init(&s_kick);
    int16_t x[441];
    for (int64_t k = 0; k < 1200; k++) {
        const double now = (double)k / 100.0;
        synth_hop(x, now, now >= 11.0 ? -1.0 : now >= 10.5 ? 10.5 : now >= 10.0 ? 10.0 : -1.0, 1.0);
        jbk_push_s16(&s_kick, x, 441);
    }
    CHECK(s_kick.frames == 1200);
    CHECK(jbk_on_line(&s_kick, 10.0, 0.5, JL_TOL) && jbk_on_line(&s_kick, 10.5, 0.5, JL_TOL));
    CHECK(!jbk_on_line(&s_kick, 10.25, 0.5, JL_TOL) && !jbk_on_line(&s_kick, 11.0, 0.5, JL_TOL));
    CHECK(!jbk_on_line(&s_kick, 5.0, 0.5, JL_TOL));                      /* no longer in the ring (2.56 s) */
    const double lv = jbk_line_level(&s_kick, 11.0, 0.5);                 /* the pad alone: 0.1 x full scale, -23 dBFS */
    CHECK(lv > -24.0 && lv < -22.0);
    CHECK(jbk_line_level(&s_kick, 11.8, 0.5) == JBK_NONE);                 /* frames not in yet */
}

static void test_lost_unit(void)
{
    struct jb_lost l;
    jl_init(&l);
    uint8_t back = 9;
    const double none[1] = {0.0};
    CHECK(jl_step(&l, none, 0, NULL, 0, 0.0, 0.5, 10.0, 1, 0, &back) == 0 && back == 0 && l.next_line < 0);
    /* a new grid is judged from JL_SEED_S back: at 12.05 s the lines 10.0 .. 11.5 (each a period + JL_LAG_S after it).
     * No beat, no kick band: they hold (nothing counts) */
    CHECK(jl_step(&l, none, 0, NULL, 1, 0.0, 0.5, 12.05, 1, 0, &back) == 0 && l.unfit_run == 4 && l.off_n == 0 && !l.lost);
    /* strong beats 0.2 period off the grid: OFF once JL_OFF_LINES lines are judged (10.0 .. 13.5, by 14.05 s) */
    jl_clear(&l);
    double off[16];
    for (int i = 0; i < 16; i++)
        off[i] = 10.1 + 0.5 * i;
    CHECK(jl_step(&l, off, 16, NULL, 1, 0.0, 0.5, 12.05, 1, 0, &back) == 0 && l.off_n == 4);
    CHECK(jl_step(&l, off, 16, NULL, 1, 0.0, 0.5, 14.05, 1, 0, &back) == 1 && l.cause == JLO_OFF && l.losts[JLO_OFF] == 1);
    /* strong beats on the grid again (14.0, 14.5, 15.0): back once JL_BACK_LINES are judged */
    const double fit[3] = {14.0, 14.5, 15.0};
    CHECK(jl_step(&l, fit, 3, NULL, 1, 0.0, 0.5, 15.05, 0, 0, &back) == 1 && back == 0 && l.fit_run == 2);
    CHECK(jl_step(&l, fit, 3, NULL, 1, 0.0, 0.5, 15.55, 0, 0, &back) == 0 && back == 1 && l.fit_run == JL_BACK_LINES);
    /* a new song (the option) counts only after JL_SONG_LINES lines with no fit (15.5 .. 17.0) */
    CHECK(jl_step(&l, fit, 3, NULL, 1, 0.0, 0.5, 16.55, 1, 1, &back) == 0 && l.unfit_run == 2);
    CHECK(jl_step(&l, fit, 3, NULL, 1, 0.0, 0.5, 17.55, 1, 1, &back) == 1 && l.cause == JLO_SONG && l.unfit_run == JL_SONG_LINES);
    jl_clear(&l);
    CHECK(!l.lost && l.next_line < 0 && l.losts[JLO_SONG] == 1 && l.losts[JLO_OFF] == 1);
}

static void test_song_unit(void)
{
    /* 40 s of one harmony, then another: NEW within JS_RECENT blocks of the change; one harmony all along: never */
    static struct jb_song s;
    js_init(&s);
    float a[JS_BANDS], b[JS_BANDS];
    int used = 0;
    for (int i = 0; i < JS_BANDS; i++) {
        used += s.pc[i] >= 0;
        a[i] = s.pc[i] == 0 || s.pc[i] == 4 || s.pc[i] == 7 ? 1.0f : 0.2f;     /* C E G */
        b[i] = s.pc[i] == 1 || s.pc[i] == 6 || s.pc[i] == 10 ? 1.0f : 0.2f;    /* C# F# A# */
    }
    CHECK(used >= 30);
    int first_new = -1;
    for (int f = 0; f < 6000; f++) {
        js_frame(&s, f < 4000 ? a : b);
        if (first_new < 0 && js_new(&s))
            first_new = f;
    }
    CHECK(first_new >= 4000 && first_new < 4000 + JS_RECENT * JS_BLOCK);
    js_reset(&s);
    int any = 0;
    for (int f = 0; f < 12000; f++) {
        js_frame(&s, a);
        any |= js_new(&s);
    }
    CHECK(!any && s.n_blocks == JS_BLOCKS);
}

static void test_lost(void)
{
    /* a 20 s drop of the same song after the last beat at 39.5 s: held through */
    const struct lost_res drop = lost_run(1, LK_DROP, 39.75, 60.0);
    printf("lost: a drop: %d stops, ran %.2f before\n", drop.stops, drop.ran_before);
    CHECK(drop.ran_before > 0.99 && drop.stops == 0 && drop.stop < 0);
    /* near silence (40 dB down) from 39.75 s: lines 40.0 and 40.5 quiet, judged a period + JL_LAG_S after: stopped by
     * 41.05 s (the next step, 0.25 s, at the latest); the music back at 60.0 s: 3 lines (60.0 .. 61.0) fitted by 61.55 s */
    const struct lost_res q = lost_run(1, LK_QUIET, 39.75, 60.0);
    printf("lost: near silence: stop %.2f s after it began, back %.2f s after the beat, %d stops\n", q.stop - 39.75,
           q.back - 60.0, q.stops);
    CHECK(q.ran_before > 0.99 && q.stops == 1);
    CHECK(q.stop >= 40.5 + 0.5 + JL_LAG_S && q.stop <= 40.5 + 0.5 + JL_LAG_S + 0.25);
    CHECK(q.back >= 61.0 + 0.5 + JL_LAG_S && q.back <= 61.0 + 0.5 + JL_LAG_S + 0.25);
    /* the kick there, the network deaf to it: held; a steady beat: never stopped */
    const struct lost_res w = lost_run(1, LK_WEAK, 39.75, 60.0);
    CHECK(w.ran_before > 0.99 && w.stops == 0 && w.stop < 0);
    const struct lost_res steady = lost_run(1, LK_DROP, 1000.0, 1000.0);
    CHECK(steady.stops == 0 && steady.ran_before > 0.99);
    test_kick_unit();
    test_lost_unit();
    test_song_unit();
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
    test_hold_shown();
    test_lost();
    int n = test_runs(dir);
    printf("tempo: %d tracks x 2 views against studies/tempo-changes tuned-l16 (state %.0g s, confidences %.0g, gates "
           "exact)\n", n, STATE_S, CONF_NEAR);
    printf("tempo: %d checks, %d failed\n", checks, fails);
    return fails;
}
