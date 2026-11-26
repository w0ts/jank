/* SPDX-License-Identifier: GPL-3.0-only */
/* Host mode: Jank's menu on the Digitone's screen, and LINK MODE (the Digitone's stage 9; owner 2026-11-25; core/jank_dn2menu.h has the rules, core/jank_dn2live.h the
 * formats). The ui task makes the pages from the same state as Jank's own screen and takes the keys (uiscreen.c, uis_dn2.c); this part, in the USB client task as the rest
 * of usb_lp.c, hands the Digitone's keys and OPENs over, sends the pages, answers the LINK REQUESTs and keeps LINK MODE as the transport rule says (core/jank_lptrans.h:
 * with peers, Link leads). Nothing here waits. Not tried on hardware: the Digitone's stage 9 is not installed. */
#include "esp_heap_caps.h"
#include "app.h"
#include "jank_dn2menu.h"
#include "jank_dn2rpc.h"
#include "jank_oled_text.h"
#include "uiscreen.h"
#include "usb_host_mode.h"
#include "usb_lp_int.h"

#define REQUEST_MAX JD2_REQUEST_MAX(4u)  /* the longest of these requests: LINK MODE PENDING, 4 bytes of arguments */

static struct jdm s_jdm;
static uint32_t s_page_sent;           /* the number of the ui task's page last sent */
static uint8_t s_page_due;             /* the page goes again even if it was sent (the Digitone refused its placing bits: now plain) */
struct page_out {
    struct jot_page page;
    uint8_t sx[JDL_JANK_PAGE_SX_MAX];
};
static struct page_out *s_po;          /* made at the first page: in PSRAM when the board has it (internal RAM is tight; this task's stack is small) */
static volatile uint8_t s_link_lead;   /* the ui task: LINK TRANSPORT is FOLLOW+LEAD */
static volatile uint8_t s_quantum = 4; /* ... the bar's beats */

void usb_lp_link_feed(int lead, int quantum)
{
    s_link_lead = (uint8_t)(lead != 0);
    s_quantum = (uint8_t)(quantum > 0 && quantum < 256 ? quantum : 4);
}

static int link_leads(void) { return usb_lp_peers() && s_link_lead; }   /* peers there and FOLLOW+LEAD: Link leads, LINK MODE on */

void lpm_init(void) { jdm_init(&s_jdm); }

/* what the ui task needs (the page is open and can be reached, and the OPENs), and the diag */
void lpm_sync(const struct jdl *d)
{
    uis_dn2_open(s_jdm.open && jdl_is_live(d) && d->jank_ok, s_jdm.opens);
    g_hostdiag.menu_open = s_jdm.open;
    g_hostdiag.menu_why = s_jdm.reason;
    g_hostdiag.menu_ok = d->jank_ok;
    g_hostdiag.menu_align = d->align_ok;
    g_hostdiag.link_mode = s_jdm.link;
    g_hostdiag.link_reqs = s_jdm.reqs;
    g_hostdiag.link_cancels = s_jdm.cancels;
}

/* a LINK REQUEST: RECORD first when it was held, the answer, then the session (the ui task starts / stops it: bridge_play may wait for the bridge, this task must not) */
static void link_request(struct jdl *d, uint32_t t, const struct jdl_link_req *r)
{
    uint8_t sx[REQUEST_MAX];
    struct jdm_act a;
    int64_t ub = 0;
    int32_t mpb = 0;
    int known = usb_lp_beat_now(t, &ub, &mpb);
    jdm_on_request(&s_jdm, r, t, link_leads(), midi_is_playing(), jdm_ms_to_bar(known, ub, mpb, s_quantum), &a);
    if (a.record)
        usb_lp_dn2_send(sx, jdl_record(d, t, 1, sx, sizeof sx));
    usb_lp_dn2_send(sx, jdl_link_mode(d, t, a.answer, a.ms, a.id, sx, sizeof sx));
    switch (a.session) {
    case JDM_SESSION_START: usb_lp_session(USB_LP_REQ_START); break;   /* its MIDI Start goes out on the bar line */
    case JDM_SESSION_STOP: usb_lp_session(USB_LP_REQ_STOP); break;     /* the MIDI Stop, then TRANSPORT STOP (usb_lp.c stop_follow) */
    case JDM_SESSION_RESYNC: midi_resync(); break;                     /* the clock already runs: stop, and start again on the bar line */
    case JDM_SESSION_DN2_STOP: {                                       /* nothing runs here: the Digitone's own STOP */
        size_t n = jdl_transport(d, t, JDL_TR_STOP, sx, sizeof sx);
        g_hostdiag.tr_sent += n != 0;
        usb_lp_dn2_send(sx, n);
        break;
    }
    default: break;
    }
}

