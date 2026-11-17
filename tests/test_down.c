/* SPDX-License-Identifier: GPL-3.0-only */
/* The downbeat estimator (core/beat/jb_down.c) against beat-listener's live/downbeat.py on golden vectors
 * (tests/down_ref/, written by beat-listener's scripts/downbeat_golden.py): the same frames (81 log bands, uint16 / 8192)
 * and the same grid / stop / reset / nudge / AUTO BAR events in, every decision (count, time, state, phi, lead, the 4
 * scores) and every beat's 22 features out, to 1e-4 (relative above 1). Plus the state machine on its own.
 *
 * test_down [REF_DIR]   (default: tests/down_ref next to this file) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jb_down.h"

static int checks, fails;
#define TOL 1e-4

static void check(int ok, const char *what, const char *name, long line)
{
    checks++;
    if (!ok) {
        fails++;
        if (fails < 20)
            printf("FAIL %s: %s (expect line %ld)\n", name, what, line);
    }
}

static int close_to(double a, double b) { return fabs(a - b) <= TOL * (fabs(b) > 1.0 ? fabs(b) : 1.0); }

struct ev {
    long frame;
    char kind;
    double t, tb, p;
    int arg;
};

static struct ev *read_events(const char *path, size_t *n)
{
    FILE *f = fopen(path, "r");
    char line[512];
    size_t cap = 64;
    struct ev *e = malloc(cap * sizeof *e);
    *n = 0;
    if (!f || !e) {
        free(e);
        if (f)
            fclose(f);
        return NULL;
    }
    while (fgets(line, sizeof line, f)) {
        struct ev v;
        memset(&v, 0, sizeof v);
        if (line[0] == '#' || line[0] == '\n')
            continue;
        v.kind = line[0];
        if (v.kind == 'g') {
            if (sscanf(line + 1, "%ld %lf %lf %lf %d", &v.frame, &v.t, &v.tb, &v.p, &v.arg) != 5)
                continue;
        } else if (v.kind == 'a') {
            if (sscanf(line + 1, "%ld %d", &v.frame, &v.arg) != 2)
                continue;
        } else if (sscanf(line + 1, "%ld", &v.frame) != 1)
            continue;
        if (*n == cap) {
            struct ev *g = realloc(e, 2 * cap * sizeof *e);
            if (!g)
                break;
            e = g;
            cap *= 2;
        }
        e[(*n)++] = v;
    }
    fclose(f);
    return e;
}

/* the next expectation line of a kind ('d' or 'x') */
static int next_line(FILE *f, char kind, char *buf, size_t cap, long *line_no)
{
    while (fgets(buf, (int)cap, f)) {
        (*line_no)++;
        if (buf[0] == kind)
            return 1;
        if (buf[0] != '#' && buf[0] != 'd' && buf[0] != 'x')
            return 0;
        if (buf[0] != '#') {           /* the other kind came first: out of step */
            return -1;
        }
    }
    return 0;
}

static void check_decision(const struct jb_down *d, const char *buf, const char *name, long ln)
{
    struct jd_view v;
    long count;
    double t, lead, s[4];
    int state, phi, have;
    char tail[256];
    jd_view(d, &v);
    have = sscanf(buf + 1, "%ld %lf %d %d %lf %255[^\n]", &count, &t, &state, &phi, &lead, tail);
    check(have == 6, "d line parses", name, ln);
    if (have != 6)
        return;
    check(v.count == count, "decision count", name, ln);
    check(fabs(v.t_beat - t) < 1e-9, "decision time", name, ln);
    check(v.state == state, "state", name, ln);
    check(v.phi == phi, "phi", name, ln);
    check(close_to(v.lead, lead), "lead", name, ln);
    if (tail[0] == '-' && (tail[1] == ' ' || tail[1] == '\0')) {     /* "- - - -": no scores yet */
        check(!v.ready, "not ready yet", name, ln);
        return;
    }
    check(sscanf(tail, "%lf %lf %lf %lf", &s[0], &s[1], &s[2], &s[3]) == 4 && v.ready, "scores ready", name, ln);
    for (int q = 0; q < 4; q++)
        check(close_to(v.scores[q], s[q]), "score", name, ln);
}

static void check_features(const struct jb_down *d, const char *buf, const char *name, long ln)
{
    char *p = (char *)buf + 1, *end;
    long count = strtol(p, &end, 10);
    double t = strtod(end, &p);
    check(d->x_count == count, "entered count", name, ln);
    check(fabs(d->x_time - t) < 1e-9, "entered time", name, ln);
    for (int k = 0; k < JD_NF; k++) {
        double x = strtod(p, &end);
        check(end != p, "x line parses", name, ln);
        p = end;
        if (!close_to(d->x[k], x) && fails < 20)
            printf("  feature %d: C %.9g, Python %.9g\n", k, d->x[k], x);
        check(close_to(d->x[k], x), "feature", name, ln);
    }
}

