/* SPDX-License-Identifier: GPL-3.0-only */
/* The OLED UI's C port against the browser emulator's own pixels: tests/oled_ref/cases.txt (gen_cases.js runs
 * player.html's OLED module, QR encoder and button host in node) holds states, button steps, the frames drawn and the
 * setter calls made. This replays each case through jo_press / jo_input_* and jo_draw and wants 0 differing pixels and
 * the same calls.
 *
 * The lcd2 layout: tests/oled_ref/cases_ink.txt (gen_ink.js runs ui/oled.js with ctx.layout 'lcd2', its own 320 x 240 grid) holds
 * the 1-bit frame and the ink plane of each case, and the setter calls; jo_press and jo_draw_lcd2 must draw both exactly and make the same
 * calls. Usage: test_oled CASES_TXT [CASES_INK_TXT] */
#include "jank_oled.h"
#include "jank_oled_int.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_fail, s_checks;
struct stats {
    int cases, frames, bad_frames;
    int ink_frames, ink_bad, ink_lit;     /* ink planes compared pixel by pixel; those with a differing pixel; lit ink pixels seen */
};
#define CHECK(c, ...)                                                    \
    do {                                                                 \
        s_checks++;                                                      \
        if (!(c)) {                                                      \
            s_fail++;                                                    \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);         \
            fprintf(stderr, __VA_ARGS__);                                \
            fputc('\n', stderr);                                         \
        }                                                                \
    } while (0)

static const char *const FIELD_NAMES[JO_F_COUNT] = {"netmode", "inoff", "out0", "out1", "out2", "rate", "threshold", "gain", "ltmode", "bright",
                                                    "sleep", "group", "lowconf", "insrc", "usbrole", "autobar", "usbmidi", "ssid", "pass", "appass", "apname"};

/* one case's world, as gen_cases.js's world(): the state the setters change, and the log of calls */
struct world {
    struct jo_ctx c;
    struct jo_qr qr;
    char log[64][96];
    int nlog;
};

