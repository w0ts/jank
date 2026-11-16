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

/* a unit the user started earlier (power-on is stopped: test_power_on_stopped) */
static void started(struct jho *h)
{
    jho_init(h);
    h->hold = 0;
}

static struct jho_in hin(uint8_t lt, uint8_t base, uint8_t locked)
{
    struct jho_in in = {.audio = 1, .lt = lt, .base = base, .locked = locked, .clock_running = 0};
    return in;
}

/* Stop stops the outputs at once whatever the transport says; the hold lasts through tracker stops and LINK mode (the
 * user's layer, 2026-11-15: nothing automatic starts them again, LINK mode included) */
static void test_hold_stops_all_outputs(void)
{
    for (uint8_t lt = JHO_IGNORE; lt <= JHO_LEAD; lt++) {
        struct jho h;
        struct jho_in in = hin(lt, 1, 1);
        started(&h);
        CHECK(jho_outputs(&h, &in) == 1);
        in.clock_running = 1;
        CHECK(jho_icon(&h, &in) == JHO_RUNNING);
        jho_set(&h, 1);
        CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && h.holds == 1);
        in.locked = 0;                                          /* the tracker stops, then locks again: still held */
        CHECK(jho_outputs(&h, &in) == 0);
        in.locked = 1;
        CHECK(jho_outputs(&h, &in) == 0 && h.hold);
        in.audio = 0;                                           /* LINK mode (the sound gone): still stopped */
        CHECK(jho_outputs(&h, &in) == 0 && h.hold && !jho_session_held(&h, &in));
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
    started(&h);
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
    jho_set(&h, 0);                                             /* Start while not held: no arming (it disarms the gate) */
    CHECK(!h.arming && h.releases == 1);
}

/* M2k: a Start's bar placement not made yet (the beat task's step): the clocks wait for it, so they start on the placed 1;
 * clocks already running are not stopped for it */
static void test_start_waits_for_the_bar(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_IGNORE, 1, 1);
    started(&h);
    jho_set(&h, 1);
    jho_set(&h, 0);
    in.locked = 1;
    in.bar_settling = 1;
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_ARMED && h.arming);
    in.bar_settling = 0;                                        /* placed: the clocks start (on the moved bar line) */
    CHECK(jho_outputs(&h, &in) == 1 && !h.arming);
    in.clock_running = 1;
    in.bar_settling = 1;                                        /* a fold while playing: the clocks go on */
    CHECK(jho_outputs(&h, &in) == 1);
}

/* FOLLOW+LEAD: the hold holds the Link session too; IGNORE and FOLLOW never touch it */
static void test_session_by_option(void)
{
    struct jho h;
    struct jho_in in;
    started(&h);
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
    started(&h);
    jho_set(&h, 1);
    jho_link_play(&h);
    CHECK(!h.hold && !h.arming && jho_outputs(&h, &in) == 1);
    CHECK(!strcmp(jho_icon_name(JHO_ARMED), "armed") && !strcmp(jho_icon_name(9), "stopped"));
}

/* LINK mode's own `stop`: the clocks stay stopped whatever the transport wants (another peer starts the session, AUTO
 * sees a peer) and through the sound coming and going (the mode is automatic), until the user's `play` */
static void test_link_stop_holds(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_IGNORE, 1, 0);
    started(&h);
    in.audio = 0;
    CHECK(jho_outputs(&h, &in) == 1);
    jho_link_stop(&h);
    CHECK(jho_outputs(&h, &in) == 0 && jho_outputs(&h, &in) == 0);
    jho_link_play(&h);
    CHECK(jho_outputs(&h, &in) == 1);
    jho_link_stop(&h);
    in.audio = 1;                                               /* sound: AUDIO mode, still the user's Stop */
    in.locked = 1;
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED);
    in.audio = 0;                                               /* back to LINK mode: still stopped */
    CHECK(jho_outputs(&h, &in) == 0);
    in.audio = 1;
    jho_set(&h, 0);                                             /* the user's Start in AUDIO mode: on the lock */
    CHECK(jho_outputs(&h, &in) == 1);
}

