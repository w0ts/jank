/* SPDX-License-Identifier: GPL-3.0-only */
/* The device's UI as the DN2's text page (jank_oled_text.h): jank_oled_draw.c's screens, line by line, in the same order. Where the
 * OLED draws a picture the page says it in characters: the transport icon as a word, the signal and the bars as '=' and '-' (a notch
 * '|'), the beat boxes as 1 2 3 4 with the beat in brackets. The fits are the OLED's own (its 6 px a character), so a value is cut where
 * the OLED cuts it. */
#include "jank_oled_text.h"
#include "jank_oled_int.h"
#include <stdio.h>
#include <string.h>

#define ROWS (JOT_LINES - 1)              /* a list's rows under its title (the OLED shows 6 menu rows too) */
#define OLED_CW 6                         /* the OLED's character, for its fits */
#define ROW_VAL_PX 116                    /* a NETWORK row: name and value within the OLED's 4..124, 8 px between them */
#define NAME_CHARS 16                     /* a network's name on HOME and LINK CLIENT / AP (the OLED's 100 px) */
#define SCAN_CHARS 15                     /* a scanned network's name (the OLED's 90 px) */
#define TEXT_CELLS JOT_FIT                /* the character editor's window */
#define LEVEL_CELLS 10                    /* HOME's IN and CF bars */
#define BATT_CELLS JOT_FIT                /* BATTERY's bar */
#define METER_MIN_DB (-90.0)              /* HOME's input meter, as jank_oled_draw.c's */
#define SILENCE_DB (-60.0)
#define BOX_LINE 2                        /* the hint, the overlay and SAVED take lines 2..4 */

static const char *const SIGNAL[4] = {"---", "=--", "==-", "==="};                 /* 0..3 bars */
static const char *const BEATS[5] = {"[1] 2 3 4", "1 [2] 3 4", "1 2 [3] 4", "1 2 3 [4]", "1 2 3 4"};
static const char *const BAR_WORDS[4] = {"", "LISTEN", "LOCKED", "MANUAL"};        /* by enum jo_bar_state, as the colour layout's */

/* ---- lines ---- */
static void put(struct jot_page *p, int i, const char *s)
{
    char *d = p->line[i];
    size_t k = 0;
    for (; s && s[k] && k < JOT_LINE_MAX; k++)
        d[k] = (unsigned char)s[k] >= 0x20u && (unsigned char)s[k] <= 0x7eu ? s[k] : '?';
    d[k] = 0;
    p->right[i] = 0;
    p->centred &= (uint8_t)~(1u << i);
}

static void put_c(struct jot_page *p, int i, const char *s)
{
    put(p, i, s);
    if (p->line[i][0])
        p->centred |= (uint8_t)(1u << i);
}

/* the left part of a split line: the DN2 splits at the FIRST double space, so none inside (a run of spaces becomes one) and none at its end */
static size_t clean_left(const char *s, char *out, size_t cap)
{
    size_t k = 0;
    for (; *s && k + 1u < cap; s++)
        if (*s != ' ' || !k || out[k - 1] != ' ')
            out[k++] = *s;
    while (k && out[k - 1] == ' ')
        k--;
    out[k] = 0;
    return k;
}

/* left, and right-aligned on the OLED: two spaces between them in the text, the first double space of the line */
static void put_lr(struct jot_page *p, int i, const char *left, const char *right)
{
    char b[JOT_LINE_MAX + 1], lc[JOT_LINE_MAX + 1];
    size_t l, r, k, rk;
    while (right && *right == ' ')
        right++;
    r = right ? strlen(right) : 0u;
    if (!r) {
        put(p, i, left);
        return;
    }
    l = clean_left(left, lc, sizeof lc);
    if (!l) {
        put(p, i, right);
        return;
    }
    k = l;
    memcpy(b, lc, k);
    for (int s = 0; s < 2 && k < JOT_LINE_MAX; s++)
        b[k++] = ' ';
    rk = r < JOT_LINE_MAX - k ? r : JOT_LINE_MAX - k;
    memcpy(b + k, right, rk);
    b[k + rk] = 0;
    put(p, i, b);
    if (l + 2u < strlen(p->line[i]))
        p->right[i] = (uint8_t)(l + 2u);
}

static void row(struct jot_page *p, int i, int sel, const char *left, const char *right)
{
    put_lr(p, i, left, right);
    if (sel)
        p->sel = (uint8_t)i;
}

