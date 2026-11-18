/* SPDX-License-Identifier: GPL-3.0-only */
/* The lcd2 layout of the UI (ui/oled.js, ctx.layout 'lcd2'; the Waveshare ESP32-S3-LCD-2): its own logical grid of 160 x 120 pixels
 * (4:3, like the panel), the same 5 x 7 font and primitives at integer scales, painted exactly x2 on 320 x 240 (core/jank_lcd.c). A port,
 * function by function, of ui/oled.js's "LCD2 LAYOUT" section; tests/test_oled.c compares it with that file's own frames and ink planes
 * (tests/oled_ref/cases_ink.txt). The OLED never runs this code. Colour is information only (the ink plane): menus, edit screens and
 * overlays have none. Not touched by the stage that gave the OLED its numbers: the OLED UI's draw is jank_oled_draw.c. */
#include "jank_bigfont.h"
#include "jank_oled_int.h"
#include <stdio.h>
#include <string.h>

#define W JO_LCD2_W
#define H JO_LCD2_H
#define ARMED_BLINK_MS 400u
#define METER_MIN_DB (-90.0)
#define QUIET_DB (-50.0)                /* under the tracker's sound level: too quiet to track */
#define CLIP_PEAK_DB (-1.0)             /* clipping: the last second's peak (unreported: the RMS maximum over -3) */
#define CLIP_RMS_MAX_DB (-3.0)
#define CONF_WARN_MARGIN 10             /* amber from the stop threshold up to this many points above it */
#define LINK_JOINED_MIN 1
#define SIGNAL_GOOD_DBM (-67)
#define SIGNAL_POOR_DBM (-80)
#define LOST_BAD_PCT 10
#define LOST_WARN_PCT 3
#define RETRY_BAD_PCT 30
#define BATT_BAD_PCT 15
#define BATT_WARN_PCT 30

/* the HOME geometry: ui/oled.js's L2 table */
#define TOP_H 13
#define ICON_Y 1
#define ICON_H 11
#define PLAY_X 2
#define LT_X 14
#define FOLD_X 31
#define NAME_XR 99
#define NAME_MAX_W 50
#define LINK_X 103
#define LINK_W 27
#define PEERS_X 118
#define BATT_X 134
#define BATT_W 24
#define TEMPO_Y 16
#define LINK_TEMPO_Y 19
#define BOX_GAP 4
#define BOX_Y 55
#define AUDIO_BOX_W 36
#define AUDIO_BOX_H 28
#define ROW_GAP 1
#define STATE_Y 113
#define LINK_BOX_SIDE 36
#define LINK_BOX_Y 73
#define LABEL_X 2
#define VALUE_XR 158
#define BAR_X 64
#define BAR_W 62
#define LINE_H 3
#define VU_H 5

/* 1-bit rows ('#' = lit): the link glyph (three linked nodes) 13 x 11, the plug drawn inside the battery while charging 9 x 7, and the LINK TRANSPORT
 * icons: IGNORE a crossed circle, FOLLOW an arrow coming in, FOLLOW+LEAD a double arrow */
static const char *const LINK_GLYPH[11] = {".....###.....", "....#####....", "....#####....", ".....###.....", "....#...#....", "...#.....#...", ".###.....###.",
                                           "#####...#####", "#############", "#####...#####", ".###.....###."};
static const char *const PLUG_IN_BATTERY[7] = {"..####...", "..#######", "#########", "######...", "#########", "..#######", "..####..."};
static const char *const LT_ICON0[11] = {"...#####...", "..##...##..", ".##.....##.", "##.....#.##", "#.....#...#", "#....#....#", "#...#.....#", "##.#.....##",
                                         ".##.....##.", "..##...##..", "...#####..."};
static const char *const LT_ICON1[11] = {".....#.......", "....##.......", "...###.......", "..####.......", ".############", "#############", ".############",
                                         "..####.......", "...###.......", "....##.......", ".....#......."};
static const char *const LT_ICON2[11] = {".....#.#.....", "....##.##....", "...###.###...", "..####.####..", ".###########.", "#############", ".###########.",
                                         "..####.####..", "...###.###...", "....##.##....", ".....#.#....."};
static const char *const *const LT_ICONS[3] = {LT_ICON0, LT_ICON1, LT_ICON2};
static const uint8_t TRI_W[11] = {2, 3, 5, 6, 8, 9, 8, 6, 5, 3, 2};          /* the play triangle's row widths (9 x 11) */

static const char *const BAR_WORDS[4] = {NULL, "LISTEN", "LOCKED", "MANUAL"};     /* by enum jo_bar_state: off, listening, locked, manual */

static void tag(int id, int x, int y, int w, int h) { jo_tag(id, x, y, w, h); }

