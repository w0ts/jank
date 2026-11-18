/* SPDX-License-Identifier: GPL-3.0-only */
/* The device's own user interface: the 128 x 64 OLED's screens, the menus and the four buttons (UP, DOWN, NO, YES).
 *
 * A port of the browser emulator's OLED module (beat-listener live/player.html, "OLED UI for jank mini 2.2"), drawing the
 * same pixels for the same state (tests/test_oled.c compares them with the emulator's own, run in node). No ESP-IDF in
 * here: the firmware (firmware/main/ui.c) fills a jo_ctx from its state and carries the setters out.
 *
 *   jo_press   one button press on the screen state machine (the emulator's OLED.press)
 *   jo_draw    the state into a frame buffer (OLED.draw), in the SSD1309's page layout
 *   jo_input_* the buttons as the emulator's host turns them into presses: NO / YES on HOME act on release, NO + UP/DOWN
 *              and YES + UP/DOWN combos (HOME only), the hint after a 400 ms hold, UP/DOWN auto-repeat (400 ms, then
 *              every 120 ms), the screen's sleep and the press that only wakes it (btnDown / btnUp in player.html) */
#ifndef JANK_OLED_H
#define JANK_OLED_H
#include <stddef.h>
#include <stdint.h>

#define JO_W 128
#define JO_H 64
#define JO_PAGES (JO_H / 8)
#define JO_FB_BYTES (JO_W * JO_PAGES)     /* byte page * 128 + x, bit y % 8: the SSD1309's GDDRAM layout */

enum jo_btn { JO_UP = 0, JO_DOWN, JO_NO, JO_YES, JO_BTNS };

enum jo_screen { JO_HOME = 0, JO_MENU, JO_SUB, JO_NET, JO_SCAN, JO_TEXT, JO_NETINFO, JO_CONFIRM, JO_INFO, JO_EDIT };

/* what the screens change (the emulator's setField names). Numbers and choices first (EDIT), then the texts */
enum jo_field {
    JO_F_NETMODE = 0,   /* 0 JANK, 1 LINK CLIENT, 2 LINK AP, 3 SETUP */
    JO_F_INOFF,         /* AUDIO IN, ms -100..100 */
    JO_F_OUT0,          /* MIDI OUT 1, ms -50..50 */
    JO_F_OUT1,          /* MIDI OUT 2 */
    JO_F_OUT2,          /* CLK OUT */
    JO_F_RATE,          /* 0 1/4, 1 1/8, 2 1/16, 3 1/32, 4 24 PPQN */
    JO_F_THRESHOLD,     /* percent, steps of 5 */
    JO_F_GAIN,          /* LEVEL, dB -12..12 */
    JO_F_LTMODE,        /* LINK TRANSPORT: 0 IGNORE, 1 FOLLOW, 2 FOLLOW+LEAD */
    JO_F_BRIGHT,        /* percent 10..100 */
    JO_F_SLEEP,         /* SCREEN OFF: 0 NEVER, 1 30 S, 2 1 MIN, 3 5 MIN */
    JO_F_GROUP,         /* the choice index: 0..8 = GROUP 1..9 */
    JO_F_LOWCONF,       /* 0 KEEP RUNNING, 1 STOP / START */
    JO_F_INSRC,         /* AUDIO > SOURCE: 0 AUTO, 1 LINE IN, 2 USB AUDIO */
    JO_F_USBROLE,       /* 0 DEVICE, 1 HOST */
    JO_F_AUTOBAR,       /* 0 OFF, 1 ON (YES on ON again: picked again); the lcd2 layout: 0 OFF, 1 AT START, 2 ALWAYS */
    JO_F_BLE,           /* the lcd2 layout's menu only: BLUETOOTH, 0 PAIR NOW, 1 ON START, 2 OFF (YES on PAIR NOW opens the window) */
    JO_F_USBMIDI,       /* the lcd2 layout's menu only: USB MIDI CLOCK, 0 ON, 1 OFF */
    JO_F_BEATFLASH,     /* the lcd2 layout's menu only: 0 BOXES, 1 SCREEN */
    JO_F_EDITS,
    JO_F_SSID = JO_F_EDITS,
    JO_F_PASS,
    JO_F_APPASS,
    JO_F_APNAME,
    JO_F_COUNT
};

