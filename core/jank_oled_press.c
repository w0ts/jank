/* SPDX-License-Identifier: GPL-3.0-only */
/* One button press on the screens (the emulator's OLED.press, startEdit / stepEdit / endEdit, the character editor). */
#include "jank_oled_int.h"
#include <stdio.h>
#include <string.h>

static const char CHARS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.!@#";
#define NCHARS ((int)sizeof CHARS - 1)

void jo_ui_init(struct jo_ui *ui)
{
    memset(ui, 0, sizeof *ui);
    ui->screen = JO_HOME;
    ui->hint = -1;
}

/* the screens' own overlay (press): the hint stays as it is */
static void overlay(struct jo_ui *ui, uint32_t now, const char *title, const char *value)
{
    ui->overlay.on = 1;
    ui->overlay.until = now + JO_OVERLAY_MS;
    snprintf(ui->overlay.title, sizeof ui->overlay.title, "%s", title);
    snprintf(ui->overlay.value, sizeof ui->overlay.value, "%s", value);
}

void jo_show_overlay(struct jo_ui *ui, uint32_t now, const char *title, const char *value)
{
    ui->hint = -1;
    overlay(ui, now, title, value);
}

static void set_field(const struct jo_ops *ops, enum jo_field f, int v)
{
    if (ops && ops->set_field)
        ops->set_field(ops->user, f, v);
}

static void start_edit(struct jo_ui *ui, enum jo_field f, const struct jo_ctx *c)
{
    int orig = jo_spec_get(f, c);
    ui->edit = (struct jo_edit){(uint8_t)f, ui->screen, orig, orig};
    ui->screen = JO_EDIT;
}

/* choices cycle round; numbers stop at their ends */
static void step_edit(struct jo_ui *ui, int d, int lcd2, const struct jo_ops *ops)
{
    struct jo_edit *e = &ui->edit;
    const struct jo_spec *sp = jo_spec_l(e->field, lcd2);
    if (sp->opts)
        e->val = (e->val + d + sp->nopts) % sp->nopts;
    else
        e->val = jo_clampi(e->val + sp->step * d, sp->lo, sp->hi);
    if (sp->live)
        set_field(ops, e->field, e->val);
}

static void end_edit(struct jo_ui *ui, int keep, int lcd2, const struct jo_ops *ops)
{
    struct jo_edit e = ui->edit;
    const struct jo_spec *sp = jo_spec_l(e.field, lcd2);
    ui->screen = e.ret;
    if (!sp->live) {                                /* applied only when confirmed (repick: also unchanged) */
        if (keep && (e.val != e.orig || sp->repick)) {
            if (e.field == JO_F_NETMODE && e.val == JO_SETUP)
                ui->screen = JO_CONFIRM;            /* stopping the device needs a second YES */
            else
                set_field(ops, e.field, e.val);
        }
    } else if (!keep)
        set_field(ops, e.field, e.orig);
}

static void start_text(struct jo_ui *ui, const struct jo_ctx *c, enum jo_net_act act)
{
    const char *t = act == JO_ACT_PASS ? c->net.pass : act == JO_ACT_APPASS ? c->net.appass : c->net.ap_name;
    struct jo_text *x = &ui->txt;
    memset(x, 0, sizeof *x);
    x->field = act == JO_ACT_PASS ? JO_F_PASS : act == JO_ACT_APPASS ? JO_F_APPASS : JO_F_APNAME;
    snprintf(x->text, sizeof x->text, "%s", t);
    x->len = (uint8_t)strlen(x->text);
    x->mask = act != JO_ACT_APNAME;
    ui->screen = JO_TEXT;
}

static int char_index(char ch)
{
    const char *p = ch ? strchr(CHARS, ch) : NULL;
    return p ? (int)(p - CHARS) : -1;
}

/* UP/DOWN cycle the character at the cursor, YES next (on the empty slot past the end: confirm), NO deletes the last
 * character (on an empty text: cancel) */
static void press_text(struct jo_ui *ui, enum jo_btn b, int d, const struct jo_ops *ops)
{
    struct jo_text *t = &ui->txt;
    int n = t->len;
    if (d) {
        if (t->pos >= n) {
            if (n < JO_TEXT_MAX) {
                t->text[n] = CHARS[d > 0 ? 0 : NCHARS - 1];
                t->text[++t->len] = 0;
            }
        } else {
            int i = char_index(t->text[t->pos]);
            t->text[t->pos] = CHARS[i < 0 ? 0 : (i + d + NCHARS) % NCHARS];
        }
    } else if (b == JO_YES) {
        if (t->pos >= n) {
            if (ops && ops->set_text)
                ops->set_text(ops->user, t->field, t->text);
            ui->screen = JO_NET;
        } else
            t->pos++;
    } else if (n == 0) {
        ui->screen = JO_NET;
    } else {
        t->text[--t->len] = 0;
        if (t->pos > t->len)
            t->pos = t->len;
    }
}

static void press_home(struct jo_ui *ui, enum jo_btn b, int d, uint32_t now, const struct jo_ctx *c, const struct jo_ops *ops)
{
    if (c->tracker_leads && (d || b == JO_YES)) {   /* AUDIO, the tracker owns tempo and transport */
        overlay(ui, now, "TRACKER LEADS", "");
    } else if (d) {
        double bpm = 0.0;
        char v[16] = "---";
        if (ops && ops->tempo_step && ops->tempo_step(ops->user, d, &bpm) == 0)
            jo_fixed(bpm, 1, v, sizeof v);
        if (!c->link)                                 /* a Link step shows on the big tempo itself */
            overlay(ui, now, d > 0 ? "DOUBLE" : "HALF", v);
    } else if (b == JO_YES) {                         /* Stop / Start the outputs; the transport icon shows it */
        if (c->link) {
            if (ops && ops->link_start_stop)
                ops->link_start_stop(ops->user, !(c->running || c->armed));
        } else if (ops && ops->set_hold)
            ops->set_hold(ops->user, c->running || c->armed);
    } else {
        ui->screen = JO_MENU;
        ui->overlay.on = 0;
    }
}

