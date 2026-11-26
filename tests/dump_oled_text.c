/* SPDX-License-Identifier: GPL-3.0-only */
/* Every screen of the device's UI as the DN2's text page (core/jank_oled_text.h), for a person to read: each page twice side by side, the
 * text as sent (left-aligned: a Digitone that refuses the placing bits) and placed as the JANK PAGE's (62) length bytes say (centred lines,
 * values on the right), both in 31 columns. '>' marks the inverted line, '_' the editor's cursor cell. The colour layout's menus first (every item; the
 * ones a unit with no screen would use), then the OLED layout's where they differ. Usage: dump_oled_text PAGES_TXT */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "oled_text_world.h"

#define COLS JOT_LINE_MAX

static FILE *s_out;
static int s_pages;
static const char *s_layout;

/* a line placed in COLS columns: as sent (aligned 0), or centred / its right part at the right edge (aligned 1) */
static void place(const struct jot_page *p, int i, int aligned, char out[COLS + 1])
{
    const char *s = p->line[i];
    size_t len = strlen(s);
    memset(out, ' ', COLS);
    out[COLS] = 0;
    if (aligned && (p->centred >> i & 1u))
        memcpy(out + (COLS - len) / 2, s, len);
    else if (aligned && p->right[i]) {
        size_t r = strlen(s + p->right[i]);
        memcpy(out, s, p->right[i] - 2u);
        memcpy(out + COLS - r, s + p->right[i], r);
    } else
        memcpy(out, s, len);
    if (p->sel == i && p->cursor != JOT_NONE && !(aligned && (p->centred >> i & 1u)))
        out[p->cursor] = '_';
}