static void logf_(struct world *w, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void logf_(struct world *w, const char *fmt, ...)
{
    va_list ap;
    if (w->nlog >= 64)
        return;
    va_start(ap, fmt);
    vsnprintf(w->log[w->nlog++], sizeof w->log[0], fmt, ap);
    va_end(ap);
}

static void set_field(void *user, enum jo_field f, int v)
{
    struct world *w = user;
    struct jo_ctx *c = &w->c;
    logf_(w, "set %s %d", FIELD_NAMES[f], v);
    switch (f) {
    case JO_F_THRESHOLD: c->th = v; break;
    case JO_F_LOWCONF: c->conf_stop = v == 1; break;
    case JO_F_LTMODE: c->lt_mode = (uint8_t)v; break;
    case JO_F_GAIN: c->gain = v; break;
    case JO_F_BRIGHT: c->bright = (uint8_t)v; break;
    case JO_F_SLEEP: c->sleep = (uint8_t)v; break;
    case JO_F_INOFF: c->in_off = v; break;
    case JO_F_OUT0: c->offs[0] = v; break;
    case JO_F_OUT1: c->offs[1] = v; break;
    case JO_F_OUT2: c->offs[2] = v; break;
    case JO_F_RATE: c->rate = (uint8_t)v; break;
    case JO_F_NETMODE:
        if (v == 3) {
            if (c->net.mode != 3)
                c->net.prev = c->net.mode;
            c->net.mode = 3;
        } else
            c->net.mode = (uint8_t)v;
        break;
    case JO_F_GROUP: c->net.group = (uint8_t)(v + 1); break;
    case JO_F_INSRC: c->in_src = (uint8_t)v; break;
    case JO_F_USBROLE: c->usb_role = (uint8_t)v; break;
    case JO_F_AUTOBAR: c->auto_bar = (uint8_t)v; break;
    case JO_F_USBMIDI: c->usb_midi = (uint8_t)v; break;
    default: break;
    }
    c->setup = c->net.mode == 3;
}

static void set_text(void *user, enum jo_field f, const char *t)
{
    struct world *w = user;
    struct jo_net *n = &w->c.net;
    logf_(w, "set %s %s", FIELD_NAMES[f], t);
    if (f == JO_F_SSID)
        snprintf(n->ssid, sizeof n->ssid, "%s", t);
    else if (f == JO_F_PASS)
        snprintf(n->pass, sizeof n->pass, "%s", t);
    else if (f == JO_F_APPASS)
        snprintf(n->appass, sizeof n->appass, "%s", t);
    else if (f == JO_F_APNAME)
        snprintf(n->ap_name, sizeof n->ap_name, "%s", t);
}

static int tempo_step(void *user, int d, double *bpm)
{
    struct world *w = user;
    struct jo_ctx *c = &w->c;
    logf_(w, "tempo %d", d);
    if (c->link) {
        if (!c->has_link_bpm)
            return -1;
        c->link_bpm = jo_clampi(jo_iround(c->link_bpm) + d, 90, 180);
        *bpm = c->link_bpm;
        return 0;
    }
    c->bpm = c->has_bpm && c->bpm != 0.0 ? (d > 0 ? c->bpm * 2 : c->bpm / 2) : 0.0;
    c->has_bpm = 1;
    *bpm = c->bpm;
    return 0;
}

static void set_hold(void *user, int on)
{
    struct world *w = user;
    logf_(w, "hold %d", on ? 1 : 0);
    w->c.running = !on;
    w->c.held = (uint8_t)(on != 0);
}

static void link_start_stop(void *user, int on)
{
    struct world *w = user;
    logf_(w, "lss %d", on ? 1 : 0);
    w->c.running = (uint8_t)(on != 0);
    w->c.held = (uint8_t)(!on);
}

static void shift_beats(void *user, int halves)
{
    struct world *w = user;
    logf_(w, "shift %d", halves);
}

static const struct jo_ctx *host_ctx(void *user, uint32_t now)
{
    struct world *w = user;
    (void)now;
    return &w->c;
}

static void parse_num(const char *v, uint8_t *has, double *out)
{
    *has = strcmp(v, "null") != 0;
    *out = *has ? strtod(v, NULL) : 0.0;
}

/* "c about ...": the ABOUT screen's 15 numbers and the last panic (state.js ABOUT_KEYS order), "null" = not known */
static void parse_about(struct jo_ctx *c, const char *v)
{
    static char version[24], reset[24], panic[32];
    char t[16][32];
    int *f[] = {&c->temp_c, &c->cpu_pct, &c->cpu1_pct, &c->heap_kb, &c->psram_kb, &c->flash_kb, &c->app_used_kb, &c->app_kb, &c->uptime_s, &c->heap_min_kb,
                NULL, &c->clk_late_us, &c->ui_loop_us, &c->beat_loop_us};
    int n = sscanf(v, "%31s %31s %31s %31s %31s %31s %31s %31s %31s %31s %31s %31s %31s %31s %31s %31s", t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], t[10],
                   t[11], t[12], t[13], t[14], t[15]);
    CHECK(n == 16, "about: %d of 16 fields", n);
    snprintf(version, sizeof version, "%s", t[0]);
    c->version = strcmp(t[0], "null") ? version : NULL;
    snprintf(reset, sizeof reset, "%s", t[11]);
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++) {
        const char *s = t[i + 1];
        if (!f[i])
            continue;
        *f[i] = strcmp(s, "null") ? atoi(s) : (f[i] == &c->temp_c ? JO_TEMP_UNKNOWN : JO_UNKNOWN);
    }
    c->reset = strcmp(t[11], "null") ? reset : NULL;
    snprintf(panic, sizeof panic, "%s", t[15]);
    c->panic = strcmp(t[15], "null") ? panic : NULL;
}

