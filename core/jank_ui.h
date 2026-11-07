/* SPDX-License-Identifier: GPL-3.0-only */
/* The one button, the group scan's bookkeeping and the LED's event patterns: small state machines, host-tested. */
#ifndef JANK_UI_H
#define JANK_UI_H
#include <stddef.h>
#include <stdint.h>

#define JB_DEBOUNCE_MS 20u             /* a level holds this long before it counts */
#define JB_DOUBLE_MS 350u              /* the second press starts within this after the first release */
#define JB_LONG_MS 1500u               /* held this long, then released: MODE JAM <-> the Wi-Fi network */
#define JB_DOWNLOAD_MS 10000u          /* held this long: download mode, said while still held */

enum jb_event {
    JB_NONE = 0,
    JB_CLICK,                          /* said JB_DOUBLE_MS after the release, once no second press came */
    JB_DOUBLE,
    JB_LONG,
    JB_DOWNLOAD
};

struct jb_button {
    uint8_t raw, level;                /* the sample last seen; the debounced state */
    uint8_t state;
    uint32_t t_raw, t_edge, t_down;
};

void jb_init(struct jb_button *b);
/* call every few ms with the pressed state; returns at most one event */
enum jb_event jb_step(struct jb_button *b, int pressed, uint32_t now_ms);

/* group scan */
#define JS_DWELL_MS 350u               /* a channel's listen: more than Link's 250 ms announce, so one alive is caught */
#define JS_HEARD_MS 3000u              /* on the home channel the radio listens all the time: this old still counts */

struct js_scan {
    uint32_t t_heard[10];              /* by group, ms */
    uint16_t heard;                    /* bit g: a frame of group g heard */
};

void js_clear(struct js_scan *s);
/* a frame's group from its jam BSSID, data frames only; 0 when it is none, own (sender mac) skipped */
uint32_t js_group_of(const uint8_t *air, size_t len, const uint8_t own_mac[6]);
void js_note(struct js_scan *s, uint32_t group, uint32_t now_ms);
/* the channels to visit: every one but home, in order; returns how many */
uint32_t js_plan(uint32_t home_group, uint8_t out[2]);
/* the next group heard after current, wrapping; 0: no other */
uint32_t js_pick(const struct js_scan *s, uint32_t current, uint32_t now_ms);

/* the control page's scan: what each group sounded like during one sweep. Senders are told apart by addr2 only:
 * an FM-1 and a jank send the same frames, and a jank shows only by its MAC prefix (jk_mac_is_jank) */
#define JS_UNITS 8u                    /* senders counted a group; more are not, their frames are */
struct js_row {
    uint32_t frames;
    int8_t rssi;                       /* best, dBm */
    uint8_t units, janks;
    uint8_t mac[JS_UNITS][6];
};
struct js_table {
    struct js_row row[10];             /* by group */
};
void js_table_clear(struct js_table *t);
void js_table_note(struct js_table *t, uint32_t group, const uint8_t sender[6], int8_t rssi);
/* "scan=group,channel,frames,units,janks,rssi" for each group heard, or "scan=none", then "ok". 0: it does not fit */
size_t js_table_report(const struct js_table *t, char *out, size_t cap);

/* the LED's event patterns, over the normal pattern */
enum jl_event {
    JL_NONE = 0,
    JL_SCANNING,                       /* magenta, steady, for the whole scan */
    JL_JOINED,                         /* white, N flashes: the new GROUP */
    JL_NO_GROUP                        /* red, 3 long pulses: none other found, stayed */
};
#define JL_FLASH_MS 240u               /* one JL_JOINED flash, on and off */
#define JL_PULSE_MS 600u               /* one JL_NO_GROUP pulse, on and off */

/* 1 while the pattern runs, *r *g *b its colour now. t_ms is the time since the event */
int jl_pattern(enum jl_event e, uint32_t group, uint32_t t_ms, uint8_t rgb[3]);
#endif