static void boxed(uint8_t *fb, int x, int y, int w, int h)
{
    jo_rect(fb, x, y, w, h, 1);
    jo_frame(fb, x + 2, y + 2, w - 4, h - 4, 0);
}

/* an outlined bar and its fill (BATTERY) */
static void bar(uint8_t *fb, int x, int y, int w, double frac)
{
    jo_frame(fb, x, y, w, 7, 1);
    jo_rect(fb, x + 1, y + 1, jo_iround((w - 2) * jo_clampd(frac, 0.0, 1.0)), 5, 1);
}

/* text with its ink: the box is the text's own width and 7 rows */
static void ink_text(uint8_t *fb, const char *s, int x, int y, int ink)
{
    jo_text(fb, s, x, y, 1, 1);
    tag(ink, x, y, jo_text_w(s, 1), 7);
}

/* ---- policy: signal quality, link state, battery, level, confidence (colour = information) ---- */
enum quality { Q_NONE, Q_POOR, Q_FAIR, Q_GOOD };

static enum quality signal_quality(const struct jo_ctx *c)
{
    if (!c->has_rssi || c->rssi == 0)
        return Q_NONE;
    return c->rssi >= SIGNAL_GOOD_DBM ? Q_GOOD : c->rssi >= SIGNAL_POOR_DBM ? Q_FAIR : Q_POOR;
}

static int link_ink(const struct jo_ctx *c)
{
    return c->peers >= LINK_JOINED_MIN ? JO_INK_GOOD : c->link ? JO_INK_BAD : JO_INK_DIM;
}

static int batt_ink(int pct) { return pct < BATT_BAD_PCT ? JO_INK_BAD : pct < BATT_WARN_PCT ? JO_INK_WARN : JO_INK_NONE; }

static int charging(const struct jo_ctx *c) { return c->usb && c->fitted && c->batt < 100; }

static int level_ink(const struct jo_ctx *c)
{
    int clip = c->has_peak ? c->peak_db >= CLIP_PEAK_DB : c->level_max >= CLIP_RMS_MAX_DB;
    if (clip)
        return JO_INK_BAD;
    return c->level_db < QUIET_DB ? JO_INK_WARN : JO_INK_NONE;
}

static int conf_ink(const struct jo_ctx *c)
{
    int pct;
    if (c->conf_unknown || c->silent)
        return JO_INK_DIM;
    pct = jo_iround(c->conf * 100);
    return pct < c->th ? JO_INK_BAD : pct <= c->th + CONF_WARN_MARGIN ? JO_INK_WARN : JO_INK_GOOD;
}

static const char *fold_label(int8_t exp)
{
    return exp == 1 ? "X2" : exp == 2 ? "X4" : exp == -1 ? "/2" : exp == -2 ? "/4" : "";
}

/* ---- the top bar ---- */
static void transport_icon(uint8_t *fb, const struct jo_ctx *c, uint32_t now)
{
    int tri = c->running || (c->armed && (now / ARMED_BLINK_MS) % 2 == 0);
    if (tri)
        for (int r = 0; r < 11; r++)
            jo_rect(fb, PLAY_X, ICON_Y + r, TRI_W[r], 1, 1);
    else if (!c->armed)
        jo_rect(fb, PLAY_X, ICON_Y + 1, 9, 9, 1);
}

/* the LINK TRANSPORT icon (both modes) and, in AUDIO mode when the fold is not 1, the fold label beside it */
static void option_icons(uint8_t *fb, const struct jo_ctx *c)
{
    const char *fl = !c->link ? fold_label(c->fold_exp) : "";
    int w = jo_text_w(fl, 1) + 4;
    if (c->lt_mode < 3)
        jo_icon(fb, LT_ICONS[c->lt_mode], 11, LT_X, ICON_Y);
    if (!fl[0])
        return;
    jo_rect(fb, FOLD_X, ICON_Y, w, ICON_H, 1);
    jo_px(fb, FOLD_X, ICON_Y, 0);
    jo_px(fb, FOLD_X + w - 1, ICON_Y, 0);
    jo_px(fb, FOLD_X, ICON_Y + ICON_H - 1, 0);
    jo_px(fb, FOLD_X + w - 1, ICON_Y + ICON_H - 1, 0);
    jo_text(fb, fl, FOLD_X + 2, ICON_Y + 2, 1, 0);
}

