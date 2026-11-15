/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of beat_latency per audio source (core/jank_lat.c) and its use in the settings (jk_lat_select, jk_lat_set,
 * the control line). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_core.h"
#include "jank_lat.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const struct jla_key COMPUTER = {JLA_COMPUTER, 0, 0}, LINE = {JLA_LINE, 0, 0};

static struct jla_key dev(uint16_t vid, uint16_t pid) { return (struct jla_key){JLA_HOSTED, vid, pid}; }

static void test_table(void)
{
    struct jla t, u;
    struct jla_key a = dev(0x1935, 0x000c), b = dev(0x1935, 0x000d);
    char name[16];
    jla_init(&t);
    CHECK(jla_get(&t, &COMPUTER) == JLA_COMPUTER_DEFAULT_MS && jla_get(&t, &LINE) == JLA_LINE_PATH_MS);
    CHECK(jla_get(&t, &a) == JLA_HOSTED_DEFAULT_MS && t.ndev == 0);    /* a device unknown: the default, not added */
    jla_set(&t, &COMPUTER, 31);
    jla_set(&t, &LINE, -4);
    jla_set(&t, &a, 7);
    jla_set(&t, &b, -2);
    CHECK(jla_get(&t, &COMPUTER) == 31 && jla_get(&t, &LINE) == -4 && jla_get(&t, &a) == 7 && jla_get(&t, &b) == -2);
    CHECK(t.ndev == 2 && t.dev[0].pid == 0x000d && t.dev[1].pid == 0x000c);   /* most recently set first */
    jla_set(&t, &a, 9);
    CHECK(t.ndev == 2 && t.dev[0].pid == 0x000c && t.dev[0].ms == 9 && t.dev[1].pid == 0x000d);
    for (uint16_t p = 100; p < 100 + JLA_DEVS; p++)
        jla_set(&t, &(struct jla_key){JLA_HOSTED, 1, p}, p);
    CHECK(t.ndev == JLA_DEVS && jla_get(&t, &a) == JLA_HOSTED_DEFAULT_MS);      /* the oldest two left */
    CHECK(t.dev[0].pid == 100 + JLA_DEVS - 1 && t.dev[JLA_DEVS - 1].pid == 100);
    jla_set(&t, &COMPUTER, 900);
    CHECK(jla_get(&t, &COMPUTER) == JLA_MAX_MS);                                 /* kept within the range */
    CHECK(!strcmp(jla_name(&a, name), "host:1935:000c") && !strcmp(jla_name(&LINE, name), "line") && !strcmp(jla_name(&COMPUTER, name), "usb"));
    CHECK(jla_same_key(&a, &a) && !jla_same_key(&a, &b) && !jla_same_key(&a, &COMPUTER));
    {
        uint8_t blob[JLA_PACKED];
        CHECK(jla_pack(&t, blob, sizeof blob) == JLA_PACKED && JLA_PACKED == 54);
        jla_init(&u);
        CHECK(jla_unpack(&u, blob, sizeof blob) == 0 && jla_same(&t, &u));
        CHECK(jla_unpack(&u, blob, sizeof blob - 1) == -1);                      /* not one: refused */
        blob[0] = 9;
        CHECK(jla_unpack(&u, blob, sizeof blob) == -1 && jla_same(&t, &u));     /* (and the table untouched) */
        blob[0] = 1;
        blob[5] = JLA_DEVS + 1;
        CHECK(jla_unpack(&u, blob, sizeof blob) == -1);
        CHECK(jla_pack(&t, blob, 10) == 0);
    }
    jla_init(&u);
    CHECK(!jla_same(&t, &u));
}

/* the settings: the live value follows the source heard; a change goes to that source's value; a change of the table is
 * a stored change (saved once stopped), a change of the source alone is not */
