/* SPDX-License-Identifier: GPL-3.0-only */
/* The OLED UI's screens into a frame buffer: the emulator's draw() and everything it calls, pixel for pixel. */
#include "jank_oled_int.h"
#include <stdio.h>
#include <string.h>

#define NET_ICON_XR 86                  /* the network icon's right edge: a gap before the plug */
#define STATUS_CYCLE_MS 2000u           /* the bottom line's items, 2 s each */
#define ARMED_BLINK_MS 400u
#define METER_MIN_DB (-90.0)
#define SILENCE_DB (-60.0)

/* the HOME top bar's LINK-mode transport icons: IGNORE a crossed circle, FOLLOW an arrow coming in, FOLLOW+LEAD a double arrow */
static const char *const LT_ICON0[] = {"..###..", ".#...##", "#...#.#", "#..#..#", "#.#...#", "##...#.", "..###.."};
static const char *const LT_ICON1[] = {"...#.....", "..##.....", ".#######.", "########.", ".#######.", "..##.....", "...#....."};
static const char *const LT_ICON2[] = {".........", "..#...#..", ".##...##.", "#########", ".##...##.", "..#...#..", "........."};
const char *const *const jo_lt_icons[3] = {LT_ICON0, LT_ICON1, LT_ICON2};
#define LT_ICONS jo_lt_icons
const char *const jo_usb_icon[5] = {".....####..", ".....######", "#########..", ".....######", ".....####.."};
#define USB_ICON jo_usb_icon

struct hint {
    const char *title, *up, *down;
};
static const struct hint HINTS[2] = {{"NUDGE BEAT", "UP: 1/2 BEAT EARLY", "DOWN: 1/2 BEAT LATE"},
                                     {"NUDGE MS", "UP: 1 MS LATER", "DOWN: 1 MS EARLIER"}};

/* fit: the first characters that fit w pixels at 6 a character */
void jo_fit(const char *s, int w, char *out, size_t cap)
{
    size_t n = strlen(s);
    if ((int)n * 6 > w)
        n = w > 0 ? (size_t)(w / 6) : 0;
    if (n >= cap)
        n = cap - 1;
    memcpy(out, s, n);
    out[n] = 0;
}

static void title_bar(uint8_t *fb, const char *title, const char *right)
{
    jo_rect(fb, 0, 0, JO_W, 9, 1);
    jo_text(fb, title, 2, 1, 1, 0);
    if (right && *right)
        jo_text_r(fb, right, JO_W - 2, 1, 1, 0);
}

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

/* a full-width 11 px bar with its value written inside, XOR-ed so it reads over the fill and over the empty part */
static void value_bar(uint8_t *fb, int y, double frac, double tick_frac, const char *label)
{
    uint8_t tmp[JO_FB_BYTES];
    jo_rect(fb, 3, y + 1, jo_iround(122 * jo_clampd(frac, 0.0, 1.0)), 9, 1);
    jo_xor_col(fb, 2 + jo_iround(123 * tick_frac), y - 1, y + 11);
    memset(tmp, 0, sizeof tmp);
    jo_text(tmp, label, 5, y + 2, 1, 1);
    for (size_t i = 0; i < sizeof tmp; i++)
        fb[i] ^= tmp[i];
}

/* the placeholder jank glyph, 11 x 8: three linked dots */
void jo_jank_glyph(uint8_t *fb, int x, int y)
{
    static const int8_t R[3][4] = {{0, 5, 3, 3}, {4, 0, 3, 3}, {8, 5, 3, 3}};
    static const int8_t P[9][2] = {{3, 4}, {3, 3}, {7, 3}, {7, 4}, {3, 6}, {4, 6}, {5, 6}, {6, 6}, {7, 6}};
    for (int i = 0; i < 3; i++)
        jo_rect(fb, x + R[i][0], y + R[i][1], R[i][2], R[i][3], 1);
    for (int i = 0; i < 9; i++)
        jo_px(fb, x + P[i][0], y + P[i][1], 1);
}