static void title(struct jot_page *p, const char *t, const char *right)
{
    p->flags |= JOT_F_TITLE;
    put_lr(p, 0, t, right);
}

/* the first n characters */
static void fit(const char *s, int n, char *out, size_t cap)
{
    snprintf(out, cap, "%.*s", n > 0 ? n : 0, s);
}

/* a bar of cells: '=' filled, '-' not, the notch '|' over the cell at tick (< 0: none) */
static void bar(char *out, int cells, double frac, double tick)
{
    int n = jo_iround(cells * jo_clampd(frac, 0.0, 1.0));
    for (int i = 0; i < cells; i++)
        out[i] = i < n ? '=' : '-';
    if (tick >= 0.0)
        out[jo_iround((cells - 1) * jo_clampd(tick, 0.0, 1.0))] = '|';
    out[cells] = 0;
}

/* ---- HOME ---- */
/* the transport icon: a triangle (the OLED blinks it while armed), the user's square, the machine's pause */
static const char *transport(const struct jo_ctx *c)
{
    if (c->running)
        return "PLAY";
    if (c->armed)
        return "ARMED";
    return c->held ? "STOP" : "PAUSE";
}

/* the top bar: the transport and, in LINK mode, LINK TRANSPORT; the plug and the cell; the network's name and its signal */
static void home_top(struct jot_page *p, const struct jo_ctx *c)
{
    char l[24], r[24] = "", name[JO_TEXT_MAX + 16] = "", shown[NAME_CHARS + 1];
    int bars = c->wifi == JO_WIFI_GOOD ? 3 : c->wifi == JO_WIFI_WEAK ? 1 : 0;
    if (c->link && c->lt_mode < 3)
        snprintf(l, sizeof l, "%s %s", transport(c), jo_lt_short[c->lt_mode]);
    else
        snprintf(l, sizeof l, "%s", transport(c));
    if (c->usb && c->fitted)
        snprintf(r, sizeof r, "USB %d%%", c->batt);
    else if (c->usb)
        snprintf(r, sizeof r, "USB");
    else if (c->fitted)
        snprintf(r, sizeof r, "%d%%", c->batt);
    put_lr(p, 0, l, r);
    if (c->net.mode == JO_NET_JANK)
        snprintf(name, sizeof name, "JANK GRP %u", c->net.group);
    else if (c->net.mode < JO_NET_SETUP)
        snprintf(name, sizeof name, "%s", c->net.mode == JO_NET_CLIENT ? c->net.ssid : c->net.ap_name);
    fit(name, NAME_CHARS, shown, sizeof shown);
    put_lr(p, 1, shown, SIGNAL[bars]);
}

/* AUTO BAR under the AUDIO boxes: its state and how sure it is */
static void bar_marks(struct jot_page *p, const struct jo_ctx *c)
{
    char s[24];
    if (!c->has_bar || c->bar_state == JO_BAR_OFF || c->bar_state > JO_BAR_MANUAL)
        return;
    if (c->bar_state == JO_BAR_MANUAL)
        snprintf(s, sizeof s, "BAR %s", BAR_WORDS[JO_BAR_MANUAL]);
    else
        snprintf(s, sizeof s, "BAR %s %d%%", BAR_WORDS[c->bar_state], jo_iround(100.0 * jo_clampd(c->bar_conf, 0.0, 1.0)));
    put(p, 3, s);
}

/* the bottom status line: one item at a time as the OLED cycles them; a bad one inverted */
static void status(struct jot_page *p, const struct jo_ctx *c, uint32_t now)
{
    struct jo_status it[JO_STATUS_MAX];
    int n = jo_status_items(c, it);
    const struct jo_status *s;
    if (!n)
        return;
    s = &it[(now / JO_STATUS_CYCLE_MS) % (uint32_t)n];
    put(p, 6, s->t);
    if (s->bad)
        p->sel = 6;
}

