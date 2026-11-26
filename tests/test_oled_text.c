/* SPDX-License-Identifier: GPL-3.0-only */
/* The DN2's text page of the device's UI (core/jank_oled_text.h): every screen's lines, the inverted row, the editor's cursor and the
 * placing beside the text (centred, right part), for the OLED's menus and the colour layout's; and the page rules on every page a random
 * walk of presses reaches: at most 7 lines of 31 characters 0x20..0x7e, and a line past 21 characters within the DN2's 125 px.
 * (tests/dump_oled_text.c writes every screen's page as the DN2 would show it.) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oled_text_world.h"

static int s_fail, s_checks;
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

/* The DN2's font (dn2-wishes csrc/livectl/msg.c GLYPH_W, measured there, each drawn alone): 0x21..0x7e in pixels, one column between
 * characters, a space 2 columns. Any 21 characters fit (21 x 6 - 1 = 125 px): a line past 21 must fit 125 px. */
static const uint8_t GLYPH_W[94] = {
    1, 3, 5, 5, 3, 3, 1, 2, 2, 3, 3, 2, 3, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 2, 3, 3, 3, 3, 5, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 1, 2, 3, 1, 3, 3, 3, 3,
    3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 1, 3, 4,
};
#define DN2_PX 125

static int dn2_px(const char *s)
{
    int w = 0;
    for (; *s; s++)
        w += *s == ' ' ? 2 : (unsigned char)*s >= 0x21u && (unsigned char)*s <= 0x7eu ? GLYPH_W[(unsigned char)*s - 0x21u] + 1 : 6;
    return w > 0 ? w - 1 : 0;
}

/* ---- the page rules, on every page rendered ---- */
static void check_page(const struct jot_page *p, const char *what)
{
    CHECK(p->n <= JOT_LINES, "%s: %u lines", what, p->n);
    CHECK(!(p->flags & ~(unsigned)JOT_F_TITLE), "%s: flags %#x", what, p->flags);
    CHECK(p->n == 0 || p->line[p->n - 1][0] || p->sel == p->n - 1, "%s: the last line sent is empty", what);
    CHECK(p->sel == JOT_NONE || p->sel < p->n, "%s: sel %u of %u lines", what, p->sel, p->n);
    CHECK(p->cursor == JOT_NONE || (p->sel != JOT_NONE && p->cursor < strlen(p->line[p->sel])), "%s: cursor %u", what, p->cursor);
    CHECK(!(p->centred >> JOT_LINES), "%s: centred %#x", what, p->centred);
    for (int i = 0; i < JOT_LINES; i++) {
        const char *s = p->line[i];
        size_t len = strlen(s);
        CHECK(len <= JOT_LINE_MAX, "%s: line %d has %zu characters", what, i, len);
        CHECK(i < p->n || !len, "%s: line %d '%s' past n %u", what, i, s, p->n);
        CHECK(len <= JOT_FIT || dn2_px(s) <= DN2_PX, "%s: line %d '%s' is %d px", what, i, s, dn2_px(s));
        CHECK(!p->right[i] || (p->right[i] >= 3 && p->right[i] < len && strstr(s, "  ") == s + p->right[i] - 2 && s[p->right[i]] != ' '),
              "%s: line %d '%s' right part at %u", what, i, s, p->right[i]);
        CHECK(!(p->right[i] && (p->centred >> i & 1u)), "%s: line %d both centred and split", what, i);
        for (size_t k = 0; k < len; k++)
            CHECK(s[k] >= 0x20 && s[k] <= 0x7e, "%s: line %d character %#x", what, i, (unsigned char)s[k]);
        CHECK(!len || s[len - 1] != ' ' || (p->sel == i && p->cursor == len - 1), "%s: line %d '%s' ends in a space", what, i, s);
    }
}

static struct jot_page s_page;
static const struct jot_page *render(const struct world *w, uint32_t now, const char *what)
{
    jot_render(&s_page, &w->ui, &w->c, now);
    check_page(&s_page, what);
    return &s_page;
}

