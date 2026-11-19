/* SPDX-License-Identifier: GPL-3.0-only */
/* The mini 3's USB host gate (firmware/main/usbport_mini3.c puts it on the pins): when HOST_EN may be high. The board has no
 * CC chip: the only sign of a computer or a charger is VBUS sense (the LCD-2's own USB-C, through a 10 k / 15 k divider).
 * A computer or a charger ALWAYS wins: HOST_EN falls in the same step in which VBUS sense reads high, with no debounce, and
 * rises only after VBUS sense has read low without a break for JUH_DEBOUNCE_MS, the setting is HOST, the role core asks for
 * it and the 5 V rail (V5_EN) has been on for JUH_V5_SETTLE_MS. Every other case: HOST_EN low. Pure: no pin, no clock. */
#ifndef JANK_USBHOST_H
#define JANK_USBHOST_H
#include <stdint.h>

#define JUH_DEBOUNCE_MS 500u           /* VBUS sense low this long, without a break, before HOST_EN may rise */
#define JUH_V5_SETTLE_MS 100u          /* V5_EN on this long (the boost's start-up, then U21's supply) before HOST_EN may rise */

struct juh {
    uint8_t vbus;                      /* what the role sees: 1 at once on a high read, 0 only after the debounce */
    uint8_t low;                       /* the last raw read was low */
    uint32_t t_low;                    /* when the raw read went low (or at the start) */
};

struct juh_in {
    uint8_t setting_host;              /* the usb setting is HOST */
    uint8_t vbus_raw;                  /* VBUS sense, as read now: 1 = something powers the LCD-2's USB-C */
    uint8_t want_en;                   /* the role core asks for the 5 V signal (jur_out.vbus_on) */
    uint8_t v5_known;                  /* the 5 V rail is on (the board drives V5_EN at the start); 0: never raise HOST_EN */
    uint32_t v5_since_ms;              /* since when */
    uint32_t now_ms;
};

struct juh_out {
    uint8_t host_en;                   /* the level of HOST_EN */
    uint8_t mode_host;                 /* 1: the port may be a host (setting HOST, nothing on the LCD-2's USB-C); 0: a device */
    uint8_t vbus;                      /* VBUS sense, debounced: 1 = a computer or a charger */
};

/* a start with VBUS high is a device; one with it low counts its debounce from now */
void juh_init(struct juh *h, int vbus_raw, uint32_t now_ms);
void juh_step(struct juh *h, const struct juh_in *in, struct juh_out *out);
#endif
