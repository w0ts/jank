/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of who starts and stops (core/jank_lptrans.c) and of the start / stop scene button in the app: without peers the Digitone leads (TRANSPORT, no Jank session, so
 * no clock and no Start byte for it), with peers Link leads and the Digitone's own keys move Jank's session, without a loop. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpapp.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const struct jln_scale MAJOR = {"Major", 7, {0, 2, 4, 5, 7, 9, 11}};

static const char *msg(struct jlapp *a)
{
    static char m[JLAPP_MSG_MAX];
    m[0] = 0;
    jlapp_take_message(a, m, sizeof m);
    return m;
}

static size_t press_start_stop(struct jlapp *a, struct jlapp_act *o)
{
    struct jlx_ev e = {JLX_SIDE, 0, 0, JLAPP_SIDE_START_STOP, 127};
    return jlapp_on_lpx_acts(a, &e, o, 1);
}

static uint8_t scene5(struct jlapp *a)
{
    struct jlx_frame f;
    jlapp_frame(a, &f);
    return f.led[JLX_SIDE0 + JLAPP_SIDE_START_STOP].color;
}

/* live, no peers, the Digitone has answered TRANSPORT (stopped) */
static struct jlapp dn2_leads(void)
{
    struct jlapp a;
    jlapp_init(&a, &MAJOR);
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
    jlapp_tick(&a, 1000);
    jlapp_on_dn2_transport(&a, JDL_TR_STOPPED);
    return a;
}

static struct jdl_live7 st7(uint8_t transport)
{
    struct jdl_live7 s;
    memset(&s, 0, sizeof s);
    s.transport = transport;
    s.playing = 0;
    s.next = JDL_NO_PATTERN;
    return s;
}

/* ---- the rule alone ---- */

static void test_who_leads(void)
{
    struct jltr t;
    uint8_t op = 9;
    jltr_init(&t);
    CHECK(!jltr_dn2_leads(&t, 1) && !jltr_press(&t, 1, 0, &op) && op == 9);   /* TRANSPORT has not answered: as before */
    CHECK(jltr_on_dn2(&t, JDL_TR_STOPPED, 1, 0) == JLTR_NO_REQUEST);
    CHECK(jltr_dn2_leads(&t, 1) && !jltr_dn2_leads(&t, 0));                    /* answered, live: the Digitone leads */
    jltr_set_peers(&t, 1);
    CHECK(!jltr_dn2_leads(&t, 1));                                             /* peers: Link leads */
    jltr_set_peers(&t, 0);
    t.cmd_ok = 0;
    CHECK(!jltr_dn2_leads(&t, 1));                                             /* refused */
    t.cmd_ok = 1;
    jltr_lost(&t);
    CHECK(!jltr_dn2_leads(&t, 1));                                             /* live control lost: must answer again */
}

static void test_a_press_while_the_digitone_leads(void)
{
    struct jltr t;
    uint8_t op;
    jltr_init(&t);
    jltr_on_dn2(&t, JDL_TR_STOPPED, 1, 0);
    CHECK(jltr_press(&t, 1, 100, &op) && op == JDL_TR_PLAY && jltr_lit(&t, 1));          /* stopped: PLAY, lit at once */
    CHECK(jltr_on_dn2(&t, JDL_TR_STOPPED, 0, 300) == JLTR_NO_REQUEST && jltr_lit(&t, 1));   /* an older status within the hold: not taken */
    jltr_on_dn2(&t, JDL_TR_PLAYING, 0, 700);
    CHECK(jltr_press(&t, 1, 800, &op) && op == JDL_TR_STOP && !jltr_lit(&t, 1));
    jltr_on_dn2(&t, JDL_TR_PAUSED, 1, 900);                                                /* TRANSPORT's own answer is always taken */
    CHECK(!jltr_lit(&t, 1) && jltr_press(&t, 1, 1000, &op) && op == JDL_TR_PLAY);         /* paused: PLAY continues */
    jltr_on_dn2(&t, JDL_TR_PLAYING, 0, 2000);
    jltr_set_jank(&t, 0, 2000);
    CHECK(jltr_lit(&t, 1));                                                                /* the light is the Digitone's, not Jank's session */
    CHECK(jltr_on_dn2(&t, JDL_TR_STOPPED, 0, 3000) == JLTR_NO_REQUEST && !jltr_lit(&t, 1));   /* its own STOP key: the light follows, Jank's session is not asked */
    CHECK(jltr_on_dn2(&t, 7, 0, 3100) == JLTR_NO_REQUEST && !jltr_lit(&t, 1));            /* not a state */
}