enum jo_wifi { JO_WIFI_GOOD = 0, JO_WIFI_WEAK, JO_WIFI_NONE };
enum jo_bar_state { JO_BAR_OFF = 0, JO_BAR_LISTENING, JO_BAR_LOCKED, JO_BAR_MANUAL };
enum jo_net_mode { JO_NET_JANK = 0, JO_NET_CLIENT, JO_NET_AP, JO_NET_SETUP };

#define JO_TEXT_MAX 63                    /* the character editor's longest text */
#define JO_SSID_MAX 32
#define JO_SCAN_MAX 16
#define JO_QR_MAX 37                      /* version 5 */

struct jo_qr {
    uint8_t size;                         /* 0: none */
    uint8_t mod[JO_QR_MAX][(JO_QR_MAX + 7) / 8];   /* row y, bit x: a dark module */
};
/* byte mode, error correction L, versions 1..5 (the emulator's QR.encode, penalty and mask choice included). 0 ok, -1 too long */
int jo_qr_encode(const char *text, struct jo_qr *out);
static inline int jo_qr_dark(const struct jo_qr *q, int x, int y) { return (q->mod[y][x >> 3] >> (x & 7)) & 1; }

struct jo_scan_row {
    char name[JO_SSID_MAX + 1];
    uint8_t bars;                         /* 0..3 */
};

/* Wi-Fi quality as Optimist's Wi-Fi page shows it: the signal and the rate, frames sent / retried / lost over 10 s */
struct jo_wifiq {
    int rssi;                             /* dBm, 0: no signal */
    double kbps;
    int tx, retried, lost;
};

struct jo_net {
    uint8_t mode, prev, group;            /* enum jo_net_mode; the mode SETUP returns to; GROUP 1..9 */
    char ssid[JO_SSID_MAX + 1];
    char pass[JO_TEXT_MAX + 1];
    char appass[JO_TEXT_MAX + 1];
    char ap_name[JO_TEXT_MAX + 1];
    char status[16];                      /* CONNECTED, CONNECTING, FAILED, NO NETWORK */
    char ip[16];
    int ap_clients;
    uint8_t nscan;
    struct jo_scan_row scan[JO_SCAN_MAX];
    char setup_name[JO_SSID_MAX + 1];
};

