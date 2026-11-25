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
#include "jank_ota.h"
#include "jank_ui.h"
#include "jank_wsvc.h"

/* power.c: the first line of app_main. On the lcd2: HOST_EN, CLK, RST low, then V5_EN high. Other boards: nothing */
void power_early_init(void);

/* store.c: settings in NVS */
void store_load(struct jk_cfg *c);
esp_err_t store_save(const struct jk_cfg *c);
void store_load_nets(struct jk_nets *s);                     /* the saved Wi-Fi networks (key "nets"); none stored or unreadable: empty */
esp_err_t store_save_nets(const struct jk_nets *s);

/* radio.c: raw 802.11 on one channel */
typedef void (*radio_rx_fn)(const uint8_t *frame, size_t len, int8_t rssi);
esp_err_t radio_start(const uint8_t mac[6], uint32_t channel, radio_rx_fn rx);
esp_err_t radio_channel(uint32_t channel);
esp_err_t radio_set_mac(const uint8_t mac[6], uint32_t channel);
esp_err_t radio_tx(const uint8_t *frame, size_t len);
/* a normal network instead of MODE JANK's air: Ethernet frames both ways */
typedef void (*radio_eth_fn)(const uint8_t *eth, size_t len);
/* an ordinary station on the saved network it hears best (app_nets_get, core/jank_nets.h jk_nets_pick): it scans at the
 * start and whenever a connection fails or is lost and does not come back, skipping a network that failed this round */
esp_err_t radio_start_network(const uint8_t mac[6], radio_eth_fn rx);
esp_err_t radio_network_tx(const uint8_t *eth, size_t len);
esp_err_t radio_network_set_mac(const uint8_t mac[6]);
int radio_network_up(void);
void radio_network_info(int8_t *rssi, uint8_t *channel);
void radio_network_ssid(char *out, size_t cap);              /* the saved network joined; "" while none is */

/* usb.c: CDC-NCM + USB MIDI, whose SysEx carries the control line */
typedef void (*usb_frame_fn)(const uint8_t *eth, size_t len);
typedef void (*usb_free_fn)(void *token);
typedef void (*usb_line_fn)(const uint8_t *data, size_t n);
esp_err_t usb_start(const uint8_t mac[6], usb_frame_fn rx, usb_free_fn tx_done, usb_line_fn serial_rx);
int usb_up(void);
/* wsvc.c: the Wi-Fi diagnostics service (core/jank_wsvc.h), MODE WIFI only: the bridge task and the control task meet in its queues */
#define WSVC_NOT_OURS 0
#define WSVC_SEND_ARP 1                /* an ARP reply is in out (*out_len bytes): send it */
#define WSVC_CONSUMED 2                /* a request, queued for the control task: nothing to send */
esp_err_t wsvc_start(void);
int wsvc_input(const uint8_t mac[6], uint32_t ip, uint32_t mask, const uint8_t *eth, size_t len, uint8_t *out, size_t out_cap, size_t *out_len);
void ctl_wake(void);                   /* ctl.c: the control task looks at the Wi-Fi request queue now, not at its next tick */
size_t wsvc_next_frame(const uint8_t mac[6], uint32_t ip, uint8_t *out, size_t cap);                  /* the next answer datagram's frame, 0: none */
int wsvc_take_request(struct jw_req *rq);                                                              /* control task */
int wsvc_put_reply(const struct jw_req *to, unsigned index, int last, const char *text, size_t n);   /* control task: 1 queued */
/* the signed update's datagrams (core/jank_wota.h): the bridge task copies one into the single slot (wsvc_input), the control task takes it, and
 * answers with raw datagrams. Accepted only while a key is set (wsvc_binary_enable). */
void wsvc_binary_enable(int on);
int wsvc_take_binary(struct jw_req *rq, const uint8_t **frame, size_t *n);                             /* control task: 1, the frame in the slot */
void wsvc_release_binary(void);                                                                        /* control task: the slot is free again */
int wsvc_put_raw(const struct jw_req *to, const uint8_t *data, size_t n);                              /* control task: a datagram as it is, 1 queued */
/* wota.c: the signed Wi-Fi update (key in flash, the control task's handling of the frames) */
void wota_start(void);                                                                                 /* wsvc_start: loads the key, enables the frames if there is one */
bool wota_key_line(const char *line);                                                                  /* "ota key ...": handled and answered -> true */
void wota_serve(void);                                                                                 /* control task, once a pass */
int usb_is_host(void);                                               /* the USB host stack is the one that started (the LED's double flash) */
int usb_lp_transport_request(void);                                 /* host mode: 1 once when the Launchpad asked to start / stop (usb_lp.c, any task); the ui task acts on it */
void usb_lp_beat_feed(int known, int64_t beat_ub, int32_t mpb);      /* host mode: the session's beat (bridge_beat, sampled by the ui task) for the step modes' playhead (usb_lp.c) */
size_t hostdiag_text(char *out, size_t cap);                         /* hostdiag.c: what the last host session saw, for `diag` */
int usb_host_spoke(void);                                            /* a control line came from the computer since the start */
void usb_beat_line(const char *text, size_t n);                      /* a beat report: SysEx F0 7D 'J' 'B' 0x10 <text> F7, dropped when full */
esp_err_t usb_send(const uint8_t *eth, size_t len, void *token);   /* tx_done(token) follows when ESP_OK */
/* usb_midi_host.c: the clock to USB-MIDI devices, when the port started as host */
esp_err_t usb_midi_host_start(void);
void usb_midi_host_send(const uint8_t *packets, size_t n);
bool usb_midi_isr(const uint8_t *bytes, size_t n);                  /* from the timer interrupt -> a task was woken */
/* restartwhy.c: say why before every esp_restart (at most 23 characters); restart_why_default keeps a reason already given; restart_why_boot at the start reads what the
 * last restart said ("none": nothing was said: a power-on, a panic, a watchdog); `diag` prints it as last_restart= */