/* "c key value": the state gen_cases.js dumped */
static void parse_state(struct jo_ctx *c, const char *key, const char *v)
{
    uint8_t has;
    double d;
    struct jo_net *n = &c->net;
    if (!strcmp(key, "link")) c->link = (uint8_t)atoi(v);
    else if (!strcmp(key, "lineIn")) c->line_in = (uint8_t)atoi(v);
    else if (!strcmp(key, "confStop")) c->conf_stop = (uint8_t)atoi(v);
    else if (!strcmp(key, "running")) c->running = (uint8_t)atoi(v);
    else if (!strcmp(key, "armed")) c->armed = (uint8_t)atoi(v);
    else if (!strcmp(key, "held")) c->held = (uint8_t)atoi(v);
    else if (!strcmp(key, "trackerLeads")) c->tracker_leads = (uint8_t)atoi(v);
    else if (!strcmp(key, "xiao")) c->no_beat_nudge = c->no_ms_nudge = (uint8_t)atoi(v);
    else if (!strcmp(key, "usb")) c->usb = (uint8_t)atoi(v);
    else if (!strcmp(key, "fitted")) c->fitted = (uint8_t)atoi(v);
    else if (!strcmp(key, "autoBar")) c->auto_bar = (uint8_t)atoi(v);
    else if (!strcmp(key, "boot")) c->boot = (uint8_t)atoi(v);
    else if (!strcmp(key, "splash")) c->splash = (uint8_t)atoi(v);
    else if (!strcmp(key, "saved")) c->saved = (uint8_t)atoi(v);
    else if (!strcmp(key, "bpm")) parse_num(v, &c->has_bpm, &c->bpm);
    else if (!strcmp(key, "linkBpm")) parse_num(v, &c->has_link_bpm, &c->link_bpm);
    else if (!strcmp(key, "conf")) c->conf = strtod(v, NULL);
    else if (!strcmp(key, "th")) c->th = atoi(v);
    else if (!strcmp(key, "gain")) c->gain = atoi(v);
    else if (!strcmp(key, "levelDb")) c->level_db = strtod(v, NULL);
    else if (!strcmp(key, "levelMin")) c->level_min = strtod(v, NULL);
    else if (!strcmp(key, "levelMax")) c->level_max = strtod(v, NULL);
    else if (!strcmp(key, "beatN")) {
        parse_num(v, &has, &d);
        c->has_beat = has;
        c->beat_n = (int)d;
    } else if (!strcmp(key, "peers")) c->peers = atoi(v);
    else if (!strcmp(key, "batt")) c->batt = atoi(v);
    else if (!strcmp(key, "ltMode")) c->lt_mode = (uint8_t)atoi(v);
    else if (!strcmp(key, "inSrc")) c->in_src = (uint8_t)atoi(v);
    else if (!strcmp(key, "usbRole")) c->usb_role = (uint8_t)atoi(v);
    else if (!strcmp(key, "inOff")) c->in_off = atoi(v);
    else if (!strcmp(key, "rate")) c->rate = (uint8_t)atoi(v);
    else if (!strcmp(key, "bright")) c->bright = (uint8_t)atoi(v);
    else if (!strcmp(key, "sleep")) c->sleep = (uint8_t)atoi(v);
    else if (!strcmp(key, "groupPeers")) c->group_peers = atoi(v);
    else if (!strcmp(key, "wifi")) c->wifi = !strcmp(v, "good") ? JO_WIFI_GOOD : !strcmp(v, "weak") ? JO_WIFI_WEAK : JO_WIFI_NONE;
    else if (!strcmp(key, "offs")) sscanf(v, "%d %d %d", &c->offs[0], &c->offs[1], &c->offs[2]);
    else if (!strcmp(key, "wifiQ")) {
        c->has_wifiq = strcmp(v, "null") != 0;
        if (c->has_wifiq)
            sscanf(v, "%d %lf %d %d %d", &c->wifiq.rssi, &c->wifiq.kbps, &c->wifiq.tx, &c->wifiq.retried, &c->wifiq.lost);
    } else if (!strcmp(key, "bar")) {
        char st[16];
        c->has_bar = strcmp(v, "null") != 0;
        if (c->has_bar && sscanf(v, "%15s %lf", st, &c->bar_conf) == 2)
            c->bar_state = !strcmp(st, "off") ? JO_BAR_OFF : !strcmp(st, "listening") ? JO_BAR_LISTENING : !strcmp(st, "locked") ? JO_BAR_LOCKED : JO_BAR_MANUAL;
    } else if (!strcmp(key, "layout")) c->layout_lcd2 = (uint8_t)atoi(v);
    else if (!strcmp(key, "about")) parse_about(c, v);
    else if (!strcmp(key, "rssi")) {
        parse_num(v, &c->has_rssi, &d);
        c->rssi = (int)d;
    } else if (!strcmp(key, "airLost")) {
        parse_num(v, &c->has_air_lost, &d);
        c->air_lost = (int)d;
    } else if (!strcmp(key, "airRetry")) {
        parse_num(v, &c->has_air_retry, &d);
        c->air_retry = (int)d;
    } else if (!strcmp(key, "rssiMin")) c->rssi_min = atoi(v);
    else if (!strcmp(key, "rssiMax")) c->rssi_max = atoi(v);
    else if (!strcmp(key, "peakDb")) parse_num(v, &c->has_peak, &c->peak_db);
    else if (!strcmp(key, "confKnown")) c->conf_unknown = !atoi(v);
    else if (!strcmp(key, "silent")) c->silent = (uint8_t)atoi(v);
    else if (!strcmp(key, "gated")) c->gated = (uint8_t)atoi(v);
    else if (!strcmp(key, "confHold")) c->conf_hold = (uint8_t)atoi(v);
    else if (!strcmp(key, "confGraded")) c->conf_graded = (uint8_t)atoi(v);
    else if (!strcmp(key, "confGate")) c->conf_gate = strtod(v, NULL);
    else if (!strcmp(key, "fold")) {
        double f = strtod(v, NULL);
        c->fold_exp = f == 2.0 ? 1 : f == 4.0 ? 2 : f == 0.5 ? -1 : f == 0.25 ? -2 : 0;
    } else if (!strcmp(key, "usbMidi")) c->usb_midi = (uint8_t)atoi(v);
    else if (!strcmp(key, "net.mode")) n->mode = (uint8_t)atoi(v);
    else if (!strcmp(key, "net.prev")) n->prev = (uint8_t)atoi(v);
    else if (!strcmp(key, "net.group")) n->group = (uint8_t)atoi(v);
    else if (!strcmp(key, "net.apClients")) n->ap_clients = atoi(v);
    else if (!strcmp(key, "net.ssid")) snprintf(n->ssid, sizeof n->ssid, "%s", v);
    else if (!strcmp(key, "net.pass")) snprintf(n->pass, sizeof n->pass, "%s", v);
    else if (!strcmp(key, "net.appass")) snprintf(n->appass, sizeof n->appass, "%s", v);
    else if (!strcmp(key, "net.apName")) snprintf(n->ap_name, sizeof n->ap_name, "%s", v);
    else if (!strcmp(key, "net.status")) snprintf(n->status, sizeof n->status, "%s", v);
    else if (!strcmp(key, "net.ip")) snprintf(n->ip, sizeof n->ip, "%s", v);
    else if (!strcmp(key, "net.setupName")) snprintf(n->setup_name, sizeof n->setup_name, "%s", v);
    else if (!strcmp(key, "scan")) {
        if (n->nscan < JO_SCAN_MAX) {
            struct jo_scan_row *r = &n->scan[n->nscan++];
            r->bars = (uint8_t)atoi(v);
            snprintf(r->name, sizeof r->name, "%s", strchr(v, ' ') ? strchr(v, ' ') + 1 : "");
        }
    } else
        CHECK(0, "unknown state key %s", key);
}