/* JANK glyph, Wi-Fi bars (CLIENT), bars + AP (AP); crossed out without signal */
static void net_icon(uint8_t *fb, const struct jo_ctx *c, int xr)
{
    int m = c->net.mode, bars = c->wifi == JO_WIFI_GOOD ? 3 : c->wifi == JO_WIFI_WEAK ? 1 : 0, left = xr - 10;
    if (m == JO_NET_JANK)
        jo_jank_glyph(fb, xr - 10, 0);
    else {
        int bx = m == JO_NET_AP ? xr - 20 : xr - 7;
        for (int i = 0; i < 3; i++) {
            int h = 2 + 2 * i;
            if (i < bars)
                jo_rect(fb, bx + 3 * i, 8 - h, 2, h, 1);
            else
                jo_rect(fb, bx + 3 * i, 7, 2, 1, 1);
        }
        if (m == JO_NET_AP)
            jo_text(fb, "AP", xr - 10, 0, 1, 1);
        left = bx;
    }
    if (c->wifi == JO_WIFI_NONE)
        for (int i = 0; i < 9; i++)
            jo_xor_px(fb, left + i + (m == JO_NET_JANK ? 1 : 0), 8 - i);
}

static void power(uint8_t *fb, const struct jo_ctx *c)
{
    net_icon(fb, c, NET_ICON_XR);
    if (c->usb)
        jo_icon(fb, USB_ICON, 5, 92, 1);              /* a plug on its cable */
    if (c->fitted) {
        jo_frame(fb, 107, 0, 15, 7, 1);
        jo_rect(fb, 122, 2, 2, 3, 1);
        jo_rect(fb, 109, 2, jo_iround(jo_clampd(c->batt, 0, 100) / 100.0 * 11), 3, 1);
    }
}

struct item {
    char t[24];
    uint8_t bad;
};

/* Wi-Fi quality as Optimist's Wi-Fi page shows it; a BAD value is drawn inverted */
static int wifi_items(const struct jo_wifiq *q, struct item *it)
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

/* the bottom status line: one item at a time, cycling */
static void status_line(uint8_t *fb, const struct jo_ctx *c, uint32_t now, int y)
{
    struct item items[4];
    int n = 0;
    const struct item *it;
    memset(items, 0, sizeof items);
    if (c->peers > 0) {
        if (c->peers == 1)
            snprintf(items[n].t, sizeof items[n].t, "1 LINK PEER");
        else
            snprintf(items[n].t, sizeof items[n].t, "%d LINK PEERS", c->peers);
        n++;
    }
    if (c->has_wifiq)
        n += wifi_items(&c->wifiq, &items[n]);
    if (!n)
        return;
    it = &items[(now / STATUS_CYCLE_MS) % (uint32_t)n];
    if (it->bad) {
        jo_rect(fb, 0, y - 1, jo_text_w(it->t, 1) + 4, 9, 1);
        jo_text(fb, it->t, 2, y, 1, 0);
    } else
        jo_text(fb, it->t, 2, y, 1, 1);
}

/* the current beat's box is filled for the whole beat; no beat: all empty */
static void beat_boxes(uint8_t *fb, const struct jo_ctx *c, int x0, int y, int w, int h, int gap)
{
    for (int i = 0; i < 4; i++) {
        int x = x0 + i * (w + gap), on = c->has_beat && ((c->beat_n % 4) + 4) % 4 == i;
        if (on)
            jo_rect(fb, x, y, w, h, 1);
        else
            jo_frame(fb, x, y, w, h, 1);
    }
}

