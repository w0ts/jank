/* SPDX-License-Identifier: GPL-3.0-only */
/* The device's own UI on the firmware (uiscreen.h): core/jank_oled.h's screens and buttons fed from the firmware's state,
 * their setters carried out through the control line's own `set` (jk_ctl_line on a copy of the settings, then
 * app_cfg_set: saved at once while stopped, once stopped while playing), so the device, the control line and the phone
 * page always agree. Everything runs in the ui task (core 0).
 *
 * What the firmware has and what it does not (the screens show both the same way):
 *   AUTO BAR, SOURCE, USB, GROUP, NETWORK JANK / CLIENT (a restart), its SSID and passkey, MIDI OUT 1's offset (`offset`),
 *   AUDIO IN (`beat_latency`, not stored: as over the control line), LINK TRANSPORT (IGNORE = `transport auto`, FOLLOW and
 *   FOLLOW+LEAD = `transport link`), LINK tempo and start / stop (`tempo`, `play` / `stop`): the real settings.
 *   BRIGHTNESS, SCREEN OFF: this file's own, stored under "ui1".
 *   LEVEL, LOW CONFIDENCE, THRESHOLD, MIDI OUT 2 and CLK OUT offsets, CLK RATE: stored under "ui1" and shown, nothing in
 *   the firmware uses them yet (no gain stage, no confidence gate, no second UART or CLK output on this branch).
 *   AUDIO mode: the tracker owns the tempo and the transport (bridge_play answers -2): UP / DOWN / YES say TRACKER LEADS,
 *   as the emulator does for the XIAO; NUDGE MS works (AUDIO IN), NUDGE BEAT says TRACKER LEADS (no bar shift yet).
 *   LINK AP, SETUP and the network scan are not in the firmware: picking them says so; the scan list is empty. */
#include "uiscreen.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "board.h"
#include "esp_log.h"
#include "jank_oled_int.h"
#include "jank_phone.h"
#include "jank_usbrole.h"
#include "nvs.h"
#include "oled.h"
#include "usb_host_mode.h"

void app_reboot(void);

#define FRAME_MS 100u                   /* 10 frames a second; the driver sends the changed pages only */
#define FRAME_MIN_MS 30u                /* a button's frame comes at once, but not more often than this */
#define SPLASH_MS 800u
#define PREFS_SETTLE_MS 2000u           /* a stored preference waits this long after its last change (live edits) */
#define REBOOT_MS 800u
#define TEMPO_MIN 90                    /* the emulator's tempo range for UP / DOWN */
#define TEMPO_MAX 180
#define METER_EVERY_MS 250u
#define METER_SPAN 12                   /* 3 s of the meter's steps */
#define METER_FLOOR_DB (-120.0)
#define BAR_CONF_FULL 2.0               /* the downbeat estimator's commit level: the line is full here */
#if CONFIG_JANK_BOARD_MINI22
#define SPLASH_MODEL "MINI 2.2"
#elif CONFIG_JANK_BOARD_LCD2
#define SPLASH_MODEL "LCD2"
#else
#define SPLASH_MODEL "XIAO"
#endif

static const char *TAG = "uiscreen";

/* the preferences stored apart ("ui1"): the screen's own and the settings nothing uses yet */
struct prefs {
    uint8_t version, lead, bright, sleep, rate, conf_stop, th;
    int8_t out1, out2, gain;
};
#define PREFS_VERSION 1
static struct prefs s_prefs = {PREFS_VERSION, 1, 100, 0, 2, 1, 80, 0, 0, 0};
static uint8_t s_prefs_pending;
static uint32_t s_prefs_changed;

static int s_screen;
static uint32_t s_start, s_last_frame, s_reboot_at;
static uint8_t s_want_frame, s_reboot, s_boot;
static struct jo_ui s_ui;
static struct jo_input s_in;
static struct jo_ctx s_ctx;
static struct jk_snapshot s_snap;
#if BOARD_LCD
#define FRAME_BYTES JO_LCD2_FB_BYTES
#else
#define FRAME_BYTES JO_FB_BYTES
#endif
static uint8_t s_fb[FRAME_BYTES];       /* the frame drawn (the driver keeps its own copy) */
static uint32_t s_quantum = 4;          /* the bar's beats, for the screen's beat flash */
static char s_ap_name[JO_SSID_MAX + 1] = "jank", s_ap_pass[JO_TEXT_MAX + 1] = "";

