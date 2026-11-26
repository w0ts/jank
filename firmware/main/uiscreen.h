/* SPDX-License-Identifier: GPL-3.0-only */
/* The device's own UI on the firmware (uiscreen.c): the screens, the menus, the four buttons; the ui task's only. */
#ifndef UISCREEN_H
#define UISCREEN_H
#include <stdint.h>
#include "jank_oled.h"

/* what the LED language needs from the same state the screen shows (the beat itself comes from bridge_beat) */
struct uis_led {
    uint8_t valid, armed, running, joined, low_conf, bar_locked, on_usb;
    int8_t batt_pct;
};

void uis_init(void);                    /* app_main: the stored screen settings */
int uis_open_screen(void);              /* app_main, before the radio: the screen found and switched on -> 1 */
int uis_has_screen(void);
/* a short message for the screen (the one-second overlay box, as for a setting that was changed); any task, a screen only: the ui task shows it. Newest wins, at most 23 characters */
void uis_message(const char *text);
void uis_start(uint32_t now);           /* the ui task's start */
void uis_button(enum jo_btn b, int down, uint32_t now);
void uis_boot_screen(void);             /* BOOT / BOOTLOADER, before download mode */
/* the timers, the frames (10 a second, at once after a button), the deferred save; *led filled at each frame */
void uis_step(uint32_t now, struct uis_led *led);
uint8_t uis_contrast(void);
struct jk_sys;
void uis_sys_step(uint32_t now);     /* samples the ABOUT numbers (rate-limited), from the ui task every loop */
int uis_lead_get(void);              /* LINK TRANSPORT FOLLOW+LEAD (1) / FOLLOW (0), the menu's preference, for the control line (any task) */
int uis_started_get(void);           /* the user's last Start (1) / Stop (0) of the outputs, as stored (read at boot) */
void uis_started_set(int on);        /* the user pressed Start (1) / Stop (0): stored by the ui task (400 ms rule) */
void uis_lead_set(int on);           /* set lead on|off: applied and saved (400 ms rule) by the ui task */
int uis_sleep_get(void);             /* SCREEN OFF (0 NEVER, 1 30 S, 2 1 MIN, 3 5 MIN) and BRIGHTNESS (10..100 %), the menu's own preferences, for the control line (any task) */
int uis_bright_get(void);
void uis_sleep_set(int v);           /* set sleep / set bright: applied and saved (400 ms rule) by the ui task */
void uis_bright_set(int v);
void uis_sys(struct jk_sys *out);    /* the ABOUT screen's sampled numbers, for the control line's `about` (any task) */

/* The Digitone's screen as this UI's (uis_dn2.c; owner 2026-11-25, core/jank_oled_text.h; usb_lpmenu.c, in the USB client task, talks to the Digitone): its keys come in
 * through a queue, the page goes out through a buffer with a sequence number. While the page is open the ui task runs the screens also on a unit with no screen */
void uis_dn2_init(void);                                   /* uis_init */
void uis_dn2_key(enum jo_btn b, int down);                 /* client task: one of the Digitone's four keys down / up (dropped when the queue is full) */
void uis_dn2_open(int open, uint32_t opens);               /* client task: the page is open and live control is up; opens counts its OPENs (each: a page afresh) */
struct jot_page;
/* client task: the last page, its number, and the OPENs count it was made for (a page made before the last OPEN is stale); 0 when there is none */
int uis_dn2_page(struct jot_page *out, uint32_t *seq, uint32_t *opens);
int uis_dn2_is_open(void);                                 /* ui task: the page is open */
int uis_dn2_active(void);                                  /* ui task: open, or one of its keys still down: the screens run */
int uis_dn2_page_wanted(void);                             /* ui task: an OPEN asks for a page made afresh */
int uis_dn2_next_key(enum jo_btn *b, int *down);           /* ui task: the next key event, 1; 0 none */
void uis_dn2_publish(const struct jot_page *page);         /* ui task: the page of the state now (kept and sent on only when it changed, or an OPEN asked) */
#endif