static enum jo_btn btn_of(char ch) { return ch == 'U' ? JO_UP : ch == 'D' ? JO_DOWN : ch == 'N' ? JO_NO : JO_YES; }

static int hexval(char h) { return h <= '9' ? h - '0' : h - 'a' + 10; }

/* a frame as the reference file has it: hex, or runs "hh:length,..." of one byte (gen_ink.js): the page bytes, -1 when it is not a whole frame */
static int decode_pages(const char *s, uint8_t *pages, int nbytes)
{
    int n = 0;
    if (!strchr(s, ':')) {
        if ((int)strlen(s) < 2 * nbytes)
            return -1;
        for (; n < nbytes; n++)
            pages[n] = (uint8_t)(hexval(s[2 * n]) << 4 | hexval(s[2 * n + 1]));
        return 0;
    }
    while (*s) {
        char *end;
        long v = strtol(s, &end, 16), len;
        if (*end != ':' || v < 0 || v > 255)
            return -1;
        len = strtol(end + 1, &end, 10);
        if (len <= 0 || n + len > nbytes)
            return -1;
        memset(pages + n, (int)v, (size_t)len);
        n += (int)len;
        s = *end == ',' ? end + 1 : end;
    }
    return n == nbytes ? 0 : -1;
}

/* the frame against the emulator's: the differing pixels, and the first one printed */
static int compare(const uint8_t *fb, const char *ref, int width, int nbytes, const char *name, unsigned long t)
{
    uint8_t want[JO_LCD2_FB_BYTES];
    int diff = 0, fx = -1, fy = -1;
    if (decode_pages(ref, want, nbytes) != 0) {
        CHECK(0, "%s: short frame", name);
        return width * nbytes / width * 8;
    }
    for (int i = 0; i < nbytes; i++) {
        uint8_t x = want[i] ^ fb[i];
        for (int b = 0; b < 8; b++)
            if (x >> b & 1) {
                if (!diff) {
                    fx = i % width;
                    fy = (i / width) * 8 + b;
                }
                diff++;
            }
    }
    CHECK(diff == 0, "%s at t=%lu: %d pixels differ (first at x %d, y %d)", name, t, diff, fx, fy);
    return diff;
}

