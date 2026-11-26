/* SPDX-License-Identifier: GPL-3.0-only */
/* Jank's menu on the Digitone's screen, and LINK MODE (jank_dn2menu.h). */
#include "jank_dn2menu.h"
#include <string.h>
#include "jank_oled.h"

#define DEFAULT_QUANTUM 4
#define UB_PER_BEAT 1000000
#define MS_MAX 65535

int jdm_button(uint8_t code)
{
    switch (code) {
    case JDL_KC_UP: return JO_UP;
    case JDL_KC_DOWN: return JO_DOWN;
    case JDL_KC_NO:
    case JDL_KC_LEFT: return JO_NO;
    case JDL_KC_YES:
    case JDL_KC_RIGHT: return JO_YES;
    default: return -1;
    }
}

size_t jdm_key_presses(const struct jdl_key *k, struct jdm_press out[JDM_PRESSES_MAX])
{
    size_t n = 0;
    if (k->kind == JDL_KEY_TURN) {
        int steps = k->delta < 0 ? -(int)k->delta : k->delta;
        uint8_t b = k->delta > 0 ? JO_UP : JO_DOWN;
        if (steps > JDM_TURN_STEPS)
            steps = JDM_TURN_STEPS;
        for (int i = 0; i < steps; i++) {
            out[n++] = (struct jdm_press){b, 1};
            out[n++] = (struct jdm_press){b, 0};
        }
        return n;
    }
    if (k->kind == JDL_KEY_PRESS || k->kind == JDL_KEY_RELEASE) {
        int b = jdm_button(k->code);
        if (b >= 0)
            out[n++] = (struct jdm_press){(uint8_t)b, (uint8_t)(k->kind == JDL_KEY_PRESS)};
    }
    return n;
}

void jdm_init(struct jdm *m)
{
    memset(m, 0, sizeof *m);
    m->link = JDM_UNSENT;
}

void jdm_live_start(struct jdm *m)
{
    m->link = JDM_UNSENT;
    m->pending = 0;
}

int jdm_on_open(struct jdm *m, const struct jdl_open *o)
{
    m->open = o->open;
    m->reason = o->reason;
    if (!o->open)
        return 0;
    m->opens++;
    return 1;
}

int jdm_on_status(struct jdm *m, const struct jdl_live8 *s)
{
    struct jdl_open o = {(uint8_t)(s->open != 0), s->reason, s->drawn};
    if (o.open == m->open)
        return 0;
    return jdm_on_open(m, &o);
}

int jdm_link_step(struct jdm *m, uint32_t now, int leads, int playing, int status_known, int dn2_on)
{
    uint8_t want = leads ? JDL_LINK_ON : JDL_LINK_OFF;
    if (m->pending && playing)
        m->pending = 0;                /* it started (the Digitone takes its MIDI Start as the answer too) */
    if (m->pending && (!leads || (int32_t)(now - m->pending_until) >= 0)) {
        m->pending = 0;
        m->cancels++;
        return JDL_LINK_CANCEL;
    }
    if (m->link != want || (status_known && (dn2_on != 0) != (want == JDL_LINK_ON) && now - m->link_ms >= JDM_LINK_RESEND_MS)) {
        m->link = want;
        m->link_ms = now;
        return want;
    }
    return JDM_SEND_NONE;
}

void jdm_link_unsent(struct jdm *m) { m->link = JDM_UNSENT; }

void jdm_on_request(struct jdm *m, const struct jdl_link_req *r, uint32_t now, int leads, int playing, uint16_t ms_to_bar, struct jdm_act *a)
{
    int again = m->req_known && r->id == m->req_id;   /* pushed again: our answer was late or lost */
    memset(a, 0, sizeof *a);
    a->id = r->id;
    m->req_known = 1;
    m->req_id = r->id;
    m->reqs += !again;
    if (r->op == JDL_LR_START) {
        if (!leads) {                  /* Link does not lead (any more): no start from here */
            a->answer = JDL_LINK_CANCEL;
            return;
        }
        a->answer = JDL_LINK_PENDING;
        a->ms = ms_to_bar;
        if (again)
            return;
        a->record = (r->flags & JDL_LR_RECORD) != 0;
        a->session = playing ? JDM_SESSION_USB_START : JDM_SESSION_START;
        m->pending = 1;
        m->pending_until = now + ms_to_bar + JDM_START_GRACE_MS;
        return;
    }
    a->answer = JDL_LINK_ACK;          /* a stop (a pause is not asked for: owner 2026-11-25, PLAY while playing is a stop) */
    m->pending = 0;
    if (!again && leads)
        a->session = playing ? JDM_SESSION_STOP : JDM_SESSION_DN2_STOP;
}

uint16_t jdm_ms_to_bar(int known, int64_t beat_ub, int32_t mpb, int quantum)
{
    int64_t bar, rem, left, ms;
    if (!known || mpb <= 0)
        return 0;
    bar = (int64_t)(quantum > 0 ? quantum : DEFAULT_QUANTUM) * UB_PER_BEAT;
    rem = beat_ub % bar;
    if (rem < 0)
        rem += bar;
    left = rem ? bar - rem : 0;
    ms = (left * mpb + (int64_t)UB_PER_BEAT * 1000 - 1) / ((int64_t)UB_PER_BEAT * 1000);   /* micro-beats x microseconds a beat: ms, rounded up */
    return (uint16_t)(ms > MS_MAX ? MS_MAX : ms);
}
