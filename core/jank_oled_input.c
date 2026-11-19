/* SPDX-License-Identifier: GPL-3.0-only */
/* The four buttons as the emulator's host turns them into presses (player.html btnDown, btnUp, armHint, comboStep):
 * NO and YES on HOME act on release, and only when no combo was used while they were held; NO + UP/DOWN nudges the beat
 * half a beat (no repeat), YES + UP/DOWN the AUDIO IN offset 1 ms (repeats); a 400 ms hold of NO or YES on HOME shows
 * what UP / DOWN will do, and its release then does nothing; UP / DOWN repeat after 400 ms, then every 120 ms; the first press on a dark screen only wakes it. */
#include "jank_oled_int.h"
#include <stdio.h>
#include <string.h>

#define NUDGE_MS 1
static const uint32_t SLEEP_MS[4] = {0, 30000u, 60000u, 300000u};

void jo_input_init(struct jo_input *in, uint32_t now)
{
    memset(in, 0, sizeof *in);
    for (int i = 0; i < JO_BTNS; i++)
        in->rep_mod[i] = -1;
    in->last_input = now;
}

int jo_on_home(const struct jo_ui *ui, const struct jo_ctx *c) { return ui->screen == JO_HOME && c->net.mode != JO_NET_SETUP; }

int jo_asleep(const struct jo_input *in, const struct jo_ctx *c, uint32_t now)
{
    uint32_t ms = c->sleep < 4 ? SLEEP_MS[c->sleep] : 0u;
    return ms > 0 && c->net.mode != JO_NET_SETUP && now - in->last_input > ms;
}

static const struct jo_ctx *ctx_of(const struct jo_host *h, uint32_t now) { return h->ctx(h->ops->user, now); }

/* NO held: half a beat; YES held: the AUDIO IN offset, 1 ms (every output). LINK mode: the session owns both */
static void combo_step(struct jo_ui *ui, int mod, int d, uint32_t now, const struct jo_host *h)
{
    const struct jo_ctx *c = ctx_of(h, now);
    if (c->link)
        return;
    if (mod == JO_NO) {
        if (c->no_beat_nudge) {
            jo_show_overlay(ui, now, "TRACKER LEADS", "");
            return;
        }
        if (h->ops->shift_beats)
            h->ops->shift_beats(h->ops->user, d);
        jo_show_overlay(ui, now, "NUDGE BEAT", d > 0 ? "+1/2" : "-1/2");
    } else {
        int ms = jo_clampi(c->in_off + d * NUDGE_MS, -100, 100);
        char v[8];
        if (c->no_ms_nudge) {
            jo_show_overlay(ui, now, "TRACKER LEADS", "");
            return;
        }
        if (h->ops->set_field)
            h->ops->set_field(h->ops->user, JO_F_INOFF, ms);
        snprintf(v, sizeof v, "%s%d", ms > 0 ? "+" : "", ms);
        jo_show_overlay(ui, now, "NUDGE MS", v);
    }
}

/* one action of a held button: a press, or the combo it repeats */
static void act(struct jo_input *in, struct jo_ui *ui, enum jo_btn b, uint32_t now, const struct jo_host *h)
{
    if (in->rep_mod[b] >= 0)
        combo_step(ui, in->rep_mod[b], b == JO_UP ? 1 : -1, now, h);
    else
        jo_press(ui, b, now, ctx_of(h, now), h->ops);
}

void jo_input_down(struct jo_input *in, struct jo_ui *ui, enum jo_btn b, uint32_t now, const struct jo_host *h)
{
    const struct jo_ctx *c;
    int ud = b == JO_UP || b == JO_DOWN, mod;
    if (in->held[b])
        return;
    in->held[b] = 1;
    c = ctx_of(h, now);
    if (jo_asleep(in, c, now)) {                      /* a dark screen: the press only wakes it */
        in->last_input = now;
        in->woke[b] = 1;
        return;
    }
    in->last_input = now;
    mod = in->held[JO_NO] ? JO_NO : in->held[JO_YES] ? JO_YES : -1;
    if ((b == JO_NO || b == JO_YES) && jo_on_home(ui, c)) {   /* decided on release */
        in->combo = 0;
        in->shown[b] = 0;
        in->deferred[b] = 1;
        in->hint_on = 1;                              /* armHint: one timer, the last NO / YES */
        in->hint_btn = (uint8_t)b;
        in->hint_due = now + JO_HINT_MS;
        return;
    }
    in->rep_mod[b] = -1;
    if (ud && mod >= 0 && in->deferred[mod] && jo_on_home(ui, c)) {   /* the modifier was pressed on HOME too */
        in->combo = 1;
        in->shown[mod] = 1;                           /* that hold is used up: its release does nothing, even after another NO / YES */
        in->rep_mod[b] = (int8_t)mod;
        if (mod == JO_NO) {                           /* a beat at a time: no auto-repeat */
            act(in, ui, b, now, h);
            return;
        }
    }
    act(in, ui, b, now, h);
    if (ud) {
        in->rep_on[b] = 1;
        in->rep_due[b] = now + JO_REPEAT_FIRST_MS;
    }
}

void jo_input_up(struct jo_input *in, struct jo_ui *ui, enum jo_btn b, uint32_t now, const struct jo_host *h)
{
    const struct jo_ctx *c;
    if (!in->held[b])
        return;
    in->held[b] = 0;
    in->rep_on[b] = 0;
    if (b == JO_NO || b == JO_YES) {                  /* dropHint */
        in->hint_on = 0;
        ui->hint = -1;
    }
    if (in->woke[b]) {                                /* the wake-up press does nothing else */
        in->woke[b] = 0;
        return;
    }
    in->last_input = now;
    if (!in->deferred[b])                             /* only a press that started on HOME acts on release */
        return;
    in->deferred[b] = 0;
    c = ctx_of(h, now);
    if (jo_on_home(ui, c) && !in->combo && !in->shown[b])
        jo_press(ui, b, now, c, h->ops);
}

/* the earliest timer due by now: 0..3 a repeat, 4 the hint; -1 none */
static int next_due(const struct jo_input *in, uint32_t now, uint32_t *due)
{
    int which = -1;
    for (int b = 0; b < JO_BTNS; b++)
        if (in->rep_on[b] && (int32_t)(now - in->rep_due[b]) >= 0 && (which < 0 || (int32_t)(in->rep_due[b] - *due) < 0)) {
            which = b;
            *due = in->rep_due[b];
        }
    if (in->hint_on && (int32_t)(now - in->hint_due) >= 0 && (which < 0 || (int32_t)(in->hint_due - *due) < 0)) {
        which = JO_BTNS;
        *due = in->hint_due;
    }
    return which;
}

void jo_input_tick(struct jo_input *in, struct jo_ui *ui, uint32_t now, const struct jo_host *h)
{
    uint32_t due = 0;
    int which;
    while ((which = next_due(in, now, &due)) >= 0) {
        if (which == JO_BTNS) {
            const struct jo_ctx *c = ctx_of(h, due);
            in->hint_on = 0;
            if (in->held[in->hint_btn] && !in->combo && !c->link && jo_on_home(ui, c)) {
                ui->hint = (int8_t)in->hint_btn;
                in->shown[in->hint_btn] = 1;          /* looking at the hint is harmless: the release does not act */
            }
        } else {
            in->rep_due[which] = due + JO_REPEAT_EVERY_MS;
            act(in, ui, (enum jo_btn)which, due, h);
        }
    }
}
