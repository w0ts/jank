/* SPDX-License-Identifier: GPL-3.0-only */
/* The lcd2 layout of the UI (ui/oled.js, ctx.layout 'lcd2'; the Waveshare ESP32-S3-LCD-2): its own logical grid of 160 x 120 pixels
 * (4:3, like the panel), the same 5 x 7 font and primitives at integer scales, painted exactly x2 on 320 x 240 (core/jank_lcd.c). A port,
 * function by function, of ui/oled.js's "LCD2 LAYOUT" section; tests/test_oled.c compares it with that file's own frames and ink planes
 * (tests/oled_ref/cases_ink.txt). The OLED never runs this code. Colour is information only (the ink plane): menus, edit screens and
 * overlays have none. Not touched by the stage that gave the OLED its numbers: the OLED UI's draw is jank_oled_draw.c. */
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
#define BATT_BAD_PCT 15
#define BATT_WARN_PCT 30

/* the HOME geometry: ui/oled.js's L2 table */
#define TOP_H 13
#define LEFT 14
#define NAME_RIGHT 66
#define LINK_X 70
#define SIGNAL_X 98
#define PLUG_X 119
#define BATT_X 134
#define BATT_W 21
#define TEMPO_Y 15
#define TEMPO_SCALE 5
#define BPM_X 138
#define BPM_Y 43
#define BOX_X 2
#define BOX_W 36
#define BOX_GAP 4
#define BOX_Y 52
#define BOX_H 26
#define LINK_BOX_Y 54
#define LINK_BOX_H 52
#define ROW_CF 81
#define ROW_IN 91
#define ROW_BAR 101
#define LABEL_X 2
#define VALUE_XR 60
#define BAR_X 64
#define BAR_W 94
#define LINE_H 3
#define STATUS_Y 111

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

/* ---- policy: signal quality, link state, battery, level, confidence (colour = information) ---- */
static int signal_bars(const struct jo_ctx *c)
{
    if (!c->has_rssi || c->rssi == 0)
        return 0;
    return c->rssi >= SIGNAL_GOOD_DBM ? 3 : c->rssi >= SIGNAL_POOR_DBM ? 2 : 1;
}

static int link_ink(const struct jo_ctx *c)
{
    return c->peers >= LINK_JOINED_MIN ? JO_INK_GOOD : c->link ? JO_INK_BAD : JO_INK_DIM;
}

static int batt_ink(int pct) { return pct < BATT_BAD_PCT ? JO_INK_BAD : pct < BATT_WARN_PCT ? JO_INK_WARN : JO_INK_GOOD; }

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

/* ---- HOME ---- */
static void top_bar(uint8_t *fb, const struct jo_ctx *c, uint32_t now)
{
    static const int8_t BARS_INK[4] = {JO_INK_DIM, JO_INK_BAD, JO_INK_WARN, JO_INK_GOOD};
    int tri = c->running || (c->armed && (now / ARMED_BLINK_MS) % 2 == 0), bars = signal_bars(c), name_left = LEFT;
    const char *const *lt = c->link && c->lt_mode < 3 ? jo_lt_icons[c->lt_mode] : NULL;
    const char *fl = !c->link ? fold_label(c->fold_exp) : "";
    char full[JO_TEXT_MAX + 16], shown[JO_TEXT_MAX + 16];
    if (tri)
        for (int r = 0; r < 9; r++)
            jo_rect(fb, 2, 2 + r, (16 * ((r < 8 - r ? r : 8 - r) + 1) + 9) / 10, 1, 1);     /* ceil((min(r, 8 - r) + 1) * 1.6) */
    else if (!c->armed)
        jo_rect(fb, 2, 3, 7, 7, 1);
    if (lt) {
        jo_icon(fb, lt, 7, LEFT, 3);
        name_left = LEFT + (int)strlen(lt[0]) + 4;
    } else if (fl[0]) {
        jo_text(fb, fl, LEFT, 3, 1, 1);
        name_left = LEFT + jo_text_w(fl, 1) + 4;
    }
    full[0] = 0;
    if (c->net.mode == JO_NET_JANK)
        snprintf(full, sizeof full, "GRP %u", c->net.group);
    else if (c->net.mode == JO_NET_CLIENT)
        snprintf(full, sizeof full, "%s", c->net.ssid);
    else if (c->net.mode == JO_NET_AP)
        snprintf(full, sizeof full, "%s", c->net.ap_name);
    if (full[0]) {
        jo_fit(full, NAME_RIGHT - name_left, shown, sizeof shown);
        jo_text_r(fb, shown, NAME_RIGHT, 3, 1, 1);
    }
    jo_jank_glyph(fb, LINK_X, 2);                                   /* the link glyph, and the Link peers beside it */
    if (c->peers > 0) {
        char n[12];
        snprintf(n, sizeof n, "%d", c->peers);
        jo_text(fb, n, LINK_X + 12, 3, 1, 1);
    }
    tag(link_ink(c), LINK_X, 0, 24, TOP_H);
    for (int i = 0; i < 3; i++) {                                   /* the air: three bars, 3 px wide, 3 / 6 / 9 px high */
        int h = 3 * (i + 1);
        if (i < bars)
            jo_rect(fb, SIGNAL_X + 4 * i, 12 - h, 3, h, 1);
        else
            jo_rect(fb, SIGNAL_X + 4 * i, 11, 3, 1, 1);
    }
    tag(BARS_INK[bars], SIGNAL_X, 0, 11, TOP_H);
    if (c->usb)
        jo_icon(fb, jo_usb_icon, 5, PLUG_X, 4);
    if (c->fitted) {
        jo_frame(fb, BATT_X, 2, BATT_W, 9, 1);
        jo_rect(fb, BATT_X + BATT_W, 4, 2, 5, 1);
        jo_rect(fb, BATT_X + 2, 4, jo_iround(17.0 * jo_clampd(c->batt, 0, 100) / 100.0), 5, 1);
        tag(batt_ink(c->batt), BATT_X, 2, BATT_W + 2, 9);
    }
}

