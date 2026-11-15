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

/* radio.c: raw 802.11 on one channel */
typedef void (*radio_rx_fn)(const uint8_t *frame, size_t len, int8_t rssi);
esp_err_t radio_start(const uint8_t mac[6], uint32_t channel, radio_rx_fn rx);
esp_err_t radio_channel(uint32_t channel);
esp_err_t radio_set_mac(const uint8_t mac[6], uint32_t channel);
esp_err_t radio_tx(const uint8_t *frame, size_t len);
/* a normal network instead of MODE JANK's air: Ethernet frames both ways */
typedef void (*radio_eth_fn)(const uint8_t *eth, size_t len);
esp_err_t radio_start_network(const uint8_t mac[6], const char *ssid, const char *pass, radio_eth_fn rx);
esp_err_t radio_network_tx(const uint8_t *eth, size_t len);
esp_err_t radio_network_set_mac(const uint8_t mac[6]);
int radio_network_up(void);
void radio_network_info(int8_t *rssi, uint8_t *channel);

/* usb.c: CDC-NCM + USB MIDI, whose SysEx carries the control line */
typedef void (*usb_frame_fn)(const uint8_t *eth, size_t len);
typedef void (*usb_free_fn)(void *token);
typedef void (*usb_line_fn)(const uint8_t *data, size_t n);
esp_err_t usb_start(const uint8_t mac[6], usb_frame_fn rx, usb_free_fn tx_done, usb_line_fn serial_rx);
int usb_up(void);
int usb_host_spoke(void);                                            /* a control line came from the computer since the start */
void usb_beat_line(const char *text, size_t n);                      /* a beat report: SysEx F0 7D 'J' 'B' 0x10 <text> F7, dropped when full */
esp_err_t usb_send(const uint8_t *eth, size_t len, void *token);   /* tx_done(token) follows when ESP_OK */
/* usb_midi_host.c: the clock to USB-MIDI devices, when the port started as host */
esp_err_t usb_midi_host_start(void);
void usb_midi_host_send(const uint8_t *packets, size_t n);
bool usb_midi_isr(const uint8_t *bytes, size_t n);                  /* from the timer interrupt -> a task was woken */
void usb_serial_write(const char *text, size_t n);                 /* the control line, as SysEx on the MIDI port */

/* usb_audio.c, beat.c: the beat-follow bench build (README): the computer's audio into the beat tracker, into the Link session */
struct jank_peer;
esp_err_t beat_start(void);
void beat_stats(struct jk_beat *out);
uint8_t beat_bar_state(void);                               /* AUTO BAR: 0 off, 1 listening, 2 locked, 3 manual (jb_down.h) */

/* linein.c: the LINE IN (board.h BOARD_HAS_LINEIN) through ADC1 continuous mode into the tracker's ring (usb_audio.h), and
 * the cell's level from the same ADC pattern. Boards without it: linein_start does nothing, no battery percent. */
esp_err_t linein_start(void);
int linein_batt_pct(void);                                   /* -1: no cell, or no reading yet */
void linein_report(char *out, size_t cap);                   /* the `line` command's answer, ending in ok */

/* midi.c: the free-running microsecond timer and the 24 PPQN output */
esp_err_t midi_start(void);
int64_t midi_now_us(void);
int64_t midi_now_us_isr(void);                       /* the same, from an IRAM interrupt with the cache off; -1 before midi_start */
void midi_set_quantum(uint32_t quantum);
void midi_publish(const struct jc_map *m);
void midi_want(int play);
int midi_start_pending(void);                        /* a MIDI start waits for its bar line */
void midi_resync(void);
void midi_stats(uint32_t *ticks, uint32_t *resyncs);
int midi_is_playing(void);                           /* a MIDI start went out (or waits for its bar line), no stop since */

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
int bridge_tempo(int32_t mpb);                       /* 0 set; -1 no session to set it in */
int bridge_play(int play);                           /* 0 a start armed for the next bar / stopped; -1 no settled session; -2 AUDIO mode */
/* the joined session plays, or (transport LINK) the MIDI clock has a start out: no flash writes now (updates refused,
 * settings saved later; the timing audit). Lock-free (bridge.c) */
