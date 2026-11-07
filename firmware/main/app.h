/* SPDX-License-Identifier: GPL-3.0-only */
/* The firmware's modules. bridge.c owns the core's state (s_lock); radio, usb and midi know nothing of each other. */
#ifndef APP_H
#define APP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "jank_clock.h"
#include "jank_core.h"
#include "jank_ui.h"

/* store.c: settings in NVS */
void store_load(struct jk_cfg *c);
esp_err_t store_save(const struct jk_cfg *c);

/* reset_button.c: a board whose only button is RST. 1 at the second reset within the window: switch the network */
int reset_button_double(void);
esp_err_t reset_button_arm(void);

/* radio.c: raw 802.11 on one channel */
typedef void (*radio_rx_fn)(const uint8_t *frame, size_t len, int8_t rssi);
esp_err_t radio_start(const uint8_t mac[6], uint32_t channel, radio_rx_fn rx);
esp_err_t radio_channel(uint32_t channel);
esp_err_t radio_set_mac(const uint8_t mac[6], uint32_t channel);
esp_err_t radio_tx(const uint8_t *frame, size_t len);
/* a normal network instead of MODE JAM's air: Ethernet frames both ways */
typedef void (*radio_eth_fn)(const uint8_t *eth, size_t len);
esp_err_t radio_start_network(const uint8_t mac[6], const char *ssid, const char *pass, radio_eth_fn rx);
esp_err_t radio_network_tx(const uint8_t *eth, size_t len);
esp_err_t radio_network_set_mac(const uint8_t mac[6]);
int radio_network_up(void);
void radio_network_info(int8_t *rssi, uint8_t *channel);

#if CONFIG_JANK_USB_NET
/* usb.c: CDC-NCM + USB MIDI, whose SysEx carries the control line */
typedef void (*usb_frame_fn)(const uint8_t *eth, size_t len);
typedef void (*usb_free_fn)(void *token);
typedef void (*usb_line_fn)(const uint8_t *data, size_t n);
esp_err_t usb_start(const uint8_t mac[6], usb_frame_fn rx, usb_free_fn tx_done, usb_line_fn serial_rx);
int usb_up(void);
esp_err_t usb_send(const uint8_t *eth, size_t len, void *token);   /* tx_done(token) follows when ESP_OK */
/* usb_midi_host.c: the clock to USB-MIDI devices, when the port started as host */
esp_err_t usb_midi_host_start(void);
void usb_midi_host_send(const uint8_t *packets, size_t n);
bool usb_midi_isr(const uint8_t *bytes, size_t n);                  /* from the timer interrupt -> a task was woken */
#else
/* serial.c: control and the test log on UART0, at the board's USB-serial chip. No computer is ever on the bridge */
esp_err_t serial_start(void);
static inline int usb_up(void) { return 0; }
void bridge_group_frames(uint32_t out[JK_GROUP_MAX + 1u]);
void radio_unicast_results(uint32_t *ok, uint32_t *fail);
#endif
void usb_serial_write(const char *text, size_t n);                 /* the control line, on either transport */

/* midi.c: the free-running microsecond timer and the 24 PPQN output */
esp_err_t midi_start(void);
int64_t midi_now_us(void);
void midi_set_quantum(uint32_t quantum);
void midi_publish(const struct jc_map *m);
void midi_want(int play);
void midi_resync(void);
void midi_stats(uint32_t *ticks, uint32_t *resyncs);

/* bridge.c */
esp_err_t bridge_start(const struct jk_cfg *cfg, const uint8_t mac[6], const uint8_t peer_mac[6]);
void bridge_apply(const struct jk_cfg *cfg);                 /* a setting changed */
void bridge_snapshot(struct jk_snapshot *s);
void bridge_away(int away);                                  /* the radio is on another channel: nothing to send */
void bridge_note_scan_groups(struct js_scan *out);
void bridge_table_start(void);                               /* frames of every group go in a table from here .. */
void bridge_table_stop(struct js_table *out);                /* .. to here */
uint32_t bridge_peers_heard(void);
int bridge_beat(int64_t *beat_ub, int32_t *mpb);    /* 1 in a session, with its beat now */
uint32_t bridge_ms(void);

/* ui.c: button, LED, group scan */
esp_err_t ui_start(void);
esp_err_t ui_scan_table(struct js_table *out);               /* blocks about 1.05 s; joins nothing */

/* ctl.c: the control protocol on the serial */
esp_err_t ctl_start(void);
void ctl_serial_rx(const uint8_t *data, size_t n);

/* main.c: the settings' one owner */
void app_cfg_get(struct jk_cfg *c);
void app_cfg_set(const struct jk_cfg *c);
#endif