/* a small progress row: label, the value right-aligned, and ONE thin line (3 px high, no outline, no track): its length is the value on
 * 94 px. A notch, 1 px wide and 5 px tall, is XOR-ed through the line at tick when it is >= 0 (a gap in the line where it passes, a stub
 * above and below where it does not). The value, the line and the notch carry ink; the label stays neutral. */
static void progress_row(uint8_t *fb, int y, const char *label, const char *value, double frac, double tick, int has_tick, int ink)
{
    int vw = jo_text_w(value, 1), n = jo_iround(BAR_W * jo_clampd(frac, 0.0, 1.0));
    jo_text(fb, label, LABEL_X, y, 1, 1);
    jo_text(fb, value, VALUE_XR - vw, y, 1, 1);
    tag(ink, VALUE_XR - vw, y, vw, 7);
    jo_rect(fb, BAR_X, y + 2, n, LINE_H, 1);
    tag(ink, BAR_X, y + 2, n, LINE_H);
    if (has_tick) {
        int x = BAR_X + jo_iround((BAR_W - 1) * tick);
        jo_xor_col(fb, x, y + 1, y + 5);
        tag(ink, x, y + 1, 1, 5);
    }
}

/* the BAR row: an outlined bar (94 x 5, a 3 px fill inside it), neutral, no tick */
static void outlined_row(uint8_t *fb, int y, const char *label, const char *value, double frac)
{
    jo_text(fb, label, LABEL_X, y, 1, 1);
    jo_text(fb, value, VALUE_XR - jo_text_w(value, 1), y, 1, 1);
    jo_frame(fb, BAR_X, y + 1, BAR_W, 5, 1);
    jo_rect(fb, BAR_X + 1, y + 2, jo_iround((BAR_W - 2) * jo_clampd(frac, 0.0, 1.0)), 3, 1);
}

/* the beat indicator (no LEDs): the lit box solid, the others 2 px outlines; the first one is amber only while lit */
static void beat_boxes(uint8_t *fb, const struct jo_ctx *c, int y, int h)
{
    for (int i = 0; i < 4; i++) {
        int x = BOX_X + i * (BOX_W + BOX_GAP), on = c->has_beat && ((c->beat_n % 4) + 4) % 4 == i;
        if (on)
            jo_rect(fb, x, y, BOX_W, h, 1);
        else {
            jo_frame(fb, x, y, BOX_W, h, 1);
            jo_frame(fb, x + 1, y + 1, BOX_W - 2, h - 2, 1);
        }
        if (i == 0 && on)
            tag(JO_INK_ONE, x, y, BOX_W, h);
    }
}

/* the Wi-Fi quality as one line, "-58dBm 24M R3% L0%": a BAD part is drawn inverted */
struct seg {
    char t[16];
    uint8_t bad;
};