void lpm_input(struct jdl *d, uint32_t t)
{
    struct jdl_keys keys;
    struct jdl_open o;
    struct jdl_link_req r;
    struct jdl_live8 l8;
    if (jdl_take_keys(d, &keys))
        for (unsigned i = 0; i < keys.n; i++) {
            int b = jdm_button(keys.k[i].code);
            if (b < 0 || (keys.k[i].kind != JDL_KEY_PRESS && keys.k[i].kind != JDL_KEY_RELEASE))
                continue;              /* (LEFT, RIGHT, FUNC and the encoders are not the menu's) */
            uis_dn2_key((enum jo_btn)b, keys.k[i].kind == JDL_KEY_PRESS);
            g_hostdiag.menu_keys++;
        }
    if (jdl_take_open(d, &o))
        jdm_on_open(&s_jdm, &o);
    if (jdl_take_live8(d, &l8)) {      /* an OPEN that was lost: the status says so */
        jdm_on_status(&s_jdm, &l8);
        g_hostdiag.dn2_pages = l8.pages;
        g_hostdiag.dn2_keys_lost = l8.keys_lost;
    }
    if (jdl_take_link_request(d, &r))
        link_request(d, t, &r);
    if (jdl_take_page_resend(d))
        s_page_due = 1;
}

/* the ui task's page, when it changed (or an OPEN asked) and was made after the last OPEN; whole, or at the next pass when the FIFO has no room */
static void page_out(struct jdl *d, uint32_t t)
{
    uint32_t seq, opens;
    if (!s_jdm.open || !d->jank_ok)
        return;
    if (!s_po) {
        s_po = heap_caps_malloc(sizeof *s_po, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_po)
            s_po = heap_caps_malloc(sizeof *s_po, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!s_po)
            return;
    }
    if (!uis_dn2_page(&s_po->page, &seq, &opens) || opens != s_jdm.opens || (seq == s_page_sent && !s_page_due))
        return;
    if (!usb_lp_dn2_try(s_po->sx, jdl_jank_page(d, t, &s_po->page, s_po->sx, sizeof s_po->sx)))
        return;
    s_page_sent = seq;
    s_page_due = 0;
    g_hostdiag.menu_pages++;
}

void lpm_pump(struct jdl *d, uint32_t t, int live, int live_start)
{
    uint8_t sx[REQUEST_MAX];
    if (live_start)
        jdm_live_start(&s_jdm);
    if (live) {
        usb_lp_dn2_send(sx, jdl_probe_jank(d, t, sx, sizeof sx));   /* once per connection: is the page open, is stage 9 there */
        if (d->link_ok) {
            int op = jdm_link_step(&s_jdm, t, link_leads(), midi_is_playing(), d->p8_valid, d->p8.link_on);
            if (op != JDM_SEND_NONE && !usb_lp_dn2_try(sx, jdl_link_mode(d, t, (uint8_t)op, 0, 0, sx, sizeof sx)))
                jdm_link_unsent(&s_jdm);   /* (no room: again at the next pass) */
        }
        page_out(d, t);
    }
    lpm_sync(d);
}
