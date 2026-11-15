/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode and the port's role: usb_hostlib.c, usb_midi_host.c, usb_uac_host.c, usbrole.c (and pump.c on boards
 * with the CLK / RST charge pump). Apart from app.h so that the integrator's merge stays small. */
#ifndef USB_HOST_MODE_H
#define USB_HOST_MODE_H
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "jank_core.h"
#include "usb/usb_host.h"

/* usb_hostlib.c: the USB Host Library, installed once; its event task */
esp_err_t usb_hostlib_start(void);
int usb_hostlib_up(void);
void usb_host_product_name(usb_device_handle_t h, char *out, size_t cap);
void usb_host_list_add(struct jk_usb *u, const char *name, int midi, int audio);

/* usb_midi_host.c (its start and send are in app.h) */
void usb_midi_host_list(struct jk_usb *u);

/* usb_uac_host.c: one hosted device's audio into the beat tracker's ring (usb_audio.h uac_feed) */
esp_err_t usb_uac_host_start(void);
void usb_uac_host_list(struct jk_usb *u);        /* its name and format */
int usb_uac_host_id(uint16_t *vid, uint16_t *pid);   /* 1: a device streams, its VID:PID (beat_latency's key, core/jank_lat.h) */

/* usbrole.c: the role at the start, then the role task */
uint8_t usbrole_boot(void);                      /* enum jur_stack to start: reads the setting, the CC chip, VBUS */
esp_err_t usbrole_start(void);                   /* after the stack started */
void usbrole_snapshot(struct jk_usb *u);         /* for `status` */

/* pump.c: the CLK / RST charge pump (CV_RAIL), held by whoever needs it (here: the host's 5 V signal) */
esp_err_t pump_hold(void);
void pump_release(void);
#endif
