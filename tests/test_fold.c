/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of AUDIO mode's fold and AUTO RANGE (core/jank_fold.c). Exit status: the number of failed checks. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "jank_fold.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static int near(double a, double b, double tol) { return fabs(a - b) <= tol; }

/* a lock: no tempo, then a tempo; returns the output tempo */
static double lock(struct jfo *f, double bpm, double session, uint32_t now)
{
    jfo_step(f, 0, 0.0, f->jumps, session, now);
    jfo_step(f, 1, bpm, f->jumps, session, now + 10u);
    return bpm * jfo_factor(f);
}

/* a jump of the tracker to bpm (the same lock, its jumps counted one more) */
static double jump(struct jfo *f, double bpm, double session, uint32_t now)
{
    jfo_step(f, 1, bpm, f->jumps + 1, session, now);
    return bpm * jfo_factor(f);
}

static void test_names_and_user(void)
{
    struct jfo f;
    jfo_init(&f);
    CHECK(f.exp == 0 && jfo_factor(&f) == 1.0 && !strcmp(jfo_name(0), "1"));
    CHECK(jfo_user(&f, 1) == 1 && jfo_user(&f, 1) == 2 && jfo_user(&f, 1) == 2);    /* x4 at most */
    CHECK(jfo_factor(&f) == 4.0 && !strcmp(jfo_name(2), "4") && f.manual);
    CHECK(jfo_user(&f, 0) == 0);
    CHECK(jfo_user(&f, -1) == -1 && jfo_user(&f, -1) == -2 && jfo_user(&f, -1) == -2);   /* 0.25 at least */
    CHECK(jfo_factor(&f) == 0.25 && !strcmp(jfo_name(-2), "0.25") && !strcmp(jfo_name(-1), "0.5") && !strcmp(jfo_name(7), "1"));
    jfo_reset(&f);
    CHECK(f.exp == 0 && !f.manual);
}

/* the user's examples: the new detection is ranged to the previous output */
static void test_auto_range_examples(void)
{
    struct jfo f;
    jfo_init(&f);
    CHECK(lock(&f, 140.0, 0.0, 1000) == 140.0 && f.exp == 0);       /* no reference yet: as detected */
    CHECK(lock(&f, 70.0, 0.0, 20000) == 140.0 && f.exp == 1);       /* a new lock 19 s later: x2 */
    jfo_init(&f);
    lock(&f, 128.0, 0.0, 0);
    CHECK(lock(&f, 64.0, 0.0, 5000) == 128.0);
    jfo_init(&f);
    lock(&f, 175.0, 0.0, 0);
    CHECK(lock(&f, 90.0, 0.0, 5000) == 180.0);                      /* 90 after 175: x2 = 180 (+2.9 %) */
    jfo_init(&f);
    lock(&f, 128.0, 0.0, 0);
    CHECK(lock(&f, 132.0, 0.0, 5000) == 132.0 && f.exp == 0);       /* a genuine change: followed */
    CHECK(lock(&f, 90.0, 0.0, 10000) == 90.0 && f.exp == 0);        /* 90 after 132 is not 180: followed */
    CHECK(jfo_best(90.0, 132.0) == 1 && jfo_range(90.0, 132.0) == 0);
    jfo_init(&f);
    lock(&f, 140.0, 0.0, 0);
    CHECK(lock(&f, 70.0, 0.0, JFO_RECENT_MS + 1000u) == 70.0);      /* the last output too old: no fold */
    jfo_init(&f);
    lock(&f, 140.0, 0.0, 0);
    CHECK(lock(&f, 70.0, 140.0, JFO_RECENT_MS + 1000u) == 140.0);   /* ... but the session still plays at 140: ranged */
    CHECK(jfo_best(30.0, 140.0) == 2 && jfo_best(560.0, 140.0) == -2 && jfo_best(0.0, 140.0) == 0);
}

/* a user's fold wins for the current tempo; the next new tempo is ranged against what the user chose */
static void test_manual_precedence(void)
{
    struct jfo f;
    jfo_init(&f);
    lock(&f, 128.0, 0.0, 0);
    jfo_user(&f, -1);                                              /* the user halves: 64 out */
    jfo_step(&f, 1, 128.0, f.jumps, 0.0, 100);
    CHECK(f.exp == -1 && f.manual && near(f.out_bpm, 64.0, 1e-9));
    jfo_step(&f, 1, 128.4, f.jumps, 0.0, 200);                     /* the same tempo going on: kept */
    CHECK(f.exp == -1 && f.manual);
    CHECK(jump(&f, 131.0, 0.0, 300) == 65.5 && f.exp == -1);       /* a jump: ranged to the user's 64, so /2 stays */
    jfo_init(&f);
    lock(&f, 128.0, 0.0, 0);
    jfo_user(&f, -1);
    jfo_step(&f, 1, 128.0, f.jumps, 0.0, 100);
    CHECK(lock(&f, 130.0, 0.0, 5000) == 65.0 && f.exp == -1 && !f.manual);   /* next track: ranged to the user's 64 */
    jfo_init(&f);
    lock(&f, 128.0, 0.0, 0);
    jfo_user(&f, 1);
    jfo_step(&f, 1, 128.0, f.jumps, 0.0, 100);                     /* 256 out, the user's */
    CHECK(lock(&f, 100.0, 0.0, 5000) == 200.0 && f.exp == 1);      /* not an octave of 256: no range match, the user's x2 kept */
}