#define LINE(p, i, want) CHECK(!strcmp((p)->line[i], (want)), "line %d '%s', want '%s'", (i), (p)->line[i], (want))
#define RIGHT(p, i, at) CHECK((p)->right[i] == (at), "line %d '%s': right part at %u, want %d", (i), (p)->line[i], (p)->right[i], (at))
#define CENTRED(p, i) CHECK((p)->centred >> (i) & 1u, "line %d '%s' not centred", (i), (p)->line[i])

/* ---- HOME ---- */
static void test_home_link(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    p = render(&w, 0, "home link");
    CHECK(p->flags == 0 && p->sel == JOT_NONE && p->cursor == JOT_NONE && p->n == 7 && p->centred == 0, "home: flags %u sel %u n %u", p->flags, p->sel, p->n);
    LINE(p, 0, "PLAY BOTH  USB 80%");
    RIGHT(p, 0, 11);
    LINE(p, 1, "JANK GRP 1  ===");
    RIGHT(p, 1, 12);
    LINE(p, 2, "");
    LINE(p, 3, "120.0 BPM");
    LINE(p, 4, "1 [2] 3 4");
    LINE(p, 6, "2 LINK PEERS");
    w.c.running = 0;
    w.c.armed = 1;
    LINE(render(&w, 0, "armed"), 0, "ARMED BOTH  USB 80%");
    w.c.armed = 0;
    w.c.held = 1;
    LINE(render(&w, 0, "held"), 0, "STOP BOTH  USB 80%");
    w.c.held = 0;
    w.c.lt_mode = 0;
    w.c.usb = 0;
    LINE(render(&w, 0, "paused"), 0, "PAUSE IGNORE  80%");
    w.c.fitted = 0;
    w.c.has_link_bpm = 0;
    w.c.has_beat = 0;
    w.c.wifi = JO_WIFI_WEAK;
    p = render(&w, 0, "no bpm");
    LINE(p, 0, "PAUSE IGNORE");
    RIGHT(p, 0, 0);
    LINE(p, 1, "JANK GRP 1  =--");
    LINE(p, 3, "--- BPM");
    LINE(p, 4, "1 2 3 4");
    w.c.peers = 0;
    CHECK(render(&w, 0, "no status")->n == 5, "no status item: %u lines", s_page.n);
}

static void test_home_status(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    w.c.has_wifiq = 1;
    w.c.wifiq = (struct jo_wifiq){-85, 1000.0, 0, 0, 0};
    p = render(&w, 0, "status 0");
    LINE(p, 6, "2 LINK PEERS");
    CHECK(p->sel == JOT_NONE, "a good item: sel %u", p->sel);
    p = render(&w, JO_STATUS_CYCLE_MS, "status 1");
    LINE(p, 6, "-85 dBm  1M");
    CHECK(p->sel == 6, "a bad item is inverted: sel %u", p->sel);
    LINE(render(&w, 2 * JO_STATUS_CYCLE_MS, "status 2"), 6, "RETRY --");
    LINE(render(&w, 4 * JO_STATUS_CYCLE_MS, "status 4"), 6, "2 LINK PEERS");
}

static void test_home_audio(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    w.c.link = 0;
    w.c.line_in = 1;
    w.c.has_bpm = 1;
    w.c.bpm = 128.0;
    w.c.has_bar = 1;
    w.c.bar_state = JO_BAR_LOCKED;
    w.c.bar_conf = 0.8;
    w.c.level_db = -23.0;
    w.c.conf = 0.8;
    p = render(&w, 0, "home audio");
    LINE(p, 0, "PLAY  USB 80%");
    RIGHT(p, 0, 6);
    LINE(p, 2, "128.0 BPM  1 [2] 3 4");
    RIGHT(p, 2, 11);
    LINE(p, 3, "BAR LOCKED 80%");
    LINE(p, 4, "IN -23dB  ===|===---");
    RIGHT(p, 4, 10);
    LINE(p, 5, "CF 80%  =====|==--");
    RIGHT(p, 5, 8);
    LINE(p, 6, "2 LINK PEERS");
    w.c.bar_state = JO_BAR_LISTENING;
    w.c.bar_conf = 0.4;
    LINE(render(&w, 0, "listening"), 3, "BAR LISTEN 40%");
    w.c.bar_state = JO_BAR_MANUAL;
    LINE(render(&w, 0, "manual"), 3, "BAR MANUAL");
    w.c.bar_state = JO_BAR_OFF;
    w.c.conf_graded = 1;                         /* the graded confidence has no threshold: no notch */
    w.c.has_bpm = 0;
    p = render(&w, 0, "graded");
    LINE(p, 2, "--- BPM  1 [2] 3 4");
    LINE(p, 3, "");
    LINE(p, 5, "CF 80%  ========--");
}

