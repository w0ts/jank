/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of Jank's menu on the Digitone's screen and LINK MODE (core/jank_dn2menu.c): the keys, OPEN, when LINK MODE goes on and off, what a LINK REQUEST is
 * answered and does (once, also when pushed again), the CANCEL of a start that did not happen, the time to the bar line. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_dn2menu.h"
#include "jank_oled.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_keys(void)
{
    CHECK(jdm_button(JDL_KC_UP) == JO_UP && jdm_button(JDL_KC_DOWN) == JO_DOWN && jdm_button(JDL_KC_NO) == JO_NO && jdm_button(JDL_KC_YES) == JO_YES);
    CHECK(jdm_button(JDL_KC_LEFT) < 0 && jdm_button(JDL_KC_RIGHT) < 0 && jdm_button(JDL_KC_FUNC) < 0 && jdm_button(41) < 0 && jdm_button(0) < 0);
}

static void test_open(void)
{
    struct jdm m;
    struct jdl_live8 s;
    jdm_init(&m);
    CHECK(!m.open && m.link == JDM_UNSENT);
    CHECK(jdm_on_open(&m, &(struct jdl_open){1, JDL_OPEN_MENU, 0}) && m.open && m.opens == 1);
    CHECK(jdm_on_open(&m, &(struct jdl_open){1, JDL_OPEN_MENU, 0}) && m.opens == 2);   /* pushed again (no page arrived): a page again */
    CHECK(!jdm_on_open(&m, &(struct jdl_open){0, JDL_OPEN_KEY, 3}) && !m.open && m.reason == JDL_OPEN_KEY);
    memset(&s, 0, sizeof s);
    CHECK(!jdm_on_status(&m, &s));                                                /* the status agrees */
    s.open = 1;
    s.reason = JDL_OPEN_LEASE;
    CHECK(jdm_on_status(&m, &s) && m.open && m.opens == 3);                       /* an OPEN that was lost: the status says so */
    CHECK(!jdm_on_status(&m, &s));
    s.open = 0;
    CHECK(!jdm_on_status(&m, &s) && !m.open);
}

static void test_link_mode_follows_the_lead(void)
{
    struct jdm m;
    jdm_init(&m);
    CHECK(jdm_link_step(&m, 1000, 0, 0, 0, 0) == JDL_LINK_OFF);                  /* said once at the start */
    CHECK(jdm_link_step(&m, 1001, 0, 0, 0, 0) == JDM_SEND_NONE);
    CHECK(jdm_link_step(&m, 1002, 1, 0, 0, 0) == JDL_LINK_ON);                   /* Link leads */
    CHECK(jdm_link_step(&m, 1003, 1, 0, 1, 0) == JDM_SEND_NONE);                 /* the status says off: not again before a second */
    CHECK(jdm_link_step(&m, 1002 + JDM_LINK_RESEND_MS, 1, 0, 1, 0) == JDL_LINK_ON);
    CHECK(jdm_link_step(&m, 3000 + JDM_LINK_RESEND_MS, 1, 0, 1, 1) == JDM_SEND_NONE);   /* it agrees */
    CHECK(jdm_link_step(&m, 3001 + JDM_LINK_RESEND_MS, 0, 0, 1, 1) == JDL_LINK_OFF);    /* peers gone, or LINK TRANSPORT not FOLLOW+LEAD */
    jdm_link_unsent(&m);                                                          /* it found no room */
    CHECK(jdm_link_step(&m, 3002 + JDM_LINK_RESEND_MS, 0, 0, 1, 1) == JDL_LINK_OFF);
    jdm_live_start(&m);                                                           /* live control again: said afresh */
    CHECK(jdm_link_step(&m, 5000, 0, 0, 0, 0) == JDL_LINK_OFF);
}

