/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the DN2's text page (tests/test_oled_text.c, tests/dump_oled_text.c): a unit's state the screens' setters change, as
 * tests/test_oled.c's world, and its UI. Made-up network names and passkeys only. */
#ifndef OLED_TEXT_WORLD_H
#define OLED_TEXT_WORLD_H
#include <stdio.h>
#include <string.h>
#include "jank_oled_int.h"
#include "jank_oled_text.h"

struct world {
    struct jo_ctx c;
    struct jo_ops ops;
    struct jo_ui ui;
};

static void world_set_field(void *user, enum jo_field f, int v)
{
    struct jo_ctx *c = &((struct world *)user)->c;
    switch (f) {
    case JO_F_THRESHOLD: c->th = v; break;
    case JO_F_LTMODE: c->lt_mode = (uint8_t)v; break;
    case JO_F_GAIN: c->gain = v; break;
    case JO_F_RATE: c->rate = (uint8_t)v; break;
    case JO_F_NETMODE: c->net.mode = (uint8_t)v; break;
    case JO_F_GROUP: c->net.group = (uint8_t)(v + 1); break;
    case JO_F_USBROLE: c->usb_role = (uint8_t)v; break;
    case JO_F_AUTOBAR: c->auto_bar = (uint8_t)v; break;
    default: break;
    }
}

static void world_set_text(void *user, enum jo_field f, const char *t)
{
    struct jo_net *n = &((struct world *)user)->c.net;
    if (f == JO_F_SSID)
        snprintf(n->ssid, sizeof n->ssid, "%s", t);
    else if (f == JO_F_PASS)
        snprintf(n->pass, sizeof n->pass, "%s", t);
}

static const struct jo_qr WORLD_NO_QR;

/* a unit in LINK mode, playing on a jank group, on USB with a cell; the text page shows its state (text_view) */
static void world_init(struct world *w, int lcd2)
{
    struct jo_ctx *c = &w->c;
    memset(w, 0, sizeof *w);
    w->ops = (struct jo_ops){w, NULL, world_set_field, world_set_text, NULL, NULL, NULL};
    jo_ui_init(&w->ui);
    c->layout_lcd2 = (uint8_t)lcd2;
    c->text_view = 1;
    c->link = 1;
    c->has_link_bpm = 1;
    c->link_bpm = 120.0;
    c->running = 1;
    c->has_beat = 1;
    c->beat_n = 1;
    c->peers = 2;
    c->wifi = JO_WIFI_GOOD;
    c->usb = 1;
    c->fitted = 1;
    c->batt = 80;
    c->lt_mode = 2;
    c->th = 50;
    c->usb_role = 1;
    c->auto_bar = 1;
    c->rate = 2;
    c->bright = 70;
    c->pause_db = -40;
    c->offs[0] = 3;
    c->offs[1] = -2;
    c->net.mode = JO_NET_JANK;
    c->net.group = 1;
    snprintf(c->net.ssid, sizeof c->net.ssid, "TESTNET-5G-EXTENDED");
    snprintf(c->net.pass, sizeof c->net.pass, "made-up-pw");
    snprintf(c->net.ap_name, sizeof c->net.ap_name, "Jank");
    snprintf(c->net.status, sizeof c->net.status, "CONNECTED");
    snprintf(c->net.ip, sizeof c->net.ip, "192.168.1.23");
    c->group_peers = 2;
    c->version = "0.72.0";
    c->temp_c = 41;
    c->cpu_pct = 57;
    c->cpu1_pct = 3;
    c->heap_kb = 61;
    c->psram_kb = 7900;
    c->flash_kb = 8192;
    c->app_used_kb = 1900;
    c->app_kb = 3072;
    c->uptime_s = 3725;
    c->heap_min_kb = 40;
    c->reset = "POWERON";
    c->clk_late_us = 12;
    c->ui_loop_us = 900;
    c->beat_loop_us = 2100;
    c->qr = &WORLD_NO_QR;
    c->splash_model = "MINI 3";
    c->splash_fw = "FW 0.72.0";
}

/* the networks a scan found: NET-1.., bars 0..3 in turn, the current one NET-3 */
static void world_scan(struct world *w, int n)
{
    w->c.net.nscan = (uint8_t)n;
    for (int i = 0; i < n && i < JO_SCAN_MAX; i++) {
        snprintf(w->c.net.scan[i].name, sizeof w->c.net.scan[i].name, "NET-%d", i + 1);
        w->c.net.scan[i].bars = (uint8_t)(i % 4);
    }
    snprintf(w->c.net.ssid, sizeof w->c.net.ssid, "NET-3");
}

static void world_press(struct world *w, enum jo_btn b, int times)
{
    for (int i = 0; i < times; i++)
        jo_press(&w->ui, b, 0, &w->c, &w->ops);
}
#endif
