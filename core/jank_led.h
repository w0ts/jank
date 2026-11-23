/* SPDX-License-Identifier: GPL-3.0-only */
/* The LED language of a unit without a screen (the RGB LEDs say what the screen would), first to last:
 *   red flash, slow            the cell is low (on the cell only): 100 ms every 3 s at 30 %, every 0.3 s at 5 % and under,
 *                              over whatever else shows (the emulator's battery blink)
 *   green flash each beat      the transport runs: 80 ms, the bar's 1 150 ms (no longer than a third of a beat);
 *                              amber on the 1 while AUTO BAR is locked; red instead of green under the confidence threshold
 *   yellow, steady             armed: a start asked for, not reached yet (the next bar line, the tracker locking)
 *   green breathing, slow      stopped, in a session (a 4 s rise and fall)
 *   dark                       stopped, nobody there
 * The event patterns (scan, joined, no group: jank_ui.h jl_pattern) come before all of it. */
#ifndef JANK_LED_H
#define JANK_LED_H
#include <stdint.h>

#define JLG_BATT_LOW_PCT 30
#define JLG_BATT_CRIT_PCT 5
#define JLG_BATT_SLOW_MS 3000u
#define JLG_BATT_FAST_MS 300u
#define JLG_BATT_FLASH_MS 100u
#define JLG_BREATH_MS 4000u
#define JLG_BREATH_MAX 12u               /* the green at the breath's top (of 255) */
#define JLG_ARMED_RGB 30, 20, 0
#define JLG_BEAT_RGB 0, 30, 0
#define JLG_LOWCONF_RGB 40, 0, 0
#define JLG_BATT_RGB 40, 0, 0
/* USB host mode: two short flashes every JLG_HOST_PERIOD_MS, over everything but the low battery's blink (a unit in host mode has
 * no control line and no log: this is how one tells it is hosting; a one-colour LED just lights) */
#define JLG_HOST_PERIOD_MS 2400u
#define JLG_HOST_FLASH_MS 70u
#define JLG_HOST_GAP_MS 90u
#define JLG_HOST_RGB 0, 0, 40

struct jlg_state {
    uint32_t now_ms;
    int8_t batt_pct;                     /* -1: no gauge */
    uint8_t on_usb;                      /* USB power: no battery warning */
    uint8_t joined;                      /* in a Link session (its beat known) */
    uint8_t running, armed;              /* the transport, as the screen's icon */
    uint8_t low_conf;                    /* AUDIO: the tracker's confidence under the threshold */
    uint8_t bar_locked;                  /* AUTO BAR locked: the bar's 1 is the music's */
    int64_t beat_ub;                     /* the session's beat now, micro-beats */
    int32_t mpb;                         /* us a beat */
    uint32_t quantum;
    uint8_t host;                        /* the USB host stack runs (the last field: the firmware fills the others by position) */
};

/* 1: now_ms is inside one of the two host-mode flashes (the same pattern on a one-colour LED, which has no jlg_color) */
int jlg_host_lit(uint32_t now_ms);

/* the colour now; 1 lit */
int jlg_color(const struct jlg_state *s, uint8_t rgb[3]);
/* the low-battery flash's period at pct (the emulator's lowPeriodMs) */
uint32_t jlg_batt_period_ms(int pct);
#endif