/* the battery: neutral while healthy, amber under BATT_WARN_PCT, red under BATT_BAD_PCT; while charging the plug is XOR-ed inside it (dark over the fill) */
static void battery(uint8_t *fb, const struct jo_ctx *c)
{
    int x = BATT_X;
    jo_frame(fb, x, ICON_Y, 22, ICON_H, 1);
    jo_px(fb, x, ICON_Y, 0);
    jo_px(fb, x + 21, ICON_Y, 0);
    jo_px(fb, x, ICON_Y + 10, 0);
    jo_px(fb, x + 21, ICON_Y + 10, 0);
    jo_rect(fb, x + 22, 4, 2, 5, 1);
    jo_rect(fb, x + 2, 3, jo_iround(18.0 * jo_clampd(c->batt, 0, 100) / 100.0), 7, 1);
    if (charging(c))
        for (int j = 0; j < 7; j++)
            for (int i = 0; PLUG_IN_BATTERY[j][i]; i++)
                if (PLUG_IN_BATTERY[j][i] == '#')
                    jo_xor_px(fb, x + 6 + i, 3 + j);
    tag(batt_ink(c->batt), x, ICON_Y, BATT_W, ICON_H);
}

/* GRP n (or the Wi-Fi / own network name), right-aligned to x 98 */
static void network_name(uint8_t *fb, const struct jo_ctx *c)
{
    char full[JO_TEXT_MAX + 16], shown[JO_TEXT_MAX + 16];
    full[0] = 0;
    if (c->net.mode == JO_NET_JANK)
        snprintf(full, sizeof full, "GRP %u", c->net.group);
    else if (c->net.mode == JO_NET_CLIENT)
        snprintf(full, sizeof full, "%s", c->net.ssid);
    else if (c->net.mode == JO_NET_AP)
        snprintf(full, sizeof full, "%s", c->net.ap_name);
    if (!full[0])
        return;
    jo_fit(full, NAME_MAX_W, shown, sizeof shown);
    if (shown[0])
        jo_text(fb, shown, NAME_XR - jo_text_w(shown, 1), 3, 1, 1);
}

static void top_bar(uint8_t *fb, const struct jo_ctx *c, uint32_t now)
{
    transport_icon(fb, c, now);
    option_icons(fb, c);
    network_name(fb, c);
    jo_icon(fb, LINK_GLYPH, 11, LINK_X, ICON_Y);                    /* the link glyph, and the Link peers beside it */
    if (c->peers > 0) {
        char n[12];
        snprintf(n, sizeof n, "%d", c->peers < 99 ? c->peers : 99);
        jo_text(fb, n, PEERS_X, 3, 1, 1);
    }
    tag(link_ink(c), LINK_X, 0, LINK_W, TOP_H);
    if (c->fitted)
        battery(fb, c);
}

/* ---- the tempo ---- */
static void tempo(uint8_t *fb, const struct jo_ctx *c)
{
    const struct jbf_font *font = c->link ? &jbf_large : &jbf_normal;
    int has = c->link ? c->has_link_bpm : c->has_bpm;
    double bpm = c->link ? c->link_bpm : c->bpm;
    char s[24];
    if (has && bpm != 0.0)
        jo_fixed(bpm, 1, s, sizeof s);
    else
        snprintf(s, sizeof s, "---");
    jbf_draw(font, fb, s, (W - jbf_width(font, s)) / 2, c->link ? LINK_TEMPO_Y : TEMPO_Y);
}

/* ---- the info rows ---- */
static int bar_fill_width(double frac) { return jo_iround(BAR_W * jo_clampd(frac, 0.0, 1.0)); }
static int bar_tick_x(double frac) { return BAR_X + jo_iround((BAR_W - 1) * frac); }
static double db_frac(double db) { return jo_clampd((db - METER_MIN_DB) / -METER_MIN_DB, 0.0, 1.0); }
static int vu_ink(int col) { return JO_INK_VU0 + JO_VU_STEPS * col / BAR_W; }

static void row_text(uint8_t *fb, int y, const char *label, const char *value)
{
    jo_text(fb, label, LABEL_X, y, 1, 1);
    jo_text(fb, value, VALUE_XR - jo_text_w(value, 1), y, 1, 1);
}

/* the CONFIDENCE row: one thin line (3 px high, no outline, no track) whose length is the value on 62 px, with a notch (1 px wide, 5 tall) XOR-ed
 * through it at tick; the value, the line and the notch carry ink, the label stays neutral */
static void line_row(uint8_t *fb, int y, const char *label, const char *value, double frac, double tick, int ink)
{
    int n = bar_fill_width(frac), x = bar_tick_x(tick);
    row_text(fb, y, label, value);
    tag(ink, VALUE_XR - jo_text_w(value, 1), y, jo_text_w(value, 1), 7);
    jo_rect(fb, BAR_X, y + 2, n, LINE_H, 1);
    tag(ink, BAR_X, y + 2, n, LINE_H);
    jo_xor_col(fb, x, y + 1, y + 5);
    tag(ink, x, y + 1, 1, 5);
}