/* ---- the lists ---- */
static void test_menu(void)
{
    static const char *const OLED9[6] = {"USB  HOST", "OFFSET", "CLK RATE  1/16", "DISPLAY", "BATTERY  80%", "ABOUT"};
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    world_press(&w, JO_NO, 1);
    p = render(&w, 0, "menu");
    CHECK(p->flags == JOT_F_TITLE && p->sel == 1 && p->n == 7, "menu: flags %u sel %u n %u", p->flags, p->sel, p->n);
    LINE(p, 0, "MENU  1/10");
    RIGHT(p, 0, 6);
    LINE(p, 1, "AUDIO");
    RIGHT(p, 1, 0);
    LINE(p, 2, "LINK TRANSPORT  BOTH");
    RIGHT(p, 2, 16);
    LINE(p, 3, "AUTO BAR  ON");
    LINE(p, 4, "NETWORK  JANK");
    LINE(p, 5, "USB  HOST");
    LINE(p, 6, "OFFSET");
    world_press(&w, JO_DOWN, 9);                      /* the list scrolls so the cursor stays on it: the OLED's own top */
    p = render(&w, 0, "menu end");
    LINE(p, 0, "MENU  10/10");
    CHECK(p->sel == 6, "menu end: sel %u", p->sel);
    for (int k = 0; k < 6; k++)
        LINE(p, 1 + k, OLED9[k]);
    world_press(&w, JO_UP, 8);
    p = render(&w, 0, "menu up");
    CHECK(p->sel == 1 && !strcmp(p->line[1], "LINK TRANSPORT  BOTH"), "menu up: sel %u '%s'", p->sel, p->line[1]);
    world_init(&w, 1);                          /* the colour layout's menu: 11 items, USB MIDI CLOCK OUT, AUTO BAR's three */
    world_press(&w, JO_NO, 1);
    p = render(&w, 0, "menu lcd2");
    LINE(p, 0, "MENU  1/11");
    LINE(p, 3, "AUTO BAR  START");
    world_press(&w, JO_DOWN, 5);
    w.c.usb_midi = 1;
    p = render(&w, 0, "menu lcd2 usb midi");
    LINE(p, 6, "USB MIDI CLOCK OUT  OFF");
    CHECK(p->sel == 6, "lcd2: sel %u", p->sel);
}

static void test_sub(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    world_press(&w, JO_NO, 1);
    world_press(&w, JO_YES, 1);
    p = render(&w, 0, "sub audio");
    CHECK(p->flags == JOT_F_TITLE && p->sel == 1 && p->n == 5, "sub: flags %u sel %u n %u", p->flags, p->sel, p->n);
    LINE(p, 0, "AUDIO");
    LINE(p, 1, "SOURCE  AUTO");
    LINE(p, 2, "LEVEL  0 dB");
    LINE(p, 3, "LOW CONFIDENCE  RUN");
    LINE(p, 4, "THRESHOLD  50%");
    world_press(&w, JO_DOWN, 9);
    CHECK(render(&w, 0, "sub end")->sel == 4, "sub end: sel %u", s_page.sel);
    world_init(&w, 1);
    world_press(&w, JO_NO, 1);
    world_press(&w, JO_YES, 1);
    p = render(&w, 0, "sub audio lcd2");
    CHECK(p->n == 6, "lcd2 AUDIO: %u lines", p->n);
    LINE(p, 5, "PAUSE LEVEL  -40 dB");
    world_press(&w, JO_NO, 1);
    world_press(&w, JO_DOWN, 6);
    world_press(&w, JO_YES, 1);
    p = render(&w, 0, "sub offset");
    LINE(p, 0, "OFFSET");
    LINE(p, 1, "AUDIO IN  0 MS");
    LINE(p, 2, "MIDI OUT 1  +3 MS");
    LINE(p, 3, "MIDI OUT 2  -2 MS");
}