/* the ink plane as gen_ink.js wrote it ("id:length,..."): -1 when it is not a whole plane */
static int decode_ink(const char *runs, uint8_t *plane)
{
    int n = 0;
    while (*runs) {
        char *end;
        long id = strtol(runs, &end, 10), len;
        if (*end != ':' || id < 0 || id >= JO_INK_IDS)
            return -1;
        len = strtol(end + 1, &end, 10);
        if (len <= 0 || n + len > JO_LCD2_INK_BYTES)
            return -1;
        memset(plane + n, (int)id, (size_t)len);
        n += (int)len;
        runs = *end == ',' ? end + 1 : end;
    }
    return n == JO_LCD2_INK_BYTES ? 0 : -1;
}

static int compare_ink(const uint8_t *ink, const uint8_t *want, const char *name, unsigned long t)
{
    int diff = 0, first = 0;
    for (int i = 0; i < JO_LCD2_INK_BYTES; i++)
        if (ink[i] != want[i]) {
            if (!diff)
                first = i;
            diff++;
        }
    CHECK(diff == 0, "%s at t=%lu: %d ink pixels differ (first at x %d, y %d: C %d, ui/oled.js %d)", name, t, diff, first % JO_LCD2_W, first / JO_LCD2_W,
          ink[first], want[first]);
    return diff;
}

static int count_lit(const uint8_t *ink)
{
    int n = 0;
    for (int i = 0; i < JO_LCD2_INK_BYTES; i++)
        n += ink[i] != 0;
    return n;
}