/* the INPUT row: a continuous VU meter 5 px high on 62 px, every column the ramp id of its position on the dBFS scale, and a notch (7 px tall,
 * XOR-ed) at the tracker's -50 dBFS. The number carries the level rule's ink. */
static void vu_row(uint8_t *fb, int y, const struct jo_ctx *c)
{
    char value[16];
    int top = y + 1, n = bar_fill_width(db_frac(c->level_db)), vw;
    snprintf(value, sizeof value, "%ddB", jo_iround(c->level_db));
    vw = jo_text_w(value, 1);
    row_text(fb, y, "INPUT", value);
    tag(level_ink(c), VALUE_XR - vw, y, vw, 7);
    jo_rect(fb, BAR_X, top, n, VU_H, 1);
    for (int i = 0; i < n; i++)
        tag(vu_ink(i), BAR_X + i, top, 1, VU_H);
    jo_xor_col(fb, bar_tick_x(db_frac(QUIET_DB)), y, y + 6);
}

/* the BAR row: AUTO BAR's state word and an outlined bar (62 x 5, a 3 px fill inside it), neutral, no tick */
static void outlined_row(uint8_t *fb, int y, const char *label, const char *value, double frac)
{
    jo_text(fb, label, LABEL_X, y, 1, 1);
    jo_text(fb, value, LABEL_X + 58 - jo_text_w(value, 1), y, 1, 1);
    jo_frame(fb, BAR_X, y + 1, BAR_W, 5, 1);
    jo_rect(fb, BAR_X + 1, y + 2, jo_iround((BAR_W - 2) * jo_clampd(frac, 0.0, 1.0)), 3, 1);
}

/* the beat indicator (no LEDs): the lit box solid, the others 2 px outlines; the first one is amber only while lit */
static void beat_boxes(uint8_t *fb, const struct jo_ctx *c, int x0, int y, int w, int h, int gap)
{
    for (int i = 0; i < 4; i++) {
        int x = x0 + i * (w + gap), on = c->has_beat && ((c->beat_n % 4) + 4) % 4 == i;
        if (on)
            jo_rect(fb, x, y, w, h, 1);
        else {
            jo_frame(fb, x, y, w, h, 1);
            jo_frame(fb, x + 1, y + 1, w - 2, h - 2, 1);
        }
        if (i == 0 && on)
            tag(JO_INK_ONE, x, y, w, h);
    }
}

static int box_row_x(int w, int gap) { return (W - (4 * w + 3 * gap)) / 2; }

/* the state line (AUDIO only), the first state that applies; nothing while it is simply running */
static void state_word(uint8_t *fb, const struct jo_ctx *c)
{
    int coast = !c->conf_hold && !c->conf_unknown && c->running && c->conf * 100 < c->th;
    const char *word = NULL;
    int ink = JO_INK_DIM;
    if (c->gated) {
        word = "GATED";
        ink = JO_INK_BAD;
    } else if (c->conf_hold)
        word = "HOLD";
    else if (coast) {
        word = "COAST";
        ink = JO_INK_WARN;
    } else if (!c->running)
        word = "STOPPED";
    else if (c->silent)
        word = "SILENT";
    if (word)
        ink_text(fb, word, (W - jo_text_w(word, 1)) / 2, STATE_Y, ink);
}

/* the info rows (INPUT, CONFIDENCE, BAR when shown), ROW_GAP apart, as one block centred between the bottom of the beat boxes and the state line:
 * the empty rows above and below differ by at most 1, the odd one goes below. Returns the first row's y. */
static int rows_top(int with_bar)
{
    int n = with_bar ? 3 : 2, block = n * 7 + (n - 1) * ROW_GAP, bottom = BOX_Y + AUDIO_BOX_H;
    return bottom + (STATE_Y - bottom - block) / 2;
}

static void home(uint8_t *fb, const struct jo_ctx *c, uint32_t now)
{
    char value[16];
    int with_bar = c->has_bar && c->bar_state != JO_BAR_OFF && c->bar_state <= JO_BAR_MANUAL, pitch = 7 + ROW_GAP, row = rows_top(with_bar);
    top_bar(fb, c, now);
    tempo(fb, c);
    if (c->link) {
        beat_boxes(fb, c, box_row_x(LINK_BOX_SIDE, BOX_GAP), LINK_BOX_Y, LINK_BOX_SIDE, LINK_BOX_SIDE, BOX_GAP);
        return;
    }
    beat_boxes(fb, c, box_row_x(AUDIO_BOX_W, BOX_GAP), BOX_Y, AUDIO_BOX_W, AUDIO_BOX_H, BOX_GAP);
    vu_row(fb, row, c);
    if (c->conf_unknown)
        snprintf(value, sizeof value, "--");
    else
        snprintf(value, sizeof value, "%d%%", jo_iround(c->conf * 100));
    line_row(fb, row + pitch, "CONFIDENCE", value, c->conf_unknown ? 0.0 : c->conf, c->th / 100.0, conf_ink(c));
    if (with_bar)
        outlined_row(fb, row + 2 * pitch, "BAR", BAR_WORDS[c->bar_state], c->bar_state == JO_BAR_MANUAL ? 0.0 : c->bar_conf);
    state_word(fb, c);
}