static void home(struct jot_page *p, const struct jo_ctx *c, uint32_t now)
{
    char t[24], v[16], s[24], g[LEVEL_CELLS + 1];
    const char *beats = BEATS[c->has_beat ? ((c->beat_n % 4) + 4) % 4 : 4];
    home_top(p, c);
    if (!c->line_in) {                              /* LINK without a line in: the tempo and the beat */
        if (c->has_link_bpm)
            jo_fixed(c->link_bpm, 1, v, sizeof v);
        else
            snprintf(v, sizeof v, "---");
        snprintf(t, sizeof t, "%s BPM", v);
        put(p, 3, t);
        put(p, 4, beats);
    } else {
        if (c->has_bpm && c->bpm != 0.0)
            jo_fixed(c->bpm, 1, v, sizeof v);
        else
            snprintf(v, sizeof v, "---");
        snprintf(t, sizeof t, "%s BPM", v);
        put_lr(p, 2, t, beats);
        bar_marks(p, c);
        snprintf(s, sizeof s, "IN %ddB", jo_iround(c->level_db));
        bar(g, LEVEL_CELLS, (c->level_db - METER_MIN_DB) / -METER_MIN_DB, (SILENCE_DB - METER_MIN_DB) / -METER_MIN_DB);
        put_lr(p, 4, s, g);
        snprintf(s, sizeof s, "CF %d%%", jo_iround(c->conf * 100));
        bar(g, LEVEL_CELLS, c->conf, c->conf_graded ? -1.0 : c->th / 100.0);
        put_lr(p, 5, s, g);
    }
    status(p, c, now);
}

/* ---- the lists ---- */
static void menu(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    int n, top = jo_clampi(ui->top, ui->cursor - ROWS + 1, ui->cursor);
    const struct jo_item *m = jo_menu_of(c->layout_lcd2, &n);
    char right[24], val[12];
    snprintf(right, sizeof right, "%d/%d", ui->cursor + 1, n);
    title(p, "MENU", right);
    for (int k = 0; k < ROWS && top + k < n; k++) {
        jo_menu_value(c, c->layout_lcd2, &m[top + k], val, sizeof val);
        row(p, 1 + k, top + k == ui->cursor, m[top + k].name, val);
    }
}

static void sub(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    const struct jo_sub *s = &jo_subs[ui->sub < 3 ? ui->sub : 0];
    int rows = jo_sub_rows(s, c->layout_lcd2);
    title(p, s->title, NULL);
    for (int i = 0; i < rows && i < ROWS; i++) {
        char val[24];
        jo_sub_value(c, s->rows[i], c->layout_lcd2, val, sizeof val);
        row(p, 1 + i, i == ui->ocur, jo_spec_l(s->rows[i], c->layout_lcd2)->title, val);
    }
}

/* ABOUT: the rows of the page a press moved to (jo_about_vis: 6 at most while the text page shows it) */
static void about(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    struct jo_about_row rows[JO_ABOUT_ROWS];
    int vis = jo_about_vis(c);
    title(p, "ABOUT", NULL);
    jo_about_rows(c, rows);
    for (int k = 0; k < vis && k < ROWS && ui->ascroll + k < JO_ABOUT_ROWS; k++)
        put_lr(p, 1 + k, rows[ui->ascroll + k].label, rows[ui->ascroll + k].val);
}

static void info(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    char rows[3][40], v[16], g[BATT_CELLS + 1];
    int n;
    if (ui->page == 1) {
        about(p, ui, c);
        return;
    }
    title(p, "BATTERY", NULL);
    if (!c->fitted) {
        snprintf(rows[0], sizeof rows[0], "NO CELL FITTED");
        snprintf(rows[1], sizeof rows[1], "POWER %s", c->usb ? "USB" : "NONE");
        n = 2;
    } else {
        jo_fixed(3.3 + 0.9 * c->batt / 100, 2, v, sizeof v);   /* rough: 3.3 V empty, 4.2 V full (the OLED's) */
        snprintf(rows[0], sizeof rows[0], "LEVEL %d%%", c->batt);
        snprintf(rows[1], sizeof rows[1], "VOLT %sV EST", v);
        snprintf(rows[2], sizeof rows[2], "%s", c->usb ? (c->batt < 100 ? "CHARGING" : "FULL") : "DISCHARGING");
        n = 3;
        bar(g, BATT_CELLS, c->batt / 100.0, -1.0);
        put(p, 5, g);
    }
    for (int i = 0; i < n; i++)
        put(p, 1 + i, rows[i]);
}

/* EDIT: the value inverted, its two help lines under it */
static void edit(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    const struct jo_spec *sp = jo_spec_l(ui->edit.field, c->layout_lcd2);
    const char *h1, *h2;
    char s[24];
    if (!sp)
        return;
    title(p, sp->title, NULL);
    jo_edit_text(sp, ui->edit.val, s, sizeof s);
    put_c(p, 2, s);
    p->sel = 2;
    jo_help_of(sp, ui->edit.val, &h1, &h2);
    put_c(p, 4, h1);
    put_c(p, 5, h2);
}