static void dump(const uint8_t *fb, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            fputc((fb[(y >> 3) * width + x] >> (y & 7)) & 1 ? '#' : '.', stderr);
        fputc('\n', stderr);
    }
}

static void run_cases(const char *path, struct stats *st)
{
    FILE *f = fopen(path, "r");
    static char line[32768];
    char name[128] = "";
    struct world w;
    struct jo_ui ui;
    struct jo_input in;
    struct jo_ops ops = {&w, tempo_step, set_field, set_text, set_hold, link_start_stop, shift_beats};
    struct jo_host host = {&ops, host_ctx};
    int want_log = 0, dumped = 0;
    static uint8_t fb[JO_LCD2_FB_BYTES], ink[JO_LCD2_INK_BYTES], want_ink[JO_LCD2_INK_BYTES];
    if (!f) {
        CHECK(0, "cannot open %s", path);
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n');
        unsigned long t = 0;
        char b = 0;
        if (nl)
            *nl = 0;
        if (line[0] == '#' || !line[0])
            continue;
        if (!strncmp(line, "case ", 5)) {
            snprintf(name, sizeof name, "%s", line + 5);
            memset(&w, 0, sizeof w);
            jo_ui_init(&ui);
            jo_input_init(&in, 0);
            want_log = 0;
        } else if (!strncmp(line, "c ", 2)) {
            char *sp = strchr(line + 2, ' ');
            if (!sp)
                continue;
            *sp = 0;
            parse_state(&w.c, line + 2, sp + 1);
            w.c.setup = w.c.net.mode == 3;
        } else if (!strncmp(line, "draw ", 5)) {
            char *hex = strchr(line + 5, ' '), *runs;
            t = strtoul(line + 5, NULL, 10);
            if (!hex)
                continue;
            runs = strchr(hex + 1, ' ');
            if (runs)
                *runs++ = 0;
            {
                char txt[64];
                snprintf(txt, sizeof txt, "WIFI:S:%s;T:nopass;;", w.c.net.setup_name);
                jo_qr_encode(txt, &w.qr);
                w.c.qr = &w.qr;
            }
            jo_input_tick(&in, &ui, (uint32_t)t, &host);
            st->frames++;
            if (runs) {                       /* an lcd2 case: the frame and the plane */
                CHECK(decode_ink(runs, want_ink) == 0, "%s: bad ink runs", name);
                memset(ink, 9, sizeof ink);
                jo_draw_lcd2(fb, ink, &ui, &w.c, (uint32_t)t);
                if (compare(fb, hex + 1, JO_LCD2_W, JO_LCD2_FB_BYTES, name, t)) {
                    st->bad_frames++;
                    if (!dumped++) {
                        fprintf(stderr, "C frame of %s at %lu:\n", name, t);
                        dump(fb, JO_LCD2_W, JO_LCD2_H);
                    }
                }
                st->ink_frames++;
                st->ink_lit += count_lit(want_ink);
                st->ink_bad += compare_ink(ink, want_ink, name, t) != 0;
                continue;
            }
            jo_draw(fb, &ui, &w.c, (uint32_t)t);
            if (compare(fb, hex + 1, JO_W, JO_FB_BYTES, name, t)) {
                st->bad_frames++;
                if (!dumped++) {
                    fprintf(stderr, "C frame of %s at %lu:\n", name, t);
                    dump(fb, JO_W, JO_H);
                }
            }
        } else if (sscanf(line, "press %c %lu", &b, &t) == 2) {
            jo_input_tick(&in, &ui, (uint32_t)t, &host);
            jo_press(&ui, btn_of(b), (uint32_t)t, &w.c, &ops);
        } else if (sscanf(line, "down %c %lu", &b, &t) == 2) {
            jo_input_tick(&in, &ui, (uint32_t)t, &host);
            jo_input_down(&in, &ui, btn_of(b), (uint32_t)t, &host);
        } else if (sscanf(line, "up %c %lu", &b, &t) == 2) {
            jo_input_tick(&in, &ui, (uint32_t)t, &host);
            jo_input_up(&in, &ui, btn_of(b), (uint32_t)t, &host);
        } else if (sscanf(line, "tick %lu", &t) == 1) {
            jo_input_tick(&in, &ui, (uint32_t)t, &host);
        } else if (!strncmp(line, "log ", 4)) {
            CHECK(want_log < w.nlog && !strcmp(w.log[want_log], line + 4), "%s: call %d is \"%s\", the emulator's \"%s\"", name, want_log,
                  want_log < w.nlog ? w.log[want_log] : "(none)", line + 4);
            want_log++;
        } else if (!strcmp(line, "end")) {
            CHECK(want_log == w.nlog, "%s: %d calls, the emulator made %d", name, w.nlog, want_log);
            st->cases++;
        } else
            CHECK(0, "bad line: %.40s", line);
    }
    fclose(f);
}