/* an octave flip within a track: the output tempo stays */
static void test_flip_within_track(void)
{
    struct jfo f;
    jfo_init(&f);
    lock(&f, 128.0, 0.0, 0);
    CHECK(jump(&f, 64.0, 128.0, 1000) == 128.0 && f.exp == 1);      /* down an octave: x2 */
    CHECK(jump(&f, 128.0, 128.0, 2000) == 128.0 && f.exp == 0);     /* back */
    CHECK(jump(&f, 256.0, 128.0, 3000) == 128.0 && f.exp == -1);    /* up an octave: /2 */
    CHECK(jump(&f, 128.0, 128.0, 4000) == 128.0 && f.exp == 0);
    CHECK(jump(&f, 66.0, 128.0, 5000) == 132.0 && f.exp == 1);      /* 3 % off the octave: still a flip */
    jfo_init(&f);
    lock(&f, 128.0, 0.0, 0);
    CHECK(jump(&f, 135.0, 128.0, 1000) == 135.0 && f.exp == 0);     /* a real change: followed */
    CHECK(jump(&f, 60.0, 135.0, 2000) == 60.0 && f.exp == 0);       /* 120 is 11 % from 135: not a flip, followed */
}

/* the folded grid: x2 keeps every tracked beat; /2 picks the beats that keep the output grid's count */
static void test_grid(void)
{
    struct jfo f;
    int64_t beat;
    double period;
    jfo_init(&f);
    jfo_grid(&f, 10000000, 500000.0, 0, 0.0, 0.0, &beat, &period);
    CHECK(beat == 10000000 && period == 500000.0);
    f.exp = 1;
    jfo_grid(&f, 10000000, 937500.0, 1, 7.0, 468750.0, &beat, &period);   /* a tracker at 64 under a 128 output, x2 */
    CHECK(beat == 10000000 && period == 468750.0);
    /* the user's /2 at 120 BPM: the tracked beat at session count 5 -> the grid on counts 4, 6, ...: the bar's 1 stays */
    jfo_init(&f);
    jfo_user(&f, -1);
    jfo_grid(&f, 10000000, 500000.0, 1, 5.0, 500000.0, &beat, &period);
    CHECK(beat == 9500000 && period == 1000000.0);
    jfo_grid(&f, 10500000, 500000.0, 1, 0.0, 500000.0, &beat, &period);   /* count 6: on that grid */
    CHECK(beat == 10500000);
    jfo_grid(&f, 11000000, 500000.0, 1, 0.0, 500000.0, &beat, &period);   /* count 7: the grid's beat before it */
    CHECK(beat == 10500000);
    /* AUTO RANGE /2 under a tracker gone to 256 (128 output): the tracked beat at count 9.5 is between beats: the one
     * before it (count 9) is taken, so the count goes on */
    jfo_init(&f);
    f.exp = -1;
    jfo_grid(&f, 10234375, 234375.0, 1, 9.5, 468750.0, &beat, &period);
    CHECK(beat == 10000000 && period == 468750.0);
    jfo_grid(&f, 10468750, 234375.0, 1, 0.0, 468750.0, &beat, &period);   /* an on-beat: itself */
    CHECK(beat == 10468750);
    /* repeated flips and a slowly moving tracked period: the anchor moves with the beats, nothing adds up */
    {
        int64_t t = 10468750, last = beat;
        double p = 234375.0;
        int ok = 1;
        for (int i = 1; i <= 4000; i++) {
            p += (i % 2 ? 1.0 : -1.0) * 3.0;                         /* +-3 us of jitter on the period */
            t += (int64_t)llround(p);
            jfo_grid(&f, t, p, 1, 0.0, 468750.0, &beat, &period);
            if (beat != t && beat != t - (int64_t)llround(p))
                ok = 0;
            if (i % 2 == 0 && beat != t)                             /* every second tracked beat is the on-beat */
                ok = 0;
            last = beat;
        }
        CHECK(ok && last == t);
    }
    /* x2 and back: a flip back to fold 1 is the tracked grid itself */
    f.exp = 0;
    jfo_grid(&f, 20000000, 468750.0, 1, 3.0, 468750.0, &beat, &period);
    CHECK(beat == 20000000 && period == 468750.0);
}

int main(void)
{
    test_names_and_user();
    test_auto_range_examples();
    test_manual_precedence();
    test_flip_within_track();
    test_grid();
    printf("fold: %d checks, %d failed\n", checks, fails);
    return fails;
}
