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
const char *const jo_lt_short[3] = {"IGNORE", "FOLLOW", "LEAD"};
static const char *const GROUPS[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
static const char *const LOWCONF[] = {"KEEP RUNNING", "STOP / START"};
static const char *const INSRC[] = {"AUTO", "LINE IN", "USB AUDIO"};
const char *const jo_usb_names[2] = {"DEVICE", "HOST"};
static const char *const OFF_ON[] = {"OFF", "ON"};
/* the lcd2 layout's own lists (ui/oled.js EDITS: AUTO BAR and SOURCE have more choices there, and USB MIDI CLOCK, BEAT FLASH) */
const char *const jo_off_on_names[2] = {"ON", "OFF"};
const char *const jo_autobar_short[3] = {"OFF", "START", "ALWAYS"};
const char *const jo_beatflash_names[2] = {"BOXES", "SCREEN"};
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
static const char *const H_BEATFLASH[][2] = {{"ONLY THE BOXES", "SHOW THE BEAT"}, {"WHOLE SCREEN FLASHES", "FIRST BEAT: AMBER"}};

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
};

/* the lcd2 layout's choices that differ from the OLED's, and the two it adds */
static const struct jo_spec SPECS_LCD2[JO_F_EDITS] = {
    [JO_F_INSRC] = {"SOURCE", OPTS(INSRC_LCD2), 0, 0, 0, "", 1, 0, H_INSRC_LCD2},
    [JO_F_AUTOBAR] = {"AUTO BAR", OPTS(AUTOBAR_LCD2), 0, 0, 0, "", 0, 1, H_AUTOBAR_LCD2},
    [JO_F_USBMIDI] = {"USB MIDI CLOCK", OPTS(jo_off_on_names), 0, 0, 0, "", 1, 0, H_USBMIDI},
    [JO_F_BEATFLASH] = {"BEAT FLASH", OPTS(jo_beatflash_names), 0, 0, 0, "", 1, 0, H_BEATFLASH},
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
    case JO_F_BEATFLASH: return c->beat_flash ? 1 : 0;
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
};

const struct jo_item jo_menu_lcd2[JO_MENU_ITEMS_LCD2] = {
    {"AUDIO", JO_ITEM_SUB, 0}, {"LINK TRANSPORT", JO_ITEM_EDIT, JO_F_LTMODE}, {"AUTO BAR", JO_ITEM_EDIT, JO_F_AUTOBAR},
    {"NETWORK", JO_ITEM_NET, 0}, {"USB", JO_ITEM_EDIT, JO_F_USBROLE},
    {"USB MIDI CLOCK", JO_ITEM_EDIT, JO_F_USBMIDI}, {"OFFSET", JO_ITEM_SUB, 1}, {"CLK RATE", JO_ITEM_EDIT, JO_F_RATE},
    {"DISPLAY", JO_ITEM_SUB, 2}, {"BEAT FLASH", JO_ITEM_EDIT, JO_F_BEATFLASH}, {"BATTERY", JO_ITEM_INFO, 0},
};

const struct jo_sub jo_subs[3] = {
    {"AUDIO", 4, {JO_F_INSRC, JO_F_GAIN, JO_F_LOWCONF, JO_F_THRESHOLD}},
    {"OFFSET", 4, {JO_F_INOFF, JO_F_OUT0, JO_F_OUT1, JO_F_OUT2}},
    {"DISPLAY", 2, {JO_F_BRIGHT, JO_F_SLEEP}},
};

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