/* ---- the lists: a title bar of 11 px, rows of 9 px from y 13 (12 rows: the whole MENU fits), full width, neutral, the selected row inverted ---- */
#define ROW0 13
#define PITCH 9
#define ROWS 12

static void title_bar(uint8_t *fb, const char *title, const char *right)
{
    jo_rect(fb, 0, 0, W, 11, 1);
    jo_text(fb, title, 3, 2, 1, 0);
    if (right && *right)
        jo_text_r(fb, right, W - 3, 2, 1, 0);
}

static void list_row(uint8_t *fb, int y, int h, int sel, const char *left, const char *right)
{
    int v = !sel;
    if (sel)
        jo_rect(fb, 0, y - 1, W, h, 1);
    jo_text(fb, left, 4, y, 1, v);
    if (right && *right)
        jo_text_r(fb, right, W - 4, y, 1, v);
}

static void menu_value(const struct jo_ctx *c, int i, char *out, size_t cap)
{
    const char *v = "";
    char b[12];
    switch (i) {
    case 1: v = c->lt_mode < 3 ? jo_lt_short[c->lt_mode] : ""; break;
    case 2: v = c->auto_bar < 3 ? jo_autobar_short[c->auto_bar] : ""; break;
    case 3: v = c->net.mode < 4 ? jo_net_short[c->net.mode] : ""; break;
    case 4: v = c->usb_role < 2 ? jo_usb_names[c->usb_role] : ""; break;
    case 5: v = c->usb_midi < 2 ? jo_off_on_names[c->usb_midi] : ""; break;
    case 7: v = c->rate < 5 ? jo_rates_short[c->rate] : ""; break;
    case 9:
        if (c->fitted)
            snprintf(b, sizeof b, "%d%%", c->batt);
        else
            snprintf(b, sizeof b, "-");
        v = b;
        break;
    default: break;
    }
    snprintf(out, cap, "%s", v);
}

static void menu(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    char right[8], val[12];
    int top = jo_clampi(ui->cursor - ROWS + 1, 0, JO_MENU_ITEMS_LCD2 > ROWS ? JO_MENU_ITEMS_LCD2 - ROWS : 0);
    snprintf(right, sizeof right, "%d/%d", ui->cursor + 1, JO_MENU_ITEMS_LCD2);
    title_bar(fb, "MENU", right);
    for (int k = 0; k < ROWS && top + k < JO_MENU_ITEMS_LCD2; k++) {
        menu_value(c, top + k, val, sizeof val);
        list_row(fb, ROW0 + k * PITCH, 9, top + k == ui->cursor, jo_menu_lcd2[top + k].name, val);
    }
}

static void sub(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    const struct jo_sub *s = &jo_subs[ui->sub];
    title_bar(fb, s->title, NULL);
    for (int i = 0; i < s->n; i++) {
        const struct jo_spec *sp = jo_spec_l(s->rows[i], 1);
        int g = jo_spec_get(s->rows[i], c);
        char val[24];
        if (sp->opts && s->rows[i] == JO_F_LOWCONF)
            snprintf(val, sizeof val, "%s", g ? "STOP" : "RUN");
        else
            jo_edit_text(sp, g, val, sizeof val);
        list_row(fb, 18 + i * 12, 11, i == ui->ocur, sp->title, val);
    }
}

static void info(uint8_t *fb, const struct jo_ctx *c)
{
    char rows[3][40];
    int n;
    title_bar(fb, "BATTERY", NULL);
    if (!c->fitted) {
        snprintf(rows[0], sizeof rows[0], "NO CELL FITTED");
        snprintf(rows[1], sizeof rows[1], "POWER %s", c->usb ? "USB" : "NONE");
        n = 2;
    } else {
        char v[16];
        double volt = 3.3 + 0.9 * c->batt / 100;
        jo_fixed(volt, 2, v, sizeof v);
        snprintf(rows[0], sizeof rows[0], "LEVEL %d%%", c->batt);
        snprintf(rows[1], sizeof rows[1], "VOLT %sV EST", v);
        snprintf(rows[2], sizeof rows[2], "%s", c->usb ? (c->batt < 100 ? "CHARGING" : "FULL") : "DISCHARGING");
        n = 3;
        bar(fb, 4, 92, W - 8, c->batt / 100.0);
    }
    for (int i = 0; i < n; i++)
        jo_text(fb, rows[i], 4, 18 + i * 12, 1, 1);
}