/* AUTO BAR under the AUDIO boxes: LOCKED lights under the 1's box, the confidence line grows, MANUAL writes an M */
static void bar_marks(uint8_t *fb, const struct jo_ctx *c, int x0, int y, int w, int h, int gap)
{
    int n;
    if (!c->has_bar || c->bar_state == JO_BAR_OFF)
        return;
    if (c->bar_state == JO_BAR_MANUAL) {
        jo_text(fb, "M", x0, y + h + 3, 1, 1);
        return;
    }
    if (c->bar_state == JO_BAR_LOCKED)
        jo_rect(fb, x0, y + h + 1, w, 2, 1);
    n = jo_iround((4 * (w + gap) - gap) * jo_clampd(c->bar_conf, 0.0, 1.0));
    if (n > 0)
        jo_rect(fb, x0, y + h + 4, n, 1, 1);
}

static void home_top(uint8_t *fb, const struct jo_ctx *c, uint32_t now)
{
    static const int NAME_RIGHT[3] = {NET_ICON_XR - 10, NET_ICON_XR - 7, NET_ICON_XR - 20};
    int stopped = !c->running, tri = !stopped || (c->armed && (now / ARMED_BLINK_MS) % 2 == 0);
    const char *const *lt = c->link && c->lt_mode < 3 ? LT_ICONS[c->lt_mode] : NULL;
    int name_left = lt ? 12 + (int)strlen(lt[0]) + 4 : 10;
    power(fb, c);
    if (tri)
        for (int r = 0; r < 7; r++)
            jo_rect(fb, 2, r, (r < 6 - r ? r : 6 - r) + 1, 1, 1);
    else if (!c->armed)
        jo_rect(fb, 2, 1, 5, 5, 1);
    if (lt)
        jo_icon(fb, lt, 7, 12, 0);
    if (c->net.mode < 3) {
        char name[JO_TEXT_MAX + 16], shown[JO_TEXT_MAX + 16];
        int right = NAME_RIGHT[c->net.mode];
        if (c->net.mode == JO_NET_JANK)
            snprintf(name, sizeof name, "JANK GRP %u", c->net.group);
        else
            snprintf(name, sizeof name, "%s", c->net.mode == JO_NET_CLIENT ? c->net.ssid : c->net.ap_name);
        if (name[0]) {
            jo_fit(name, right - 2 - name_left, shown, sizeof shown);
            jo_text_r(fb, shown, right - 2, 0, 1, 1);
        }
    }
}

static void home(uint8_t *fb, const struct jo_ctx *c, uint32_t now)
{
    char s[24];
    home_top(fb, c, now);
    if (!c->line_in) {                                /* LINK without a line in: big and plain */
        if (c->has_link_bpm)
            jo_fixed(c->link_bpm, 1, s, sizeof s);
        else
            snprintf(s, sizeof s, "---");
        jo_text(fb, s, 2, 9, 4, 1);
        beat_boxes(fb, c, 2, 41, 30, 12, 2);
        status_line(fb, c, now, 57);
        return;
    }
    if (c->has_bpm && c->bpm != 0.0)
        jo_fixed(c->bpm, 1, s, sizeof s);
    else
        snprintf(s, sizeof s, "---");
    jo_text(fb, s, 2, 9, 3, 1);
    beat_boxes(fb, c, 87, 9, 9, 9, 1);
    bar_marks(fb, c, 87, 9, 9, 9, 1);
    snprintf(s, sizeof s, "IN %ddB", jo_iround(c->level_db));
    value_bar(fb, 31, (c->level_db - METER_MIN_DB) / -METER_MIN_DB, (SILENCE_DB - METER_MIN_DB) / -METER_MIN_DB, s);
    jo_xor_col(fb, 3 + jo_iround(121 * jo_clampd((c->level_min - METER_MIN_DB) / -METER_MIN_DB, 0.0, 1.0)), 32, 41);
    jo_xor_col(fb, 3 + jo_iround(121 * jo_clampd((c->level_max - METER_MIN_DB) / -METER_MIN_DB, 0.0, 1.0)), 32, 41);
    snprintf(s, sizeof s, "CF %d%%", jo_iround(c->conf * 100));
    value_bar(fb, 44, c->conf, c->th / 100.0, s);
    status_line(fb, c, now, 57);
}

