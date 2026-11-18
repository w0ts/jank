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
void uis_start(uint32_t now);           /* the ui task's start */
void uis_button(enum jo_btn b, int down, uint32_t now);
void uis_boot_screen(void);             /* BOOT / BOOTLOADER, before download mode */
/* the timers, the frames (10 a second, at once after a button), the deferred save; *led filled at each frame */
void uis_step(uint32_t now, struct uis_led *led);
uint8_t uis_contrast(void);
struct jk_sys;
void uis_sys_step(uint32_t now);     /* samples the ABOUT numbers (rate-limited), from the ui task every loop */
int uis_lead_get(void);              /* LINK TRANSPORT FOLLOW+LEAD (1) / FOLLOW (0), the menu's preference, for the control line (any task) */
void uis_lead_set(int on);           /* set lead on|off: applied and saved (400 ms rule) by the ui task */
void uis_sys(struct jk_sys *out);    /* the ABOUT screen's sampled numbers, for the control line's `about` (any task) */
#endif