static void page(const struct world *w, uint32_t now, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void page(const struct world *w, uint32_t now, const char *fmt, ...)
{
    struct jot_page p;
    char what[160], a[COLS + 1], b[COLS + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(what, sizeof what, fmt, ap);
    va_end(ap);
    jot_render(&p, &w->ui, &w->c, now);
    s_pages++;
    fprintf(s_out, "\n--- %s / %s\n    %u line%s%s", s_layout, what, p.n, p.n == 1 ? "" : "s", p.flags & JOT_F_TITLE ? ", line 0 a title" : "");
    if (p.sel != JOT_NONE)
        fprintf(s_out, ", line %u inverted", p.sel);
    if (p.cursor != JOT_NONE)
        fprintf(s_out, ", cursor at column %u ('%c')", p.cursor, p.line[p.sel][p.cursor]);
    fprintf(s_out, "\n      %-*s    %s\n", COLS + 2, "as sent", "placed");
    fprintf(s_out, "     +---------------------+---------+  +---------------------+---------+\n");
    for (int i = 0; i < JOT_LINES; i++) {
        place(&p, i, 0, a);
        place(&p, i, 1, b);
        fprintf(s_out, "   %c |%s| %c|%s|\n", p.sel == i ? '>' : ' ', a, p.sel == i ? '>' : ' ', b);
    }
    fprintf(s_out, "     +-------------------------------+  +-------------------------------+\n");
}

static void edit_page(struct world *w, enum jo_field f, int v, const char *path)
{
    const struct jo_spec *sp = jo_spec_l(f, w->c.layout_lcd2);
    char val[24];
    w->ui.screen = JO_EDIT;
    w->ui.edit = (struct jo_edit){(uint8_t)f, w->ui.edit.ret, v, v};
    jo_edit_text(sp, v, val, sizeof val);
    page(w, 0, "%s > %s = %s (EDIT)", path, sp->title, val);
}

/* every choice; a number at its ends and its value now */
static void edits(struct world *w, enum jo_field f, int ret, const char *path)
{
    const struct jo_spec *sp = jo_spec_l(f, w->c.layout_lcd2);
    w->ui.edit.ret = (uint8_t)ret;
    if (sp->opts)
        for (int v = 0; v < sp->nopts; v++)
            edit_page(w, f, v, path);
    else {
        edit_page(w, f, sp->lo, path);
        edit_page(w, f, jo_spec_get(f, &w->c), path);
        edit_page(w, f, sp->hi, path);
    }
}

static void home_pages(struct world *w)
{
    page(w, 0, "HOME, LINK mode, playing");
    w->c.running = 0;
    w->c.armed = 1;
    page(w, 0, "HOME, armed (a start asked for: the OLED blinks its triangle)");
    w->c.armed = 0;
    w->c.held = 1;
    w->c.lt_mode = 1;
    page(w, 0, "HOME, stopped by the user, LINK TRANSPORT FOLLOW");
    w->c.held = 0;
    w->c.usb = 0;
    w->c.has_link_bpm = 0;
    w->c.has_beat = 0;
    w->c.wifi = JO_WIFI_NONE;
    w->c.has_wifiq = 1;
    w->c.wifiq = (struct jo_wifiq){0, 0.0, 0, 0, 0};
    page(w, JO_STATUS_CYCLE_MS, "HOME, no session, on the cell, the status line's second item (bad: inverted)");
    world_init(w, w->c.layout_lcd2);
    w->ui.hint = JO_NO;
    page(w, 0, "HOME, NO held 400 ms (the hint)");
    w->ui.hint = JO_YES;
    page(w, 0, "HOME, YES held 400 ms (the hint)");
    jo_show_overlay(&w->ui, 0, "FOLLOW ON", "");
    page(w, 0, "HOME, a Launchpad message (uis_message)");
    jo_show_overlay(&w->ui, 0, "LINK", "NO SESSION");
    page(w, 0, "HOME, YES without a session (the overlay)");
    world_init(w, w->c.layout_lcd2);
    w->c.link = 0;
    w->c.line_in = 1;
    w->c.has_bpm = 1;
    w->c.bpm = 128.0;
    w->c.beat_n = 0;
    w->c.has_bar = 1;
    w->c.bar_state = JO_BAR_LOCKED;
    w->c.bar_conf = 0.9;
    w->c.level_db = -18.0;
    w->c.conf = 0.72;
    page(w, 0, "HOME, AUDIO mode, AUTO BAR locked");
    w->c.bar_state = JO_BAR_LISTENING;
    w->c.bar_conf = 0.35;
    w->c.level_db = -64.0;
    w->c.conf = 0.31;
    page(w, 0, "HOME, AUDIO mode, AUTO BAR listening, quiet, low confidence");
    jo_show_overlay(&w->ui, 0, "TRACKER LEADS", "");
    page(w, 0, "HOME, AUDIO mode, UP pressed (the tracker leads)");
}

static void sub_pages(struct world *w, int sub, int menu_at)
{
    const struct jo_sub *s = &jo_subs[sub];
    int rows = jo_sub_rows(s, w->c.layout_lcd2);
    char path[64];
    snprintf(path, sizeof path, "MENU > %s", s->title);
    for (int r = 0; r < rows; r++) {
        w->ui.screen = JO_SUB;
        w->ui.sub = (uint8_t)sub;
        w->ui.ocur = (uint8_t)r;
        w->ui.cursor = (uint8_t)menu_at;
        page(w, 0, "%s, cursor on %s (SUB)", path, jo_spec_l(s->rows[r], w->c.layout_lcd2)->title);
    }
    for (int r = 0; r < rows; r++)
        edits(w, s->rows[r], JO_SUB, path);
}

static void text_pages(struct world *w, int row, const char *what)
{
    w->ui.screen = JO_NET;
    w->ui.ncur = (uint8_t)row;
    world_press(w, JO_YES, 1);
    page(w, 0, "MENU > NETWORK > %s (TEXT), cursor on the first character", what);
    world_press(w, JO_YES, 3);
    page(w, 0, "MENU > NETWORK > %s (TEXT), cursor on the 4th", what);
    while (w->ui.txt.pos < w->ui.txt.len)
        world_press(w, JO_YES, 1);
    page(w, 0, "MENU > NETWORK > %s (TEXT), cursor on the empty slot (YES confirms)", what);
}

static void network_pages(struct world *w)
{
    static const char *const MODES[3] = {"JANK", "LINK CLIENT", "LINK AP"};
    for (int m = 0; m < 3; m++) {
        struct jo_net_row rows[4];
        int n;
        w->c.net.mode = (uint8_t)m;
        n = jo_net_rows(&w->c, rows);
        for (int r = 0; r < n; r++) {
            w->ui.screen = JO_NET;
            w->ui.ncur = (uint8_t)r;
            page(w, 0, "MENU > NETWORK, TYPE %s, cursor on %s (NET)", MODES[m], rows[r].name);
        }
    }
    w->c.net.mode = JO_NET_JANK;
    edits(w, JO_F_NETMODE, JO_NET, "MENU > NETWORK > TYPE");
    w->ui.screen = JO_CONFIRM;
    page(w, 0, "MENU > NETWORK > TYPE = SETUP, YES (CONFIRM)");
    w->ui.edit.ret = JO_NET;
    edit_page(w, JO_F_GROUP, 0, "MENU > NETWORK > GROUP");
    edit_page(w, JO_F_GROUP, 8, "MENU > NETWORK > GROUP");
    w->c.net.mode = JO_NET_CLIENT;
    world_scan(w, 8);
    w->ui.screen = JO_SCAN;
    w->ui.scur = w->ui.stop = 0;
    page(w, 0, "MENU > NETWORK > NETWORK (SCAN), 8 found");
    world_press(w, JO_DOWN, 7);
    page(w, 0, "MENU > NETWORK > NETWORK (SCAN), the last one");
    w->c.net.nscan = 0;
    w->ui.scur = w->ui.stop = 0;
    page(w, 0, "MENU > NETWORK > NETWORK (SCAN), none (the firmware has no scan yet)");
    snprintf(w->c.net.ssid, sizeof w->c.net.ssid, "TESTNET-5G-EXTENDED");
    text_pages(w, 2, "PASSKEY");
    w->ui.screen = JO_NETINFO;
    page(w, 0, "MENU > NETWORK > STATUS (NETINFO), LINK CLIENT");
    w->c.net.mode = JO_NET_AP;
    text_pages(w, 1, "NAME");
    w->ui.screen = JO_NETINFO;
    page(w, 0, "MENU > NETWORK > STATUS (NETINFO), LINK AP");
    w->c.net.mode = JO_NET_JANK;
}

static void info_pages(struct world *w, int about_at)
{
    w->ui.screen = JO_INFO;
    w->ui.page = 0;
    page(w, 0, "MENU > BATTERY (INFO), on USB with a cell");
    w->c.fitted = 0;
    page(w, 0, "MENU > BATTERY (INFO), no cell");
    w->c.fitted = 1;
    w->ui.screen = JO_MENU;
    w->ui.cursor = (uint8_t)about_at;
    world_press(w, JO_YES, 1);
    for (int k = 0; k < 3; k++) {
        page(w, 0, "MENU > ABOUT (INFO), page %d", k + 1);
        world_press(w, JO_DOWN, 1);
    }
}

static void other_pages(struct world *w)
{
    w->c.setup = 1;
    w->c.net.mode = JO_NET_SETUP;
    snprintf(w->c.net.setup_name, sizeof w->c.net.setup_name, "JANK-SETUP-4F2A");
    page(w, 0, "SETUP (the firmware has no SETUP yet)");
    world_press(w, JO_NO, 1);
    page(w, 0, "SETUP, NO (leave?)");
    world_init(w, w->c.layout_lcd2);
    w->c.saved = 1;
    page(w, 0, "SAVED over HOME (the emulator's; the firmware never sets it)");
    w->c.saved = 0;
    w->c.splash = 1;
    page(w, 0, "the splash (start-up)");
    w->c.splash = 0;
    w->c.boot = 1;
    page(w, 0, "BOOT (download mode)");
}

static void menu_pages(struct world *w)
{
    int n;
    const struct jo_item *m = jo_menu_of(w->c.layout_lcd2, &n);
    world_press(w, JO_NO, 1);
    for (int i = 0; i < n; i++) {
        page(w, 0, "MENU, cursor on %s", m[i].name);
        world_press(w, JO_DOWN, 1);
    }
}

static void colour(void)
{
    struct world w;
    int n;
    const struct jo_item *m = jo_menu_of(1, &n);
    s_layout = "colour";
    world_init(&w, 1);
    home_pages(&w);
    world_init(&w, 1);
    menu_pages(&w);
    for (int i = 0; i < n; i++) {
        world_init(&w, 1);
        if (m[i].kind == JO_ITEM_SUB)
            sub_pages(&w, m[i].ref, i);
        else if (m[i].kind == JO_ITEM_EDIT)
            edits(&w, m[i].ref, JO_MENU, "MENU");
        else if (m[i].kind == JO_ITEM_NET)
            network_pages(&w);
    }
    world_init(&w, 1);
    info_pages(&w, n - 1);
    other_pages(&w);
}

/* the OLED layout: what differs from the colour layout's (its menu, AUDIO without PAUSE LEVEL, AUTO BAR OFF / ON, SOURCE's three, ABOUT) */
static void oled(void)
{
    struct world w;
    s_layout = "OLED";
    world_init(&w, 0);
    menu_pages(&w);
    world_init(&w, 0);
    w.ui.screen = JO_SUB;
    w.ui.sub = 0;
    page(&w, 0, "MENU > AUDIO (SUB)");
    edits(&w, JO_F_INSRC, JO_SUB, "MENU > AUDIO");
    edits(&w, JO_F_AUTOBAR, JO_MENU, "MENU");
    world_init(&w, 0);
    info_pages(&w, JO_MENU_ITEMS - 1);
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(s_out = fopen(argv[1], "w"))) {
        fprintf(stderr, "usage: dump_oled_text PAGES_TXT (a file it can write)\n");
        return 2;
    }
    fprintf(s_out, "jank's menu on a Digitone II: every screen as the JANK PAGE (62) carries it (tests/dump_oled_text.c, made-up names).\n"
                   "Left: the text as sent, left-aligned (what a Digitone that refuses the placing bits draws). Right: placed as the line's\n"
                   "length byte says (bit 6 centred; bit 7 the part after the first double space at the right edge, the two spaces not drawn).\n"
                   "31 columns stand for the DN2's width in capitals (4 px each); the ruler's '+' is column 21 (any 21 characters always fit).\n"
                   "'>' the inverted line (sel), '_' the TEXT editor's cursor cell.\n");
    colour();
    oled();
    fprintf(s_out, "\n%d pages\n", s_pages);
    if (fclose(s_out) != 0) {
        fprintf(stderr, "dump_oled_text: %s not written\n", argv[1]);
        return 1;
    }
    printf("dump_oled_text: %d pages\n", s_pages);
    return 0;
}