/* everything the screens show (the emulator's ctx). has_*: 0 = the emulator's null */
struct jo_ctx {
    uint8_t link;                         /* LINK mode (no audio source), else AUDIO */
    uint8_t line_in;                      /* HOME's AUDIO layout; 0: the LINK layout */
    uint8_t has_bpm, has_link_bpm;
    double bpm, link_bpm;                 /* the tracker's (AUDIO, 0 shows ---), the session's */
    double conf;                          /* 0..1 */
    int th, gain;
    uint8_t conf_stop;
    double level_db, level_min, level_max;   /* the input meter, dBFS */
    uint8_t running, armed;               /* the transport icon */
    uint8_t tracker_leads;                /* AUDIO with the tracker owning tempo and transport: UP / DOWN / YES only say so */
    uint8_t no_beat_nudge, no_ms_nudge;   /* NO + UP/DOWN, YES + UP/DOWN not available: they say TRACKER LEADS */
    uint8_t has_beat;
    int beat_n;                           /* the bar's beat count (Link's), for the boxes */
    int peers;                            /* Link peers */
    uint8_t wifi;                         /* enum jo_wifi: the network icon's bars */
    uint8_t has_wifiq;
    struct jo_wifiq wifiq;
    uint8_t usb, fitted;                  /* USB power plugged; a cell fitted */
    int batt;                             /* percent */
    uint8_t lt_mode, in_src, usb_role, auto_bar;
    uint8_t has_bar, bar_state;           /* the downbeat estimator, enum jo_bar_state */
    double bar_conf;
    int in_off, offs[3];
    uint8_t rate, bright, sleep;
    uint8_t boot, splash, setup, saved;
    struct jo_net net;
    int group_peers;
    const struct jo_qr *qr;               /* SETUP's hotspot code */
    const char *splash_model, *splash_fw; /* the splash's two lower lines ("MINI 2.2", "FW EMU" in the emulator) */
    /* the lcd2 layout (ctx.layout 'lcd2' of ui/oled.js: its own 160 x 120 grid, jo_draw_lcd2). The fields below feed it; has_* 0 =
     * unreported (grey, never made up). The OLED layout reads none of them. */
    uint8_t layout_lcd2;                  /* jo_press picks the lcd2 menu and edit specs; jo_draw_lcd2 draws this layout */
    uint8_t has_rssi;
    int rssi;                             /* dBm of the best peer (jank) or the router (Wi-Fi client); 0: none heard */
    uint8_t has_peak;
    double peak_db;                       /* the input's peak over the last second, dBFS */
    uint8_t conf_unknown;                 /* the tracker has no confidence to give: "--", grey */
    uint8_t silent, gated, conf_hold;     /* silence on the source; the outputs stopped for low confidence; the confidence holds */
    int8_t fold_exp;                      /* AUDIO's fold, 2^fold_exp: the label X2 X4 /2 /4 on HOME; 0 and anything else: none */
    uint8_t ble_mode;                     /* BLUETOOTH setting: 0 PAIR NOW, 1 ON START, 2 OFF */
    uint8_t ble_phone, has_ble_left;      /* a phone is connected; the pair window is open */
    int ble_left;                         /* seconds of the pair window left */
    uint8_t usb_midi;                     /* USB MIDI CLOCK: 0 ON, 1 OFF */
    uint8_t beat_flash;                   /* BEAT FLASH: 0 BOXES, 1 SCREEN */
};

struct jo_edit {
    uint8_t field, ret;                   /* enum jo_field; the screen EDIT returns to */
    int orig, val;
};

struct jo_text {
    uint8_t field;                        /* JO_F_PASS, JO_F_APPASS, JO_F_APNAME */
    uint8_t len, pos, mask;
    char text[JO_TEXT_MAX + 1];
};

struct jo_overlay {
    uint8_t on;
    uint32_t until;                       /* ms */
    char title[24];
    char value[16];
    uint8_t live;                         /* the lcd2 layout shows the pair window's seconds left instead of value (jo_pair_overlay) */
};

struct jo_ui {
    uint8_t screen;                       /* enum jo_screen */
    uint8_t cursor, top, ocur, ncur, scur, stop;
    uint8_t sub;                          /* the SUB shown: 0 AUDIO, 1 OFFSET, 2 DISPLAY */
    uint8_t leave;                        /* SETUP's "LEAVE SETUP?" */
    int8_t hint;                          /* -1 none, JO_NO / JO_YES: the held button's hint on HOME */
    struct jo_edit edit;
    struct jo_text txt;
    struct jo_overlay overlay;
};

/* the setters (the emulator's ctx actions) */
struct jo_ops {
    void *user;
    /* HOME UP / DOWN: LINK +-1 BPM, AUDIO x2 / /2. 0 and *bpm the BPM shown; -1 none known (the overlay says ---) */
    int (*tempo_step)(void *user, int d, double *bpm);
    void (*set_field)(void *user, enum jo_field f, int value);
    void (*set_text)(void *user, enum jo_field f, const char *text);
    void (*set_hold)(void *user, int on);             /* AUDIO: Stop holds, Start resyncs and runs */
    void (*link_start_stop)(void *user, int on);      /* LINK: the clocks (and the session in FOLLOW+LEAD) */
    void (*shift_beats)(void *user, int halves);      /* NUDGE BEAT: +-1 = half a beat */
};

