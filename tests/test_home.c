/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the HOME line and the control line's home command. Exit status: non-zero on a failure. */
#include <stdio.h>
#include <string.h>
#include "jank_core.h"
#include "jank_home.h"
#include "jank_hold.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void snapshot(struct jk_snapshot *s, struct jk_cfg *c)
{
    static const uint8_t mac[6] = {0x02, 0x6A, 0x6B, 1, 2, 3};
    memset(s, 0, sizeof *s);
    jk_cfg_defaults(c);
    jk_init(&s->b, mac, 3, JK_FILTER_LINK);
    c->group = 3;
    s->batt_pct = -1;
}

static void test_home_line(void)
{
    struct jk_snapshot s;
    struct jk_cfg c;
    char out[JHM_LINE_MAX + 1];
    snapshot(&s, &c);
    /* no session, no audio, no battery */
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0);
    CHECK(!strcmp(out, "h 0 j3 0 - 4 0 0 - - - - - 0"));
    /* 120 BPM, beat 6.75 of a 4/4 bar: 2.75 into the bar; playing; two Link peers; the tracker hearing */
    s.mpb = 500000;
    s.beat_ub = 6750000;
    s.playing = 1;
    s.link_peers = 2;
    s.beat.audio = 1;
    s.beat.level_db = -24;
    s.beat.level_min_db = -31;
    s.beat.level_max_db = -18;
    s.beat.conf_pct = 87;
    s.batt_pct = 64;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0);
    CHECK(!strcmp(out, "h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18 0"));
    /* negative beats (before the session's zero) go into the bar as Link's phase does; armed beats playing */
    s.beat_ub = -1500000;
    s.armed = 1;
    c.quantum = 3;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0);
    CHECK(!strcmp(out, "h 12000 j3 2 150 3 2 0 64 -24 87 -31 -18 0"));
    /* AUDIO mode (sound): the transport is the clock outputs' (jank_hold.h), whatever the session does */
    s.beat.audio_live = 1;
    s.outputs = JHO_STOPPED;                                    /* held: stopped, the session armed / playing */
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 j3 0 ", 13));
    s.outputs = JHO_RUNNING;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 j3 1 ", 13));
    s.outputs = JHO_ARMED;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 j3 2 ", 13));
    CHECK(!strcmp(out + strlen(out) - 2, " 1"));                /* the mode: AUDIO */
    s.beat.fold_exp = 1;                                        /* AUDIO folded: x2 x4 /2 /4 */
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strcmp(out + strlen(out) - 3, " x2"));
    s.beat.fold_exp = -2;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strcmp(out + strlen(out) - 3, " /4"));
    s.beat.fold_exp = 2;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strcmp(out + strlen(out) - 3, " x4"));
    s.beat.fold_exp = -1;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strcmp(out + strlen(out) - 3, " /2"));
    s.beat.fold_exp = 0;
    {
        char conf[8];
        snprintf(conf, sizeof conf, " %u ", (unsigned)s.beat.conf_pct);
        CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && strstr(out, conf));
        s.beat.conf_hold = 1;                                   /* M2k: the confidence holds: "<pct>h" */
        snprintf(conf, sizeof conf, " %uh ", (unsigned)(s.beat.conf_pct > 99 ? 99 : s.beat.conf_pct));
        CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && strstr(out, conf));
        s.beat.conf_hold = 0;
        s.beat.coast = 1;                                       /* the coast (robust follower): "<pct>c" */
        snprintf(conf, sizeof conf, " %uc ", (unsigned)(s.beat.conf_pct > 99 ? 99 : s.beat.conf_pct));
        CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && strstr(out, conf));
        s.beat.conf_hold = 1;                                   /* (a hold wins: it is not measured) */
        snprintf(conf, sizeof conf, " %uh ", (unsigned)(s.beat.conf_pct > 99 ? 99 : s.beat.conf_pct));
        CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && strstr(out, conf));
        s.beat.conf_hold = 0;
        s.beat.coast = 0;
    }
    s.beat.audio_live = 0;
    s.beat_ub = -1;                                             /* just below zero: the bar's last hundredth */
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out + 13, "299 ", 4));
    /* on the Wi-Fi network: w1 / w0 and no air peers */
    c.net = JK_NET_WIFI;
    s.wifi_up = 0;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 w0 ", 11));
    s.wifi_up = 1;
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 w1 ", 11));
    /* the widest line fits a push frame */
    s.mpb = 60061;                                              /* 999 BPM */
    c.quantum = 16;
    s.beat_ub = 15999999;
    s.link_peers = 99;
    s.batt_pct = 100;
    s.beat.level_db = s.beat.level_min_db = s.beat.level_max_db = -99;
    s.beat.conf_pct = 100;
    s.beat.audio_live = 1;
    s.beat.fold_exp = -2;                                       /* the mode at its widest: two characters */
    s.beat.conf_hold = 1;                                       /* and the hold (M2k): "99h", no wider */
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && strlen(out) <= JHM_LINE_MAX && strstr(out, " 99h "));
    s.beat.conf_hold = 0;
    s.beat.coast = 1;                                           /* "99c" is as wide as "99h" */
    CHECK(jhm_home_line(&c, &s, 0, out, sizeof out) > 0 && strlen(out) <= JHM_LINE_MAX && strstr(out, " 99c "));
    printf("widest HOME line: %zu of %u\n", strlen(out), JHM_LINE_MAX);
    CHECK(jhm_home_line(&c, &s, 0, out, 10) == 0);              /* too small: nothing */
}

static void test_commands(void)
{
    struct jk_snapshot s;
    struct jk_cfg c;
    char out[1400];
    snapshot(&s, &c);
    CHECK(jk_ctl_line("home", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strcmp(out, "home=h 0 j3 0 - 4 0 0 - - - - - 0\nok\n"));
    CHECK(jk_ctl_line("get", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strstr(out, "ble"));
    CHECK(jk_ctl_line("set ble off", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strcmp(out, "err bad setting\n"));
    CHECK(jk_ctl_line("ble pair", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strcmp(out, "err unknown command\n"));
    CHECK(jk_ctl_line("status", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strstr(out, "ble_"));
    CHECK(jk_ctl_line("help", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strstr(out, "ble"));
}

int main(void)
{
    test_home_line();
    test_commands();
    printf("home: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