void restart_why_set(const char *why);
void restart_why_default(const char *why);
void restart_why_boot(void);
const char *restart_why_last(void);
void usb_midi_clock_force(int on);                                 /* host mode: the Launchpad started the transport: the clock goes out even without peers (usb.c) */
void usb_midi_clock_peers(int has_peers);                         /* host mode: the clock only while there are peers on the Jank / Link network (usb_lp.c) */
void usb_midi_clock(int on);                                     /* USB MIDI CLOCK (setting usbmidi): off, none of it to USB */
void usb_serial_write(const char *text, size_t n);                 /* the control line, as SysEx on the MIDI port */

/* usb_audio.c, beat.c: the beat-follow bench build (README): the computer's audio into the beat tracker, into the Link session */
struct jank_peer;
esp_err_t beat_start(void);
void beat_stats(struct jk_beat *out);
uint32_t beat_loop_max_us(void);                     /* the beat task's longest pass since boot, us */
uint8_t beat_bar_state(void);
/* AUDIO mode (the tracker on and its source sounding, core/jank_live.h) and the tracker locked on a steady beat; cheap
 * (the bridge's task asks it at every step) */
void beat_state(uint8_t *audio, uint8_t *locked);
/* LOW CONFIDENCE (core/jank_conf.h), in AUDIO mode: the gate closed (STOP / START under the threshold), 0.5 s of silence,
 * and the confidence at the threshold + 5 (core/jank_hold.h jho_in gated, silent, conf_open); cheap, for the bridge's step */
void beat_gate(uint8_t *gated, uint8_t *silent, uint8_t *conf_open);
/* AUDIO mode's fold (core/jank_fold.h): dir +1 x2, -1 /2, 0 back to 1; taken by the beat task within 5 ms. Returns the
 * new exp (fold 2^exp), -99 in LINK mode */
int beat_fold(int dir);
void beat_fold_resync(void);                                /* Start after Stop: fold 1, as a resync */
int beat_bar_settling(void);                                /* a Start's / a fold's bar placement not made yet (M2k) */
/* NUDGE BEAT (core/jank_beatlink.h jbl_nudge; the control line's `nudge beat`, the device's NO + UP/DOWN): half beats,
 * +: Link's beats come earlier. The beat task applies it at its next pass and this waits for that (up to ~200 ms; call it
 * from a task that may block, never from the beat or bridge task). *total: the user's nudges after it, half beats; view
 * (may be NULL, cap bytes): the downbeat estimator before it, "bar=..\nbar_phi=..\nbar_count=..\nbar_scores=a,b,c,d\n". A
 * nudge sets AUTO BAR to MANUAL until its reset or AUTO BAR picked again; the nudges clear when the tracker starts over or
 * stops (AUTO BAR's reset events) */
enum beat_nudge_rc { BEAT_NUDGE_OK = 0, BEAT_NUDGE_LINK = -1, BEAT_NUDGE_NO_BEAT = -2, BEAT_NUDGE_LIMIT = -3, BEAT_NUDGE_BUSY = -4 };
int beat_nudge(int halves, int8_t *total, char *view, size_t cap);
/* NUDGE MS for the device's YES + UP/DOWN is core/jank_core.h jk_nudge_ms on the settings (app_cfg_get, jk_nudge_ms,
 * app_cfg_set), as the control line's `nudge ms` does */                               /* AUTO BAR: 0 off, 1 listening, 2 locked, 3 manual (jb_down.h) */

/* linein.c: the LINE IN (board.h BOARD_HAS_LINEIN) through ADC1 continuous mode into the tracker's ring (usb_audio.h), and
 * the cell's level from the same ADC pattern. Boards without it: linein_start does nothing, no battery percent. */
esp_err_t linein_start(void);
int linein_batt_pct(void);                                   /* -1: no cell, or no reading yet */
void linein_cell_step(uint32_t now_ms);                      /* boards with a cell pin and no line in: a oneshot read every few seconds (ui task, core 0); else nothing */
void linein_report(char *out, size_t cap);                   /* the `line` command's answer, ending in ok */