struct meter_step {
    uint32_t t;
    double db;
};
static struct {
    uint32_t at;
    double bar, lo, hi;
    struct meter_step hist[METER_SPAN];
    int n;
} s_meter = {0, METER_FLOOR_DB, METER_FLOOR_DB, METER_FLOOR_DB, {{0, 0}}, 0};

static void prefs_load(void)
{
    nvs_handle_t h;
    struct prefs p;
    size_t n = sizeof p;
    if (nvs_open("jank", NVS_READONLY, &h) != ESP_OK)
        return;
    if (nvs_get_blob(h, "ui1", &p, &n) == ESP_OK && n == sizeof p && p.version == PREFS_VERSION && p.bright >= 10 && p.bright <= 100 &&
        p.sleep < 4 && p.rate < 5 && p.th <= 100)
        s_prefs = p;
    nvs_close(h);
}

static void prefs_save(void)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open("jank", NVS_READWRITE, &h);
    if (e == ESP_OK) {
        e = nvs_set_blob(h, "ui1", &s_prefs, sizeof s_prefs);
        if (e == ESP_OK)
            e = nvs_commit(h);
        nvs_close(h);
    }
    if (e != ESP_OK)
        ESP_LOGE(TAG, "screen settings not saved: %s", esp_err_to_name(e));
}

static void prefs_changed(uint32_t now)
{
    s_prefs_pending = 1;
    s_prefs_changed = now;
}

/* the flash rule of the settings (jank_core.h jk_save_due): only while stopped; and after the edits have settled */
static void prefs_step(uint32_t now)
{
    if (s_prefs_pending && now - s_prefs_changed >= PREFS_SETTLE_MS && jk_save_due(&s_prefs_pending, bridge_is_playing()))
        prefs_save();
}

