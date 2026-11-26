/* SPDX-License-Identifier: GPL-3.0-only */
/* The Digitone's Session page in the Launchpad app (jank_lpsess.h, the page hooks of jank_lpapp_int.h): its pads, its answers, and the Digitone's stage 8 (TRANSPORT, CHAIN,
 * the protocol-7 status). The unit's file: another unit has jank_lpclips.c instead (jank_lpunit.h JLAPP_CLIPS). */
#include "jank_lpapp_int.h"

void jlapp_page_init(struct jlapp *a)
{
    jlss_init(&a->sess);
    a->pat_reads_ok = 1;
    a->pat_cue_ok = 1;
    a->chain_ok = 1;
}

void jlapp_page_open(struct jlapp *a, int live)
{
    if (live)
        jlss_open(&a->sess);
    else
        jlss_forget(&a->sess);
}

void jlapp_page_frame(const struct jlapp *a, struct jlx_frame *f) { jlss_frame(&a->sess, f); }

/* the held pad let go: the chain built on it goes (CHAIN SET), or the playing pattern held alone clears the chain (CHAIN CLEAR) */
static size_t session_release(struct jlapp *a, uint8_t pad, struct jlapp_act *out, size_t max)
{
    int k = jlss_release(&a->sess, pad, a->now_ms);
    size_t n;
    if (k == JLSS_RELEASE_NONE)
        return 0;
    if (a->live_state != JDL_LIVE) {
        jlapp_say_why_not(a);
        return 0;
    }
    if (!a->chain_ok) {
        jlapp_say(a, "CHAIN: DN2 TOO OLD");
        return 0;
    }
    if (max < 1)
        return 0;
    if (k == JLSS_RELEASE_CLEAR) {
        jlss_chain_cleared(&a->sess);
        jlapp_say(a, "CHAIN CLEARED");
        jlapp_act(out, JLAPP_ACT_CHAIN, 0, 0);
        out->vel = JDL_CHAIN_CLEAR;
        return 1;
    }
    jlss_chain_list(&a->sess, &n);
    jlss_chain_sent(&a->sess);         /* shown at once, read back; the list stays until the next press (the firmware reads it with jlapp_chain_list) */
    jlapp_say(a, "CHAIN OF %u", (unsigned)n);
    jlapp_act(out, JLAPP_ACT_CHAIN, 0, (uint8_t)n);
    out->vel = JDL_CHAIN_SET;
    return 1;
}

/* A pad of the Session page: the first one pressed cues its pattern (PATTERN how 0), which flashes at once; with it held, the others build a chain (jank_lpsess.h) */
size_t jlapp_session_pad(struct jlapp *a, const struct jlx_ev *ev, struct jlapp_act *out, size_t max)
{
    uint8_t pad = (uint8_t)(ev->row * 8u + ev->col), p = jlss_pattern_of_pad(&a->sess, ev->col, ev->row);
    char name[4];
    if (!ev->value)
        return session_release(a, pad, out, max);
    if (a->live_state != JDL_LIVE) {
        jlapp_say_why_not(a);
        return 0;
    }
    jlss_name(p, name);
    switch (jlss_press(&a->sess, pad, p, a->now_ms)) {
    case JLSS_PRESS_ADDED:
        jlapp_say(a, "CHAIN %u: %s", (unsigned)a->sess.build_n, name);
        return 0;
    case JLSS_PRESS_FULL:
        jlapp_say(a, "CHAIN FULL (%u)", (unsigned)JDL_CHAIN_MAX);
        return 0;
    default:
        break;
    }
    if (!a->pat_cue_ok) {
        jlapp_say(a, "PATTERN: DN2 TOO OLD");
        return 0;
    }
    if (max < 1)
        return 0;
    jlss_cue(&a->sess, p, a->now_ms);
    jlapp_say(a, "CUE %s", name);
    jlapp_act(out, JLAPP_ACT_PATTERN, 0, p);
    out->vel = JDL_PATTERN_CUE;
    return 1;
}

int jlapp_session_due(struct jlapp *a, struct jlss_req *r)
{
    unsigned ok = (a->pat_reads_ok ? JLSS_OK_READS : 0u) | (a->pat_cue_ok ? JLSS_OK_CUE : 0u) | (a->chain_ok ? JLSS_OK_CHAIN : 0u) | (a->live7 ? JLSS_OK_LIVE7 : 0u);
    if (a->mode != JLAPP_MODE_SESSION || a->live_state != JDL_LIVE)
        return 0;
    return jlss_due(&a->sess, a->now_ms, ok, r);
}

void jlapp_on_patterns(struct jlapp *a, const struct jdl_patterns *p) { jlss_on_patterns(&a->sess, p); }

void jlapp_on_pattern(struct jlapp *a, const struct jdl_pattern *p) { jlss_on_next(&a->sess, p->playing, p->next, a->now_ms); }

void jlapp_on_chain(struct jlapp *a, const struct jdl_chain *c) { jlss_on_chain(&a->sess, c); }

const uint8_t *jlapp_chain_list(const struct jlapp *a, size_t *n) { return jlss_chain_list(&a->sess, n); }

void jlapp_set_patterns(struct jlapp *a, int reads_ok, int cue_ok)
{
    if (a->pat_cue_ok && !cue_ok) {    /* a Digitone without PATTERN: its pads cue nothing (said again at each press) */
        a->sess.cued = JDL_NO_PATTERN;
        jlapp_say(a, "PATTERN: DN2 TOO OLD");
    }
    a->pat_cue_ok = cue_ok != 0;
    a->pat_reads_ok = reads_ok != 0;
}

/* ---- the Digitone's TRANSPORT (jank_lptrans.h) ---- */

void jlapp_set_peers(struct jlapp *a, int peers) { jltr_set_peers(&a->tr, peers); }

int jlapp_dn2_leads(const struct jlapp *a) { return jltr_dn2_leads(&a->tr, a->live_state == JDL_LIVE); }

void jlapp_set_stage8(struct jlapp *a, int transport_ok, int chain_ok)
{
    /* silent: the probe runs each time live control comes up, and a Digitone without stage 8 would show the same refusal every time (owner's screen). Scene 5 is
     * then Jank's session as before stage 8; a chain or a cue press says "... DN2 TOO OLD" itself */
    a->tr.cmd_ok = transport_ok != 0;
    if (!transport_ok)
        a->tr.has_cmd = 0;
    a->chain_ok = chain_ok != 0;
}

int jlapp_on_dn2_transport(struct jlapp *a, int state)
{
    if (state < 0)
        return JLTR_NO_REQUEST;
    return jltr_on_dn2(&a->tr, (unsigned)state, 1, a->now_ms);
}

int jlapp_on_live7(struct jlapp *a, const struct jdl_live7 *s)
{
    a->live7 = 1;
    jlss_on_next(&a->sess, s->playing, s->next, a->now_ms);
    jlss_on_chain_count(&a->sess, s->chain_count);
    return jltr_on_dn2(&a->tr, s->transport, 0, a->now_ms);
}