/* every field's EDIT page, every choice (numbers: the ends and 0): the title, the value inverted on line 2, the help on 4 and 5, centred */
static void check_edit(struct world *w, enum jo_field f, int v)
{
    const struct jo_spec *sp = jo_spec_l(f, w->c.layout_lcd2);
    const struct jot_page *p;
    const char *h1, *h2;
    char val[24];
    w->ui.screen = JO_EDIT;
    w->ui.edit = (struct jo_edit){(uint8_t)f, JO_MENU, v, v};
    p = render(w, 0, sp->title);
    jo_edit_text(sp, v, val, sizeof val);
    jo_help_of(sp, v, &h1, &h2);
    CHECK(p->flags == JOT_F_TITLE && p->sel == 2, "edit %s: flags %u sel %u", sp->title, p->flags, p->sel);
    LINE(p, 0, sp->title);
    LINE(p, 2, val);
    LINE(p, 4, h1);
    LINE(p, 5, h2);
    CHECK(p->centred == (0x04u | (h1[0] ? 0x10u : 0u) | (h2[0] ? 0x20u : 0u)), "edit %s: centred %#x", sp->title, p->centred);
    for (int i = 0; i < JOT_LINES && !w->c.layout_lcd2; i++)
        CHECK(strlen(p->line[i]) <= JOT_FIT, "edit %s: line %d '%s' past 21", sp->title, i, p->line[i]);
}

static void test_edit(void)
{
    struct world w;
    for (int lcd2 = 0; lcd2 < 2; lcd2++) {
        world_init(&w, lcd2);
        for (int f = 0; f < JO_F_EDITS; f++) {
            const struct jo_spec *sp = jo_spec_l((enum jo_field)f, lcd2);
            if (!sp)
                continue;
            if (sp->opts)
                for (int v = 0; v < sp->nopts; v++)
                    check_edit(&w, (enum jo_field)f, v);
            else {
                check_edit(&w, (enum jo_field)f, sp->lo);
                check_edit(&w, (enum jo_field)f, sp->hi);
                if (sp->lo < 0 && sp->hi > 0)
                    check_edit(&w, (enum jo_field)f, 0);
            }
        }
    }
}

static void test_net(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    world_press(&w, JO_NO, 1);
    world_press(&w, JO_DOWN, 3);
    world_press(&w, JO_YES, 1);
    p = render(&w, 0, "net jank");
    CHECK(p->flags == JOT_F_TITLE && p->sel == 1 && p->n == 7, "net: flags %u sel %u n %u", p->flags, p->sel, p->n);
    LINE(p, 0, "NETWORK");
    LINE(p, 1, "TYPE  JANK");
    LINE(p, 2, "GROUP  1");
    LINE(p, 3, "PEERS  2");
    LINE(p, 4, "");
    LINE(p, 5, "HOW THE DEVICE JOINS");
    LINE(p, 6, "A NETWORK");
    world_press(&w, JO_DOWN, 1);
    p = render(&w, 0, "net group");
    CHECK(p->sel == 2, "net group: sel %u", p->sel);
    LINE(p, 5, "UNITS ON THE SAME");
    LINE(p, 6, "GROUP PLAY TOGETHER");
    w.c.net.mode = JO_NET_CLIENT;               /* the values cut where the OLED cuts them */
    p = render(&w, 0, "net client");
    LINE(p, 1, "TYPE  CLIENT");
    LINE(p, 2, "NETWORK  TESTNET-5G-E");
    LINE(p, 3, "PASSKEY  ******");
    LINE(p, 4, "STATUS  CONNECTED");
    LINE(p, 5, "THE ROUTER TO JOIN");
    LINE(p, 6, "");
    CHECK(p->n == 6, "net client: %u lines", p->n);
    snprintf(w.c.net.ssid, sizeof w.c.net.ssid, "Caf\xc3\xa9");
    LINE(render(&w, 0, "net utf-8"), 2, "NETWORK  Caf??");
    w.c.net.mode = JO_NET_AP;
    p = render(&w, 0, "net ap");
    LINE(p, 2, "NAME  Jank");
    LINE(p, 3, "PASSKEY");
    LINE(p, 4, "STATUS  0 CLIENTS");
}

