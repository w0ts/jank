/* SPDX-License-Identifier: GPL-3.0-only */
/* The starter scale list (jank_lpscales.h). */
#include "jank_lpscales.h"

static const struct jln_scale LIST[JLS_COUNT] = {
    {"Major", 7, {0, 2, 4, 5, 7, 9, 11}},
    {"Minor", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"Dorian", 7, {0, 2, 3, 5, 7, 9, 10}},
    {"Mixolydian", 7, {0, 2, 4, 5, 7, 9, 10}},
    {"Lydian", 7, {0, 2, 4, 6, 7, 9, 11}},
    {"Phrygian", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"Locrian", 7, {0, 1, 3, 5, 6, 8, 10}},
    {"Whole Tone", 6, {0, 2, 4, 6, 8, 10}},
    {"Half-Whole Dim", 8, {0, 1, 3, 4, 6, 7, 9, 10}},
    {"Whole-Half Dim", 8, {0, 2, 3, 5, 6, 8, 9, 11}},
    {"Minor Blues", 6, {0, 3, 5, 6, 7, 10}},
    {"Major Blues", 6, {0, 2, 3, 4, 7, 9}},
    {"Minor Pentatonic", 5, {0, 3, 5, 7, 10}},
    {"Major Pentatonic", 5, {0, 2, 4, 7, 9}},
    {"Harmonic Minor", 7, {0, 2, 3, 5, 7, 8, 11}},
    {"Harmonic Major", 7, {0, 2, 4, 5, 7, 8, 11}},
    {"Melodic Minor", 7, {0, 2, 3, 5, 7, 9, 11}},
    {"Dorian #4", 7, {0, 2, 3, 6, 7, 9, 10}},
    {"Phrygian Dominant", 7, {0, 1, 4, 5, 7, 8, 10}},
    {"Lydian Augmented", 7, {0, 2, 4, 6, 8, 9, 11}},
    {"Lydian Dominant", 7, {0, 2, 4, 6, 7, 9, 10}},
    {"Super Locrian", 7, {0, 1, 3, 4, 6, 8, 10}},
    {"Neapolitan Minor", 7, {0, 1, 3, 5, 7, 8, 11}},
    {"Neapolitan Major", 7, {0, 1, 3, 5, 7, 9, 11}},
    {"Hungarian Minor", 7, {0, 2, 3, 6, 7, 8, 11}},
    {"Bhairav", 7, {0, 1, 4, 5, 7, 8, 11}},
    {"Hirajoshi", 5, {0, 2, 3, 7, 8}},
    {"In-Sen", 5, {0, 1, 5, 7, 10}},
    {"Iwato", 5, {0, 1, 5, 6, 10}},
    {"Kumoi", 5, {0, 2, 3, 7, 9}},
    {"Chromatic", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {"Bebop Dominant", 8, {0, 2, 4, 5, 7, 9, 10, 11}},
};

unsigned jls_count(void) { return JLS_COUNT; }

const struct jln_scale *jls_get(unsigned i) { return i < JLS_COUNT ? &LIST[i] : (const struct jln_scale *)0; }

int jls_index_of(const struct jln_scale *s)
{
    if (s >= LIST && s < LIST + JLS_COUNT)
        return (int)(s - LIST);
    return -1;
}
