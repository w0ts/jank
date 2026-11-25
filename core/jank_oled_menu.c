/* SPDX-License-Identifier: GPL-3.0-only */
/* The OLED UI's screens as a state machine: the emulator's EDITS, MENU, SUBS, netRows, the text editor and press(). */
#include "jank_oled_int.h"
#include <stdio.h>
#include <string.h>

static const char *const RATES[] = {"1/4", "1/8", "1/16", "1/32", "24 PPQN"};
const char *const jo_rates_short[5] = {"1/4", "1/8", "1/16", "1/32", "24P"};
static const char *const NET_MODES[] = {"JANK", "LINK CLIENT", "LINK AP", "SETUP"};
const char *const jo_net_short[4] = {"JANK", "CLIENT", "AP", "SETUP"};
#define LATER "+ LATER  - EARLIER"
static const char *const SLEEP_NAMES[] = {"NEVER", "30 S", "1 MIN", "5 MIN"};
static const char *const LT_MODES[] = {"IGNORE", "FOLLOW", "FOLLOW+LEAD"};
const char *const jo_lt_short[3] = {"IGNORE", "FOLLOW", "BOTH"};
static const char *const GROUPS[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
static const char *const LOWCONF[] = {"KEEP RUNNING", "STOP / START"};
static const char *const INSRC[] = {"AUTO", "LINE IN", "USB AUDIO"};
const char *const jo_usb_names[2] = {"DEVICE", "HOST"};
static const char *const OFF_ON[] = {"OFF", "ON"};
/* the lcd2 layout's own lists (ui/oled.js EDITS: AUTO BAR and SOURCE have more choices there, and USB MIDI CLOCK) */
const char *const jo_off_on_names[2] = {"ON", "OFF"};
const char *const jo_autobar_short[3] = {"OFF", "START", "ALWAYS"};
static const char *const INSRC_LCD2[] = {"AUTO", "LINE IN", "USB AUDIO", "LINK"};
static const char *const AUTOBAR_LCD2[] = {"OFF", "AT START", "ALWAYS"};

static const char *const H_NETMODE[][2] = {{"JANK GROUP NETWORK", ""}, {"JOIN A WIFI ROUTER", ""}, {"OWN WIFI NETWORK", ""},
                                           {"STOPS THE DEVICE", "OPENS SETUP HOTSPOT"}};
static const char *const H_INOFF[][2] = {{"INPUT LATENCY, MOVES", "ALL OUTPUTS. + LATER"}};
static const char *const H_OUT0[][2] = {{"MIDI OUT 1 TIMING", LATER}};
static const char *const H_OUT1[][2] = {{"MIDI OUT 2 TIMING", LATER}};
static const char *const H_OUT2[][2] = {{"MODULAR CLOCK TIMING", LATER}};
static const char *const H_RATE[][2] = {{"1 PULSE PER BEAT", "ON CLK OUT"}, {"2 PULSES PER BEAT", "ON CLK OUT"}, {"4 PULSES PER BEAT", "ON CLK OUT"},
                                        {"8 PULSES PER BEAT", "ON CLK OUT"}, {"24 PULSES PER BEAT", "ON CLK OUT"}};
static const char *const H_THRESHOLD[][2] = {{"BELOW IT: STOP", "(LOW CONFIDENCE)"}};
static const char *const H_PAUSE[][2] = {{"UNDER IT: A PAUSE", "THEN RESYNC"}};
static const char *const H_GAIN[][2] = {{"INPUT LEVEL", "- CUT   + BOOST"}};
static const char *const H_LTMODE[][2] = {{"CLOCKS RUN ON LINK", "TEMPO, OWN START"}, {"CLOCKS START + STOP", "WITH THE SESSION"},
                                          {"FOLLOWS, AND START/", "STOP DRIVES LINK"}};
static const char *const H_BRIGHT[][2] = {{"SCREEN BRIGHTNESS", "LOWER SAVES BATTERY"}};
static const char *const H_SLEEP[][2] = {{"SCREEN STAYS ON", ""}, {"DARK AFTER 30 S", "ANY BUTTON WAKES IT"}, {"DARK AFTER 1 MIN", "ANY BUTTON WAKES IT"},
                                         {"DARK AFTER 5 MIN", "ANY BUTTON WAKES IT"}};
static const char *const H_GROUP[][2] = {{"UNITS ON THE SAME", "GROUP PLAY TOGETHER"}, {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"},
                                         {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"}, {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"},
                                         {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"}, {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"},
                                         {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"}, {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"},
                                         {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"}};
static const char *const H_LOWCONF[][2] = {{"NEVER STOPS", ""}, {"STOPS BELOW THE", "THRESHOLD"}};
static const char *const H_INSRC[][2] = {{"LINE IN IF PLUGGED", "ELSE USB AUDIO"}, {"THE LINE IN JACK", "ONLY"}, {"AUDIO OVER USB", "ONLY"}};
static const char *const H_USBROLE[][2] = {{"TO A COMPUTER", "MIDI + AUDIO"}, {"TO SELF-POWERED", "MIDI + AUDIO GEAR"}};
static const char *const H_AUTOBAR[][2] = {{"THE BAR 1 STAYS", "WHERE IT IS"}, {"FINDS BEAT 1 IN THE", "MUSIC. NUDGE: MANUAL"}};

static const char *const H_INSRC_LCD2[][2] = {{"LINE IN IF PLUGGED", "ELSE USB AUDIO"}, {"THE LINE IN JACK", "ONLY"}, {"AUDIO OVER USB", "ONLY"},
                                              {"USB AUDIO IGNORED", "LINE STILL WINS"}};
static const char *const H_AUTOBAR_LCD2[][2] = {{"THE BAR 1 STAYS", "WHERE IT IS"}, {"1 PLACED AT A START", "NEVER WHILE PLAYING"}, {"MOVES THE 1 ALSO", "WHILE PLAYING"}};
static const char *const H_USBMIDI[][2] = {{"CLOCK ALSO OVER USB", "TO COMPUTER OR GEAR"}, {"CLOCK ON MIDI OUTS", "ONLY, NOT OVER USB"}};

#define OPTS(a) (a), (uint8_t)(sizeof(a) / sizeof((a)[0]))
/* by enum jo_field: title, choices or lo / hi / step / unit, live, repick, help */
static const struct jo_spec SPECS[JO_F_EDITS] = {
    [JO_F_NETMODE] = {"NETWORK", OPTS(NET_MODES), 0, 0, 0, "", 0, 0, H_NETMODE},
    [JO_F_INOFF] = {"AUDIO IN", NULL, 0, -100, 100, 1, " MS", 1, 0, H_INOFF},
    [JO_F_OUT0] = {"MIDI OUT 1", NULL, 0, -50, 50, 1, " MS", 1, 0, H_OUT0},
    [JO_F_OUT1] = {"MIDI OUT 2", NULL, 0, -50, 50, 1, " MS", 1, 0, H_OUT1},
    [JO_F_OUT2] = {"CLK OUT", NULL, 0, -50, 50, 1, " MS", 1, 0, H_OUT2},
    [JO_F_RATE] = {"CLOCK RATE", OPTS(RATES), 0, 0, 0, "", 1, 0, H_RATE},
    [JO_F_THRESHOLD] = {"THRESHOLD", NULL, 0, 0, 100, 5, "%", 1, 0, H_THRESHOLD},
    [JO_F_GAIN] = {"LEVEL", NULL, 0, -12, 12, 1, " dB", 1, 0, H_GAIN},
    [JO_F_LTMODE] = {"LINK TRANSPORT", OPTS(LT_MODES), 0, 0, 0, "", 1, 0, H_LTMODE},
    [JO_F_BRIGHT] = {"BRIGHTNESS", NULL, 0, 10, 100, 10, "%", 1, 0, H_BRIGHT},
    [JO_F_SLEEP] = {"SCREEN OFF", OPTS(SLEEP_NAMES), 0, 0, 0, "", 1, 0, H_SLEEP},
    [JO_F_GROUP] = {"JANK GROUP", OPTS(GROUPS), 0, 0, 0, "", 0, 0, H_GROUP},
    [JO_F_LOWCONF] = {"LOW CONFIDENCE", OPTS(LOWCONF), 0, 0, 0, "", 1, 0, H_LOWCONF},
    [JO_F_INSRC] = {"SOURCE", OPTS(INSRC), 0, 0, 0, "", 1, 0, H_INSRC},
    [JO_F_USBROLE] = {"USB", OPTS(jo_usb_names), 0, 0, 0, "", 1, 0, H_USBROLE},
    [JO_F_AUTOBAR] = {"AUTO BAR", OPTS(OFF_ON), 0, 0, 0, "", 0, 1, H_AUTOBAR},
    [JO_F_PAUSE] = {"PAUSE LEVEL", NULL, 0, -70, -20, 1, " dB", 1, 0, H_PAUSE},
};

/* the lcd2 layout's choices that differ from the OLED's, and the one it adds */
static const struct jo_spec SPECS_LCD2[JO_F_EDITS] = {
    [JO_F_INSRC] = {"SOURCE", OPTS(INSRC_LCD2), 0, 0, 0, "", 1, 0, H_INSRC_LCD2},
    [JO_F_AUTOBAR] = {"AUTO BAR", OPTS(AUTOBAR_LCD2), 0, 0, 0, "", 0, 1, H_AUTOBAR_LCD2},
    [JO_F_USBMIDI] = {"USB MIDI CLOCK OUT", OPTS(jo_off_on_names), 0, 0, 0, "", 1, 0, H_USBMIDI},
};

const struct jo_spec *jo_spec_l(enum jo_field f, int lcd2)
{
    if (f >= JO_F_EDITS)
        return NULL;
    if (lcd2 && SPECS_LCD2[f].title)
        return &SPECS_LCD2[f];
    return SPECS[f].title ? &SPECS[f] : NULL;
}

const struct jo_spec *jo_spec(enum jo_field f) { return jo_spec_l(f, 0); }

int jo_spec_get(enum jo_field f, const struct jo_ctx *c)
{
    switch (f) {
    case JO_F_NETMODE: return c->net.mode;
    case JO_F_INOFF: return c->in_off;
    case JO_F_OUT0: return c->offs[0];
    case JO_F_OUT1: return c->offs[1];
    case JO_F_OUT2: return c->offs[2];
    case JO_F_RATE: return c->rate;
    case JO_F_THRESHOLD: return c->th;
    case JO_F_PAUSE: return c->pause_db;
    case JO_F_GAIN: return c->gain;
    case JO_F_LTMODE: return c->lt_mode;
    case JO_F_BRIGHT: return c->bright;
    case JO_F_SLEEP: return c->sleep;
    case JO_F_GROUP: return c->net.group - 1;
    case JO_F_LOWCONF: return c->conf_stop ? 1 : 0;
    case JO_F_INSRC: return c->in_src;
    case JO_F_USBROLE: return c->usb_role;
    case JO_F_AUTOBAR: return c->layout_lcd2 ? c->auto_bar : c->auto_bar ? 1 : 0;
    case JO_F_USBMIDI: return c->usb_midi;
    default: return 0;
    }
}

/* editText: a choice's name, else the number (signed when it can be negative) and its unit */
void jo_edit_text(const struct jo_spec *sp, int v, char *out, size_t cap)
{
    if (sp->opts)
        snprintf(out, cap, "%s", v >= 0 && v < sp->nopts ? sp->opts[v] : "");
    else if (sp->lo < 0)
        snprintf(out, cap, "%s%d%s", v > 0 ? "+" : "", v, sp->unit);
    else
        snprintf(out, cap, "%d%s", v, sp->unit);
}

void jo_help_of(const struct jo_spec *sp, int v, const char **h1, const char **h2)
{
    int i = sp->opts ? v : 0;
    if (i < 0 || (sp->opts && i >= sp->nopts)) {
        *h1 = *h2 = "";
        return;
    }
    *h1 = sp->help[i][0];
    *h2 = sp->help[i][1];
}

const struct jo_item jo_menu[JO_MENU_ITEMS] = {
    {"AUDIO", JO_ITEM_SUB, 0}, {"LINK TRANSPORT", JO_ITEM_EDIT, JO_F_LTMODE}, {"AUTO BAR", JO_ITEM_EDIT, JO_F_AUTOBAR},
    {"NETWORK", JO_ITEM_NET, 0}, {"USB", JO_ITEM_EDIT, JO_F_USBROLE}, {"OFFSET", JO_ITEM_SUB, 1},
    {"CLK RATE", JO_ITEM_EDIT, JO_F_RATE}, {"DISPLAY", JO_ITEM_SUB, 2}, {"BATTERY", JO_ITEM_INFO, 0},
    {"ABOUT", JO_ITEM_INFO, 1},
};

const struct jo_item jo_menu_lcd2[JO_MENU_ITEMS_LCD2] = {
    {"AUDIO", JO_ITEM_SUB, 0}, {"LINK TRANSPORT", JO_ITEM_EDIT, JO_F_LTMODE}, {"AUTO BAR", JO_ITEM_EDIT, JO_F_AUTOBAR},
    {"NETWORK", JO_ITEM_NET, 0}, {"USB", JO_ITEM_EDIT, JO_F_USBROLE},
    {"USB MIDI CLOCK OUT", JO_ITEM_EDIT, JO_F_USBMIDI}, {"OFFSET", JO_ITEM_SUB, 1}, {"CLK RATE", JO_ITEM_EDIT, JO_F_RATE},
    {"DISPLAY", JO_ITEM_SUB, 2}, {"BATTERY", JO_ITEM_INFO, 0},
    {"ABOUT", JO_ITEM_INFO, 1},
};

const struct jo_sub jo_subs[3] = {
    {"AUDIO", 5, 4, {JO_F_INSRC, JO_F_GAIN, JO_F_LOWCONF, JO_F_THRESHOLD, JO_F_PAUSE}},
    {"OFFSET", 4, 4, {JO_F_INOFF, JO_F_OUT0, JO_F_OUT1, JO_F_OUT2}},
    {"DISPLAY", 2, 2, {JO_F_BRIGHT, JO_F_SLEEP}},
};

const struct jo_item *jo_menu_of(int lcd2, int *n)
{
    *n = lcd2 ? JO_MENU_ITEMS_LCD2 : JO_MENU_ITEMS;
    return lcd2 ? jo_menu_lcd2 : jo_menu;
}

static const char *pick(const char *const *names, unsigned n, unsigned v) { return v < n ? names[v] : ""; }

void jo_menu_value(const struct jo_ctx *c, int lcd2, const struct jo_item *it, char *out, size_t cap)
{
    const char *v = "";
    if (it->kind == JO_ITEM_NET)
        v = pick(jo_net_short, 4, c->net.mode);
    else if (it->kind == JO_ITEM_INFO && it->ref == 0) {          /* BATTERY */
        if (c->fitted)
            snprintf(out, cap, "%d%%", c->batt);
        else
            snprintf(out, cap, "-");
        return;
    } else if (it->kind == JO_ITEM_EDIT)
        switch (it->ref) {
        case JO_F_LTMODE: v = pick(jo_lt_short, 3, c->lt_mode); break;
        case JO_F_AUTOBAR: v = lcd2 ? pick(jo_autobar_short, 3, c->auto_bar) : c->auto_bar ? "ON" : "OFF"; break;
        case JO_F_USBROLE: v = pick(jo_usb_names, 2, c->usb_role); break;
        case JO_F_USBMIDI: v = pick(jo_off_on_names, 2, c->usb_midi); break;
        case JO_F_RATE: v = pick(jo_rates_short, 5, c->rate); break;
        default: break;
        }
    snprintf(out, cap, "%s", v);
}

int jo_sub_rows(const struct jo_sub *s, int lcd2) { return lcd2 ? s->n : s->n_oled; }

void jo_sub_value(const struct jo_ctx *c, enum jo_field f, int lcd2, char *out, size_t cap)
{
    const struct jo_spec *sp = jo_spec_l(f, lcd2);
    int g = jo_spec_get(f, c);
    if (sp->opts && f == JO_F_LOWCONF)
        snprintf(out, cap, "%s", g ? "STOP" : "RUN");
    else
        jo_edit_text(sp, g, out, cap);
}

/* Wi-Fi quality as Optimist's Wi-Fi page shows it */
static int wifi_items(const struct jo_wifiq *q, struct jo_status *it)
{
    int retry = 0, lost = 0;
    if (q->tx) {
        retry = jo_iround(100.0 * q->retried / q->tx);
        lost = jo_iround(100.0 * q->lost / q->tx);
    }
    if (q->rssi)
        snprintf(it[0].t, sizeof it[0].t, "%d dBm  %dM", q->rssi, jo_iround(q->kbps / 1000.0));
    else
        snprintf(it[0].t, sizeof it[0].t, "NO SIGNAL");
    it[0].bad = !q->rssi || q->rssi < -80;
    if (q->tx)
        snprintf(it[1].t, sizeof it[1].t, "RETRY %d%%", retry);
    else
        snprintf(it[1].t, sizeof it[1].t, "RETRY --");
    it[1].bad = q->tx && retry >= 30;
    if (q->tx)
        snprintf(it[2].t, sizeof it[2].t, "LOST %d%%", lost);
    else
        snprintf(it[2].t, sizeof it[2].t, "LOST --");
    it[2].bad = q->tx && lost >= 10;
    return 3;
}

int jo_status_items(const struct jo_ctx *c, struct jo_status it[JO_STATUS_MAX])
{
    int n = 0;
    memset(it, 0, JO_STATUS_MAX * sizeof it[0]);
    if (c->peers > 0) {
        if (c->peers == 1)
            snprintf(it[n].t, sizeof it[n].t, "1 LINK PEER");
        else
            snprintf(it[n].t, sizeof it[n].t, "%d LINK PEERS", c->peers);
        n++;
    }
    if (c->has_wifiq)
        n += wifi_items(&c->wifiq, &it[n]);
    return n;
}

static void stars(char *out, size_t len)
{
    size_t n = len < 6 ? len : 6;
    memset(out, '*', n);
    out[n] = 0;
}

int jo_net_rows(const struct jo_ctx *c, struct jo_net_row rows[4])
{
    const struct jo_net *n = &c->net;
    memset(rows, 0, 4 * sizeof rows[0]);
    rows[0] = (struct jo_net_row){"TYPE", "", JO_ACT_MODE, {"HOW THE DEVICE JOINS", "A NETWORK"}};
    snprintf(rows[0].val, sizeof rows[0].val, "%s", n->mode < 4 ? jo_net_short[n->mode] : "");
    if (n->mode == JO_NET_JANK) {
        rows[1] = (struct jo_net_row){"GROUP", "", JO_ACT_GROUP, {"UNITS ON THE SAME", "GROUP PLAY TOGETHER"}};
        snprintf(rows[1].val, sizeof rows[1].val, "%u", n->group);
        rows[2] = (struct jo_net_row){"PEERS", "", JO_ACT_NONE, {"JANK DEVICES IN", "THE GROUP"}};
        snprintf(rows[2].val, sizeof rows[2].val, "%d", c->group_peers);
        return 3;
    }
    if (n->mode == JO_NET_CLIENT) {
        rows[1] = (struct jo_net_row){"NETWORK", "", JO_ACT_SSID, {"THE ROUTER TO JOIN", ""}};
        snprintf(rows[1].val, sizeof rows[1].val, "%s", n->ssid[0] ? n->ssid : "-");
        rows[2] = (struct jo_net_row){"PASSKEY", "", JO_ACT_PASS, {"ITS WIFI PASSKEY", ""}};
        stars(rows[2].val, strlen(n->pass));
        rows[3] = (struct jo_net_row){"STATUS", "", JO_ACT_STATUS, {"CONNECTION STATE", "AND IP ADDRESS"}};
        snprintf(rows[3].val, sizeof rows[3].val, "%s", n->status);
        return 4;
    }
    rows[1] = (struct jo_net_row){"NAME", "", JO_ACT_APNAME, {"NAME OF THE", "DEVICE NETWORK"}};
    snprintf(rows[1].val, sizeof rows[1].val, "%s", n->ap_name);
    rows[2] = (struct jo_net_row){"PASSKEY", "", JO_ACT_APPASS, {"PASSKEY OF THE", "DEVICE NETWORK"}};
    stars(rows[2].val, strlen(n->appass));
    rows[3] = (struct jo_net_row){"STATUS", "", JO_ACT_STATUS, {"ADDRESS AND", "CONNECTED CLIENTS"}};
    snprintf(rows[3].val, sizeof rows[3].val, "%d CLIENTS", n->ap_clients);
    return 4;
}

/* the ABOUT rows, a label (left) and its value (right aligned by the screens), "-" for what is not known: VERSION, REPO, UPTIME, RESET,
 * TEMP, CPU, MEMORY, MIN HEAP, FLASH, CLOCK LATE, UI LOOP, BEAT LOOP, PANIC. The repo is shortened on the OLED (21 characters across); the lcd2 layout
 * adds the PSRAM to MEMORY */
static void about_num(struct jo_about_row *r, const char *label, int v, const char *unit)
{
    snprintf(r->label, sizeof r->label, "%s", label);
    if (v >= 0)
        snprintf(r->val, sizeof r->val, "%d%s", v, unit);
    else
        snprintf(r->val, sizeof r->val, "-");
}

/* CPU: "core 0 / core 1 %", "-" for a core not known (57/3%, 57/-%, -/-%) */
static void about_cpu(struct jo_about_row *r, int c0, int c1)
{
    char a[12] = "-", b[12] = "-";
    if (c0 >= 0)
        snprintf(a, sizeof a, "%d", c0);
    if (c1 >= 0)
        snprintf(b, sizeof b, "%d", c1);
    snprintf(r->label, sizeof r->label, "CPU");
    snprintf(r->val, sizeof r->val, "%s/%s%%", a, b);
}

static void about_txt(struct jo_about_row *r, const char *label, const char *val)
{
    snprintf(r->label, sizeof r->label, "%s", label);
    snprintf(r->val, sizeof r->val, "%s", val);
}

/* PANIC: "-" when none, else "<task> 0x4200abcd" from the "<task>@0x4200abcd" the firmware keeps. The task is cut to what fits beside the label
 * (3 characters on the OLED's 20, 7 on the colour layout); the address is always whole */
static void about_panic(struct jo_about_row *r, const char *p, int lcd2)
{
    const char *at = p ? strrchr(p, '@') : NULL;
    int task = at ? (int)(at - p) : 0, max = lcd2 ? 7 : 3;
    snprintf(r->label, sizeof r->label, "PANIC");
    if (!p || !p[0])
        snprintf(r->val, sizeof r->val, "-");
    else if (!at)
        snprintf(r->val, sizeof r->val, "%s", p);
    else
        snprintf(r->val, sizeof r->val, "%.*s%s%s", task < max ? task : max, p, task ? " " : "", at + 1);
}

void jo_about_rows(const struct jo_ctx *c, struct jo_about_row rows[JO_ABOUT_ROWS])
{
    char a[16], b[16], up[24] = "-", t[24] = "-", mem[40] = "-", fl[64] = "-";
    if (c->uptime_s >= 86400)
        snprintf(up, sizeof up, "%dd %dh", c->uptime_s / 86400, c->uptime_s % 86400 / 3600);
    else if (c->uptime_s >= 0)
        snprintf(up, sizeof up, "%d:%02d:%02d", c->uptime_s / 3600, c->uptime_s % 3600 / 60, c->uptime_s % 60);
    if (c->temp_c != JO_TEMP_UNKNOWN)
        snprintf(t, sizeof t, "%d C", c->temp_c);
    if (c->heap_kb >= 0) {
        snprintf(mem, sizeof mem, "%dK", c->heap_kb);
        if (c->layout_lcd2 && c->psram_kb >= 0)
            snprintf(mem, sizeof mem, "%dK PSRAM %dK", c->heap_kb, c->psram_kb);
    }
    if (c->flash_kb >= 0) {
        snprintf(fl, sizeof fl, "%dM -", c->flash_kb / 1024);
        if (c->app_used_kb >= 0 && c->app_kb >= 0) {
            jo_fixed(c->app_used_kb / 1024.0, 1, a, sizeof a);
            jo_fixed(c->app_kb / 1024.0, 1, b, sizeof b);
            snprintf(fl, sizeof fl, "%dM %s/%sM", c->flash_kb / 1024, a, b);
        }
    }
    about_txt(&rows[0], "VERSION", c->version ? c->version : "-");
    about_txt(&rows[1], "REPO", c->layout_lcd2 ? "github.com/w0ts/jank" : "w0ts/jank");
    about_txt(&rows[2], "UPTIME", up);
    about_txt(&rows[3], "RESET", c->reset ? c->reset : "-");
    about_txt(&rows[4], "TEMP", t);
    about_cpu(&rows[5], c->cpu_pct, c->cpu1_pct);
    about_txt(&rows[6], "MEMORY", mem);
    about_num(&rows[7], "MIN HEAP", c->heap_min_kb, "K");
    about_txt(&rows[8], "FLASH", fl);
    about_num(&rows[9], "CLOCK LATE", c->clk_late_us, " US");
    about_num(&rows[10], "UI LOOP", c->ui_loop_us, " US");
    about_num(&rows[11], "BEAT LOOP", c->beat_loop_us, " US");
    about_panic(&rows[12], c->panic, c->layout_lcd2);
    if (c->layout_lcd2 && c->panic && c->panic[0]) {   /* colour layout: a panic on record puts its row first, the others follow */
        struct jo_about_row p = rows[12];
        memmove(&rows[1], &rows[0], 12 * sizeof rows[0]);
        rows[0] = p;
    }
}

/* the lcd2 layout's 9 rows do not fit the text page: while it shows the state, a press moves 6 (the colour screen then shows 3 rows twice) */
int jo_about_vis(const struct jo_ctx *c)
{
    int vis = c->layout_lcd2 ? JO_ABOUT_VIS_LCD2 : JO_ABOUT_VIS_OLED;
    return c->text_view && vis > JO_ABOUT_VIS_TEXT ? JO_ABOUT_VIS_TEXT : vis;
}