static void test_settings(void)
{
    struct jk_cfg c, before;
    struct jla_key elektron = dev(0x1935, 0x000c);
    jk_cfg_defaults(&c);
    CHECK(c.beat_latency_ms == JK_BEAT_LATENCY_DEFAULT_MS && c.lat_key.kind == JLA_COMPUTER && c.usbmidi == 1);
    jk_lat_set(&c, 40);                                       /* Calibrate on the computer's audio */
    CHECK(c.beat_latency_ms == 40 && jla_get(&c.lat, &COMPUTER) == 40);
    before = c;
    CHECK(jk_lat_select(&c, &elektron) == 1 && c.beat_latency_ms == JLA_HOSTED_DEFAULT_MS);   /* the box: its own */
    CHECK(!jk_cfg_stored_differs(&before, &c));               /* a source switch writes nothing */
    CHECK(jk_lat_select(&c, &elektron) == 0);
    CHECK(jk_nudge_ms(&c, 1) == 1 && jk_nudge_ms(&c, 1) == 1 && c.beat_latency_ms == 2);   /* YES + UP twice */
    CHECK(jla_get(&c.lat, &elektron) == 2 && jla_get(&c.lat, &COMPUTER) == 40);
    CHECK(jk_cfg_stored_differs(&before, &c));                /* saved (once stopped) */
    CHECK(jk_lat_select(&c, &COMPUTER) == 1 && c.beat_latency_ms == 40);   /* back on the computer: its value */
    CHECK(jk_lat_select(&c, &LINE) == 1 && c.beat_latency_ms == JLA_LINE_PATH_MS);
    CHECK(jk_nudge_ms(&c, -3) == 1 && c.beat_latency_ms == JLA_LINE_PATH_MS - 3 && c.lat.line_trim_ms == -3);
    CHECK(jk_lat_select(&c, &elektron) == 1 && c.beat_latency_ms == 2);
    c.beat_latency_ms = JK_BEAT_LATENCY_MAX_MS;
    CHECK(jk_nudge_ms(&c, 1) == 0);                           /* at the top: nothing */
}

/* the control line: set beat_latency (Calibrate) is the current source's; beat_latency_usb the computer's always;
 * get and status say the source; usbmidi */
static void test_ctl(void)
{
    static char out[2048];
    struct jk_cfg c;
    struct jk_snapshot s;
    struct jla_key elektron = dev(0x1935, 0x000c);
    jk_cfg_defaults(&c);
    memset(&s, 0, sizeof s);
    CHECK(jk_ctl_line("set beat_latency 33", &c, &s, 0, out, sizeof out) == JK_CTL_CFG_CHANGED && jla_get(&c.lat, &COMPUTER) == 33);
    jk_lat_select(&c, &elektron);
    CHECK(jk_ctl_line("set beat_latency 6", &c, &s, 0, out, sizeof out) == JK_CTL_CFG_CHANGED);
    CHECK(c.beat_latency_ms == 6 && jla_get(&c.lat, &elektron) == 6 && jla_get(&c.lat, &COMPUTER) == 33);
    CHECK(jk_ctl_line("set beat_latency_usb 28", &c, &s, 0, out, sizeof out) == JK_CTL_CFG_CHANGED);
    CHECK(c.beat_latency_ms == 6 && jla_get(&c.lat, &COMPUTER) == 28);    /* the computer's, the box's live value kept */
    CHECK(jk_ctl_line("set beat_latency_usb 301", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && strstr(out, "err"));
    CHECK(jk_ctl_line("get", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && strstr(out, "beat_latency=6\nbeat_latency_src=host:1935:000c\n"));
    CHECK(strstr(out, "usbmidi=on\n"));
    CHECK(jk_ctl_line("set usbmidi off", &c, &s, 0, out, sizeof out) == JK_CTL_CFG_CHANGED && c.usbmidi == 0);
    CHECK(jk_ctl_line("set usbmidi maybe", &c, &s, 0, out, sizeof out) == JK_CTL_NONE && strstr(out, "err") && c.usbmidi == 0);
    {
        struct jk_cfg d;
        jk_cfg_defaults(&d);
        c = d;
        c.usbmidi = 0;
        CHECK(jk_cfg_stored_differs(&c, &d));                 /* a stored setting */
    }
    jk_cfg_defaults(&c);
    jk_lat_select(&c, &elektron);
    CHECK(jk_ctl_line("status", &c, &s, 0, out, sizeof out) == JK_CTL_NONE);
    CHECK(strstr(out, "beat_latency=0\nbeat_latency_src=host:1935:000c\nusbmidi=on\n"));
}

int main(void)
{
    test_table();
    test_settings();
    test_ctl();
    printf("lat: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