static void menu(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    char right[8];
    snprintf(right, sizeof right, "%d/%d", ui->cursor + 1, JO_MENU_ITEMS);
    title_bar(fb, "MENU", right);
    for (int k = 0; k < JO_VISIBLE; k++) {
        int i = ui->top + k, y = 11 + k * 9, sel = i == ui->cursor, v = !sel;
        char val[12] = "";
        if (i >= JO_MENU_ITEMS)
            break;
        switch (i) {
        case 1: snprintf(val, sizeof val, "%s", c->lt_mode < 3 ? jo_lt_short[c->lt_mode] : ""); break;
        case 2: snprintf(val, sizeof val, "%s", c->auto_bar ? "ON" : "OFF"); break;
        case 3: snprintf(val, sizeof val, "%s", c->net.mode < 4 ? jo_net_short[c->net.mode] : ""); break;
        case 4: snprintf(val, sizeof val, "%s", c->usb_role < 2 ? jo_usb_names[c->usb_role] : ""); break;
        case 6: snprintf(val, sizeof val, "%s", c->rate < 5 ? jo_rates_short[c->rate] : ""); break;
        case 8:
            if (c->fitted)
                snprintf(val, sizeof val, "%d%%", c->batt);
            else
                snprintf(val, sizeof val, "-");
            break;
        default: break;
        }
        if (sel)
            jo_rect(fb, 0, y - 1, JO_W, 9, 1);
        jo_text(fb, jo_menu[i].name, 4, y, 1, v);
        jo_text_r(fb, val, JO_W - 4, y, 1, v);
    }
}

static void sub(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    const struct jo_sub *s = &jo_subs[ui->sub];
    title_bar(fb, s->title, NULL);
    for (int i = 0; i < s->n; i++) {
        int y = 14 + i * 11, sel = i == ui->ocur, v = !sel, g = jo_spec_get(s->rows[i], c);
        const struct jo_spec *sp = jo_spec(s->rows[i]);
        char val[24];
        if (sp->opts && s->rows[i] == JO_F_LOWCONF)
            snprintf(val, sizeof val, "%s", g ? "STOP" : "RUN");
        else
            jo_edit_text(sp, g, val, sizeof val);
        if (sel)
            jo_rect(fb, 0, y - 2, JO_W, 11, 1);
        jo_text(fb, sp->title, 4, y, 1, v);
        jo_text_r(fb, val, JO_W - 4, y, 1, v);
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
        double volt = 3.3 + 0.9 * c->batt / 100;      /* rough: 3.3 V empty, 4.2 V full */
        jo_fixed(volt, 2, v, sizeof v);
        snprintf(rows[0], sizeof rows[0], "LEVEL %d%%", c->batt);
        snprintf(rows[1], sizeof rows[1], "VOLT %sV EST", v);
        snprintf(rows[2], sizeof rows[2], "%s", c->usb ? (c->batt < 100 ? "CHARGING" : "FULL") : "DISCHARGING");
        n = 3;
        bar(fb, 4, 50, 120, c->batt / 100.0);
    }
    for (int i = 0; i < n; i++)
        jo_text(fb, rows[i], 4, 14 + i * 11, 1, 1);
}

static void edit(uint8_t *fb, const struct jo_ui *ui)
{
    const struct jo_spec *sp = jo_spec(ui->edit.field);
    const char *h1, *h2;
    char s[24];
    int w, x;
    title_bar(fb, sp->title, NULL);
    jo_edit_text(sp, ui->edit.val, s, sizeof s);      /* the menus' one font size, always */
    w = jo_text_w(s, 1) + 12;
    x = jo_iround((double)(JO_W - w) / 2.0);
    jo_rect(fb, x, 16, w, 13, 1);
    jo_text(fb, s, x + 6, 19, 1, 0);                 /* the value, inverted */
    jo_help_of(sp, ui->edit.val, &h1, &h2);
    jo_text_c(fb, h1, 44, 1, 1);
    jo_text_c(fb, h2, 54, 1, 1);
}

