/* SPDX-License-Identifier: GPL-3.0-only */
/* The Digitone II's scale list (jank_lpscales.h). */
#include "jank_lpscales.h"

struct entry {
    const char *name;
    uint16_t mask;                     /* bit k: the note k semitones above the root */
};

/* names and masks as the Digitone session sent them (live-control SCALE, stock OS 1.12) */
static const struct entry TABLE[JLS_COUNT] = {
    {"Chromatic", 0xFFF},      {"Ionian (Maj)", 0xAB5},    {"Dorian", 0x6AD},         {"Phrygian", 0x5AB},     {"Lydian", 0xAD5},         {"Mixolydian", 0x6B5},
    {"Aeolian (Min)", 0x5AD},  {"Locrian", 0x56B},         {"Pentaton. Min", 0x4A9},  {"Pentaton. Maj", 0x295}, {"Melodic Minor", 0xAAD},  {"Harmonic Min", 0x9AD},
    {"Whole Tone", 0x555},     {"Blues", 0x4E9},           {"Combo Minor", 0xDAD},    {"Persian", 0x973},      {"Iwato", 0x463},          {"In-Sen", 0x4A3},
    {"Hirajoshi", 0x18D},      {"Pelog", 0x18B},           {"Phrygian Dom", 0x5B3},   {"Whole-Half Dim", 0xB6D}, {"Half-Whole Dim", 0x6DB}, {"Spanish", 0x57B},
    {"Major Locrian", 0x575},  {"Super Locrian", 0x55B},   {"Dorian B2", 0x6AB},      {"Lydian Aug", 0xB55},   {"Lydian Dom", 0x6D5},     {"Dbl Harm Maj", 0x9B3},
    {"Lydian #2 #6", 0xCD9},   {"Ultraphrygian", 0x39B},   {"Hungarian Min", 0x9CD},  {"Oriental", 0x673},     {"Ionian #2 #5", 0xB39},   {"Locr. bb3 bb7", 0x367},
};

/* the layout code wants the intervals as a list: built from the masks the first time one is asked for (one task uses the list) */
static struct jln_scale LIST[JLS_COUNT];
static uint8_t s_built;

static void build(void)
{
    for (unsigned i = 0; i < JLS_COUNT; i++) {
        struct jln_scale *s = &LIST[i];
        s->name = TABLE[i].name;
        s->n = 0;
        for (unsigned k = 0; k < 12u; k++)
            if ((TABLE[i].mask >> k) & 1u)
                s->iv[s->n++] = (uint8_t)k;
    }
    s_built = 1;
}

unsigned jls_count(void) { return JLS_COUNT; }

const struct jln_scale *jls_get(unsigned i)
{
    if (!s_built)
        build();
    return i < JLS_COUNT ? &LIST[i] : (const struct jln_scale *)0;
}

int jls_index_of(const struct jln_scale *s)
{
    if (!s_built)
        build();
    if (s >= LIST && s < LIST + JLS_COUNT)
        return (int)(s - LIST);
    return -1;
}

unsigned jls_mask(unsigned i) { return i < JLS_COUNT ? TABLE[i].mask : 0u; }
