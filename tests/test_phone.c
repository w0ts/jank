/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the phone link: when Bluetooth is on (the pair window, the phone, the setting), the HOME line and its rate,
 * and the control line's ble and home commands. Exit status: non-zero on a failure. */
#include <stdio.h>
#include <string.h>
#include "jank_core.h"
#include "jank_phone.h"
#include "jank_hold.h"
#include "jank_sysex.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void test_pair_mode(void)
{
    struct jph p;
    uint32_t t = 5000;
    jph_init(&p, JK_BLE_PAIR, t);
    CHECK(!jph_radio_on(&p) && !jph_advertising(&p));           /* the default: off until asked */
    CHECK(!strcmp(jph_state_name(&p), "off"));
    CHECK(jph_pair(&p, t) == 0);
    CHECK(jph_radio_on(&p) && jph_advertising(&p) && jph_left_s(&p, t) == JPH_WINDOW_MS / 1000u);
    jph_step(&p, t + JPH_WINDOW_MS - 1u);
    CHECK(jph_advertising(&p) && jph_left_s(&p, t + JPH_WINDOW_MS - 1u) == 1u);
    jph_step(&p, t + JPH_WINDOW_MS);
    CHECK(!jph_radio_on(&p) && jph_left_s(&p, t + JPH_WINDOW_MS) == 0u);   /* nobody came: off */
    /* asking again in a window starts it over */
    CHECK(jph_pair(&p, t) == 0);
    CHECK(jph_pair(&p, t + 60000u) == 0);
    jph_step(&p, t + JPH_WINDOW_MS + 1000u);
    CHECK(jph_advertising(&p));
    jph_step(&p, t + 60000u + JPH_WINDOW_MS);
    CHECK(!jph_radio_on(&p));
}

static void test_phone_comes_and_goes(void)
{
    struct jph p;
    uint32_t t = 100;
    jph_init(&p, JK_BLE_PAIR, t);
    jph_pair(&p, t);
    jph_connected(&p, t + 10000u);
    CHECK(jph_radio_on(&p) && !jph_advertising(&p) && !strcmp(jph_state_name(&p), "connected"));
    jph_step(&p, t + 10u * JPH_WINDOW_MS);                      /* connected: no window to run out */
    CHECK(jph_radio_on(&p));
    CHECK(jph_pair(&p, t) == 0 && !jph_advertising(&p));        /* asking while connected: nothing to do */
    jph_disconnected(&p, 1000000u);
    CHECK(jph_advertising(&p) && jph_left_s(&p, 1000000u) == JPH_REJOIN_MS / 1000u);   /* a short window to come back */
    jph_connected(&p, 1010000u);
    CHECK(!jph_advertising(&p) && jph_radio_on(&p));
    jph_disconnected(&p, 2000000u);
    jph_step(&p, 2000000u + JPH_REJOIN_MS);
    CHECK(!jph_radio_on(&p));
    /* a disconnect with the radio already off (closed by hand) opens nothing */
    jph_pair(&p, 3000000u);
    jph_connected(&p, 3000001u);
    jph_close(&p);
    jph_disconnected(&p, 3000002u);
    CHECK(!jph_radio_on(&p));
    /* a link that comes up after the window closed is not kept */
    jph_connected(&p, 3000003u);
    CHECK(!jph_radio_on(&p));
}

static void test_auto_and_off(void)
{
    struct jph p;
    jph_init(&p, JK_BLE_AUTO, 0);
    CHECK(jph_advertising(&p) && jph_left_s(&p, 0) == JPH_WINDOW_MS / 1000u);   /* auto: a window at the start */
    jph_step(&p, JPH_WINDOW_MS);
    CHECK(!jph_radio_on(&p));
    jph_init(&p, JK_BLE_OFF, 0);
    CHECK(!jph_radio_on(&p) && jph_pair(&p, 0) == -1 && !jph_radio_on(&p));
    jph_set_mode(&p, JK_BLE_PAIR);
    CHECK(!jph_radio_on(&p) && jph_pair(&p, 0) == 0 && jph_advertising(&p));
    jph_connected(&p, 1);
    jph_set_mode(&p, JK_BLE_AUTO);
    CHECK(jph_radio_on(&p));                                    /* another on-setting leaves the phone on */
    jph_set_mode(&p, JK_BLE_OFF);
    CHECK(!jph_radio_on(&p));                                   /* off: at once, the phone too */
    jph_disconnected(&p, 2);
    CHECK(!jph_radio_on(&p));
    jph_init(&p, 7, 0);                                         /* a bad value: the default */
    CHECK(p.mode == JK_BLE_PAIR && !jph_radio_on(&p));
    /* the clock wraps inside a window */
    jph_init(&p, JK_BLE_PAIR, 0);
    jph_pair(&p, 0xFFFFFF00u);
    jph_step(&p, 0x100u);
    CHECK(jph_advertising(&p));
    jph_step(&p, 0xFFFFFF00u + JPH_WINDOW_MS);
    CHECK(!jph_radio_on(&p));
}

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
    char out[JX_PUSH_TEXT_MAX + 1];
    snapshot(&s, &c);
    /* no session, no audio, no battery */
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0);
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
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0);
    CHECK(!strcmp(out, "h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18 0"));
    /* negative beats (before the session's zero) go into the bar as Link's phase does; armed beats playing */
    s.beat_ub = -1500000;
    s.armed = 1;
    c.quantum = 3;
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0);
    CHECK(!strcmp(out, "h 12000 j3 2 150 3 2 0 64 -24 87 -31 -18 0"));
    /* AUDIO mode (sound): the transport is the clock outputs' (jank_hold.h), whatever the session does */
    s.beat.audio_live = 1;
    s.outputs = JHO_STOPPED;                                    /* held: stopped, the session armed / playing */
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 j3 0 ", 13));
    s.outputs = JHO_RUNNING;
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 j3 1 ", 13));
    s.outputs = JHO_ARMED;
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 j3 2 ", 13));
    CHECK(!strcmp(out + strlen(out) - 2, " 1"));                /* the mode: AUDIO */
    s.beat.fold_exp = 1;                                        /* AUDIO folded: x2 x4 /2 /4 */
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strcmp(out + strlen(out) - 3, " x2"));
    s.beat.fold_exp = -2;
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strcmp(out + strlen(out) - 3, " /4"));
    s.beat.fold_exp = 2;
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strcmp(out + strlen(out) - 3, " x4"));
    s.beat.fold_exp = -1;
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strcmp(out + strlen(out) - 3, " /2"));
    s.beat.fold_exp = 0;
    {
        char conf[8];
        snprintf(conf, sizeof conf, " %u ", (unsigned)s.beat.conf_pct);
        CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && strstr(out, conf));
        s.beat.conf_hold = 1;                                   /* M2k: the confidence holds: "<pct>h" */
        snprintf(conf, sizeof conf, " %uh ", (unsigned)(s.beat.conf_pct > 99 ? 99 : s.beat.conf_pct));
        CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && strstr(out, conf));
        s.beat.conf_hold = 0;
    }
    s.beat.audio_live = 0;
    s.beat_ub = -1;                                             /* just below zero: the bar's last hundredth */
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out + 13, "299 ", 4));
    /* on the Wi-Fi network: w1 / w0 and no air peers */
    c.net = JK_NET_WIFI;
    s.wifi_up = 0;
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 w0 ", 11));
    s.wifi_up = 1;
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && !strncmp(out, "h 12000 w1 ", 11));
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
    CHECK(jph_home_line(&c, &s, 0, out, sizeof out) > 0 && strlen(out) <= JX_PUSH_TEXT_MAX && strstr(out, " 99h "));
    printf("widest HOME line: %zu of %u\n", strlen(out), JX_PUSH_TEXT_MAX);
    CHECK(jph_home_line(&c, &s, 0, out, 10) == 0);              /* too small: nothing */
}