/* JavaScript's number rules the screens depend on */
static void test_js_numbers(void)
{
    static const struct {
        double x;
        int d;
        const char *want;
    } F[] = {{124.25, 1, "124.3"}, {0.15, 1, "0.1"}, {0.25, 1, "0.3"}, {1.005, 2, "1.00"}, {120.0, 1, "120.0"}, {174.96, 1, "175.0"},
             {3.3 + 0.9 * 37 / 100, 2, "3.63"}, {0.05, 1, "0.1"}, {2.5, 0, "3"}, {0.0, 1, "0.0"}, {-1.25, 1, "-1.3"}, {99.95, 1, "100.0"}};
    char s[32];
    for (size_t i = 0; i < sizeof F / sizeof F[0]; i++) {
        jo_fixed(F[i].x, F[i].d, s, sizeof s);
        CHECK(!strcmp(s, F[i].want), "toFixed(%.17g, %d) = %s, want %s", F[i].x, F[i].d, s, F[i].want);
    }
    CHECK(jo_round(2.5) == 3.0 && jo_round(-2.5) == -2.0 && jo_round(-0.5) == 0.0 && jo_round(0.49999999999999994) == 0.0, "Math.round");
}

int main(int argc, char **argv)
{
    struct stats oled = {0}, colour = {0};
    test_js_numbers();
    if (argc < 2) {
        fprintf(stderr, "usage: test_oled CASES_TXT\n");
        return 2;
    }
    run_cases(argv[1], &oled);
    CHECK(oled.cases >= 100 && oled.frames >= 380, "only %d cases, %d frames", oled.cases, oled.frames);
    if (argc > 2) {
        run_cases(argv[2], &colour);
        CHECK(colour.cases >= 180 && colour.ink_frames >= 380 && colour.ink_lit > 100000, "only %d ink cases, %d planes, %d lit ink pixels", colour.cases,
              colour.ink_frames, colour.ink_lit);
    }
    if (s_fail) {
        fprintf(stderr, "test_oled: %d of %d checks failed (%d of %d frames differ; %d of %d ink planes differ)\n", s_fail, s_checks,
                oled.bad_frames + colour.bad_frames, oled.frames + colour.frames, colour.ink_bad, colour.ink_frames);
        return 1;
    }
    printf("test_oled: %d cases, %d frames, 0 differing pixels against the emulator; %d checks\n", oled.cases, oled.frames, s_checks);
    printf("test_oled: lcd2 layout (%d x %d): %d cases, %d frames and %d ink planes (%d pixels each, %d lit ink pixels) identical to ui/oled.js\n", JO_LCD2_W,
           JO_LCD2_H, colour.cases, colour.frames, colour.ink_frames, JO_LCD2_INK_BYTES, colour.ink_lit);
    return 0;
}