static void edit(uint8_t *fb, const struct jo_ui *ui)
{
    const struct jo_spec *sp = jo_spec_l(ui->edit.field, 1);
    const char *h1, *h2;
    char s[24];
    int sc, w, h, x;
    title_bar(fb, sp->title, NULL);
    jo_edit_text(sp, ui->edit.val, s, sizeof s);
    sc = jo_text_w(s, 2) + 16 <= W - 4 ? 2 : 1;       /* the value in the biggest of two sizes that fits */
    w = jo_text_w(s, sc) + 16;
    h = 7 * sc + 8;
    x = jo_iround((double)(W - w) / 2.0);
    jo_rect(fb, x, 24, w, h, 1);
    jo_text(fb, s, x + 8, 28, sc, 0);
    jo_help_of(sp, ui->edit.val, &h1, &h2);
    jo_text_c(fb, h1, 70, 1, 1);
    jo_text_c(fb, h2, 82, 1, 1);
}

/* the read-only SIGNAL row under the NETWORK rows: the dBm (the best jank peer, or the router in Wi-Fi mode) and a word coloured by its quality */
static void signal_row(uint8_t *fb, int y, const struct jo_ctx *c)
{
    static const char *const WORDS[4] = {"NONE", "POOR", "FAIR", "GOOD"};
    static const uint8_t INKS[4] = {JO_INK_DIM, JO_INK_BAD, JO_INK_WARN, JO_INK_GOOD};
    enum quality q = signal_quality(c);
    int wx = W - 4 - jo_text_w(WORDS[q], 1);
    char dbm[16];
    jo_text(fb, "SIGNAL", 4, y, 1, 1);
    if (!c->has_rssi) {
        jo_text_r(fb, "-", W - 4, y, 1, 1);
        return;
    }
    ink_text(fb, WORDS[q], wx, y, INKS[q]);
    if (c->rssi != 0) {
        snprintf(dbm, sizeof dbm, "%ddBm", c->rssi);
        jo_text_r(fb, dbm, wx - 6, y, 1, 1);
    }
}

/* a percentage on the right of its row with its ink, or "-" while not known */
static void pct_row(uint8_t *fb, int y, const char *name, int has, int pct, int ink)
{
    char s[8];
    jo_text(fb, name, 4, y, 1, 1);
    if (!has) {
        jo_text_r(fb, "-", W - 4, y, 1, 1);
        return;
    }
    snprintf(s, sizeof s, "%d%%", pct);
    ink_text(fb, s, W - 4 - jo_text_w(s, 1), y, ink);
}

/* MODE JANK's read-only air rows under SIGNAL, from the peers' frames of the last 10 s window */
static void air_rows(uint8_t *fb, int y, const struct jo_ctx *c)
{
    char level[24] = "-";
    if (c->rssi_min && c->rssi_max)
        snprintf(level, sizeof level, "%d/%d dBm", c->rssi_min, c->rssi_max);
    pct_row(fb, y, "LOST", c->has_air_lost, c->air_lost, c->air_lost >= LOST_BAD_PCT ? JO_INK_BAD : c->air_lost >= LOST_WARN_PCT ? JO_INK_WARN : JO_INK_GOOD);
    jo_text(fb, "LEVEL", 4, y + PITCH, 1, 1);
    jo_text_r(fb, level, W - 4, y + PITCH, 1, 1);
    pct_row(fb, y + 2 * PITCH, "RX RETRY", c->has_air_retry, c->air_retry, c->air_retry >= RETRY_BAD_PCT ? JO_INK_BAD : JO_INK_NONE);
}

static void net(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    struct jo_net_row rows[4];
    int n = jo_net_rows(c, rows);
    title_bar(fb, "NETWORK", NULL);
    for (int i = 0; i < n; i++) {
        int y = ROW0 + i * PITCH, sel = i == ui->ncur;
        char val[48];
        jo_fit(rows[i].val, W - 8 - 6 - jo_text_w(rows[i].name, 1), val, sizeof val);
        list_row(fb, y, 9, sel, rows[i].name, val);
        if (sel)
            for (int k = 0; k < 2; k++)
                if (rows[i].help[k] && rows[i].help[k][0])
                    jo_text(fb, rows[i].help[k], 3, 101 + 9 * k, 1, 1);
    }
    signal_row(fb, ROW0 + n * PITCH, c);
    if (c->net.mode == JO_NET_JANK)
        air_rows(fb, ROW0 + (n + 1) * PITCH, c);
}

