/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode and the port's role: usb_hostlib.c, usb_midi_host.c, usb_uac_host.c, usbrole.c (and pump.c on boards
 * with the CLK / RST charge pump). Apart from app.h so that the integrator's merge stays small. */
#ifndef USB_HOST_MODE_H
#define USB_HOST_MODE_H
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "jank_core.h"
#include "jank_hostdiag.h"
#include "jank_lpapp.h"
#include "usb/usb_host.h"

/* usb_hostlib.c: the USB Host Library, installed once; its event task */
esp_err_t usb_hostlib_start(void);
int usb_hostlib_up(void);
void usb_host_product_name(usb_device_handle_t h, char *out, size_t cap);
void usb_host_list_add(struct jk_usb *u, const char *name, int midi, int audio);

/* hostdiag.c: what the host clients saw (core/jank_hostdiag.h), filled from the client task, saved to flash for `diag` */
extern struct jhd g_hostdiag;
void hostdiag_start(void);
void hostdiag_start_saver(void);                           /* the flash-saver task, made once the start-up is through */
esp_err_t hostdiag_skiplist_save(const struct jhf_set *s); /* the devices to leave alone behind a hub, kept in flash (read again by hostdiag_start) */
void hostdiag_trace_reset(void);                           /* a host start begins: the stage trace (RTC memory) starts afresh */
void hostdiag_mark(uint8_t step);                          /* the internal heap, at the stage (enum jhd_step) about to run; host mode only */
void hostdiag_fail(uint8_t step, esp_err_t e);             /* a host start failed at step (enum jhd_step): kept in flash now */
void hostdiag_clear_failure(void);                         /* a host start worked: an older failure is cleared */
uint8_t usb_midi_host_start_step(void);                    /* usb_midi_host.c: the step of its start that failed */
uint16_t usb_midi_host_stack_free(void);                   /* its client task's stack never used, bytes */
void usbrole_host_inflight(int on);                        /* usbrole.c: a host run is young (not yet stable): a crash in it is found at the next start */
void usb_host_stage(uint8_t step);                         /* usb.c: which step of the host start (enum jhd_step) is under way, for the hang guard */
void usb_host_boot_done(void);                             /* usb.c: the start-up (USB and radio) is through: the hang guard stands down */
void usbrole_host_failed(uint8_t step, esp_err_t e);       /* usbrole.c: keeps the failure, blocks host until a power cycle, restarts as a device; does not return */

/* usb_midi_host.c (its start and send are in app.h) */
void usb_midi_host_list(struct jk_usb *u);
void usb_midi_host_send_to(enum jlapp_role r, const uint8_t *packets, size_t n);   /* to the device that has that role, if any */
size_t usb_midi_host_room(enum jlapp_role r);                                      /* packets still fitting in its queue; 0 if none */
void usb_midi_host_wake(void);                                                     /* any task: the client task runs its pass now (a page for the Digitone is ready) */

/* usb_lp.c: Launchpad X -> Digitone II, Note mode (core/jank_lpapp.h), called from usb_midi_host.c's client task only */
void usb_lp_attach(enum jlapp_role r);
void usb_lp_detach(enum jlapp_role r);
void usb_lp_rx(enum jlapp_role r, const uint8_t *packets, size_t n);               /* 4-byte USB-MIDI packets the device sent */
void usb_lp_pump(void);                                                          /* waiting packets for the Digitone, more lights: as the queues have room */
uint32_t usb_lp_wait_ticks(void);                                                /* how long the client task may sleep (portMAX_DELAY: until an event) */

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