static void net(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    struct jo_net_row rows[4];
    int n = jo_net_rows(c, rows);
    title_bar(fb, "NETWORK", NULL);
    for (int i = 0; i < n; i++) {
        int y = 11 + i * 9, sel = i == ui->ncur, v = !sel;
        char val[48];
        if (sel)
            jo_rect(fb, 0, y - 1, JO_W, 9, 1);
        jo_text(fb, rows[i].name, 4, y, 1, v);
        jo_fit(rows[i].val, 124 - 8 - jo_text_w(rows[i].name, 1), val, sizeof val);
        jo_text_r(fb, val, JO_W - 4, y, 1, v);
        if (sel)
            for (int k = 0; k < 2; k++)
                if (rows[i].help[k] && rows[i].help[k][0])
                    jo_text(fb, rows[i].help[k], 2, 49 + 8 * k, 1, 1);
    }
}

static void scan(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    char right[12] = "";
    int n = c->net.nscan;
    if (n)
        snprintf(right, sizeof right, "%d/%d", ui->scur + 1, n);
    title_bar(fb, "NETWORK", right);
    if (!n) {
        jo_text_c(fb, "NO NETWORKS", 28, 1, 1);
        return;
    }
    for (int k = 0; k < 5; k++) {
        int i = ui->stop + k, y = 11 + k * 10, sel = i == ui->scur, v = !sel;
        const struct jo_scan_row *r;
        char s[JO_SSID_MAX + 2];
        if (i >= n)
            break;
        r = &c->net.scan[i];
        if (sel)
            jo_rect(fb, 0, y - 1, JO_W, 10, 1);
        s[0] = strcmp(r->name, c->net.ssid) ? ' ' : '*';
        jo_fit(r->name, 90, s + 1, sizeof s - 1);
        jo_text(fb, s, 2, y, 1, v);
        for (int b = 0; b < 3; b++) {
            int h = 2 + 2 * b;
            if (b < r->bars)
                jo_rect(fb, 112 + 4 * b, y + 7 - h, 3, h, v);
            else
                jo_px(fb, 113 + 4 * b, y + 6, v);
        }
    }
}

static void text_screen(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    const struct jo_text *t = &ui->txt;
    int n = t->len, start = t->pos > 18 ? t->pos - 18 : 0;   /* the menus' one font size: 19 cells of 6 px */
    char len[8];
    snprintf(len, sizeof len, "%d", n);
    title_bar(fb, t->field == JO_F_APNAME ? "AP NAME" : "PASSKEY", len);
    for (int k = 0; k < 19; k++) {
        int i = start + k, x = 7 + k * 6, sel = i == t->pos;
        char real[2] = {i < n ? t->text[i] : ' ', 0};
        if (i > n)
            break;
        if (sel) {
            jo_rect(fb, x - 1, 15, 7, 11, 1);
            jo_text(fb, real, x, 17, 1, 0);
        } else
            jo_text(fb, t->mask && i < n ? "*" : real, x, 17, 1, 1);
    }
    jo_text_c(fb, "YES NEXT  NO DEL", 33, 1, 1);
    jo_text_c(fb, "easier: web UI", 43, 1, 1);
    jo_text_c(fb, c->net.mode == JO_NET_AP ? "192.168.4.1" : "jank.local", 53, 1, 1);
}

static void net_info(uint8_t *fb, const struct jo_ctx *c)
{
    const struct jo_net *n = &c->net;
    char rows[3][JO_TEXT_MAX + 16];
    title_bar(fb, n->mode == JO_NET_CLIENT ? "LINK CLIENT" : "LINK AP", NULL);
    if (n->mode == JO_NET_CLIENT) {
        char s[24];
        jo_fit(n->ssid[0] ? n->ssid : "-", 100, s, sizeof s);
        snprintf(rows[0], sizeof rows[0], "STATE %s", n->status);
        snprintf(rows[1], sizeof rows[1], "IP %s", n->ip);
        snprintf(rows[2], sizeof rows[2], "SSID %s", s);
    } else {
        snprintf(rows[0], sizeof rows[0], "IP 192.168.4.1");
        snprintf(rows[1], sizeof rows[1], "CLIENTS %d", n->ap_clients);
        snprintf(rows[2], sizeof rows[2], "NAME %s", n->ap_name);
    }
    for (int i = 0; i < 3; i++)
        jo_text(fb, rows[i], 4, 14 + i * 11, 1, 1);
}