static void test_scan(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    w.c.net.mode = JO_NET_CLIENT;
    world_scan(&w, 8);
    snprintf(w.c.net.scan[7].name, sizeof w.c.net.scan[7].name, "A-VERY-LONG-NAME-OF-A-NETWORK");
    w.ui.screen = JO_SCAN;
    p = render(&w, 0, "scan");
    CHECK(p->flags == JOT_F_TITLE && p->sel == 1 && p->n == 7, "scan: flags %u sel %u n %u", p->flags, p->sel, p->n);
    LINE(p, 0, "NETWORK  1/8");
    LINE(p, 1, " NET-1  ---");
    RIGHT(p, 1, 8);
    LINE(p, 2, " NET-2  =--");
    LINE(p, 3, "*NET-3  ==-");
    LINE(p, 4, " NET-4  ===");
    world_press(&w, JO_DOWN, 7);
    p = render(&w, 0, "scan end");
    LINE(p, 0, "NETWORK  8/8");
    CHECK(p->sel == 5 && p->n == 6, "scan end: sel %u n %u", p->sel, p->n);
    LINE(p, 1, " NET-4  ===");
    LINE(p, 5, " A-VERY-LONG-NAM  ===");
    w.c.net.nscan = 0;
    w.ui.scur = w.ui.stop = 0;
    p = render(&w, 0, "scan none");
    LINE(p, 0, "NETWORK");
    LINE(p, 3, "NO NETWORKS");
    CENTRED(p, 3);
    CHECK(p->sel == JOT_NONE, "scan none: sel %u", p->sel);
}

/* the character editor: masked but for the cursor's cell, which shows the real character; the cursor is a column of the inverted row */
static void test_text(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    w.c.net.mode = JO_NET_CLIENT;
    w.ui.screen = JO_NET;
    w.ui.ncur = 2;
    world_press(&w, JO_YES, 1);
    p = render(&w, 0, "text");
    CHECK(p->flags == JOT_F_TITLE && p->sel == 1 && p->cursor == 0, "text: flags %u sel %u cursor %u", p->flags, p->sel, p->cursor);
    LINE(p, 0, "PASSKEY  10");
    LINE(p, 1, "m*********");
    LINE(p, 3, "YES NEXT  NO DEL");
    LINE(p, 4, "easier: web UI");
    LINE(p, 5, "jank.local");
    CHECK((p->centred & 0x38u) == 0x38u && p->right[3] == 0, "text: centred %#x right %u", p->centred, p->right[3]);
    world_press(&w, JO_YES, 3);
    p = render(&w, 0, "text 3");
    LINE(p, 1, "***e******");
    CHECK(p->cursor == 3, "text 3: cursor %u", p->cursor);
    world_press(&w, JO_YES, 7);
    p = render(&w, 0, "text end");
    LINE(p, 1, "********** ");                   /* the empty slot past the end: YES there confirms */
    CHECK(p->cursor == 10, "text end: cursor %u", p->cursor);
    snprintf(w.ui.txt.text, sizeof w.ui.txt.text, "abcdefghijabcdefghijabcdefghijabcdefghij");
    w.ui.txt.len = 40;
    w.ui.txt.pos = 30;
    p = render(&w, 0, "text long");
    LINE(p, 0, "PASSKEY  40");
    LINE(p, 1, "********************a");
    CHECK(p->cursor == 20, "text long: cursor %u", p->cursor);
    w.c.net.mode = JO_NET_AP;
    w.ui.screen = JO_NET;
    w.ui.ncur = 1;
    world_press(&w, JO_YES, 1);
    world_press(&w, JO_YES, 1);
    p = render(&w, 0, "ap name");
    LINE(p, 0, "AP NAME  4");
    LINE(p, 1, "Jank");
    LINE(p, 5, "192.168.4.1");
    CHECK(p->cursor == 1, "ap name: cursor %u", p->cursor);
}