int bridge_is_playing(void);
void bridge_diag(char *out, size_t cap);             /* the Link clock filter, for `diag` */
void app_diag(char *out, size_t cap);                /* restarts and the last reset's reason, for `diag` */
/* fn on the Link peer, with the bridge's lock held and the host time now (the beat tracker steers the session through it) */
void bridge_with_peer(void (*fn)(struct jank_peer *p, int64_t now, void *ctx), void *ctx);
uint32_t bridge_ms(void);

/* ui.c: button, LED, group scan */
esp_err_t ui_start(void);
esp_err_t ui_scan_table(struct js_table *out);               /* blocks about 1.05 s; joins nothing */

/* ctl.c: the control protocol on the serial */
enum ctl_src { CTL_SRC_USB, CTL_SRC_BLE, CTL_SRC_COUNT };
esp_err_t ctl_start(void);
void ctl_serial_rx(const uint8_t *data, size_t n);           /* = ctl_line_rx(CTL_SRC_USB, ..) */
void ctl_line_rx(enum ctl_src src, const uint8_t *data, size_t n);   /* text of the control line, from src */
void ctl_reply(const char *text, size_t n);                  /* the answer to the line being run, to where it came from */
uint32_t ctl_beats(void);                                    /* counts up while the task runs */

#if CONFIG_JANK_BLE_MIDI
/* ble_midi.c: a BLE MIDI peripheral for a phone: the control line's SysEx and the HOME line. Bluetooth is on only for a
 * pair window or a connected phone (core/jank_phone.h) */
esp_err_t ble_midi_start(const uint8_t mac[6], uint32_t mode);   /* mode: the `ble` setting, enum jk_ble */
void ble_midi_write(const char *text, size_t n);             /* control line text, as SysEx frames; waits for buffers */
int ble_midi_pair(void);                                     /* open a pair window: 0, or -1 with the setting off */
void ble_midi_close(void);                                   /* the window and the phone's link off now */
void ble_midi_setting(uint32_t mode);                        /* the `ble` setting changed */
void ble_midi_state(uint8_t *state, uint32_t *left_s);       /* enum jph_state, the window's seconds left */
int ble_midi_advertising(void);                              /* a pair window is open, no phone yet (the LED blinks) */
void ble_midi_diag(char *out, size_t cap);
#endif

/* ota.c: firmware updates over the control line, and the rollback's confirmation */
bool ota_busy(void);                                         /* a transfer over MIDI is running */
bool ota_line(const char *line);                             /* an "ota ..." line: handled and answered -> true */
void ota_idle(void);                                         /* called about once a second: drops a silent transfer */
void ota_validate_start(void);                               /* a new image confirms itself once USB and the control task work */
void ota_mark_valid(void);

/* drive.c: the JANK drive, made up (core/jank_vdrive.h): jank.html on it, a jank.bin copied onto it streams into the spare
 * app slot and is installed */
esp_err_t drive_start(void);
/* the spare app slot's one writer at a time: the drive's stream or an update over MIDI (taken under a lock) */
enum slot_user { SLOT_FREE = 0, SLOT_DRIVE, SLOT_MIDI };
bool slot_take(enum slot_user who);                          /* true: who has it now (or had it already) */
void slot_give(enum slot_user who);
#ifdef JANK_CLOCK_PROBE
esp_err_t probe_start(void);                                 /* clkprobe.c: a test build's pin probe */
void probe_mark(int64_t due, int64_t isr_now);
size_t probe_report(char *out, size_t cap, int reset);
#endif
void drive_note(const char *slot, const char *text);         /* UPDATE.TXT at the next start, if slot runs (else: rolled back) */
void drive_note_forget(void);
void drive_state(char *out, size_t cap);                     /* `drive`: ok, or the start step that failed; the stream's state */

/* download.c: the ROM's download mode, with USB */
void usb_to_rom_download(void);                              /* does not return */

/* main.c: the settings' one owner */
void app_cfg_get(struct jk_cfg *c);
void app_cfg_set(const struct jk_cfg *c);                     /* saved first (or once stopped, while playing), then published */
void app_cfg_flush(int force);                               /* a deferred save, once stopped (force: a restart follows) */
#endif