/* NETWORK: the rows, and the selected row's help at the bottom. A value is cut to what fits beside its name on the OLED (jo_text_w reads
 * the grid being drawn: call this between draws, in the task that draws, as the screens' own code is) */
static void net(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    struct jo_net_row rows[4];
    int n = jo_net_rows(c, rows);
    title(p, "NETWORK", NULL);
    for (int i = 0; i < n; i++) {
        char val[JO_TEXT_MAX + 8];
        fit(rows[i].val, (ROW_VAL_PX - jo_text_w(rows[i].name, 1)) / OLED_CW, val, sizeof val);
        row(p, 1 + i, i == ui->ncur, rows[i].name, val);
        if (i == ui->ncur)
            for (int k = 0; k < 2; k++)
                put(p, 5 + k, rows[i].help[k]);
    }
}

static void scan(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    char right[24] = "";
    int n = c->net.nscan < JO_SCAN_MAX ? c->net.nscan : JO_SCAN_MAX, top = jo_clampi(ui->stop, ui->scur - ROWS + 1, ui->scur);
    if (n)
        snprintf(right, sizeof right, "%d/%d", ui->scur + 1, n);
    title(p, "NETWORK", right);
    if (!n) {
        put_c(p, 3, "NO NETWORKS");
        return;
    }
    for (int k = 0; k < ROWS && top + k < n; k++) {
        const struct jo_scan_row *r = &c->net.scan[top + k];
        char s[SCAN_CHARS + 2];
        s[0] = strcmp(r->name, c->net.ssid) ? ' ' : '*';
        fit(r->name, SCAN_CHARS, s + 1, sizeof s - 1);
        row(p, 1 + k, top + k == ui->scur, s, SIGNAL[r->bars < 3 ? r->bars : 3]);
    }
}

/* the character editor: masked but for the cursor's cell (the real character), the empty slot past the end; the window follows the cursor */
static void text_screen(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    const struct jo_text *t = &ui->txt;
    int n = t->len, start = t->pos > TEXT_CELLS - 1 ? t->pos - (TEXT_CELLS - 1) : 0, k;
    char len[8], cells[TEXT_CELLS + 1];
    snprintf(len, sizeof len, "%d", n);
    title(p, t->field == JO_F_APNAME ? "AP NAME" : "PASSKEY", len);
    for (k = 0; k < TEXT_CELLS && start + k <= n; k++) {
        int i = start + k;
        char real = i < n ? t->text[i] : ' ';
        cells[k] = i != t->pos && t->mask && i < n ? '*' : real;
    }
    cells[k] = 0;
    put(p, 1, cells);
    if (t->pos <= n) {
        p->sel = 1;
        p->cursor = (uint8_t)(t->pos - start);
    }
    put_c(p, 3, "YES NEXT  NO DEL");
    put_c(p, 4, "easier: web UI");
    put_c(p, 5, c->net.mode == JO_NET_AP ? "192.168.4.1" : "jank.local");
}

static void net_info(struct jot_page *p, const struct jo_ctx *c)
{
    const struct jo_net *n = &c->net;
    char rows[3][JO_TEXT_MAX + 16], s[NAME_CHARS + 1];
    title(p, n->mode == JO_NET_CLIENT ? "LINK CLIENT" : "LINK AP", NULL);
    if (n->mode == JO_NET_CLIENT) {
        fit(n->ssid[0] ? n->ssid : "-", NAME_CHARS, s, sizeof s);
        snprintf(rows[0], sizeof rows[0], "STATE %s", n->status);
        snprintf(rows[1], sizeof rows[1], "IP %s", n->ip);
        snprintf(rows[2], sizeof rows[2], "SSID %s", s);
    } else {
        fit(n->ap_name, NAME_CHARS, s, sizeof s);
        snprintf(rows[0], sizeof rows[0], "IP 192.168.4.1");
        snprintf(rows[1], sizeof rows[1], "CLIENTS %d", n->ap_clients);
        snprintf(rows[2], sizeof rows[2], "NAME %s", s);
    }
    for (int i = 0; i < 3; i++)
        put(p, 1 + i, rows[i]);
}

static void confirm(struct jot_page *p, const char *t, const char *l1, const char *l2)
{
    title(p, t, NULL);
    put_c(p, 2, l1);
    put_c(p, 3, l2);
    put_c(p, 5, "YES      NO");
}