void jo_ui_init(struct jo_ui *ui);
void jo_press(struct jo_ui *ui, enum jo_btn b, uint32_t now, const struct jo_ctx *c, const struct jo_ops *ops);
void jo_draw(uint8_t fb[JO_FB_BYTES], const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now);

/* The lcd2 layout (ui/oled.js with ctx.layout 'lcd2'): its own grid of 160 x 120 pixels, painted exactly x2 on the 320 x 240 panel. fb is
 * 160 x 120 bits in the same page layout (byte (y >> 3) * 160 + x, bit y & 7), JO_LCD2_FB_BYTES; ink (or NULL) one byte per pixel,
 * y * 160 + x, JO_LCD2_INK_BYTES, naming which LIT pixels carry information (OLED.INK; the ids are meanings, never colours: core/jank_lcd.h
 * maps them). Menus, edit, info, network, setup screens and overlays have no ink. The OLED draws with jo_draw above, never with this. */
#define JO_LCD2_W 160
#define JO_LCD2_H 120
#define JO_LCD2_FB_BYTES (JO_LCD2_W * JO_LCD2_H / 8)
#define JO_LCD2_INK_BYTES (JO_LCD2_W * JO_LCD2_H)
enum jo_ink { JO_INK_NONE = 0, JO_INK_GOOD, JO_INK_WARN, JO_INK_BAD, JO_INK_DIM, JO_INK_ONE, JO_INK_IDS };
void jo_draw_lcd2(uint8_t fb[JO_LCD2_FB_BYTES], uint8_t ink[JO_LCD2_INK_BYTES], const struct jo_ui *ui, const struct jo_ctx *c, uint32_t now);
/* the triple NO's Bluetooth pair window: a box with its countdown (ctx ble_left; the OLED draws the text "PAIR") */
void jo_pair_overlay(struct jo_ui *ui, uint32_t now);
/* one overlay box for 1 s (the emulator's showOverlay): drops the hint */
void jo_show_overlay(struct jo_ui *ui, uint32_t now, const char *title, const char *value);

/* the buttons (the emulator's host: btnDown / btnUp). The host gives a fresh ctx for each action */
#define JO_HINT_MS 400u
#define JO_REPEAT_FIRST_MS 400u
#define JO_REPEAT_EVERY_MS 120u
#define JO_OVERLAY_MS 1000u
struct jo_host {
    const struct jo_ops *ops;
    const struct jo_ctx *(*ctx)(void *user, uint32_t now);   /* called with ops->user */
};
struct jo_input {
    uint8_t held[JO_BTNS], deferred[JO_BTNS], woke[JO_BTNS];
    uint8_t combo;
    uint8_t rep_on[JO_BTNS];
    int8_t rep_mod[JO_BTNS];              /* -1: the repeat presses; JO_NO / JO_YES: it repeats that combo */
    uint32_t rep_due[JO_BTNS];
    uint8_t hint_on, hint_btn;
    uint32_t hint_due;
    uint32_t last_input;
};
void jo_input_init(struct jo_input *in, uint32_t now);
void jo_input_down(struct jo_input *in, struct jo_ui *ui, enum jo_btn b, uint32_t now, const struct jo_host *h);
void jo_input_up(struct jo_input *in, struct jo_ui *ui, enum jo_btn b, uint32_t now, const struct jo_host *h);
/* the timers: the repeats and the hint due by now fire in time order, each at its own due time */
void jo_input_tick(struct jo_input *in, struct jo_ui *ui, uint32_t now, const struct jo_host *h);
/* the screen is dark: SCREEN OFF's time without a button (SETUP keeps it on) */
int jo_asleep(const struct jo_input *in, const struct jo_ctx *c, uint32_t now);
/* HOME and not SETUP: where NO / YES act on release and the combos work */
int jo_on_home(const struct jo_ui *ui, const struct jo_ctx *c);
#endif
