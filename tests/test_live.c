/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of sound on the source and the input meter (core/jank_live.c), and of AUDIO mode's Stop / Start
 * (core/jank_hold.c). Exit status: the number of failed checks. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "jank_hold.h"
#include "jank_live.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define MS(n) ((size_t)(n) * JLV_RATE / 1000u)

/* a sine of amplitude a (full scale 32767) for ms, in 10 ms pushes as the tracker's hops; returns when it went live (ms
 * into this call), -1 never */
static long sine(struct jlv *l, double a, unsigned ms)
{
    static double ph;
    int16_t x[441];
    long went = -1;
    for (unsigned t = 0; t < ms; t += 10u) {
        int was = jlv_live(l);
        for (unsigned i = 0; i < 441u; i++, ph += 2.0 * M_PI * 220.0 / JLV_RATE)
            x[i] = (int16_t)lround(a * sin(ph));
        jlv_push(l, x, 441u);
        if (!was && jlv_live(l) && went < 0)
            went = (long)t + 10;
    }
    return went;
}

/* digital silence for ms; returns when it stopped being live (ms into this call), -1 never */
static long silence(struct jlv *l, unsigned ms, int as_null)
{
    static const int16_t z[441];
    long went = -1;
    for (unsigned t = 0; t < ms; t += 10u) {
        int was = jlv_live(l);
        jlv_push(l, as_null ? NULL : z, 441u);
        if (was && !jlv_live(l) && went < 0)
            went = (long)t + 10;
    }
    return went;
}

static double amp(int db) { return 32768.0 * sqrt(2.0) * pow(10.0, db / 20.0); }   /* a sine's peak for an RMS in dBFS */

static void test_db(void)
{
    CHECK(jlv_db(0.0) == JLV_FLOOR_DB);
    CHECK(jlv_db(32768.0 * 32768.0) == 0);
    CHECK(jlv_db(32768.0 * 32768.0 * 1e-5) == -50);
    CHECK(jlv_db(1.0) == -90);                    /* one LSB of RMS: -90 dBFS, kept */
    CHECK(jlv_db(1e-3) == JLV_FLOOR_DB);
    CHECK(jlv_db(32768.0 * 32768.0 * 4.0) == 0); /* (a mean square above full scale cannot come from int16; clamped) */
}

/* music in: live after JLV_ON_MS; silence: not live JLV_OFF_MS after the window emptied */
static void test_on_off(void)
{
    struct jlv l;
    long t;
    jlv_init(&l);
    CHECK(silence(&l, 5000, 0) < 0 && !jlv_live(&l));          /* an open stream of zeros: never sound */
    t = sine(&l, amp(-20), 3000);
    CHECK(t >= 950 && t <= 1100);                               /* ~1 s */
    CHECK(l.lives == 1);
    t = silence(&l, 8000, 0);
    CHECK(t >= 4000 + 700 && t <= 4000 + 850);                  /* 4 s after the 0.75 s window emptied */
    t = sine(&l, amp(-20), 3000);
    CHECK(t >= 950 && t <= 1100 && l.lives == 2);
    t = silence(&l, 8000, 1);                                   /* the source sent nothing: the same as zeros */
    CHECK(t >= 4700 && t <= 4850);
}

/* the threshold: -45 dBFS RMS is sound, -52 is not (between the levels: it never goes live), -58 ends it, -52 keeps it */
static void test_levels(void)
{
    struct jlv l;
    jlv_init(&l);
    CHECK(sine(&l, amp(-45), 2000) > 0);
    CHECK(silence(&l, 0, 0) < 0 && jlv_live(&l));
    sine(&l, amp(-52), 10000);                                  /* a noise floor between the two levels: stays live */
    CHECK(jlv_live(&l));
    sine(&l, amp(-58), 5000);                                   /* under the off level: gone */
    CHECK(!jlv_live(&l));
    CHECK(sine(&l, amp(-52), 5000) < 0 && !jlv_live(&l));       /* and not live again from there */
}

/* a breakdown's quiet bar (2 s of silence) keeps it live; a notification blip (0.2 s) does not make it live; a kick at
 * 90 BPM (100 ms of sound, 567 ms of digital silence) is sound */