static void confirm(uint8_t *fb, const char *title, const char *l1, const char *l2)
{
    title_bar(fb, title, NULL);
    jo_text_c(fb, l1, 16, 1, 1);
    jo_text_c(fb, l2, 27, 1, 1);
    jo_text_c(fb, "YES      NO", 48, 1, 1);
}

/* SETUP: the open hotspot's QR code (dark modules on a lit box) with the text beside it */
static void setup(uint8_t *fb, const struct jo_ui *ui, const struct jo_ctx *c)
{
    char a[12], b[JO_SSID_MAX + 1];
    size_t n = strlen(c->net.setup_name);
    if (ui->leave) {
        confirm(fb, "SETUP", "LEAVE SETUP?", "RUN AGAIN");
        return;
    }
    jo_rect(fb, 0, 1, 62, 62, 1);
    if (c->qr)
        for (int y = 0; y < c->qr->size; y++)
            for (int x = 0; x < c->qr->size; x++)
                if (jo_qr_dark(c->qr, x, y))
                    jo_rect(fb, 2 + 2 * x, 3 + 2 * y, 2, 2, 0);
    snprintf(a, sizeof a, "%.11s", c->net.setup_name);
    snprintf(b, sizeof b, "%s", n > 11 ? c->net.setup_name + 11 : "");
    jo_text(fb, "SETUP MODE", 63, 3, 1, 1);
    jo_text(fb, a, 63, 15, 1, 1);
    jo_text(fb, b, 63, 24, 1, 1);
    jo_text(fb, "open", 63, 37, 1, 1);
    jo_text(fb, "192.168.4.1", 63, 46, 1, 1);
    jo_text(fb, "NO: LEAVE", 63, 56, 1, 1);
}

static void overlays(uint8_t *fb, const struct jo_ui *ui, uint32_t now)
{
    if (ui->hint == JO_NO || ui->hint == JO_YES) {    /* NO / YES held on HOME: what UP / DOWN will do */
        const struct hint *h = &HINTS[ui->hint == JO_YES];
        boxed(fb, 6, 12, 116, 40);
        jo_text_c(fb, h->title, 17, 1, 0);
        jo_text_c(fb, h->up, 31, 1, 0);
        jo_text_c(fb, h->down, 42, 1, 0);
        return;
    }
    if (ui->overlay.on && (int32_t)(now - ui->overlay.until) < 0) {
        const char *v = ui->overlay.value;
        int sc = jo_text_w(v, 3) <= 96 ? 3 : jo_text_w(v, 2) <= 96 ? 2 : 1;   /* the largest size that fits the box */
        boxed(fb, 14, 12, 100, 40);
        jo_text_c(fb, ui->overlay.title, 17, 1, 0);
        jo_text_c(fb, v, 38 - jo_iround(7 * sc / 2.0), sc, 0);
    }
}

void jo_draw(uint8_t fb[JO_FB_BYTES], const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now)
{
    memset(fb, 0, JO_FB_BYTES);
    if (c->boot) {
        jo_text_c(fb, "BOOT", 14, 4, 1);
        jo_text_c(fb, "BOOTLOADER", 48, 1, 1);
        return;
    }
    if (c->splash) {
        jo_text_c(fb, "JANK", 8, 3, 1);
        jo_text_c(fb, c->splash_model ? c->splash_model : "MINI 2.2", 34, 2, 1);
        jo_text_c(fb, c->splash_fw ? c->splash_fw : "FW EMU", 54, 1, 1);
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
        boxed(fb, 30, 22, 68, 22);
        jo_text_c(fb, "SAVED", 29, 2, 0);
    }
}
