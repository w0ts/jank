/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB port's hardware under the role logic (core/jank_usbrole.h): what is on the cable, the port's mode, and jank's own
 * 5 V on VBUS. One implementation per kind of port chip; the board picks one (board.h BOARD_USBPORT_*), or none (the XIAO:
 * no port chip, the role core then runs on the setting alone).
 * - usbport_tusb320.c: jank mini 2.2 battery order: TI TUSB320LI (CC logic, I2C) + the CLK / RST charge pump + HT7550 +
 *   HOST_EN (Q7 / Q8) + Q9.
 * - usbport_mini3.c: jank mini 3 (lcd2): no chip; VBUS sense decides, HOST_EN puts the 5 V presence signal on J4.
 * - a power-bank IC with its own DRP / OTG role logic and boost (under study for the board) would be another file with the
 *   same five calls: attach state and orientation, set role, prepare and switch VBUS out. */
#ifndef USBPORT_H
#define USBPORT_H
#include <stdint.h>
#include "esp_err.h"
#include "board.h"

struct usbport_state {
    uint8_t cc;                          /* enum jur_cc: nothing, a device (jank sources), a host or charger (jank sinks) */
    uint8_t dir;                         /* orientation: 1 CC1, 2 CC2, 0 unknown */
};

struct usbport {
    const char *name;
    uint32_t prep_ms;                    /* the 5 V source is up this long after vbus_prep(1) */
    int irq_gpio;                        /* the chip's interrupt line, active low; -1: polled only */
    uint32_t poll_ms;                    /* read() at least this often; 0: every 500 ms (usbrole.c) */
    int (*open)(void);                   /* at the start, before any stack: 1 the chip answered, 0 none (device only) */
    int (*read)(struct usbport_state *st);   /* 0 ok, -1 not readable (the role core then sees nothing attached) */
    int (*set_mode)(uint8_t mode);       /* enum jur_port_mode: 1 written, 0 already so, -1 error */
    esp_err_t (*vbus_prep)(int on);      /* prepare / release jank's 5 V source */
    void (*vbus_on)(int on);             /* jank's 5 V onto VBUS or off it: any time, at once */
};

/* the board's port, or NULL (no port HAL): usbport_tusb320 (jank mini 2.2) or usbport_mini3 (jank mini 3), by the board table */
const struct usbport *usbport_tusb320(void);
const struct usbport *usbport_mini3(void);
static inline const struct usbport *usbport_board(void)
{
    return BOARD_USBPORT_MINI3 ? usbport_mini3() : BOARD_USBPORT_TUSB320 ? usbport_tusb320() : NULL;
}
#endif