static void test_breaks_and_blips(void)
{
    struct jlv l;
    jlv_init(&l);
    sine(&l, amp(-20), 2000);
    CHECK(jlv_live(&l));
    CHECK(silence(&l, 2000, 0) < 0 && jlv_live(&l));
    CHECK(silence(&l, 2000, 0) < 0 && jlv_live(&l));           /* 4 s in a row: still live (the window's 0.75 s first) */
    sine(&l, amp(-20), 500);
    CHECK(silence(&l, 3500, 0) < 0 && jlv_live(&l));

    jlv_init(&l);
    for (int i = 0; i < 10; i++) {
        sine(&l, amp(-15), 200);
        silence(&l, 3000, 0);
    }
    CHECK(!jlv_live(&l) && l.lives == 0);

    jlv_init(&l);
    for (int i = 0; i < 6; i++) {
        sine(&l, amp(-15), 100);
        silence(&l, 567, 0);
    }
    CHECK(jlv_live(&l));
}

/* the meter: 0.3 s RMS, a value every 250 ms, min / max of the last 3 s */
static void test_meter(void)
{
    struct jlv l;
    jlv_init(&l);
    CHECK(l.rms_db == JLV_FLOOR_DB && l.min_db == JLV_FLOOR_DB && l.max_db == JLV_FLOOR_DB);
    sine(&l, amp(-20), 240);
    CHECK(l.rms_db == JLV_FLOOR_DB);                            /* no value before the first 250 ms */
    sine(&l, amp(-20), 760);
    CHECK(l.rms_db == -20 && l.max_db == -20 && l.min_db == -20);
    sine(&l, amp(-40), 1000);
    CHECK(l.rms_db == -40 && l.max_db == -20 && l.min_db == -40);
    sine(&l, amp(-40), 2250);                                   /* 3 s of values from windows of -40 only: -20 is out of the span */
    CHECK(l.max_db == -40);
    silence(&l, 500, 0);
    CHECK(l.rms_db == JLV_FLOOR_DB && l.min_db == JLV_FLOOR_DB && l.max_db == -40);
    jlv_push(&l, NULL, MS(100));                                /* odd sizes: whole blocks only */
    jlv_push(&l, NULL, 7);
    CHECK(l.acc_n == (MS(100) + 7u) % JLV_BLOCK || l.acc_n < JLV_BLOCK);
}

/* ---- AUDIO mode's Stop / Start ---- */

static struct jho_in hin(uint8_t lt, uint8_t base, uint8_t locked)
{
    struct jho_in in = {.audio = 1, .lt = lt, .base = base, .locked = locked, .clock_running = 0};
    return in;
}

/* Stop stops the outputs at once whatever the transport says; the hold lasts through tracker stops and LINK mode */
static void test_hold_stops_all_outputs(void)
{
    for (uint8_t lt = JHO_IGNORE; lt <= JHO_LEAD; lt++) {
        struct jho h;
        struct jho_in in = hin(lt, 1, 1);
        jho_init(&h);
        CHECK(jho_outputs(&h, &in) == 1);
        in.clock_running = 1;
        CHECK(jho_icon(&h, &in) == JHO_RUNNING);
        jho_set(&h, 1);
        CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && h.holds == 1);
        in.locked = 0;                                          /* the tracker stops, then locks again: still held */
        CHECK(jho_outputs(&h, &in) == 0);
        in.locked = 1;
        CHECK(jho_outputs(&h, &in) == 0 && h.hold);
        in.audio = 0;                                           /* LINK mode: the transport's rule, the hold kept */
        CHECK(jho_outputs(&h, &in) == 1 && h.hold && !jho_session_held(&h, &in));
        in.audio = 1;
        CHECK(jho_outputs(&h, &in) == 0);
        jho_set(&h, 1);                                         /* Stop again: nothing more */
        CHECK(h.holds == 1);
    }
}

/* Start arms: the outputs wait for the tracker's lock, then follow the transport (the clock starts on its bar line) */
static void test_release_arms_until_locked(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_FOLLOW, 1, 0);
    jho_init(&h);
    jho_set(&h, 1);
    jho_set(&h, 0);
    CHECK(h.arming && h.releases == 1);
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_ARMED);   /* not locked: armed, the clocks quiet */
    CHECK(jho_outputs(&h, &in) == 0);
    in.locked = 1;
    CHECK(jho_outputs(&h, &in) == 1 && !h.arming);
    in.locked = 0;                                              /* the arming is over: a later unlock is the transport's */
    CHECK(jho_outputs(&h, &in) == 1);
    in.base = 0;                                                /* FOLLOW: the session stopped -> the clocks stop */
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_ARMED);
    jho_set(&h, 0);                                             /* Start while not held: nothing */
    CHECK(!h.arming && h.releases == 1);
}

