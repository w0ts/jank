/* SPDX-License-Identifier: GPL-3.0-only */
/* Jank's menu on a Digitone II's screen, and LINK MODE (owner 2026-11-25; the Digitone's stage 9, core/jank_dn2live.h): what the page (core/jank_oled_text.h) and the
 * Digitone's keys need between them, and what LINK MODE says and does. Pure, host-tested (tests/test_dn2menu.c); the firmware (usb_lp.c, the USB client task) feeds it
 * what live control brings and sends what it answers.
 *
 *   The page    the Digitone opens it from its SETTINGS (JANK, below SYSTEM) and says so (OPEN); a page goes at each OPEN with open 1 (it is pushed again every
 *               second until one arrives), and whenever the page changes. The Digitone's UP, DOWN, NO and YES are Jank's four buttons, down and up, so holds,
 *               combos and repeats are jank_oled_input.c's own (the Digitone sends no repeats).
 *   LINK MODE   on while Link leads (peers there and LINK TRANSPORT is FOLLOW+LEAD), off otherwise. The Digitone's PLAY / STOP then ask (LINK REQUEST): a start is
 *               answered PENDING with the time to the bar line and Jank's session starts (its MIDI Start goes out on the bar line); RECORD held: the Digitone's
 *               RECORD first. Under Link, PLAY while playing is a STOP (owner 2026-11-25: starts happen on the bar line), answered ACK; Jank's session stops and
 *               the Digitone ends stopped (usb_lp.c stop_follow). A request pushed again (the answer was late) is answered again, not acted on twice. */
#ifndef JANK_DN2MENU_H
#define JANK_DN2MENU_H
#include <stdint.h>
#include "jank_dn2live.h"

#define JDM_LINK_RESEND_MS 1000u       /* LINK MODE again, at most this often, while the Digitone's status says the other */
#define JDM_START_GRACE_MS 500u        /* a start answered PENDING that has not started this long after its bar line is cancelled */
#define JDM_UNSENT 255u
#define JDM_SEND_NONE 254u             /* jdm_link_step: nothing to send */

/* a Digitone key code as one of Jank's buttons (jank_oled.h enum jo_btn), -1 for the keys the menu does not use (LEFT, RIGHT, FUNC, the encoders) */
int jdm_button(uint8_t code);

struct jdm {
    uint8_t open, reason;              /* the Digitone shows the JANK page: its last OPEN (or its status) */
    uint32_t opens;                    /* OPENs with open 1: each wants a page (the ui task renders afresh for it) */
    uint8_t link;                      /* LINK MODE last sent: JDL_LINK_OFF, JDL_LINK_ON, JDM_UNSENT */
    uint32_t link_ms;
    uint8_t req_known, req_id;         /* the last LINK REQUEST taken */
    uint8_t pending;                   /* a start answered PENDING that has not started yet */
    uint32_t pending_until;
    uint32_t reqs, cancels;            /* LINK REQUESTs acted on, CANCELs sent */
};

void jdm_init(struct jdm *m);
/* live control came up (again): LINK MODE is said afresh */
void jdm_live_start(struct jdm *m);
/* an OPEN (pushed, or the read's answer); 1 when a page must go now (open 1) */
int jdm_on_open(struct jdm *m, const struct jdl_open *o);
/* the Digitone's status (protocol 8) says whether the page is open (an OPEN that was lost); 1 when it says open and Jank did not know */
int jdm_on_status(struct jdm *m, const struct jdl_live8 *s);

/* LINK MODE now: JDM_SEND_NONE, or the op to send (JDL_LINK_ON, JDL_LINK_OFF, JDL_LINK_CANCEL). leads: Link leads; playing: Jank's clock runs (or waits for its bar
 * line); dn2_on: the Digitone's own LINK MODE as its status says, when status_known. A start answered PENDING that did not start by its time, or that Link stopped
 * leading meanwhile, is cancelled first */
int jdm_link_step(struct jdm *m, uint32_t now, int leads, int playing, int status_known, int dn2_on);
/* the op jdm_link_step gave could not be sent: it is tried again */
void jdm_link_unsent(struct jdm *m);

enum jdm_session { JDM_SESSION_NONE = 0, JDM_SESSION_START, JDM_SESSION_STOP, JDM_SESSION_RESYNC, JDM_SESSION_DN2_STOP };
struct jdm_act {
    uint8_t record;                    /* RECORD (58) op 1, before the rest */
    uint8_t answer;                    /* JDL_LINK_PENDING (ms, id), JDL_LINK_ACK (id) or JDL_LINK_CANCEL */
    uint16_t ms;
    uint8_t id;
    uint8_t session;                   /* enum jdm_session: START / STOP Jank's session (the ui task), RESYNC (its clock already runs: start again on the bar line, so
                                          the Digitone gets a Start), DN2_STOP (nothing runs on Jank's side: the Digitone's TRANSPORT STOP) */
};
/* a LINK REQUEST: what to answer and do. ms_to_bar: jdm_ms_to_bar's */
void jdm_on_request(struct jdm *m, const struct jdl_link_req *r, uint32_t now, int leads, int playing, uint16_t ms_to_bar, struct jdm_act *a);
/* ms from the session's beat (micro-beats, mpb microseconds a beat) to its next bar line (a multiple of quantum beats); 0 when not known */
uint16_t jdm_ms_to_bar(int known, int64_t beat_ub, int32_t mpb, int quantum);
#endif
