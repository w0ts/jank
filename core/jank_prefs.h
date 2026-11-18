/* SPDX-License-Identifier: GPL-3.0-only */
/* The preferences the menu keeps apart from the settings ("ui1" in NVS): BRIGHTNESS, SCREEN OFF, and the ones nothing in the firmware
 * uses yet (LEVEL, LOW CONFIDENCE, THRESHOLD, MIDI OUT 2 and CLK OUT offsets, CLK RATE, the FOLLOW / FOLLOW+LEAD choice). The blob's
 * layout is the one the firmware always wrote (10 bytes, version 1): an old blob loads as it is. */
#ifndef JANK_PREFS_H
#define JANK_PREFS_H
#include <stddef.h>
#include <stdint.h>
#include "jank_oled.h"

#define JK_PREFS_VERSION 1
#define JK_PREFS_SIZE 10u
/* a stored preference is written this long after its LAST change: long enough that a held button's repeats make one write, short
 * enough that a jank unplugged right after the edit still has it (it was 2000 ms, and a stop was needed first) */
#define JK_PREFS_SETTLE_MS 400u

struct jk_prefs {
    uint8_t version, lead, bright, sleep, rate, conf_stop, th;
    int8_t out1, out2, gain;
};

/* when to write: pending is set by a change, due once the changes have settled (and, unless force, nothing plays) */
struct jk_prefs_save {
    uint8_t pending;
    uint32_t changed_ms;
};

void jk_prefs_defaults(struct jk_prefs *p);
size_t jk_prefs_pack(const struct jk_prefs *p, uint8_t *out, size_t cap);            /* JK_PREFS_SIZE, 0: no room */
int jk_prefs_unpack(struct jk_prefs *p, const uint8_t *in, size_t n);               /* 1 loaded; 0: not a valid blob, p untouched */
/* a menu field's new value (the menu's set_field): 1 when it is a preference (p changed), 0 when it is not one of these */
int jk_prefs_set_field(struct jk_prefs *p, enum jo_field f, int v);
void jk_prefs_changed(struct jk_prefs_save *s, uint32_t now_ms);
/* 1: write now (clears pending). force: a restart follows, whatever plays and whatever the settle time */
int jk_prefs_due(struct jk_prefs_save *s, uint32_t now_ms, int playing, int force);

#endif