/* FOLLOW+LEAD: the hold holds the Link session too; IGNORE and FOLLOW never touch it */
static void test_session_by_option(void)
{
    struct jho h;
    struct jho_in in;
    jho_init(&h);
    jho_set(&h, 1);
    in = hin(JHO_LEAD, 1, 1);
    CHECK(jho_session_held(&h, &in));
    in = hin(JHO_FOLLOW, 1, 1);
    CHECK(!jho_session_held(&h, &in));
    in = hin(JHO_IGNORE, 1, 1);
    CHECK(!jho_session_held(&h, &in));
    jho_set(&h, 0);
    in = hin(JHO_LEAD, 0, 0);
    CHECK(!jho_session_held(&h, &in));
}

/* `play` in LINK mode ends a hold: no surprise stop when the sound comes back */
static void test_link_play_ends_hold(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_LEAD, 1, 1);
    jho_init(&h);
    jho_set(&h, 1);
    jho_link_play(&h);
    CHECK(!h.hold && !h.arming && jho_outputs(&h, &in) == 1);
    CHECK(!strcmp(jho_icon_name(JHO_ARMED), "armed") && !strcmp(jho_icon_name(9), "stopped"));
}

/* LINK mode's own `stop`: the clocks stay stopped whatever the transport wants (another peer starts the session, AUTO
 * sees a peer), until `play` or a change of mode; AUDIO mode is not touched by it */
static void test_link_stop_holds(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_IGNORE, 1, 0);
    jho_init(&h);
    in.audio = 0;
    CHECK(jho_outputs(&h, &in) == 1);
    jho_link_stop(&h);
    CHECK(jho_outputs(&h, &in) == 0 && jho_outputs(&h, &in) == 0);
    jho_link_play(&h);
    CHECK(jho_outputs(&h, &in) == 1);
    jho_link_stop(&h);
    in.audio = 1;                                               /* sound: AUDIO mode, its own rules */
    in.locked = 1;
    CHECK(jho_outputs(&h, &in) == 1);
    in.audio = 0;                                               /* back to LINK mode: its clocks on again */
    CHECK(jho_outputs(&h, &in) == 1);
}

/* LOW CONFIDENCE's gate: the outputs stop in every option, the session only with FOLLOW+LEAD; lifted, the transport's
 * rule again (the clock starts on its bar line: armed until then); LINK mode: no gate */
static void test_gate_stops_outputs(void)
{
    for (uint8_t lt = JHO_IGNORE; lt <= JHO_LEAD; lt++) {
        struct jho h;
        struct jho_in in = hin(lt, 1, 1);
        jho_init(&h);
        in.clock_running = 1;
        CHECK(jho_outputs(&h, &in) == 1 && jho_icon(&h, &in) == JHO_RUNNING && !jho_session_held(&h, &in));
        in.gated = 1;
        CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED);
        CHECK(jho_session_held(&h, &in) == (lt == JHO_LEAD));
        CHECK(!h.hold && h.holds == 0);                         /* not the user's hold */
        in.audio = 0;                                           /* LINK mode: the transport's rule */
        CHECK(jho_outputs(&h, &in) == 1 && !jho_session_held(&h, &in));
        in.audio = 1;
        in.gated = 0;                                           /* back above the threshold */
        in.clock_running = 0;
        CHECK(jho_outputs(&h, &in) == 1 && jho_icon(&h, &in) == JHO_ARMED && !jho_session_held(&h, &in));
        in.clock_running = 1;                                   /* the bar line came */
        CHECK(jho_icon(&h, &in) == JHO_RUNNING);
    }
}

/* the user's hold wins over the gate: held, nothing starts when the gate lifts; Start while gated arms, and the outputs
 * wait for the gate and the lock both */
static void test_hold_wins_over_gate(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_LEAD, 1, 1);
    jho_init(&h);
    in.gated = 1;
    jho_set(&h, 1);
    in.gated = 0;
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && jho_session_held(&h, &in));
    in.gated = 1;
    jho_set(&h, 0);                                             /* Start while gated */
    CHECK(h.arming && jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && jho_session_held(&h, &in));
    CHECK(h.arming);                                            /* still waiting: the gate is not the lock */
    in.gated = 0;
    in.locked = 0;
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_ARMED && !jho_session_held(&h, &in));
    in.locked = 1;
    CHECK(jho_outputs(&h, &in) == 1 && !h.arming);
}

int main(void)
{
    test_db();
    test_on_off();
    test_levels();
    test_breaks_and_blips();
    test_meter();
    test_hold_stops_all_outputs();
    test_release_arms_until_locked();
    test_session_by_option();
    test_link_play_ends_hold();
    test_link_stop_holds();
    test_gate_stops_outputs();
    test_hold_wins_over_gate();
    printf("live: %d checks, %d failed\n", checks, fails);
    return fails;
}