static void test_start_request(void)
{
    struct jdm m;
    struct jdm_act a;
    const struct jdl_link_req start = {JDL_LR_START, JDL_LR_RECORD, 0, 7};
    jdm_init(&m);
    jdm_link_step(&m, 1000, 1, 0, 0, 0);
    jdm_on_request(&m, &start, 2000, 1, 0, 1250, &a);
    CHECK(a.answer == JDL_LINK_PENDING && a.ms == 1250 && a.id == 7 && a.record && a.session == JDM_SESSION_START && m.pending && m.reqs == 1);
    jdm_on_request(&m, &start, 2500, 1, 0, 750, &a);                             /* pushed again 500 ms later: answered again, nothing done twice */
    CHECK(a.answer == JDL_LINK_PENDING && a.ms == 750 && a.id == 7 && !a.record && a.session == JDM_SESSION_NONE && m.reqs == 1);
    CHECK(jdm_link_step(&m, 3000, 1, 0, 0, 0) == JDM_SEND_NONE);                 /* waiting for the bar line */
    CHECK(jdm_link_step(&m, 3260, 1, 1, 0, 0) == JDM_SEND_NONE && !m.pending);   /* it started */
    CHECK(jdm_link_step(&m, 9000, 1, 1, 0, 0) == JDM_SEND_NONE && m.cancels == 0);
    jdm_on_request(&m, &(struct jdl_link_req){JDL_LR_START, 0, 0, 8}, 10000, 1, 0, 400, &a);   /* a start that never comes (no session): CANCEL after its time */
    CHECK(!a.record && a.session == JDM_SESSION_START);
    CHECK(jdm_link_step(&m, 10000 + 400 + JDM_START_GRACE_MS - 1u, 1, 0, 0, 0) == JDM_SEND_NONE);
    CHECK(jdm_link_step(&m, 10000 + 400 + JDM_START_GRACE_MS, 1, 0, 0, 0) == JDL_LINK_CANCEL && !m.pending && m.cancels == 1);
    jdm_on_request(&m, &(struct jdl_link_req){JDL_LR_START, 0, 0, 9}, 20000, 1, 0, 400, &a);   /* Link stops leading meanwhile: CANCEL, then OFF */
    CHECK(jdm_link_step(&m, 20010, 0, 0, 0, 0) == JDL_LINK_CANCEL);
    CHECK(jdm_link_step(&m, 20011, 0, 0, 0, 0) == JDL_LINK_OFF);
    jdm_on_request(&m, &(struct jdl_link_req){JDL_LR_START, 0, 0, 10}, 30000, 0, 0, 400, &a);   /* Link does not lead: no start */
    CHECK(a.answer == JDL_LINK_CANCEL && a.session == JDM_SESSION_NONE && !m.pending);
    jdm_on_request(&m, &(struct jdl_link_req){JDL_LR_START, 0, 0, 11}, 40000, 1, 1, 400, &a);   /* Jank's clock already runs: start it again on the bar line */
    CHECK(a.answer == JDL_LINK_PENDING && a.session == JDM_SESSION_RESYNC);
}

static void test_stop_request(void)
{
    struct jdm m;
    struct jdm_act a;
    const struct jdl_link_req stop = {JDL_LR_STOP, JDL_LR_SECOND_STOP, 1, 20};
    jdm_init(&m);
    jdm_on_request(&m, &(struct jdl_link_req){JDL_LR_START, 0, 0, 19}, 1000, 1, 0, 300, &a);
    jdm_on_request(&m, &stop, 1100, 1, 1, 0, &a);
    CHECK(a.answer == JDL_LINK_ACK && a.id == 20 && a.session == JDM_SESSION_STOP && !m.pending && !a.record);
    jdm_on_request(&m, &stop, 1600, 1, 0, 0, &a);                                /* pushed again: ACK only */
    CHECK(a.answer == JDL_LINK_ACK && a.session == JDM_SESSION_NONE);
    jdm_on_request(&m, &(struct jdl_link_req){JDL_LR_STOP, 0, 0, 21}, 2000, 1, 0, 0, &a);   /* nothing runs on Jank's side: the Digitone's own STOP */
    CHECK(a.answer == JDL_LINK_ACK && a.session == JDM_SESSION_DN2_STOP);
    jdm_on_request(&m, &(struct jdl_link_req){JDL_LR_PAUSE, 0, 0, 22}, 3000, 1, 1, 0, &a);  /* a pause is not asked for; if it comes, a stop */
    CHECK(a.answer == JDL_LINK_ACK && a.session == JDM_SESSION_STOP);
    jdm_on_request(&m, &(struct jdl_link_req){JDL_LR_STOP, 0, 0, 23}, 4000, 0, 1, 0, &a);  /* Link does not lead: answered, nothing done */
    CHECK(a.answer == JDL_LINK_ACK && a.session == JDM_SESSION_NONE && m.reqs == 5);   /* 19, 20, 21, 22, 23: the one pushed again is not counted */
}

static void test_ms_to_bar(void)
{
    CHECK(jdm_ms_to_bar(0, 1500000, 500000, 4) == 0);                            /* no session */
    CHECK(jdm_ms_to_bar(1, 1500000, 0, 4) == 0);                                  /* no tempo */
    CHECK(jdm_ms_to_bar(1, 1500000, 500000, 4) == 1250);                          /* 120 BPM, beat 1.5: 2.5 beats to the bar */
    CHECK(jdm_ms_to_bar(1, 4000000, 500000, 4) == 0);                             /* on the bar line */
    CHECK(jdm_ms_to_bar(1, 4000001, 500000, 4) == 2000);                          /* just past it: a whole bar, rounded up */
    CHECK(jdm_ms_to_bar(1, -1000000, 500000, 4) == 500);                          /* before the session's beat 0 */
    CHECK(jdm_ms_to_bar(1, 1500000, 500000, 0) == 1250);                          /* no quantum: 4 */
    CHECK(jdm_ms_to_bar(1, 0, 500000, 3) == 0 && jdm_ms_to_bar(1, 1000000, 500000, 3) == 1000);
    CHECK(jdm_ms_to_bar(1, 1, 2000000000, 16) == 65535);                          /* longer than the field: its most */
}

int main(void)
{
    test_keys();
    test_open();
    test_link_mode_follows_the_lead();
    test_start_request();
    test_stop_request();
    test_ms_to_bar();
    printf("test_dn2menu: %d checks, %d failed\n", checks, fails);
    return fails;
}