/* midi.c: the free-running microsecond timer and the 24 PPQN output */
esp_err_t midi_start(void);
int64_t midi_now_us(void);
uint32_t midi_late_max_us(void);                    /* the clock alarm's worst lateness since boot, us */
uint32_t midi_busy_cycles(void);                   /* running count of CPU cycles spent in the MIDI clock interrupt (core 1), wraps at 2^32 */
int64_t midi_now_us_isr(void);                       /* the same, from an IRAM interrupt with the cache off; -1 before midi_start */
void midi_set_quantum(uint32_t quantum);
void midi_set_clk_rate(unsigned rate);                  /* the CLK RATE setting 0..4 for the CLK jack (mini 3; no effect elsewhere) */
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
/* YES / `play` / `stop`. AUDIO mode: Start / Stop of the outputs (the hold, core/jank_hold.h), always 0. LINK mode: the
 * session: 0 a start armed for the next bar / stopped; -1 no settled session; `play` also ends a hold */
int bridge_play(int play);
/* LINK TRANSPORT FOLLOW (0) or FOLLOW+LEAD (1, the default) while the transport setting is LINK (AUTO = IGNORE): the
 * device UI's choice (branch fw/ui keeps it in its prefs: it calls this when it loads or changes it) */
void bridge_set_lead(int lead);
/* AUDIO mode's Stop holds the Link session (FOLLOW+LEAD): for the beat tracker's step, the bridge's lock held */
int bridge_session_held(void);
/* the user's layer is stopped (power-on, or the user's Stop: core/jank_hold.h) */
int bridge_user_stopped(void);
/* the session plays (joined, or AUDIO mode's own), or (transport LINK) the MIDI clock has a start out: no flash writes
 * now (updates excepted; settings saved later; the timing audit). Lock-free (bridge.c) */
int bridge_is_playing(void);
void bridge_diag(char *out, size_t cap);             /* the Link clock filter, for `diag` */
void panic_info_take(void);                         /* at boot: the last panic's record moves out of RTC memory (panic_info.c) */
size_t panic_info_text(char *out, size_t cap);      /* its `diag` lines, nothing when there was none */
const char *panic_info_brief(void);                 /* "<task>@0x4200abcd" for the ABOUT screen and `about`; "" when there was none (stays valid) */
size_t panic_info_file(char *out, size_t cap);      /* PANIC.TXT's text (JK_PANIC_FILE_MAX is enough); 0 and "" when there was none */
void app_wipe_settings(void);                        /* `wipe settings`: every stored setting erased, then a restart (main.c) */
void app_diag(char *out, size_t cap);                /* restarts and the last reset's reason, for `diag` */
/* fn on the Link peer, with the bridge's lock held and the host time now (the beat tracker steers the session through it) */
void bridge_with_peer(void (*fn)(struct jank_peer *p, int64_t now, void *ctx), void *ctx);
uint32_t bridge_ms(void);

/* ui.c: button, LED, group scan */
esp_err_t ui_screen_early(void);                             /* app_main, before the radio: the screen found and switched on */
esp_err_t ui_start(void);
esp_err_t ui_scan_table(struct js_table *out);               /* blocks about 1.05 s; joins nothing */

/* ctl.c: the control protocol on the serial */
esp_err_t ctl_start(void);
void ctl_serial_rx(const uint8_t *data, size_t n);           /* text of the control line, from the MIDI port */
uint32_t ctl_beats(void);                                    /* counts up while the task runs */

/* ota.c: firmware updates over the control line, and the rollback's confirmation */
bool ota_busy(void);                                         /* a transfer over MIDI is running */
bool ota_line(const char *line);                             /* an "ota ..." line: handled and answered -> true */
void ota_idle(void);                                         /* called about once a second: drops a silent transfer */
void ota_validate_start(void);                               /* a new image confirms itself once USB and the control task work */
void ota_mark_valid(void);
/* the Wi-Fi update's use of the same engine (wota.c; answers as an enum jo_err, ota_net_offset() is the offset the answer carries) */
enum jo_err ota_net_begin(uint32_t size, uint32_t crc, uint8_t target);
enum jo_err ota_net_data(uint32_t offset, uint32_t crc, const uint8_t *data, size_t n);
enum jo_err ota_net_end(void);                                       /* JO_OK: answer, then ota_net_restart() */
void ota_net_restart(void);
void ota_net_abort(void);
uint32_t ota_net_offset(void);
void usbrole_hint_stay(void);                                        /* usbrole.c: the restart that follows an update brings a unit that runs as host back as host at once */
unsigned ota_net_confirm(void);                                      /* 0, or JWO_S_NOT_PENDING / JWO_S_TOO_EARLY (jank_wota.h) */

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
void app_cfg_flush(int force);
void app_nets_get(struct jk_nets *s);                        /* the saved Wi-Fi networks */
int app_nets_forget(const char *ssid);                       /* 0 forgotten and saved, -1 not saved */                               /* a deferred save, once stopped (force: a restart follows) */
#endif