/* ABOUT: a press moves the layout's page, 6 rows while the text page shows it (jo_about_vis) */
static void test_about(void)
{
    struct world w;
    const struct jot_page *p;
    struct jo_ctx c;
    memset(&c, 0, sizeof c);
    CHECK(jo_about_vis(&c) == JO_ABOUT_VIS_OLED, "OLED vis %d", jo_about_vis(&c));
    c.text_view = 1;
    CHECK(jo_about_vis(&c) == JO_ABOUT_VIS_OLED, "OLED text vis %d", jo_about_vis(&c));
    c.layout_lcd2 = 1;
    CHECK(jo_about_vis(&c) == JO_ABOUT_VIS_TEXT, "lcd2 text vis %d", jo_about_vis(&c));
    c.text_view = 0;
    CHECK(jo_about_vis(&c) == JO_ABOUT_VIS_LCD2, "lcd2 vis %d", jo_about_vis(&c));
    world_init(&w, 0);
    world_press(&w, JO_NO, 1);
    world_press(&w, JO_DOWN, 9);
    world_press(&w, JO_YES, 1);
    p = render(&w, 0, "about");
    CHECK(p->flags == JOT_F_TITLE && p->sel == JOT_NONE && p->n == 6, "about: flags %u sel %u n %u", p->flags, p->sel, p->n);
    LINE(p, 0, "ABOUT");
    LINE(p, 1, "VERSION  0.72.0");
    LINE(p, 2, "REPO  w0ts/jank");
    LINE(p, 3, "UPTIME  1:02:05");
    LINE(p, 5, "TEMP  41 C");
    world_press(&w, JO_DOWN, 1);
    p = render(&w, 0, "about 2");
    LINE(p, 1, "CPU  57/3%");
    LINE(p, 4, "FLASH  8M 1.9/3.0M");
    world_press(&w, JO_DOWN, 1);
    p = render(&w, 0, "about 3");
    CHECK(p->n == 4, "about 3: %u lines", p->n);
    LINE(p, 3, "PANIC  -");
    world_init(&w, 1);
    world_press(&w, JO_NO, 1);
    world_press(&w, JO_DOWN, 10);
    world_press(&w, JO_YES, 1);
    p = render(&w, 0, "about lcd2");
    CHECK(p->n == 7, "about lcd2: %u lines", p->n);
    LINE(p, 2, "REPO  github.com/w0ts/jank");
    LINE(p, 6, "CPU  57/3%");
    world_press(&w, JO_DOWN, 1);
    p = render(&w, 0, "about lcd2 2");
    LINE(p, 1, "MEMORY  61K PSRAM 7900K");
    LINE(p, 6, "BEAT LOOP  2100 US");
    world_press(&w, JO_DOWN, 1);
    LINE(render(&w, 0, "about lcd2 3"), 1, "PANIC  -");
}