static int wifi_segs(const struct jo_wifiq *q, struct seg *sg)
{
    int retry = -1, lost = -1;
    if (!q->rssi) {
        snprintf(sg[0].t, sizeof sg[0].t, "NO SIGNAL");
        sg[0].bad = 1;
        return 1;
    }
    if (q->tx) {
        retry = jo_iround(100.0 * q->retried / q->tx);
        lost = jo_iround(100.0 * q->lost / q->tx);
    }
    snprintf(sg[0].t, sizeof sg[0].t, "%ddBm", q->rssi);
    snprintf(sg[1].t, sizeof sg[1].t, "%dM", jo_iround(q->kbps / 1000.0));
    if (retry < 0)
        snprintf(sg[2].t, sizeof sg[2].t, "R--");
    else
        snprintf(sg[2].t, sizeof sg[2].t, "R%d%%", retry);
    if (lost < 0)
        snprintf(sg[3].t, sizeof sg[3].t, "L--");
    else
        snprintf(sg[3].t, sizeof sg[3].t, "L%d%%", lost);
    sg[0].bad = sg[1].bad = q->rssi < SIGNAL_POOR_DBM;
    sg[2].bad = retry >= 30;
    sg[3].bad = lost >= 10;
    return 4;
}

/* the bottom line: the confidence words on the left (AUDIO), else the Wi-Fi line; the threshold on the right */
static void status_line(uint8_t *fb, const struct jo_ctx *c)
{
    int y = STATUS_Y, coast = !c->conf_hold && !c->conf_unknown && c->running && c->conf * 100 < c->th;
    const char *word = NULL;
    int ink = JO_INK_NONE;
    char s[32];
    if (!c->link) {
        if (c->gated) {
            word = "GATED";
            ink = JO_INK_BAD;
        } else if (c->conf_hold) {
            word = "HOLD";
            ink = JO_INK_DIM;
        } else if (coast) {
            word = "COAST";
            ink = JO_INK_WARN;
        }
    }
    if (word) {
        jo_text(fb, word, 2, y, 1, 1);
        tag(ink, 2, y, jo_text_w(word, 1), 7);
    } else if (c->has_wifiq) {
        struct seg sg[4];
        int n = wifi_segs(&c->wifiq, sg), x = 2;
        for (int i = 0; i < n; i++) {
            int w = jo_text_w(sg[i].t, 1);
            if (sg[i].bad) {
                jo_rect(fb, x - 1, y - 1, w + 2, 9, 1);
                jo_text(fb, sg[i].t, x, y, 1, 0);
                tag(JO_INK_BAD, x - 1, y - 1, w + 2, 9);
            } else
                jo_text(fb, sg[i].t, x, y, 1, 1);
            x += w + 6;
        }
    }
    if (!c->link) {
        snprintf(s, sizeof s, "%s %d", c->conf_stop ? "STOP" : "MIN", c->th);
        jo_text_r(fb, s, W - 3, y, 1, 1);
    }
}

static void home(uint8_t *fb, const struct jo_ctx *c, uint32_t now)
{
    char tempo[24], value[16];
    int has = c->link ? c->has_link_bpm : c->has_bpm;
    double bpm = c->link ? c->link_bpm : c->bpm;
    top_bar(fb, c, now);
    if (has && bpm != 0.0)
        jo_fixed(bpm, 1, tempo, sizeof tempo);
    else
        snprintf(tempo, sizeof tempo, "---");
    jo_text(fb, tempo, 2, TEMPO_Y, TEMPO_SCALE, 1);
    jo_text(fb, "BPM", BPM_X, BPM_Y, 1, 1);
    if (c->link) {
        beat_boxes(fb, c, LINK_BOX_Y, LINK_BOX_H);
        status_line(fb, c);
        return;
    }
    beat_boxes(fb, c, BOX_Y, BOX_H);
    if (c->conf_unknown)
        snprintf(value, sizeof value, "--");
    else
        snprintf(value, sizeof value, "%d%%", jo_iround(c->conf * 100));
    progress_row(fb, ROW_CF, "CF", value, c->conf_unknown ? 0.0 : c->conf, c->th / 100.0, 1, conf_ink(c));
    snprintf(value, sizeof value, "%ddB", jo_iround(c->level_db));
    progress_row(fb, ROW_IN, "IN", value, (c->level_db - METER_MIN_DB) / -METER_MIN_DB, (QUIET_DB - METER_MIN_DB) / -METER_MIN_DB, 1, level_ink(c));
    if (c->has_bar && c->bar_state != JO_BAR_OFF && c->bar_state <= JO_BAR_MANUAL)
        outlined_row(fb, ROW_BAR, "BAR", BAR_WORDS[c->bar_state], c->bar_state == JO_BAR_MANUAL ? 0.0 : c->bar_conf);
    status_line(fb, c);
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
    case 9: v = c->beat_flash ? "SCREEN" : "BOXES"; break;
    case 10:
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
