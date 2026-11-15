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
 * TWO LAYERS a source (M2h, the user: "ms is more an audio shift ... zero is auto"): the AUTO base (`set beat_latency`,
 * the player's Auto latency, or the value remembered) and the user's AUDIO SHIFT on top (`nudge ms`, the device's YES +
 * UP/DOWN, Calibrate's `set lat_trim`). Neither changes the other; a shift of 0 is the auto base alone. The value used
 * (jla_get) is their sum, within JLA_MIN_MS..MAX; a shift is kept so that the sum with its base stays in that range.
 *
 * The live value is the settings' beat_latency_ms (the sum); switching the source loads that source's. Stored with the
 * settings (store.c): the bases in their NVS blob as before (jla_pack, 54 bytes), the shifts in one of their own
 * (jla_pack_trim), so a table an older firmware stored is read as it was, with no shift. Saved only once stopped, as every
 * setting. No ESP-IDF. */
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
#define JLA_PACKED (1u + 2u + 2u + 1u + JLA_DEVS * 6u)   /* the bases' stored blob: 54 bytes */
#define JLA_TRIM_PACKED (1u + 2u + 2u + JLA_DEVS * 6u)   /* the shifts' (vid, pid, shift a device): 53 bytes */

enum jla_kind { JLA_COMPUTER = 0, JLA_LINE = 1, JLA_HOSTED = 2 };
struct jla_key {
    uint8_t kind;                       /* enum jla_kind */
    uint16_t vid, pid;                  /* JLA_HOSTED: the device */
};
struct jla_dev {
    uint16_t vid, pid;
    int16_t ms;                         /* the auto base */
    int16_t trim;                       /* the AUDIO SHIFT */
};
struct jla {
    int16_t computer_ms, computer_trim;
    int16_t line_base_ms;               /* the line in's base is JLA_LINE_PATH_MS + this */
    int16_t line_trim;
    uint8_t ndev;
    struct jla_dev dev[JLA_DEVS];       /* most recently set first */
};

void jla_init(struct jla *t);
/* the source's value: its base + its shift (a device not in the table: JLA_HOSTED_DEFAULT_MS) */
int16_t jla_get(const struct jla *t, const struct jla_key *k);
int16_t jla_base(const struct jla *t, const struct jla_key *k);
int16_t jla_trim(const struct jla *t, const struct jla_key *k);
/* the source's auto base set (within JLA_MIN_MS..MAX), its shift kept; a device goes first in the table, the oldest of
 * JLA_DEVS leaves */
void jla_set(struct jla *t, const struct jla_key *k, int ms);
/* the source's AUDIO SHIFT set (0: auto alone), kept so that base + shift stays within JLA_MIN_MS..MAX; a device as jla_set */
void jla_set_trim(struct jla *t, const struct jla_key *k, int ms);
int jla_same_key(const struct jla_key *a, const struct jla_key *b);
int jla_same(const struct jla *a, const struct jla *b);
/* "usb", "line", "host:1935:000c" (out holds 16) */
const char *jla_name(const struct jla_key *k, char *out);
/* the stored form: a version byte, then little-endian fields. unpack: 0 ok (t set), -1 not one (t untouched) */
size_t jla_pack(const struct jla *t, uint8_t *out, size_t cap);
int jla_unpack(struct jla *t, const uint8_t *in, size_t n);
/* the shifts' blob: a version byte, the computer's and the line's, then (vid, pid, shift) a device. unpack after jla_unpack:
 * a device's shift goes to its entry (none: dropped). 0 ok, -1 not one (t untouched) */
size_t jla_pack_trim(const struct jla *t, uint8_t *out, size_t cap);
int jla_unpack_trim(struct jla *t, const uint8_t *in, size_t n);
#endif