/* ---- the other screens ---- */
static void test_screens(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    w.ui.screen = JO_INFO;
    p = render(&w, 0, "battery");
    LINE(p, 0, "BATTERY");
    LINE(p, 1, "LEVEL 80%");
    LINE(p, 2, "VOLT 4.02V EST");
    LINE(p, 3, "CHARGING");
    LINE(p, 5, "=================----");
    w.c.fitted = 0;
    p = render(&w, 0, "no cell");
    CHECK(p->n == 3, "no cell: %u lines", p->n);
    LINE(p, 1, "NO CELL FITTED");
    LINE(p, 2, "POWER USB");
    w.ui.screen = JO_NETINFO;
    w.c.net.mode = JO_NET_CLIENT;
    p = render(&w, 0, "netinfo");
    LINE(p, 0, "LINK CLIENT");
    LINE(p, 1, "STATE CONNECTED");
    LINE(p, 2, "IP 192.168.1.23");
    LINE(p, 3, "SSID TESTNET-5G-EXTEN");
    w.c.net.mode = JO_NET_AP;
    p = render(&w, 0, "netinfo ap");
    LINE(p, 0, "LINK AP");
    LINE(p, 2, "CLIENTS 0");
    LINE(p, 3, "NAME Jank");
    w.ui.screen = JO_CONFIRM;
    p = render(&w, 0, "confirm");
    LINE(p, 0, "SETUP");
    LINE(p, 2, "STOP AND START");
    LINE(p, 3, "SETUP?");
    LINE(p, 5, "YES      NO");
    CHECK(p->centred == 0x2cu && p->sel == JOT_NONE, "confirm: centred %#x sel %u", p->centred, p->sel);
    w.c.setup = 1;
    w.c.net.mode = JO_NET_SETUP;
    snprintf(w.c.net.setup_name, sizeof w.c.net.setup_name, "JANK-SETUP-ABCDEF123456");
    p = render(&w, 0, "setup");
    CHECK(p->flags == 0 && p->n == 7, "setup: flags %u n %u", p->flags, p->n);
    LINE(p, 0, "SETUP MODE");
    LINE(p, 1, "JANK-SETUP-ABCDEF1234");
    LINE(p, 2, "56");
    LINE(p, 4, "open");
    LINE(p, 5, "192.168.4.1");
    LINE(p, 6, "NO: LEAVE");
    world_press(&w, JO_NO, 1);
    p = render(&w, 0, "setup leave");
    LINE(p, 2, "LEAVE SETUP?");
    LINE(p, 3, "RUN AGAIN");
    w.c.boot = 1;
    p = render(&w, 0, "boot");
    LINE(p, 2, "BOOT");
    LINE(p, 5, "BOOTLOADER");
    CHECK(p->centred == 0x24u && p->flags == 0, "boot: centred %#x", p->centred);
    w.c.boot = 0;
    w.c.splash = 1;
    p = render(&w, 0, "splash");
    LINE(p, 1, "JANK");
    LINE(p, 3, "MINI 3");
    LINE(p, 5, "FW 0.72.0");
}

/* the hint, the overlay box and SAVED take lines 2..4 over the screen, inverted at their first line */
static void test_overlays(void)
{
    struct world w;
    const struct jot_page *p;
    world_init(&w, 0);
    w.ui.hint = JO_NO;
    p = render(&w, 0, "hint");
    LINE(p, 0, "PLAY BOTH  USB 80%");
    LINE(p, 2, "NUDGE BEAT");
    LINE(p, 3, "UP: 1/2 BEAT EARLY");
    LINE(p, 4, "DOWN: 1/2 BEAT LATE");
    LINE(p, 6, "2 LINK PEERS");
    CHECK(p->sel == 2 && (p->centred & 0x1cu) == 0x1cu, "hint: sel %u centred %#x", p->sel, p->centred);
    w.ui.hint = JO_YES;
    LINE(render(&w, 0, "hint yes"), 2, "NUDGE MS");
    w.ui.hint = -1;
    jo_show_overlay(&w.ui, 1000, "DOUBLE", "240.0");
    p = render(&w, 1999, "overlay");
    LINE(p, 2, "DOUBLE");
    LINE(p, 3, "240.0");
    LINE(p, 4, "");
    CHECK(p->sel == 2, "overlay: sel %u", p->sel);
    p = render(&w, 2000, "overlay over");
    LINE(p, 3, "120.0 BPM");
    CHECK(p->sel == JOT_NONE, "overlay over: sel %u", p->sel);
    w.c.net.mode = JO_NET_CLIENT;               /* over the editor: the cursor goes with the row */
    w.ui.screen = JO_NET;
    w.ui.ncur = 2;
    world_press(&w, JO_YES, 1);
    jo_show_overlay(&w.ui, 3000, "PASSKEY", "8+ CHARS");
    p = render(&w, 3000, "overlay text");
    CHECK(p->sel == 2 && p->cursor == JOT_NONE, "overlay text: sel %u cursor %u", p->sel, p->cursor);
    w.c.saved = 1;
    p = render(&w, 9000, "saved");
    LINE(p, 2, "");
    LINE(p, 3, "SAVED");
    CHECK(p->sel == 3, "saved: sel %u", p->sel);
}