static void golden(const char *dir, const char *name)
{
    char path[1024], buf[1024];
    FILE *fb, *fx;
    size_t nev = 0, ie = 0;
    struct ev *ev;
    static jd_seg_row seg[JD_SEGS];
    static struct jb_down d;
    uint16_t q[JD_BANDS];
    float L[JD_BANDS];
    long frame = 0, ln = 0;
    uint32_t seq = 0, entered = 0;
    int decisions = 0, beats = 0;
    snprintf(path, sizeof path, "%s/%s.events.txt", dir, name);
    ev = read_events(path, &nev);
    snprintf(path, sizeof path, "%s/%s.bands", dir, name);
    fb = fopen(path, "rb");
    snprintf(path, sizeof path, "%s/%s.expect.txt", dir, name);
    fx = fopen(path, "r");
    check(ev && fb && fx && nev > 0, "golden files open", name, 0);
    if (!ev || !fb || !fx) {
        free(ev);
        if (fb)
            fclose(fb);
        if (fx)
            fclose(fx);
        return;
    }
    jd_init(&d, seg);
    while (fread(q, sizeof q[0], JD_BANDS, fb) == JD_BANDS) {
        for (int b = 0; b < JD_BANDS; b++)
            L[b] = (float)q[b] / 8192.0f;
        jd_frame(&d, frame, L);
        if (d.seq != seq) {
            seq = d.seq;
            decisions++;
            check(next_line(fx, 'd', buf, sizeof buf, &ln) == 1, "a decision expected here", name, ln);
            check_decision(&d, buf, name, ln);
        }
        if (d.entered != entered) {
            entered = d.entered;
            beats++;
            check(next_line(fx, 'x', buf, sizeof buf, &ln) == 1, "an entered beat expected here", name, ln);
            check_features(&d, buf, name, ln);
        }
        for (; ie < nev && ev[ie].frame == frame; ie++) {
            const struct ev *e = &ev[ie];
            if (e->kind == 'g')
                check(jd_grid(&d, e->t, e->tb, e->p, e->arg) == 0, "grid", name, ln);
            else if (e->kind == 's')
                jd_stop(&d);
            else if (e->kind == 'r')
                jd_reset(&d);
            else if (e->kind == 'n')
                jd_nudge(&d);
            else if (e->kind == 'a')
                jd_set_auto(&d, e->arg);
        }
        frame++;
    }
    check(ie == nev, "every event replayed", name, ln);
    check(next_line(fx, 'd', buf, sizeof buf, &ln) == 0, "no expectation left over", name, ln);
    printf("%s: %ld frames, %zu events, %d decisions, %d beats' features matched\n", name, frame, nev, decisions, beats);
    free(ev);
    fclose(fb);
    fclose(fx);
}

/* the state machine without audio: the policy fed through the accumulators would need music, so only the edges */
static void states(void)
{
    static jd_seg_row seg[JD_SEGS];
    static struct jb_down d;
    struct jd_view v;
    float L[JD_BANDS];
    jd_init(&d, seg);
    jd_view(&d, &v);
    check(v.state == JD_LISTENING && v.phi == -1 && !v.ready, "starts listening", "states", 0);
    check(jd_grid(&d, 1.0, 1.0, 0.0, 1) == -1, "a zero period is refused", "states", 0);
    jd_nudge(&d);
    jd_view(&d, &v);
    check(v.state == JD_MANUAL, "a nudge: manual", "states", 0);
    jd_set_auto(&d, 1);
    jd_view(&d, &v);
    check(v.state == JD_LISTENING, "AUTO BAR picked again: listening", "states", 0);
    jd_set_auto(&d, 0);
    jd_nudge(&d);
    jd_view(&d, &v);
    check(v.state == JD_OFF, "off: a nudge leaves it off", "states", 0);
    jd_reset(&d);
    jd_view(&d, &v);
    check(v.state == JD_OFF, "off survives a reset", "states", 0);
    jd_set_auto(&d, 1);
    /* a steady grid at 120 BPM: one count a beat, the first decision with scores at the 9th counted beat */
    for (int b = 0; b < JD_BANDS; b++)
        L[b] = 0.2f;
    check(jd_grid(&d, 2.0, 2.0, 0.5, 1) == 0, "grid", "states", 0);
    for (int64_t f = 0; f < 700; f++) {
        L[0] = (f % 200) < 3 ? 2.0f : 0.2f;      /* a low hit every 4th beat */
        jd_frame(&d, f, L);
        if (f % 50 == 0 && f >= 200)
            jd_grid(&d, f / 100.0, 2.0, 0.5, 1);
    }
    jd_view(&d, &v);
    check(v.seq == 10 && v.count == 9 && v.ready, "ten counted beats, ready", "states", 0);
    check(jd_counting(&d) && jd_playing(&d), "counting", "states", 0);
    jd_stop(&d);
    jd_view(&d, &v);
    check(!jd_counting(&d) && !jd_playing(&d) && v.state == JD_LISTENING, "a stop resets", "states", 0);
}

int main(int argc, char **argv)
{
    char dir[1024];
    if (argc > 1)
        snprintf(dir, sizeof dir, "%s", argv[1]);
    else {
        const char *f = __FILE__, *slash = strrchr(f, '/');
        snprintf(dir, sizeof dir, "%.*s/down_ref", slash ? (int)(slash - f) : 1, slash ? f : ".");
    }
    printf("sizeof struct jb_down %zu, segment ring %zu bytes\n", sizeof(struct jb_down), sizeof(jd_seg_row) * JD_SEGS);
    states();
    golden(dir, "disco.00005");
    golden(dir, "pop.00020");
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