static void test_home_rate(void)
{
    struct jph_rate r;
    jph_rate_init(&r);
    CHECK(jph_home_due(&r, "a", 1, 1000) == 1);                 /* the first at once */
    CHECK(jph_home_due(&r, "b", 1, 1000 + JPH_HOME_EVERY_MS - 1u) == 0);
    CHECK(jph_home_due(&r, "b", 1, 1000 + JPH_HOME_EVERY_MS) == 1);
    CHECK(jph_home_due(&r, "b", 1, 1000 + 2u * JPH_HOME_EVERY_MS) == 0);   /* the same: held back */
    CHECK(jph_home_due(&r, "b", 1, 1000 + JPH_HOME_EVERY_MS + JPH_HOME_IDLE_MS) == 1);   /* until the idle period */
    /* at most 2.5 a second whatever changes */
    {
        uint32_t sent = 0;
        char line[8];
        jph_rate_init(&r);
        for (uint32_t t = 0; t < 10000u; t += 10u) {
            snprintf(line, sizeof line, "%lu", (unsigned long)t);
            sent += (uint32_t)jph_home_due(&r, line, strlen(line), t);
        }
        CHECK(sent == 25u);
    }
}

static void test_commands(void)
{
    struct jk_snapshot s;
    struct jk_cfg c;
    char out[1400];
    snapshot(&s, &c);
    CHECK(jk_ctl_line("get", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && strstr(out, "\nble=pair\n"));
    CHECK(jk_ctl_line("set ble off", &c, &s, 0, out, sizeof out) == JK_CTL_CFG_CHANGED && c.ble == JK_BLE_OFF);
    CHECK(jk_ctl_line("set ble auto", &c, &s, 0, out, sizeof out) == JK_CTL_CFG_CHANGED && c.ble == JK_BLE_AUTO);
    CHECK(jk_ctl_line("get", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && strstr(out, "\nble=auto\n"));
    CHECK(jk_ctl_line("set ble on", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strcmp(out, "err bad setting\n") && c.ble == JK_BLE_AUTO);
    CHECK(jk_ctl_line("set ble pair", &c, &s, 0, out, sizeof out) == JK_CTL_CFG_CHANGED && c.ble == JK_BLE_PAIR);
    CHECK(jk_ctl_line("ble pair", &c, &s, 0, out, sizeof out) == JK_CTL_BLE_PAIR && !out[0]);
    CHECK(jk_ctl_line(" ble close ", &c, &s, 0, out, sizeof out) == JK_CTL_BLE_CLOSE && !out[0]);
    CHECK(jk_ctl_line("ble", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strcmp(out, "err unknown command\n"));
    CHECK(jk_ctl_line("home", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strcmp(out, "home=h 0 j3 0 - 4 0 0 - - - - - 0\nok\n"));
    /* status names the link only in a build with Bluetooth */
    CHECK(jk_ctl_line("status", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && !strstr(out, "ble_state="));
    s.ble_built = 1;
    s.ble_state = JPH_WINDOW;
    s.ble_left_s = 87;
    CHECK(jk_ctl_line("status", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && strstr(out, "\nble_state=window\nble_left=87\n"));
    CHECK(jk_ctl_line("help", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && strstr(out, "ble pair|close\n") && strstr(out, "|ble|"));
}

int main(void)
{
    test_pair_mode();
    test_phone_comes_and_goes();
    test_auto_and_off();
    test_home_line();
    test_home_rate();
    test_commands();
    printf("phone: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
