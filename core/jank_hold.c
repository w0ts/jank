/* SPDX-License-Identifier: GPL-3.0-only */
/* AUDIO mode's Stop / Start of the outputs (jank_hold.h). */
#include "jank_hold.h"
#include <string.h>

void jho_init(struct jho *h) { memset(h, 0, sizeof *h); }

void jho_set(struct jho *h, int hold)
{
    if (hold && !h->hold) {
        h->hold = 1;
        h->arming = 0;
        h->holds++;
    } else if (!hold && h->hold) {
        h->hold = 0;
        h->arming = 1;
        h->releases++;
    }
}

void jho_link_play(struct jho *h)
{
    h->hold = 0;
    h->arming = 0;
    h->link_stop = 0;
}

void jho_link_stop(struct jho *h) { h->link_stop = 1; }

int jho_outputs(struct jho *h, const struct jho_in *in)
{
    if (in->audio != h->was_audio) {
        h->was_audio = in->audio;
        h->link_stop = 0;              /* a change of mode: LINK mode starts with its clocks on (the emulator's) */
    }
    if (!in->audio)                    /* LINK mode: the hold does not apply (it stays for when sound returns) */
        return h->link_stop ? 0 : in->base;
    if (h->hold || in->gated)
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
    return in->audio && in->lt == JHO_LEAD && (h->hold || in->gated);
}

uint8_t jho_icon(const struct jho *h, const struct jho_in *in)
{
    if (h->hold || in->gated)
        return JHO_STOPPED;
    return in->clock_running ? JHO_RUNNING : JHO_ARMED;
}

const char *jho_icon_name(uint8_t icon)
{
    static const char *const names[] = {"stopped", "armed", "running"};
    return icon <= JHO_RUNNING ? names[icon] : names[JHO_STOPPED];
}