/* the two layers (the user, 2026-11-15: "if I manually stopped, don't start again automatically"): after the user's Stop
 * no automatic event starts the outputs (the gate opening, silence then sound, a re-lock, the mode going and coming, the
 * session started by a peer); the user's Start does; then an automatic stop (the gate) may restart automatically */
static void test_user_layer(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_LEAD, 1, 1);
    started(&h);
    jho_set(&h, 1);
    in.gated = 1;                                               /* the gate closes and opens */
    CHECK(jho_outputs(&h, &in) == 0);
    in.gated = 0;
    in.conf_open = 1;
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && jho_session_held(&h, &in));
    in.silent = 1;                                              /* silence, then sound */
    CHECK(jho_outputs(&h, &in) == 0);
    in.silent = 0;
    CHECK(jho_outputs(&h, &in) == 0);
    in.locked = 0;                                              /* a new track: the lock lost and found */
    CHECK(jho_outputs(&h, &in) == 0);
    in.locked = 1;
    CHECK(jho_outputs(&h, &in) == 0);
    in.audio = 0;                                               /* the 4.75 s rule: LINK mode, then AUDIO again */
    CHECK(jho_outputs(&h, &in) == 0);
    in.audio = 1;
    in.base = 1;                                                /* a peer starts the shared session */
    CHECK(jho_outputs(&h, &in) == 0 && h.hold);
    jho_set(&h, 0);                                             /* the user's Start */
    CHECK(jho_outputs(&h, &in) == 1 && !h.hold);
    in.conf_open = 1;                                           /* the gate re-armed ... */
    CHECK(jho_outputs(&h, &in) == 1 && !h.gate_off);
    in.conf_open = 0;
    in.gated = 1;                                               /* ... an automatic stop ... */
    CHECK(jho_outputs(&h, &in) == 0);
    in.gated = 0;                                               /* ... and an automatic start */
    CHECK(jho_outputs(&h, &in) == 1);
    in.silent = 1;
    CHECK(jho_outputs(&h, &in) == 0);
    in.silent = 0;
    CHECK(jho_outputs(&h, &in) == 1);
}

/* LOW CONFIDENCE's gate: the outputs stop in every option, the session only with FOLLOW+LEAD; lifted, the transport's
 * rule again (the clock starts on its bar line: armed until then); LINK mode: no gate */
static void test_gate_stops_outputs(void)
{
    for (uint8_t lt = JHO_IGNORE; lt <= JHO_LEAD; lt++) {
        struct jho h;
        struct jho_in in = hin(lt, 1, 1);
        started(&h);
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

/* the user's hold wins over the gate: held, nothing starts when the gate lifts; Start while gated (M2g): the gate is
 * disarmed, the outputs wait for the lock alone */
static void test_hold_wins_over_gate(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_LEAD, 1, 1);
    started(&h);
    in.gated = 1;
    jho_set(&h, 1);
    in.gated = 0;
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && jho_session_held(&h, &in));
    in.gated = 1;
    in.locked = 0;
    jho_set(&h, 0);                                             /* Start while gated */
    CHECK(h.arming && h.gate_off && jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_ARMED && !jho_session_held(&h, &in));
    in.locked = 1;
    CHECK(jho_outputs(&h, &in) == 1 && !h.arming);
}

/* M2g, the user's Start wins over LOW CONFIDENCE (2026-11-15: a Start at conf 49 started nothing until 95): the Start
 * disarms the gate, the outputs start (on the session's bar line) once the tracker is locked; the gate re-arms once the
 * confidence has reached the threshold + 5, then stops and starts as before. The icon: armed until running */
