/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the menu's own preferences (core/jank_prefs.c, NVS key "ui1"): a field set through the menu's path survives
 * pack -> unpack, an old blob still loads, and the write is due soon after the last change (SCREEN OFF was lost: it waited 2 s). */
#include <stdio.h>
#include <string.h>
#include "jank_prefs.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static struct jk_prefs round_trip(const struct jk_prefs *p)
{
    uint8_t b[JK_PREFS_SIZE];
    struct jk_prefs q;
    jk_prefs_defaults(&q);
    CHECK(jk_prefs_pack(p, b, sizeof b) == JK_PREFS_SIZE);
    CHECK(jk_prefs_unpack(&q, b, sizeof b));
    return q;
}

/* every field the menu has: set it as the menu does, store, load: the value is back, and no other preference moved */
static void test_every_field(void)
{
    static const struct {
        enum jo_field f;
        int v;
    } SET[] = {{JO_F_OUT1, -50}, {JO_F_OUT2, 50}, {JO_F_RATE, 4}, {JO_F_THRESHOLD, 35}, {JO_F_GAIN, -12}, {JO_F_BRIGHT, 10}, {JO_F_SLEEP, 3}, {JO_F_LOWCONF, 0}};
    int prefs = 0;
    for (int f = 0; f < JO_F_COUNT; f++) {
        struct jk_prefs p;
        jk_prefs_defaults(&p);
        if (jk_prefs_set_field(&p, (enum jo_field)f, 1))
            prefs++;
    }
    CHECK(prefs == (int)(sizeof SET / sizeof SET[0]));           /* the other fields are the settings' (control line), not these */
    for (size_t i = 0; i < sizeof SET / sizeof SET[0]; i++) {
        struct jk_prefs p, q, d;
        jk_prefs_defaults(&p);
        jk_prefs_defaults(&d);
        CHECK(jk_prefs_set_field(&p, SET[i].f, SET[i].v));
        q = round_trip(&p);
        CHECK(!memcmp(&p, &q, sizeof p));
        CHECK(memcmp(&q, &d, sizeof q));                          /* it changed something */
    }
    {
        struct jk_prefs p, q;
        jk_prefs_defaults(&p);
        for (size_t i = 0; i < sizeof SET / sizeof SET[0]; i++)
            jk_prefs_set_field(&p, SET[i].f, SET[i].v);
        q = round_trip(&p);
        CHECK(q.out1 == -50 && q.out2 == 50 && q.rate == 4 && q.th == 35 && q.gain == -12 && q.bright == 10 && q.sleep == 3 && q.conf_stop == 0);
    }
    for (int s = 0; s < 4; s++) {                                 /* SCREEN OFF: all four choices */
        struct jk_prefs p, q;
        jk_prefs_defaults(&p);
        jk_prefs_set_field(&p, JO_F_SLEEP, s);
        q = round_trip(&p);
        CHECK(q.sleep == s);
    }
}

/* the blob an earlier build wrote (10 bytes, version 1): loads as it is; anything else is ignored whole */
static void test_old_blob(void)
{
    static const uint8_t OLD[10] = {1, 2, 40, 2, 3, 0, 55, 0xFB, 5, 0xFE};   /* lead 2, bright 40, SCREEN OFF 1 MIN, ... out1 -5, gain -2 */
    uint8_t bad[10];
    struct jk_prefs p, keep;
    jk_prefs_defaults(&p);
    CHECK(jk_prefs_unpack(&p, OLD, sizeof OLD));
    CHECK(p.lead == 2 && p.bright == 40 && p.sleep == 2 && p.rate == 3 && p.conf_stop == 0 && p.th == 55 && p.out1 == -5 && p.out2 == 5 && p.gain == -2);
    jk_prefs_defaults(&keep);
    keep = p;
    memcpy(bad, OLD, sizeof bad);
    bad[0] = 2;
    CHECK(!jk_prefs_unpack(&p, bad, sizeof bad) && !memcmp(&p, &keep, sizeof p));     /* another version */
    memcpy(bad, OLD, sizeof bad);
    bad[3] = 4;
    CHECK(!jk_prefs_unpack(&p, bad, sizeof bad));
    CHECK(!jk_prefs_unpack(&p, OLD, 9) && !jk_prefs_unpack(&p, OLD, 11));
    {
        uint8_t small[9];
        struct jk_prefs d;
        jk_prefs_defaults(&d);
        CHECK(jk_prefs_pack(&d, small, sizeof small) == 0);
    }
}