/* Link leads: the Digitone's own keys start / stop Jank's session; Jank's own changes coming back through the Digitone are not sent back */
static void test_with_peers_the_digitones_keys_move_the_session(void)
{
    struct jltr t;
    uint8_t op;
    jltr_init(&t);
    jltr_set_peers(&t, 1);
    jltr_on_dn2(&t, JDL_TR_STOPPED, 1, 0);
    CHECK(!jltr_press(&t, 1, 10, &op));                                                    /* scene 5: Jank's session */
    CHECK(jltr_on_dn2(&t, JDL_TR_PLAYING, 0, 5000) == 1);                                  /* its PLAY key: start Jank's session */
    jltr_set_jank(&t, 1, 5050);                                                            /* the session starts ... */
    CHECK(jltr_on_dn2(&t, JDL_TR_PLAYING, 0, 5100) == JLTR_NO_REQUEST);                   /* ... nothing more */
    jltr_set_jank(&t, 0, 9000);                                                            /* a Link peer stops the session; Jank's Stop reaches the Digitone */
    CHECK(jltr_on_dn2(&t, JDL_TR_STOPPED, 0, 9200) == JLTR_NO_REQUEST);                   /* it agrees: no request */
    jltr_set_jank(&t, 1, 12000);                                                           /* Jank starts (scene 5), the Digitone follows a moment later */
    CHECK(jltr_on_dn2(&t, JDL_TR_PLAYING, 0, 12300) == JLTR_NO_REQUEST);
    CHECK(jltr_on_dn2(&t, JDL_TR_STOPPED, 0, 12500) == JLTR_NO_REQUEST);                  /* a stale bounce inside the guard: not sent back */
    CHECK(jltr_on_dn2(&t, JDL_TR_PLAYING, 0, 12600) == JLTR_NO_REQUEST);
    CHECK(jltr_on_dn2(&t, JDL_TR_STOPPED, 0, 20000) == 0);                                 /* later, its STOP key: stop Jank's session */
    CHECK(jltr_on_dn2(&t, JDL_TR_PLAYING, 0, 20100) == JLTR_NO_REQUEST);                  /* right after that request: the guard */
    jltr_set_jank(&t, 0, 20200);
    CHECK(jltr_on_dn2(&t, JDL_TR_PAUSED, 0, 30000) == JLTR_NO_REQUEST);                   /* paused while Jank is stopped: they agree */
    CHECK(jltr_lit(&t, 1) == 0);                                                           /* the light: Jank's session */
}

/* ---- the scene button in the app ---- */

static void test_the_app_without_peers(void)
{
    struct jlapp a = dn2_leads();
    struct jlapp_act o;
    CHECK(jlapp_dn2_leads(&a));
    CHECK(scene5(&a) == JLAPP_GREEN_THIRD);
    CHECK(press_start_stop(&a, &o) == 1 && o.kind == JLAPP_ACT_DN2_TRANSPORT && o.vel == JDL_TR_PLAY && strcmp(msg(&a), "START") == 0);   /* never JLAPP_ACT_TRANSPORT: no clock */
    CHECK(scene5(&a) == JLAPP_GREEN);
    jlapp_tick(&a, 3000);
    {
        struct jdl_live7 s = st7(JDL_TR_PLAYING);
        CHECK(jlapp_on_live7(&a, &s) == JLTR_NO_REQUEST);
    }
    CHECK(press_start_stop(&a, &o) == 1 && o.kind == JLAPP_ACT_DN2_TRANSPORT && o.vel == JDL_TR_STOP && strcmp(msg(&a), "STOP") == 0);
    jlapp_set_transport(&a, 1);                                                            /* Jank's own session playing changes nothing here */
    jlapp_tick(&a, 9000);
    {
        struct jdl_live7 s = st7(JDL_TR_STOPPED);
        CHECK(jlapp_on_live7(&a, &s) == JLTR_NO_REQUEST && scene5(&a) == JLAPP_GREEN_THIRD);
    }
    jlapp_set_stage8(&a, 0, 1);                                                            /* the Digitone refuses TRANSPORT: as before, said once */
    CHECK(strcmp(msg(&a), "TRANSP: DN2 TOO OLD") == 0 && !jlapp_dn2_leads(&a));
    jlapp_set_stage8(&a, 0, 1);
    CHECK(msg(&a)[0] == 0);
    CHECK(press_start_stop(&a, &o) == 1 && o.kind == JLAPP_ACT_TRANSPORT);                 /* Jank's session (and its forced clock), as before stage 8 */
    CHECK(scene5(&a) == JLAPP_GREEN);                                                      /* the light: Jank's session again */
}

static void test_the_app_before_the_answer_and_with_peers(void)
{
    struct jlapp a;
    struct jlapp_act o;
    jlapp_init(&a, &MAJOR);
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
    CHECK(press_start_stop(&a, &o) == 1 && o.kind == JLAPP_ACT_TRANSPORT);                 /* TRANSPORT not answered yet: as before */
    a = dn2_leads();
    jlapp_set_peers(&a, 1);
    CHECK(!jlapp_dn2_leads(&a) && press_start_stop(&a, &o) == 1 && o.kind == JLAPP_ACT_TRANSPORT);   /* peers: Jank's session, the clock goes */
    jlapp_tick(&a, 6000);
    {
        struct jdl_live7 s = st7(JDL_TR_PLAYING);                                          /* the Digitone's PLAY key */
        CHECK(jlapp_on_live7(&a, &s) == 1);
        jlapp_tick(&a, 6100);
        jlapp_set_transport(&a, 1);                                                        /* the session started */
        CHECK(jlapp_on_live7(&a, &s) == JLTR_NO_REQUEST && scene5(&a) == JLAPP_GREEN);
    }
    jlapp_set_live(&a, JDL_ASKING, 1, 1);                                                  /* live control lost: TRANSPORT must answer again */
    jlapp_set_peers(&a, 0);
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
    CHECK(!jlapp_dn2_leads(&a));
    CHECK(jlapp_on_dn2_transport(&a, -1) == JLTR_NO_REQUEST && !jlapp_dn2_leads(&a));     /* (no answer) */
    jlapp_on_dn2_transport(&a, JDL_TR_STOPPED);
    CHECK(jlapp_dn2_leads(&a));
}

int main(void)
{
    test_who_leads();
    test_a_press_while_the_digitone_leads();
    test_with_peers_the_digitones_keys_move_the_session();
    test_the_app_without_peers();
    test_the_app_before_the_answer_and_with_peers();
    printf("test_lptrans: %d checks, %d failed\n", checks, fails);
    return fails;
}