static void test_start_wins_over_gate(void)
{
    for (uint8_t lt = JHO_IGNORE; lt <= JHO_LEAD; lt++) {
        struct jho h;
        struct jho_in in = hin(lt, 1, 1);
        started(&h);
        in.gated = 1;                                           /* conf 49 under 80 */
        CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && jho_session_held(&h, &in) == (lt == JHO_LEAD));
        jho_set(&h, 0);                                         /* Start, not held */
        CHECK(h.gate_off && h.holds == 0);
        CHECK(jho_outputs(&h, &in) == 1 && jho_icon(&h, &in) == JHO_ARMED && !jho_session_held(&h, &in));
        in.clock_running = 1;                                   /* the bar line came */
        CHECK(jho_outputs(&h, &in) == 1 && jho_icon(&h, &in) == JHO_RUNNING);
        in.gated = 0;                                           /* up to the threshold, not threshold + 5 */
        CHECK(jho_outputs(&h, &in) == 1 && h.gate_off);
        in.gated = 1;                                           /* a drop before the re-arm: no stop */
        CHECK(jho_outputs(&h, &in) == 1 && jho_icon(&h, &in) == JHO_RUNNING && !jho_session_held(&h, &in));
        in.gated = 0;
        in.conf_open = 1;                                       /* threshold + 5 reached once: re-armed */
        CHECK(jho_outputs(&h, &in) == 1 && !h.gate_off);
        in.conf_open = 0;
        in.gated = 1;                                           /* a drop now stops, as before */
        CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && jho_session_held(&h, &in) == (lt == JHO_LEAD));
    }
}

/* silence stops at once, Start or not; the Start's disarm stays (the confidence re-arms it); Stop ends it */
static void test_silence_after_start(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_LEAD, 1, 1);
    started(&h);
    in.gated = 1;
    jho_set(&h, 0);
    CHECK(jho_outputs(&h, &in) == 1);
    in.silent = 1;
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && jho_session_held(&h, &in));
    in.silent = 0;                                              /* M2h: the silence ended the Start's override: the */
    CHECK(jho_outputs(&h, &in) == 0 && !h.gate_off && !h.hold);  /* next music waits for the gate, no new Start needed */
    CHECK(jho_icon(&h, &in) == JHO_STOPPED);
    in.gated = 0;
    CHECK(jho_outputs(&h, &in) == 1);
    jho_set(&h, 1);                                             /* Stop: held */
    CHECK(jho_outputs(&h, &in) == 0 && !h.gate_off);
    in.gated = 1;
    jho_set(&h, 0);                                             /* Start again: the override again */
    CHECK(h.gate_off && jho_outputs(&h, &in) == 1);
}

/* M2h, the user (2026-11-15): "come up with manual state: stopped". Power-on is the user's Stop (a constant, nothing
 * stored): stopped in both modes, whatever the transport and the gate say, until the user's Start */
static void test_power_on_stopped(void)
{
    struct jho h;
    struct jho_in in = hin(JHO_LEAD, 1, 1);
    jho_init(&h);
    CHECK(h.hold && jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_STOPPED && jho_session_held(&h, &in));
    in.audio = 0;                                               /* LINK mode, the session playing */
    CHECK(jho_outputs(&h, &in) == 0);
    in.audio = 1;
    in.locked = 0;
    jho_set(&h, 0);                                             /* the user's Start: armed, then on the lock */
    CHECK(jho_outputs(&h, &in) == 0 && jho_icon(&h, &in) == JHO_ARMED);
    in.locked = 1;
    CHECK(jho_outputs(&h, &in) == 1);
    jho_init(&h);                                               /* and LINK mode's `play` starts it as well */
    in.audio = 0;
    jho_link_play(&h);
    CHECK(jho_outputs(&h, &in) == 1);
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
    test_start_waits_for_the_bar();
    test_session_by_option();
    test_link_play_ends_hold();
    test_link_stop_holds();
    test_user_layer();
    test_gate_stops_outputs();
    test_hold_wins_over_gate();
    test_start_wins_over_gate();
    test_silence_after_start();
    test_power_on_stopped();
    printf("live: %d checks, %d failed\n", checks, fails);
    return fails;
}