static void scan(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    char right[12] = "";
    int len = c->net.nscan, rows = ROWS - 1, top;
    if (len)
        snprintf(right, sizeof right, "%d/%d", ui->scur + 1, len);
    title_bar(fb, "NETWORK", right);
    if (!len) {
        jo_text_c(fb, "NO NETWORKS", 56, 1, 1);
        return;
    }
    top = jo_clampi(ui->scur - rows / 2, 0, len > rows ? len - rows : 0);
    for (int k = 0; k < rows && top + k < len; k++) {
        int i = top + k, y = ROW0 + k * PITCH, sel = i == ui->scur, v = !sel;
        const struct jo_scan_row *r = &c->net.scan[i];
        char s[JO_SSID_MAX + 2];
        if (sel)
            jo_rect(fb, 0, y - 1, W, 9, 1);
        s[0] = strcmp(r->name, c->net.ssid) ? ' ' : '*';
        jo_fit(r->name, W - 40, s + 1, sizeof s - 1);
        jo_text(fb, s, 3, y, 1, v);
        for (int b = 0; b < 3; b++) {
            int h = 2 + 2 * b;
            if (b < r->bars)
                jo_rect(fb, W - 16 + 4 * b, y + 7 - h, 3, h, v);
            else
                jo_px(fb, W - 15 + 4 * b, y + 6, v);
        }
    }
}

static void text_screen(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    const struct jo_text *t = &ui->txt;
    int n = t->len, cells = 25, start = t->pos > cells - 1 ? t->pos - (cells - 1) : 0;
    char len[8];
    snprintf(len, sizeof len, "%d", n);
    title_bar(fb, t->field == JO_F_APNAME ? "AP NAME" : "PASSKEY", len);
    for (int k = 0; k < cells; k++) {
        int i = start + k, x = 5 + k * 6, sel = i == t->pos;
        char real[2] = {i < n ? t->text[i] : ' ', 0};
        if (i > n)
            break;
        if (sel) {
            jo_rect(fb, x - 1, 17, 7, 11, 1);
            jo_text(fb, real, x, 19, 1, 0);
        } else
            jo_text(fb, t->mask && i < n ? "*" : real, x, 19, 1, 1);
    }
    jo_text_c(fb, "YES NEXT  NO DEL", 46, 1, 1);
    jo_text_c(fb, "easier: web UI", 58, 1, 1);
    jo_text_c(fb, c->net.mode == JO_NET_AP ? "192.168.4.1" : "jank.local", 70, 1, 1);
}

static void net_info(uint8_t *fb, const struct jo_ctx *c)
{
    const struct jo_net *n = &c->net;
    char rows[3][JO_TEXT_MAX + 16];
    title_bar(fb, n->mode == JO_NET_CLIENT ? "LINK CLIENT" : "LINK AP", NULL);
    if (n->mode == JO_NET_CLIENT) {
        char s[24];                                   /* W - 36 px is 20 characters */
        jo_fit(n->ssid[0] ? n->ssid : "-", W - 36, s, sizeof s);
        snprintf(rows[0], sizeof rows[0], "STATE %s", n->status);
        snprintf(rows[1], sizeof rows[1], "IP %s", n->ip);
        snprintf(rows[2], sizeof rows[2], "SSID %s", s);
    } else {
        snprintf(rows[0], sizeof rows[0], "IP 192.168.4.1");
        snprintf(rows[1], sizeof rows[1], "CLIENTS %d", n->ap_clients);
        snprintf(rows[2], sizeof rows[2], "NAME %s", n->ap_name);
    }
    for (int i = 0; i < 3; i++)
        jo_text(fb, rows[i], 4, 18 + i * 12, 1, 1);
}

static void confirm(uint8_t *fb, const char *title, const char *l1, const char *l2)
{
    title_bar(fb, title, NULL);
    jo_text_c(fb, l1, 34, 1, 1);
    jo_text_c(fb, l2, 46, 1, 1);
    jo_text_c(fb, "YES      NO", 84, 2, 1);
}

