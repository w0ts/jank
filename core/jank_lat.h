/* SPDX-License-Identifier: GPL-3.0-only */
/* beat_latency per audio source, remembered: how long after its samples reach the stick the music is heard depends on
 * where the samples come from, and only the user's ear (NUDGE MS, Calibrate) knows it for a source.
 *
 *   the computer (USB audio, jank a device)   one value: the computer's output buffer against its speakers
 *   a hosted USB audio device (jank the host) one value a device, by VID:PID: an Elektron box streams to jank over USB
 *                                             while the user hears its analog outs; the gap is the box's own constant.
 *                                             The last JLA_DEVS devices given a value are kept, most recent first.
 *   the line in                               JLA_LINE_PATH_MS (the analog in's own delay: the stick hears the line after
 *                                             the speakers do) plus the user's trim; one value
 *
 * The live value is the settings' beat_latency_ms; switching the source loads that source's value into it, and a change of
 * it (`set beat_latency`, `nudge ms`, the device's YES + UP/DOWN, Calibrate) goes to the current source's. Stored with the
 * settings (store.c, its own NVS blob), saved only once stopped, as every setting. No ESP-IDF. */
#ifndef JANK_LAT_H
#define JANK_LAT_H
#include <stddef.h>
#include <stdint.h>

#define JLA_DEVS 8u
#define JLA_COMPUTER_DEFAULT_MS 25      /* = JK_BEAT_LATENCY_DEFAULT_MS: the value before there were sources */
#define JLA_HOSTED_DEFAULT_MS 0         /* a box's outs and its USB stream leave it together (the user's ear trims it) */
#define JLA_LINE_PATH_MS 0              /* NOT MEASURED: the line in's path is not on this branch; its ADC delay, negated,
                                           goes here once measured on a board that has one */
#define JLA_MIN_MS (-300)               /* = JK_BEAT_LATENCY_MIN_MS .. MAX */
#define JLA_MAX_MS 300
#define JLA_PACKED (1u + 2u + 2u + 1u + JLA_DEVS * 6u)   /* the stored blob: 54 bytes */

enum jla_kind { JLA_COMPUTER = 0, JLA_LINE = 1, JLA_HOSTED = 2 };
struct jla_key {
    uint8_t kind;                       /* enum jla_kind */
    uint16_t vid, pid;                  /* JLA_HOSTED: the device */
};
struct jla_dev {
    uint16_t vid, pid;
    int16_t ms;
};
struct jla {
    int16_t computer_ms;
    int16_t line_trim_ms;               /* the line in's value is JLA_LINE_PATH_MS + this */
    uint8_t ndev;
    struct jla_dev dev[JLA_DEVS];       /* most recently set first */
};

void jla_init(struct jla *t);
/* the source's value (a device not in the table: JLA_HOSTED_DEFAULT_MS) */
int16_t jla_get(const struct jla *t, const struct jla_key *k);
/* the source's value set (within JLA_MIN_MS..MAX); a device goes first in the table, the oldest of JLA_DEVS leaves */
void jla_set(struct jla *t, const struct jla_key *k, int ms);
int jla_same_key(const struct jla_key *a, const struct jla_key *b);
int jla_same(const struct jla *a, const struct jla *b);
/* "usb", "line", "host:1935:000c" (out holds 16) */
const char *jla_name(const struct jla_key *k, char *out);
/* the stored form: a version byte, then little-endian fields. unpack: 0 ok (t set), -1 not one (t untouched) */
size_t jla_pack(const struct jla *t, uint8_t *out, size_t cap);
int jla_unpack(struct jla *t, const uint8_t *in, size_t n);
#endif
