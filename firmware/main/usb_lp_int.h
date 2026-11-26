/* SPDX-License-Identifier: GPL-3.0-only */
/* Inside host mode's Launchpad / Digitone part: what usb_lp.c shares with usb_lpmenu.c. The USB client task's only. */
#ifndef USB_LP_INT_H
#define USB_LP_INT_H
#include <stddef.h>
#include <stdint.h>
#include "jank_dn2live.h"

/* usb_lp.c */
int usb_lp_dn2_try(const uint8_t *sx, size_t n);           /* a SysEx for the Digitone, whole into its FIFO: 1; 0 no room (or no Digitone) */
void usb_lp_dn2_send(const uint8_t *sx, size_t n);         /* ... and one that found no room counted as dropped (diag live dropped:) */
int usb_lp_beat_now(uint32_t t, int64_t *beat_ub, int32_t *mpb);   /* the session's beat now and its tempo (the ui task's samples); 0 not known */
int usb_lp_peers(void);                                    /* the ui task's peers: Link leads the transport with them */
void usb_lp_session(int req);                              /* enum usb_lp_req: what the ui task is to do with Jank's session */

/* usb_lpmenu.c: Jank's menu on the Digitone's screen, and LINK MODE (the Digitone's stage 9, core/jank_dn2menu.h) */
void lpm_init(void);                                       /* a new connection: the Digitone says again whether its page is open */
void lpm_input(struct jdl *d, uint32_t t);                 /* after each SysEx from the Digitone: its keys, OPEN, a LINK REQUEST, the status */
void lpm_pump(struct jdl *d, uint32_t t, int live, int live_start);   /* each pass: LINK MODE, the page */
void lpm_sync(const struct jdl *d);                        /* the ui task's flags and the diag */
#endif