/* the user's Start / Stop (bit 7 of the SCREEN OFF byte): round trip, the sleep value untouched, an old blob = stopped (power-on) */
/* a SCREEN OFF / BRIGHTNESS write (the menu's and the control line's `set sleep` / `set bright`) keeps the started bit, and the other way round */
static void test_display_keeps_started(void)
{
    struct jk_prefs p, q;
    uint8_t b[JK_PREFS_SIZE];
    jk_prefs_defaults(&p);
    p.started = 1;
    p.sleep = 2;
    p.bright = 30;
    jk_prefs_pack(&p, b, sizeof b);
    CHECK(b[3] == (0x80u | 2u) && b[2] == 30);
    CHECK(jk_prefs_unpack(&q, b, sizeof b) && q.started == 1 && q.sleep == 2 && q.bright == 30);
    q.sleep = 3;                                                  /* a sleep write */
    q.bright = 10;
    jk_prefs_pack(&q, b, sizeof b);
    CHECK(b[3] == (0x80u | 3u) && b[2] == 10);
    CHECK(jk_prefs_unpack(&p, b, sizeof b) && p.started == 1 && p.sleep == 3 && p.bright == 10);
}

static void test_started(void)
{
    static const uint8_t OLD[10] = {1, 1, 100, 3, 2, 1, 80, 0, 0, 0};
    struct jk_prefs p, q;
    uint8_t b[JK_PREFS_SIZE];
    jk_prefs_defaults(&p);
    CHECK(p.started == 0);
    CHECK(jk_prefs_unpack(&p, OLD, sizeof OLD) && p.started == 0 && p.sleep == 3);
    p.started = 1;                                                /* the user pressed Start */
    q = round_trip(&p);
    CHECK(q.started == 1 && q.sleep == 3 && !memcmp(&p, &q, sizeof p));
    jk_prefs_pack(&p, b, sizeof b);
    CHECK(b[3] == (0x80u | 3u) && !memcmp(b, OLD, 3) && !memcmp(b + 4, OLD + 4, 6));
    p.started = 0;                                                /* then Stop */
    q = round_trip(&p);
    CHECK(q.started == 0 && q.sleep == 3);
    b[3] = 0x84u;                                                 /* sleep 4 is still invalid with the bit set */
    CHECK(!jk_prefs_unpack(&q, b, sizeof b));
}

static void test_save_timing(void)
{
    struct jk_prefs_save s;
    memset(&s, 0, sizeof s);
    CHECK(!jk_prefs_due(&s, 100000, 0, 0));                       /* nothing changed: nothing to write */
    jk_prefs_changed(&s, 1000);
    CHECK(!jk_prefs_due(&s, 1000, 0, 0));
    CHECK(!jk_prefs_due(&s, 1000 + JK_PREFS_SETTLE_MS - 1, 0, 0));
    CHECK(jk_prefs_due(&s, 1000 + JK_PREFS_SETTLE_MS, 0, 0));     /* settled, stopped: written (and once) */
    CHECK(!jk_prefs_due(&s, 1000 + JK_PREFS_SETTLE_MS + 1, 0, 0));
    CHECK(JK_PREFS_SETTLE_MS < 1000u);                            /* the unplugged-soon-after case: well under a second (was 2 s) */
    /* a held button: each repeat moves the settle time, one write after the last */
    jk_prefs_changed(&s, 0);
    for (uint32_t t = 100; t <= 1000; t += 100) {
        CHECK(!jk_prefs_due(&s, t, 0, 0));
        jk_prefs_changed(&s, t);
    }
    CHECK(jk_prefs_due(&s, 1000 + JK_PREFS_SETTLE_MS, 0, 0));
    /* playing: kept until it stops (the flash rule), then written at once */
    jk_prefs_changed(&s, 5000);
    CHECK(!jk_prefs_due(&s, 60000, 1, 0) && s.pending);
    CHECK(jk_prefs_due(&s, 60001, 0, 0));
    /* a restart follows: now, playing or not, settled or not */
    jk_prefs_changed(&s, 70000);
    CHECK(jk_prefs_due(&s, 70001, 1, 1) && !s.pending);
    CHECK(!jk_prefs_due(&s, 70002, 1, 1));
    /* the millisecond counter wraps */
    jk_prefs_changed(&s, 0xFFFFFF00u);
    CHECK(!jk_prefs_due(&s, 0xFFFFFF10u, 0, 0));
    CHECK(jk_prefs_due(&s, 0xFFFFFF00u + JK_PREFS_SETTLE_MS, 0, 0));
}

int main(void)
{
    test_every_field();
    test_old_blob();
    test_display_keeps_started();
    test_started();
    test_save_timing();
    printf("prefs: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