/* a setting through the control line's `set`: 0 changed (and published, saved by the usual rule) */
static int ctl_set(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int ctl_set(const char *fmt, ...)
{
    char line[96], out[64];
    struct jk_cfg cfg;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    app_cfg_get(&cfg);
    if (jk_ctl_line(line, &cfg, &s_snap, bridge_ms(), out, sizeof out) != JK_CTL_CFG_CHANGED) {
        ESP_LOGW(TAG, "%s: %s", line, out);
        return -1;
    }
    app_cfg_set(&cfg);
    return 0;
}

/* the input meter as the emulator's: a step every 250 ms, the bar easing toward the level, the min / max lines toward
 * the extremes of the last 3 s */
static void meter_step(uint32_t now, int audio, double level_db)
{
    double dt, db, lo, hi;
    if (s_meter.at && now - s_meter.at < METER_EVERY_MS)
        return;
    dt = s_meter.at ? (now - s_meter.at) / 1000.0 : 1e9;
    s_meter.at = now;
    db = audio ? fmin(0.0, level_db + s_prefs.gain) : METER_FLOOR_DB;
    if (s_meter.n == METER_SPAN)
        memmove(s_meter.hist, s_meter.hist + 1, (METER_SPAN - 1) * sizeof s_meter.hist[0]);
    else
        s_meter.n++;
    s_meter.hist[s_meter.n - 1] = (struct meter_step){now, db};
    lo = hi = db;
    for (int i = 0; i < s_meter.n; i++) {
        lo = fmin(lo, s_meter.hist[i].db);
        hi = fmax(hi, s_meter.hist[i].db);
    }
    s_meter.bar += (db - s_meter.bar) * (1.0 - exp(-dt / 0.6));
    s_meter.lo += (lo - s_meter.lo) * (1.0 - exp(-dt / 0.5));
    s_meter.hi += (hi - s_meter.hi) * (1.0 - exp(-dt / 0.5));
}

static void ip_text(uint32_t ip, char *out, size_t cap)
{
    snprintf(out, cap, "%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)(ip >> 16 & 255u), (unsigned)(ip >> 8 & 255u), (unsigned)(ip & 255u));
}

static void net_ctx(struct jo_ctx *c, const struct jk_cfg *cfg, uint32_t now)
{
    struct jo_net *n = &c->net;
    int rssi = cfg->net == JK_NET_WIFI ? (s_snap.wifi_up ? s_snap.wifi_rssi : 0) : jk_best_rssi(&s_snap.b, now);
    c->has_rssi = 1;                    /* 0: none heard (jank: the best peer's, Wi-Fi client: the router's) */
    c->rssi = rssi;
    n->mode = cfg->net == JK_NET_WIFI ? JO_NET_CLIENT : JO_NET_JANK;
    n->prev = n->mode;
    n->group = cfg->group;
    snprintf(n->ssid, sizeof n->ssid, "%s", cfg->ssid);
    snprintf(n->pass, sizeof n->pass, "%s", cfg->pass);
    snprintf(n->appass, sizeof n->appass, "%s", s_ap_pass);
    snprintf(n->ap_name, sizeof n->ap_name, "%s", s_ap_name);
    snprintf(n->status, sizeof n->status, "%s", !cfg->ssid[0] ? "NO NETWORK" : s_snap.wifi_up && s_snap.wifi_ip ? "CONNECTED" : "CONNECTING");
    if (s_snap.wifi_ip)
        ip_text(s_snap.wifi_ip, n->ip, sizeof n->ip);
    else
        snprintf(n->ip, sizeof n->ip, "-");
    n->ap_clients = 0;
    n->nscan = 0;                       /* no scan for networks in the firmware yet */
    n->setup_name[0] = 0;
    c->wifi = rssi == 0 ? JO_WIFI_NONE : rssi < -75 ? JO_WIFI_WEAK : JO_WIFI_GOOD;
    c->has_wifiq = cfg->net == JK_NET_JANK;   /* MODE JANK's air: its signal at 1 Mbit/s; no retry / lost counts kept */
    c->wifiq = (struct jo_wifiq){rssi, 1000.0, 0, 0, 0};
    c->group_peers = (int)jk_peers_heard(&s_snap.b, now);
}

static const struct jo_ctx *build_ctx(void *user, uint32_t now)
{
    struct jo_ctx *c = &s_ctx;
    struct jk_cfg cfg;
    const struct jk_beat *b = &s_snap.beat;
    int audio;
    (void)user;
    app_cfg_get(&cfg);
    bridge_snapshot(&s_snap);
    usbrole_snapshot(&s_snap.usb);
    audio = b->on && b->audio;
    meter_step(now, audio, b->level_db);
    memset(c, 0, sizeof *c);
    c->link = !audio;
    c->line_in = (uint8_t)audio;
    c->has_bpm = b->bpm_x100 > 0;
    c->bpm = b->bpm_x100 / 100.0;
    c->has_link_bpm = s_snap.mpb > 0;
    c->link_bpm = s_snap.mpb > 0 ? (double)((6000000000ull + (uint64_t)s_snap.mpb / 2u) / (uint32_t)s_snap.mpb) / 100.0 : 0.0;
    c->conf = audio ? b->conf_pct / 100.0 : 0.0;
    c->layout_lcd2 = BOARD_LCD;
    c->conf_unknown = !audio;
    c->has_peak = (uint8_t)audio;       /* the input's peak over the last second, as the input gives it (no LEVEL offset) */
    c->peak_db = b->peak_db;
    c->silent = audio ? b->silent : 0;
    c->gated = audio ? b->gated : 0;
    c->conf_hold = audio ? b->conf_hold : 0;
    c->fold_exp = audio ? b->fold_exp : 0;
    c->ble_mode = cfg.ble == JK_BLE_PAIR ? 0 : cfg.ble == JK_BLE_AUTO ? 1 : 2;      /* PAIR NOW, ON START, OFF: the menu's order */
    c->ble_phone = s_snap.ble_built && s_snap.ble_state == JPH_CONNECTED;
    c->has_ble_left = s_snap.ble_built && s_snap.ble_state == JPH_WINDOW;
    c->ble_left = (int)s_snap.ble_left_s;
    c->usb_midi = cfg.usbmidi ? 0 : 1;                                              /* ON, OFF */
    c->beat_flash = cfg.beat_flash;
    s_quantum = cfg.quantum ? cfg.quantum : 4u;
    c->th = s_prefs.th;
    c->gain = s_prefs.gain;
    c->conf_stop = s_prefs.conf_stop;
    c->level_db = s_meter.bar;
    c->level_min = s_meter.lo;
    c->level_max = s_meter.hi;
    c->armed = s_snap.armed;
    c->running = s_snap.playing && !s_snap.armed;
    c->tracker_leads = (uint8_t)audio;
    c->no_beat_nudge = 1;
    if (c->running && s_snap.mpb > 0) {
        int64_t beat = s_snap.beat_ub >= 0 ? s_snap.beat_ub / 1000000 : -((-s_snap.beat_ub + 999999) / 1000000);
        int64_t q = cfg.quantum ? cfg.quantum : 4;
        c->has_beat = 1;
        c->beat_n = (int)(((beat % q) + q) % q);
    }
    c->peers = (int)s_snap.link_peers;
    c->usb = (uint8_t)(s_snap.usb_up || s_snap.usb.vbus);
    c->fitted = s_snap.batt_pct >= 0;
    c->batt = s_snap.batt_pct >= 0 ? s_snap.batt_pct : 0;
    c->lt_mode = cfg.transport == JK_TRANSPORT_AUTO ? 0 : s_prefs.lead ? 2 : 1;
    c->in_src = cfg.source;
    c->usb_role = cfg.usb;
    c->auto_bar = cfg.autobar;
    c->has_bar = (uint8_t)audio;
    c->bar_state = b->bar & 3u;
    c->bar_conf = jo_clampd(b->bar_lead_x100 / 100.0 / BAR_CONF_FULL, 0.0, 1.0);
    c->in_off = jo_clampi(cfg.beat_latency_ms, -100, 100);
    c->offs[0] = jo_iround(cfg.offset_10us / 100.0);
    c->offs[1] = s_prefs.out1;
    c->offs[2] = s_prefs.out2;
    c->rate = s_prefs.rate;
    c->bright = s_prefs.bright;
    c->sleep = s_prefs.sleep;
    c->boot = s_boot;
    c->splash = s_screen && now - s_start < SPLASH_MS;
    c->splash_model = SPLASH_MODEL;
    c->splash_fw = "FW " JK_VERSION;
    net_ctx(c, &cfg, now);
    return c;
}

static int tempo_step(void *user, int d, double *bpm)
{
    const struct jo_ctx *c = build_ctx(user, bridge_ms());
    int v;
    if (!c->has_link_bpm)
        return -1;
    v = jo_clampi(jo_iround(c->link_bpm) + d, TEMPO_MIN, TEMPO_MAX);
    if (bridge_tempo((int32_t)((60000000 + v / 2) / v)) != 0)
        return -1;
    *bpm = v;
    return 0;
}

static void say(const char *title, const char *value) { jo_show_overlay(&s_ui, bridge_ms(), title, value); }

static void set_pref(int8_t *p8, uint8_t *u8, int v)
{
    if (p8)
        *p8 = (int8_t)v;
    else
        *u8 = (uint8_t)v;
    prefs_changed(bridge_ms());
}

static void set_network(int v)
{
    struct jk_cfg cfg;
    app_cfg_get(&cfg);
    if (v >= JO_NET_AP) {
        say(v == JO_NET_AP ? "LINK AP" : "SETUP", "NOT YET");
        return;
    }
    if (v == JO_NET_CLIENT && !cfg.ssid[0]) {
        say("LINK CLIENT", "NO SSID");
        return;
    }
    if (ctl_set("set net %s", v == JO_NET_CLIENT ? "wifi" : "jank") == 0) {   /* read at start-up: a restart */
        say("NETWORK", "RESTART");
        s_reboot = 1;
        s_reboot_at = bridge_ms() + REBOOT_MS;
    }
}

static void set_field(void *user, enum jo_field f, int v)
{
    static const char *const SRC[] = {"auto", "line", "usb", "link"};
    static const char *const AUTOBAR[] = {"off", "start", "always"};
    static const char *const BLE[] = {"pair", "auto", "off"};         /* the menu's order: PAIR NOW, ON START, OFF */
    (void)user;
    switch (f) {
    case JO_F_NETMODE: set_network(v); break;
    case JO_F_INOFF: ctl_set("set beat_latency %d", v); break;
    case JO_F_OUT0: ctl_set("set offset %d", v); break;
    case JO_F_OUT1: set_pref(&s_prefs.out1, NULL, v); break;
    case JO_F_OUT2: set_pref(&s_prefs.out2, NULL, v); break;
    case JO_F_RATE: set_pref(NULL, &s_prefs.rate, v); break;
    case JO_F_THRESHOLD: set_pref(NULL, &s_prefs.th, v); break;
    case JO_F_GAIN: set_pref(&s_prefs.gain, NULL, v); break;
    case JO_F_BRIGHT: set_pref(NULL, &s_prefs.bright, v); break;
    case JO_F_SLEEP: set_pref(NULL, &s_prefs.sleep, v); break;
    case JO_F_LOWCONF: set_pref(NULL, &s_prefs.conf_stop, v == 1); break;
    case JO_F_LTMODE:                   /* IGNORE: transport auto; FOLLOW / FOLLOW+LEAD: transport link, and the lead kept here */
        ctl_set("set transport %s", v == 0 ? "auto" : "link");
        if (v)
            set_pref(NULL, &s_prefs.lead, v == 2);
        break;
    case JO_F_GROUP: ctl_set("set group %d", v + 1); break;
    case JO_F_INSRC: ctl_set("set source %s", v >= 0 && v < 4 ? SRC[v] : "auto"); break;
    case JO_F_USBROLE: ctl_set("set usb %s", v ? "host" : "device"); break;
    case JO_F_AUTOBAR: ctl_set("set autobar %s", v >= 0 && v < 3 ? AUTOBAR[v] : "off"); break;
    case JO_F_BLE:                      /* PAIR NOW picked (even when it is the setting already) also opens the pair window */
        ctl_set("set ble %s", v >= 0 && v < 3 ? BLE[v] : "pair");
#if CONFIG_JANK_BLE_MIDI
        if (v == 0)
            ble_midi_pair();
#endif
        break;
    case JO_F_USBMIDI: ctl_set("set usbmidi %s", v ? "off" : "on"); break;
    case JO_F_BEATFLASH: ctl_set("set beat_flash %d", v ? 1 : 0); break;
    default: break;
    }
}

static void set_text(void *user, enum jo_field f, const char *t)
{
    (void)user;
    if (f == JO_F_SSID)
        ctl_set("set ssid %s", t);
    else if (f == JO_F_PASS) {
        if (!t[0] ? ctl_set("set pass") : ctl_set("set pass %s", t))
            say("PASSKEY", "8+ CHARS");
    } else if (f == JO_F_APPASS)
        snprintf(s_ap_pass, sizeof s_ap_pass, "%s", t);
    else if (f == JO_F_APNAME)
        snprintf(s_ap_name, sizeof s_ap_name, "%s", t);
}

static void set_hold(void *user, int on)
{
    (void)user;
    (void)on;                           /* AUDIO mode: the tracker leads (never reached: tracker_leads) */
}

static void link_start_stop(void *user, int on)
{
    (void)user;
    if (bridge_play(on) == -1)
        say("LINK", "NO SESSION");
}

static const struct jo_ops OPS = {NULL, tempo_step, set_field, set_text, set_hold, link_start_stop, NULL};
static const struct jo_host HOST = {&OPS, build_ctx};

uint8_t uis_contrast(void) { return (uint8_t)((s_prefs.bright * 255u + 50u) / 100u); }

void uis_init(void)
{
    prefs_load();
}

int uis_open_screen(void)
{
    s_screen = oled_open(uis_contrast());
    return s_screen;
}

void uis_start(uint32_t now)
{
    s_start = now;
    jo_ui_init(&s_ui);
    jo_input_init(&s_in, now);
    s_want_frame = 1;
}

int uis_has_screen(void) { return s_screen; }

void uis_button(enum jo_btn b, int down, uint32_t now)
{
    if (down)
        jo_input_down(&s_in, &s_ui, b, now, &HOST);
    else
        jo_input_up(&s_in, &s_ui, b, now, &HOST);
    s_want_frame = 1;
}

void uis_cancel_no(void)
{
    s_in.deferred[JO_NO] = 0;           /* the pair window's third press: its release does nothing */
    s_in.hint_on = 0;
    s_ui.hint = -1;
    s_ui.screen = JO_HOME;
#if BOARD_LCD
    jo_pair_overlay(&s_ui, bridge_ms());   /* the box counts the window's seconds down */
#else
    say("BLUETOOTH", "PAIR");
#endif
    s_want_frame = 1;
}

/* the frame into fb (and, on the colour screen, the ink plane lcd_ink() holds), and to the screen */
static void draw_frame(uint8_t *fb, const struct jo_ctx *c, uint32_t now)
{
#if BOARD_LCD
    jo_draw_lcd2(fb, lcd_ink(), &s_ui, c, now);
#else
    jo_draw(fb, &s_ui, c, now);
#endif
}

static void dark_frame(uint8_t *fb)
{
    memset(fb, 0, FRAME_BYTES);
#if BOARD_LCD
    memset(lcd_ink(), 0, JO_LCD2_INK_BYTES);
#endif
}

/* flash_ok: the colour screen may flash this frame on the beat (the BEAT FLASH setting SCREEN, HOME, the transport running; never a dark frame) */
static void show_frame(const uint8_t *fb, int flash_ok)
{
#if BOARD_LCD
    lcd_show(fb, flash_ok, s_quantum);
#else
    (void)flash_ok;
    oled_show(fb, uis_contrast());
#endif
}

void uis_boot_screen(void)
{
    s_boot = 1;
    if (!s_screen)
        return;
    draw_frame(s_fb, build_ctx(NULL, bridge_ms()), bridge_ms());
    show_frame(s_fb, 0);
}

void uis_step(uint32_t now, struct uis_led *led)
{
    uint8_t *fb = s_fb;
    const struct jo_ctx *c;
    jo_input_tick(&s_in, &s_ui, now, &HOST);
    prefs_step(now);
    if (s_reboot && (int32_t)(now - s_reboot_at) >= 0) {
        app_reboot();
        return;
    }
    if (!(s_want_frame && now - s_last_frame >= FRAME_MIN_MS) && now - s_last_frame < FRAME_MS)
        return;
    s_want_frame = 0;
    s_last_frame = now;
    c = build_ctx(NULL, now);
    led->armed = c->armed;
    led->running = c->running;
    led->joined = s_snap.mpb > 0;
    led->low_conf = !c->link && c->conf * 100.0 < c->th;
    led->bar_locked = !c->link && c->bar_state == JO_BAR_LOCKED;
    led->batt_pct = s_snap.batt_pct;
    led->on_usb = c->usb;
    led->valid = 1;
    if (!s_screen)
        return;
    if (jo_asleep(&s_in, c, now)) {
        dark_frame(fb);                   /* SCREEN OFF: a dark frame; the panel stays on (the 2.42"'s power rule) */
        show_frame(fb, 0);
        return;
    }
    draw_frame(fb, c, now);
    show_frame(fb, c->beat_flash && s_ui.screen == JO_HOME && !c->setup && !c->boot && !c->splash && c->has_beat);
}