/* setup: the hotspot's QR code on a lit box (dark modules, so it scans), at the biggest module size that leaves the text its room */
static void setup(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    static const int SIZES[3] = {4, 3, 2};
    int qsize = c->qr ? c->qr->size : 0, text_min = jo_text_w("192.168.4.1", 1), pad = 3, sc = 2, box, tx;
    char a[12], b[JO_SSID_MAX + 1];
    size_t n = strlen(c->net.setup_name);
    if (ui->leave) {
        confirm(fb, "SETUP", "LEAVE SETUP?", "RUN AGAIN");
        return;
    }
    for (int i = 0; i < 3; i++)
        if (SIZES[i] * qsize + 2 * pad <= H - 2 && SIZES[i] * qsize + 2 * pad + 3 + text_min <= W) {
            sc = SIZES[i];
            break;
        }
    box = sc * qsize + 2 * pad;
    jo_rect(fb, 0, 1, box, box, 1);
    for (int y = 0; y < qsize; y++)
        for (int x = 0; x < qsize; x++)
            if (jo_qr_dark(c->qr, x, y))
                jo_rect(fb, pad + sc * x, 1 + pad + sc * y, sc, sc, 0);
    tx = box + 3;
    snprintf(a, sizeof a, "%.11s", c->net.setup_name);
    snprintf(b, sizeof b, "%s", n > 11 ? c->net.setup_name + 11 : "");
    jo_text(fb, "SETUP MODE", tx, 4, 1, 1);
    jo_text(fb, a, tx, 18, 1, 1);
    jo_text(fb, b, tx, 28, 1, 1);
    jo_text(fb, "open", tx, 46, 1, 1);
    jo_text(fb, "192.168.4.1", tx, 58, 1, 1);
    jo_text(fb, "NO: LEAVE", tx, 100, 1, 1);
}

#define OV_X 15
#define OV_Y 30
#define OV_W 130
#define OV_H 60

static void overlays(uint8_t *fb, const struct jo_ui *ui, uint32_t now)
{
    static const char *const HINTS[2][3] = {{"NUDGE BEAT", "UP: 1/2 BEAT EARLY", "DOWN: 1/2 BEAT LATE"}, {"NUDGE MS", "UP: 1 MS LATER", "DOWN: 1 MS EARLIER"}};
    if (ui->hint == JO_NO || ui->hint == JO_YES) {    /* NO / YES held on HOME: what UP / DOWN will do */
        const char *const *h = HINTS[ui->hint == JO_YES];
        boxed(fb, OV_X, OV_Y, OV_W, OV_H);
        jo_untag(OV_X, OV_Y, OV_W, OV_H);
        jo_text_c(fb, h[0], 38, 1, 0);
        jo_text_c(fb, h[1], 55, 1, 0);
        jo_text_c(fb, h[2], 69, 1, 0);
        return;
    }
    if (ui->overlay.on && (int32_t)(now - ui->overlay.until) < 0) {
        const char *v = ui->overlay.value;
        int sc = 1;
        for (int k = 4; k >= 1; k--)
            if (jo_text_w(v, k) <= OV_W - 12) {
                sc = k;
                break;
            }
        boxed(fb, OV_X, OV_Y, OV_W, OV_H);
        jo_untag(OV_X, OV_Y, OV_W, OV_H);
        jo_text_c(fb, ui->overlay.title, 38, 1, 0);
        jo_text_c(fb, v, 66 - jo_iround(7 * sc / 2.0), sc, 0);
    }
}

static void screen(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now)
{
    memset(fb, 0, JO_LCD2_FB_BYTES);
    if (c->boot) {
        jo_text_c(fb, "BOOT", 36, 5, 1);
        jo_text_c(fb, "BOOTLOADER", 90, 1, 1);
        return;
    }
    if (c->splash) {
        jo_text_c(fb, "JANK", 22, 5, 1);
        jo_text_c(fb, c->splash_model ? c->splash_model : "MINI 2.2", 66, 3, 1);
        jo_text_c(fb, c->splash_fw ? c->splash_fw : "FW EMU", 100, 1, 1);
        return;
    }
    if (c->setup)
        setup(fb, ui, c);
    else
        switch (ui->screen) {
        case JO_HOME: home(fb, c, now); break;
        case JO_NET: net(fb, ui, c); break;
        case JO_SCAN: scan(fb, ui, c); break;
        case JO_TEXT: text_screen(fb, ui, c); break;
        case JO_NETINFO: net_info(fb, c); break;
        case JO_CONFIRM: confirm(fb, "SETUP", "STOP AND START", "SETUP?"); break;
        case JO_MENU: menu(fb, ui, c); break;
        case JO_SUB: sub(fb, ui, c); break;
        case JO_INFO: info(fb, c); break;
        case JO_EDIT: edit(fb, ui); break;
        default: break;
        }
    overlays(fb, ui, now);
    if (c->saved) {
        boxed(fb, 35, 42, 90, 34);
        jo_untag(35, 42, 90, 34);
        jo_text_c(fb, "SAVED", 52, 2, 0);
    }
}

void jo_draw_lcd2(uint8_t fb[JO_LCD2_FB_BYTES], uint8_t ink[JO_LCD2_INK_BYTES], const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now)
{
    jo_gw = JO_LCD2_W;
    jo_gh = JO_LCD2_H;
    jo_ink_begin(fb, ink);
    screen(fb, ui, c, now);
    jo_ink_lit_only();
    jo_ink_end();
    jo_gw = JO_W;
    jo_gh = JO_H;
}