static void test_equal(void)
{
    struct world w;
    struct jot_page a, b;
    world_init(&w, 0);
    world_press(&w, JO_NO, 1);
    jot_render(&a, &w.ui, &w.c, 0);
    jot_render(&b, &w.ui, &w.c, 0);
    CHECK(jot_equal(&a, &b), "the same state, the same page");
    b.right[3]++;
    CHECK(!jot_equal(&a, &b), "a right part moved");
    world_press(&w, JO_DOWN, 1);
    jot_render(&b, &w.ui, &w.c, 0);
    CHECK(!jot_equal(&a, &b), "the cursor moved");
    w.ui.overlay = (struct jo_overlay){1, 5000, "AB", ""};
    jot_render(&a, &w.ui, &w.c, 0);             /* a shorter line written over a longer one: the bytes after it do not count */
    jot_render(&b, &w.ui, &w.c, 0);
    snprintf(b.line[2], sizeof b.line[2], "ABCDEFGH");
    snprintf(b.line[2], sizeof b.line[2], "AB");
    CHECK(jot_equal(&a, &b), "bytes past the end");
}

/* every page a random walk of presses reaches, both layouts, the state changing under it */
static void test_walk(void)
{
    uint32_t seed = 12345u;
    int pages = 0;
    for (int lcd2 = 0; lcd2 < 2; lcd2++) {
        struct world w;
        world_init(&w, lcd2);
        w.c.net.nscan = 12;
        for (int i = 0; i < 12; i++)
            snprintf(w.c.net.scan[i].name, sizeof w.c.net.scan[i].name, "WALK-NET-%02d-WITH-A-LONGER-NAME", i);
        for (int step = 0; step < 4000; step++) {
            uint32_t r;
            seed = seed * 1103515245u + 12345u;
            r = seed >> 16;
            if (r % 50 == 0)
                w.c.net.mode = (uint8_t)(r / 50 % 3);
            if (r % 70 == 1) {
                w.c.link = !w.c.link;
                w.c.line_in = !w.c.link;
                w.c.has_bar = !w.c.link;
                w.c.bar_state = (uint8_t)(r / 70 % 4);
            }
            if (r % 90 == 2)
                w.c.peers = (int)(r / 90 % 3);
            if (r % 200 == 3)
                jo_show_overlay(&w.ui, step * 10u, "WALK", "1");
            w.ui.hint = r % 300 == 4 && w.ui.screen == JO_HOME ? JO_NO : -1;
            jo_press(&w.ui, (enum jo_btn)(r % JO_BTNS), step * 10u, &w.c, &w.ops);
            render(&w, step * 10u, "walk");
            pages++;
        }
    }
    CHECK(pages == 8000, "walk: %d pages", pages);
}

int main(void)
{
    test_home_link();
    test_home_status();
    test_home_audio();
    test_menu();
    test_sub();
    test_edit();
    test_net();
    test_scan();
    test_text();
    test_about();
    test_screens();
    test_overlays();
    test_equal();
    test_walk();
    if (s_fail) {
        fprintf(stderr, "test_oled_text: %d of %d checks FAILED\n", s_fail, s_checks);
        return 1;
    }
    printf("test_oled_text: %d checks passed\n", s_checks);
    return 0;
}