/* SETUP: the hotspot's name and address (the OLED's QR code has no text form) */
static void setup(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c)
{
    char a[JOT_FIT + 1];
    size_t n = strlen(c->net.setup_name);
    if (ui->leave) {
        confirm(p, "SETUP", "LEAVE SETUP?", "RUN AGAIN");
        return;
    }
    fit(c->net.setup_name, JOT_FIT, a, sizeof a);
    put(p, 0, "SETUP MODE");
    put(p, 1, a);
    put(p, 2, n > JOT_FIT ? c->net.setup_name + JOT_FIT : "");
    put(p, 4, "open");
    put(p, 5, "192.168.4.1");
    put(p, 6, "NO: LEAVE");
}

/* ---- over the screen ---- */
static void box(struct jot_page *p, const char *a, const char *b, const char *c, int sel)
{
    put_c(p, BOX_LINE, a);
    put_c(p, BOX_LINE + 1, b);
    put_c(p, BOX_LINE + 2, c);
    p->sel = (uint8_t)sel;
    p->cursor = JOT_NONE;
}

static void overlays(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now)
{
    static const char *const HINTS[2][3] = {{"NUDGE BEAT", "UP: 1/2 BEAT EARLY", "DOWN: 1/2 BEAT LATE"}, {"NUDGE MS", "UP: 1 MS LATER", "DOWN: 1 MS EARLIER"}};
    if (ui->hint == JO_NO || ui->hint == JO_YES) {   /* NO / YES held on HOME: what UP / DOWN will do */
        const char *const *h = HINTS[ui->hint == JO_YES];
        box(p, h[0], h[1], h[2], BOX_LINE);
    } else if (ui->overlay.on && (int32_t)(now - ui->overlay.until) < 0)
        box(p, ui->overlay.title, ui->overlay.value, "", BOX_LINE);
    if (c->saved)
        box(p, "", "SAVED", "", BOX_LINE + 1);
}

/* the trailing spaces go (but the cursor's cell); n is the last line with text, or the inverted one */
static void finish(struct jot_page *p)
{
    p->n = 0;
    for (int i = 0; i < JOT_LINES; i++) {
        char *s = p->line[i];
        size_t len = strlen(s), keep = p->sel == i && p->cursor != JOT_NONE ? p->cursor + 1u : 0u;
        while (len > keep && s[len - 1] == ' ')
            s[--len] = 0;
        if (p->right[i] >= len)
            p->right[i] = 0;
        if (!len)
            p->centred &= (uint8_t)~(1u << i);
        if (len || p->sel == i)
            p->n = (uint8_t)(i + 1);
    }
}

static void screen(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now)
{
    switch (ui->screen) {
    case JO_HOME: home(p, c, now); break;
    case JO_NET: net(p, ui, c); break;
    case JO_SCAN: scan(p, ui, c); break;
    case JO_TEXT: text_screen(p, ui, c); break;
    case JO_NETINFO: net_info(p, c); break;
    case JO_CONFIRM: confirm(p, "SETUP", "STOP AND START", "SETUP?"); break;
    case JO_MENU: menu(p, ui, c); break;
    case JO_SUB: sub(p, ui, c); break;
    case JO_INFO: info(p, ui, c); break;
    case JO_EDIT: edit(p, ui, c); break;
    default: break;
    }
}

void jot_render(struct jot_page *p, const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now)
{
    memset(p, 0, sizeof *p);
    p->sel = p->cursor = JOT_NONE;
    if (c->boot) {
        put_c(p, 2, "BOOT");
        put_c(p, 5, "BOOTLOADER");
    } else if (c->splash) {
        put_c(p, 1, "JANK");
        put_c(p, 3, c->splash_model ? c->splash_model : "MINI 2.2");
        put_c(p, 5, c->splash_fw ? c->splash_fw : "FW EMU");
    } else {
        if (c->setup)
            setup(p, ui, c);
        else
            screen(p, ui, c, now);
        overlays(p, ui, c, now);
    }
    finish(p);
}

int jot_equal(const struct jot_page *a, const struct jot_page *b)
{
    if (a->flags != b->flags || a->sel != b->sel || a->cursor != b->cursor || a->n != b->n || a->centred != b->centred)
        return 0;
    for (int i = 0; i < JOT_LINES; i++)
        if (a->right[i] != b->right[i] || strcmp(a->line[i], b->line[i]))
            return 0;
    return 1;
}
