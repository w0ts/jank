/* SPDX-License-Identifier: GPL-3.0-only */
/* Novation Launchpad X in Programmer mode (Launchpad X Programmer's reference manual, Novation/Focusrite): what its pads and
 * buttons send, and the 4-byte USB-MIDI event packets that light them. Pure: no clock, no USB stack, host-tested
 * (tests/test_lpx.c). Verified against the manual: header F0 00 20 29 02 0C, command 0E <0 Live | 1 Programmer>, a pad's
 * number is 10 x row + column (rows 1..8 from the bottom, columns 1..8), the top buttons are CC 91..98, the side buttons CC 89
 * (top) down to CC 19 (bottom), the logo CC 99, a colour is a Note On (or CC for the buttons) whose channel picks the lighting
 * (1 static, 2 flashing, 3 pulsing) and whose velocity (value) is the palette index (0 off). Flashing flips between the colour
 * set static (A) and the flashing one (B), so a flash is sent as its static A, then B. The Launchpad X has two USB-MIDI
 * interfaces: DAW (the first) and MIDI (the second); Programmer mode is on the MIDI one. */
#ifndef JANK_LPX_H
#define JANK_LPX_H
#include <stddef.h>
#include <stdint.h>

/* the lights, in one list: pads row * 8 + column (row 0 = bottom), then the buttons */
#define JLX_PAD0 0u
#define JLX_TOP0 64u                   /* 8 top buttons, left to right: up, down, left, right, session, note, custom, capture */
#define JLX_SIDE0 72u                  /* 8 side buttons, top (scene 1) to bottom (scene 8) */
#define JLX_LOGO 80u
#define JLX_LEDS 81u

enum jlx_kind { JLX_NONE, JLX_PAD, JLX_TOP, JLX_SIDE };

struct jlx_ev {
    uint8_t kind;                      /* enum jlx_kind */
    uint8_t row, col;                  /* JLX_PAD: 0..7, row 0 = bottom */
    uint8_t idx;                       /* JLX_TOP, JLX_SIDE: 0..7 as above */
    uint8_t value;                     /* 0 = released, else the velocity (a button: 127) */
};

/* msg = the three bytes of a Note On / Note Off / Control Change (any channel). 1 and *ev filled: a pad or a button; 0: not one */
int jlx_decode(const uint8_t msg[3], struct jlx_ev *ev);

/* the Programmer mode number (note, or CC for the buttons) of light i (< JLX_LEDS) */
uint8_t jlx_number(uint32_t i);

/* the SysEx "Programmer mode" (on) or "Live mode" (off), 3 packets = 12 bytes, on cable `cable` */
size_t jlx_programmer(uint8_t cable, int on, uint8_t *out);

/* What Launchpad 95 sends right after Programmer mode (Launchpad.py handle_sysex, for the Launchpad X), and without which the Launchpad takes Programmer
 * mode and every light message and shows nothing (measured 2026-11-24: dark from the unit and from a Mac, lit from the Mac once these two were sent):
 *   F0 00 20 29 02 0C 0A 00 01 F7   LED feedback: the pads' own light off (internal 0), the host's messages on (external 1)
 *   F0 00 20 29 02 0C 09 01 F7      sleep mode off (awake)
 * 7 packets = 28 bytes on cable `cable`; JLX_SETUP_PACKETS. */
#define JLX_SETUP_PACKETS 7u
size_t jlx_setup(uint8_t cable, uint8_t *out);

/* The SysEx that asks which mode the Launchpad is in (`F0 00 20 29 02 0C 0E F7`): 3 packets = 12 bytes on cable `cable`. It answers with `0E mode F7`, the
 * packet jlx_programmer_ack reads (1: Programmer, 0: Live). A Launchpad that is in Live mode takes no lights on this port. */
size_t jlx_mode_query(uint8_t cable, uint8_t *out);

/* The Launchpad X answers a Programmer / Live mode message with the same message (measured 2026-11-24 over Wi-Fi diag: three packets in
 * after the unit's SysEx, the last one 07 0E 01 F7). 1: pkt is that last packet, `0E mode F7` ending a SysEx (cable ignored); *mode is 1 for
 * Programmer, 0 for Live. The unit waits for it before it sends lights, and sends the message again when it does not come. */
int jlx_programmer_ack(const uint8_t pkt[4], int *mode);

enum jlx_mode { JLX_STATIC, JLX_FLASH, JLX_PULSE };

struct jlx_led {
    uint8_t color;                     /* the palette index, 0 off; for JLX_FLASH the flashing colour (B) */
    uint8_t alt;                       /* JLX_FLASH only: the static colour it flips with (A) */
    uint8_t mode;                      /* enum jlx_mode */
};

struct jlx_frame {
    struct jlx_led led[JLX_LEDS];
};

/* marks every light of `shown` as not known: the next diff sends them all (after a Programmer mode switch or a reconnect) */
void jlx_forget(struct jlx_frame *shown);

/* writes the packets that bring the Launchpad from `shown` to `want` (a light that is the same sends nothing) into out, at most
 * max_packets, and updates `shown` for the lights it sent only; returns the packets written. A flash takes two. The rest goes
 * out in the next call. */
size_t jlx_diff(uint8_t cable, struct jlx_frame *shown, const struct jlx_frame *want, uint8_t *out, size_t max_packets);
#endif