static void press_menu(struct jo_ui *ui, enum jo_btn b, int d, const struct jo_ctx *c)
{
    const struct jo_item *menu = c->layout_lcd2 ? jo_menu_lcd2 : jo_menu;
    int n = c->layout_lcd2 ? JO_MENU_ITEMS_LCD2 : JO_MENU_ITEMS;
    if (d) {
        ui->cursor = (uint8_t)jo_clampi(ui->cursor - d, 0, n - 1);
        ui->top = (uint8_t)jo_clampi(ui->top, ui->cursor - JO_VISIBLE + 1, ui->cursor);
    } else if (b == JO_NO)
        ui->screen = JO_HOME;
    else {
        const struct jo_item *it = &menu[ui->cursor];
        if (it->kind == JO_ITEM_NET) {
            ui->screen = JO_NET;
            ui->ncur = 0;
        } else if (it->kind == JO_ITEM_EDIT)
            start_edit(ui, it->ref, c);
        else if (it->kind == JO_ITEM_SUB) {
            ui->screen = JO_SUB;
            ui->sub = it->ref;
            ui->ocur = 0;
        } else {
            ui->screen = JO_INFO;
            ui->page = it->ref;
            ui->ascroll = 0;
        }
    }
}

static void press_net(struct jo_ui *ui, enum jo_btn b, int d, const struct jo_ctx *c)
{
    struct jo_net_row rows[4];
    int n = jo_net_rows(c, rows);
    if (d)
        ui->ncur = (uint8_t)jo_clampi(ui->ncur - d, 0, n - 1);
    else if (b == JO_NO)
        ui->screen = JO_MENU;
    else if (ui->ncur < n) {
        uint8_t act = rows[ui->ncur].act;
        if (act == JO_ACT_MODE)
            start_edit(ui, JO_F_NETMODE, c);
        else if (act == JO_ACT_SSID) {
            ui->screen = JO_SCAN;
            ui->scur = 0;
            ui->stop = 0;
        } else if (act == JO_ACT_GROUP)
            start_edit(ui, JO_F_GROUP, c);
        else if (act == JO_ACT_PASS || act == JO_ACT_APPASS || act == JO_ACT_APNAME)
            start_text(ui, c, act);
        else if (act == JO_ACT_STATUS)
            ui->screen = JO_NETINFO;
    }
}

static void press_scan(struct jo_ui *ui, enum jo_btn b, int d, const struct jo_ctx *c, const struct jo_ops *ops)
{
    int n = c->net.nscan;
    if (d) {
        ui->scur = (uint8_t)jo_clampi(ui->scur - d, 0, n > 1 ? n - 1 : 0);
        ui->stop = (uint8_t)jo_clampi(ui->stop, ui->scur - 4, ui->scur);
    } else if (b == JO_YES && n) {
        if (ops && ops->set_text)
            ops->set_text(ops->user, JO_F_SSID, c->net.scan[ui->scur].name);
        ui->screen = JO_NET;
    } else if (b == JO_NO)
        ui->screen = JO_NET;
}

void jo_press(struct jo_ui *ui, enum jo_btn b, uint32_t now, const struct jo_ctx *c, const struct jo_ops *ops)
{
    int d = b == JO_UP ? 1 : b == JO_DOWN ? -1 : 0;
    if (c->setup) {                                   /* SETUP replaces every other screen */
        if (ui->leave) {
            if (b == JO_YES)
                set_field(ops, JO_F_NETMODE, c->net.prev);
            if (!d)
                ui->leave = 0;
        } else if (b == JO_NO)
            ui->leave = 1;
        return;
    }
    switch (ui->screen) {
    case JO_HOME: press_home(ui, b, d, now, c, ops); break;
    case JO_MENU: press_menu(ui, b, d, c); break;
    case JO_SUB:
        if (d)
            ui->ocur = (uint8_t)jo_clampi(ui->ocur - d, 0, jo_subs[ui->sub].n - 1);
        else if (b == JO_NO)
            ui->screen = JO_MENU;
        else
            start_edit(ui, jo_subs[ui->sub].rows[ui->ocur], c);
        break;
    case JO_NET: press_net(ui, b, d, c); break;
    case JO_SCAN: press_scan(ui, b, d, c, ops); break;
    case JO_TEXT: press_text(ui, b, d, ops); break;
    case JO_NETINFO:
        if (!d)
            ui->screen = JO_NET;
        break;
    case JO_CONFIRM:
        if (b == JO_YES) {
            set_field(ops, JO_F_NETMODE, JO_SETUP);
            ui->screen = JO_NET;
        } else if (b == JO_NO)
            ui->screen = JO_NET;
        break;
    case JO_INFO:
        if (!d)
            ui->screen = JO_MENU;
        else if (ui->page == 1) {                     /* ABOUT: scroll when its rows do not all fit */
            int vis = c->layout_lcd2 ? JO_ABOUT_VIS_LCD2 : JO_ABOUT_VIS_OLED;      /* a press moves one page */
            int last = (JO_ABOUT_ROWS - 1) / vis * vis;         /* pages start at multiples of vis: no row on two pages */
            ui->ascroll = (uint8_t)jo_clampi(ui->ascroll - d * vis, 0, last);
        }
        break;
    case JO_EDIT:
        if (d)
            step_edit(ui, d, c->layout_lcd2, ops);
        else
            end_edit(ui, b == JO_YES, c->layout_lcd2, ops);
        break;
    default: break;
    }
}
