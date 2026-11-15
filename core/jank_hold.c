/* SPDX-License-Identifier: GPL-3.0-only */
/* AUDIO mode's Stop / Start of the outputs (jank_hold.h). */
#include "jank_hold.h"
#include <string.h>

void jho_init(struct jho *h)
{
    memset(h, 0, sizeof *h);
    h->hold = 1;                       /* power-on: the user's Stop (M2h), until the user's Start */
}

void jho_set(struct jho *h, int hold)
{
    if (hold) {
        h->gate_off = 0;               /* Stop: the gate's own rules again from the next Start on */
        if (!h->hold) {
            h->hold = 1;
            h->arming = 0;
            h->holds++;
        }
        return;
    }
    h->gate_off = 1;                   /* Start, held or not: the user's wins over LOW CONFIDENCE (M2g) */
    if (h->hold) {
        h->hold = 0;
        h->arming = 1;
        h->releases++;
    }
}

/* the outputs stopped by the tracker: the silence always; LOW CONFIDENCE's gate unless a Start disarmed it */
static int gate_closed(const struct jho *h, const struct jho_in *in) { return in->silent || (in->gated && !h->gate_off); }

/* LINK mode's Start: the user's, so it ends the user's Stop of either mode and, as AUDIO mode's, disarms the gate for
 * when the sound comes */
void jho_link_play(struct jho *h)
{
    h->hold = 0;
    h->arming = 0;
    h->gate_off = 1;
}

/* LINK mode's Stop: the same user's Stop as AUDIO mode's (the mode itself is automatic: the sound coming and going) */
void jho_link_stop(struct jho *h) { jho_set(h, 1); }

int jho_outputs(struct jho *h, const struct jho_in *in)
{
    if (!in->audio)                    /* LINK mode: the user's Stop holds here too; no gate */
        return h->hold ? 0 : in->base;
    if (h->gate_off && (in->conf_open || in->silent))
        h->gate_off = 0;               /* the confidence reached the threshold + 5 once, or the silence (the automatic
                                        * layer starts over, M2h): the gate is armed again */
    if (h->hold || gate_closed(h, in))
        return 0;                      /* (gated: an arming waits on: it ends with the lock once the gate lifts) */
    if (h->arming) {
        if (!in->locked)
            return 0;
        h->arming = 0;                 /* locked: from here the transport's own rule */
    }
    return in->base;
}

int jho_session_held(const struct jho *h, const struct jho_in *in)
{
    return in->audio && in->lt == JHO_LEAD && (h->hold || gate_closed(h, in));
}

uint8_t jho_icon(const struct jho *h, const struct jho_in *in)
{
    if (h->hold || gate_closed(h, in))
        return JHO_STOPPED;
    return in->clock_running ? JHO_RUNNING : JHO_ARMED;
}

const char *jho_icon_name(uint8_t icon)
{
    static const char *const names[] = {"stopped", "armed", "running"};
    return icon <= JHO_RUNNING ? names[icon] : names[JHO_STOPPED];
}
