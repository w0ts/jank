/* SPDX-License-Identifier: GPL-3.0-only */
/* The phone over BLE MIDI (docs/ble.md): when the Bluetooth controller is on, and the HOME line it gets.
 *
 * The user's rulings (2026-11-15): the radio is shared with Wi-Fi and Wi-Fi comes first, so Bluetooth is on only while a
 * phone is connected or a pair window is open. The window is started from the device (three clicks of the button, the
 * control line's `ble pair`), lasts JPH_WINDOW_MS, and closes when a phone connects; when that phone leaves, a short window
 * (JPH_REJOIN_MS) lets it come back. Otherwise the controller is off (not idle: off), and MODE JANK's frames have the radio
 * to themselves. The setting `ble`:
 *   pair  (default) a window only when asked for on the device;
 *   auto  also a window at every start, for a unit with no screen and no button at hand;
 *   off   never: three clicks are refused (red pulses), `ble pair` answers err.
 * Over BLE the phone gets the menu (get/set) and the HOME line, a push of a few numbers at most JPH_HOME_EVERY_MS apart. */
#ifndef JANK_PHONE_H
#define JANK_PHONE_H
#include <stddef.h>
#include <stdint.h>
#include "jank_core.h"

#define JPH_WINDOW_MS 120000u          /* a pair window: two minutes to find "jank XXXX" on the phone */
#define JPH_REJOIN_MS 30000u           /* after the phone leaves: this long for it to come back */
#define JPH_HOME_EVERY_MS 400u         /* the HOME line at most 2.5 times a second ... */
#define JPH_HOME_IDLE_MS 2000u         /* ... and when nothing in it changed, once in 2 s */

enum jph_state {
    JPH_OFF = 0,                       /* the controller is off */
    JPH_WINDOW,                        /* on, advertising until until_ms */
    JPH_CONNECTED                      /* on, a phone connected, no advertising */
};

struct jph {
    uint8_t mode;                      /* enum jk_ble */
    uint8_t state;                     /* enum jph_state */
    uint32_t until_ms;
};

void jph_init(struct jph *p, uint32_t mode, uint32_t now_ms);          /* auto: the start's window opens */
int jph_pair(struct jph *p, uint32_t now_ms);                          /* 0 a window is open (or a phone is on); -1 the setting is off */
void jph_close(struct jph *p);                                         /* off now: the window, and the phone's link */
void jph_set_mode(struct jph *p, uint32_t mode);                       /* off closes; the others leave things as they are */
void jph_connected(struct jph *p, uint32_t now_ms);
void jph_disconnected(struct jph *p, uint32_t now_ms);                 /* -> a JPH_REJOIN_MS window */
void jph_step(struct jph *p, uint32_t now_ms);                         /* a window over -> off */
int jph_radio_on(const struct jph *p);
int jph_advertising(const struct jph *p);
uint32_t jph_left_s(const struct jph *p, uint32_t now_ms);             /* the window's seconds left, 0 when none */
const char *jph_state_name(const struct jph *p);                       /* "off", "window", "connected" */

/* The HOME line, F0 7D 'J' 'B' JX_PUSH_HOME <text> F7 (jank_sysex.h), space separated:
 *   h <bpm x100> <net> <transport> <bar> <quantum> <link peers> <air peers> <battery> <level> <conf> <min> <max>
 *   bpm x100   the session's tempo, 0 with no session
 *   net        j<group> in MODE JANK; w1 on the Wi-Fi network, w0 still looking for it
 *   transport  0 stopped, 1 playing, 2 a start armed for a bar line ahead (AUDIO mode: the clock outputs, jank_hold.h)
 *   bar        where the session is in the bar, hundredths of a beat (0 .. quantum x 100 - 1); "-" with no session
 *   quantum    beats a bar
 *   link peers Link peers in the session, the stick not counted
 *   air peers  units heard on the group's channel (MODE JANK; 0 on Wi-Fi)
 *   battery    percent, "-" with no gauge (no board measures one yet)
 *   level      the input meter: RMS over 0.3 s, a value every 250 ms, dBFS (-99 .. 0); "-" with no audio stream
 *   conf       the tracker's beat activation peak over the last second, percent; "-" with no audio stream
 *   min max    the lowest and highest meter values over the last 3 s (the meter's lines); "-" with no audio stream
 * -> its length, 0 when it does not fit */
size_t jph_home_line(const struct jk_cfg *c, const struct jk_snapshot *s, uint32_t now_ms, char *out, size_t cap);

struct jph_rate {
    uint32_t last_ms;
    uint8_t sent;
    char last[64];
};
void jph_rate_init(struct jph_rate *r);
/* 1: send this line now (it is remembered); 0: not yet */
int jph_home_due(struct jph_rate *r, const char *line, size_t n, uint32_t now_ms);

#endif
