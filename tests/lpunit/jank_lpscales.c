/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's 16 scales for tests/test_lpunit.c (jank_lpscales.h here). Written as that unit could: the lists as data, no C library. */
#include "jank_lpscales.h"

static const struct jln_scale LIST[JLS_COUNT] = {
    {"CHR", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}}, {"MAJ", 7, {0, 2, 4, 5, 7, 9, 11}}, {"MIN", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"DOR", 7, {0, 2, 3, 5, 7, 9, 10}},  {"MIX", 7, {0, 2, 4, 5, 7, 9, 10}},  {"PEN", 5, {0, 2, 4, 7, 9}},       {"MPEN", 5, {0, 3, 5, 7, 10}},
    {"HARM", 7, {0, 2, 3, 5, 7, 8, 11}}, {"PHRY", 7, {0, 1, 3, 5, 7, 8, 10}}, {"LYD", 7, {0, 2, 4, 6, 7, 9, 11}}, {"LOC", 7, {0, 1, 3, 5, 6, 8, 10}},
    {"MEL", 7, {0, 2, 3, 5, 7, 9, 11}},  {"BLUES", 6, {0, 3, 5, 6, 7, 10}},   {"WHOLE", 6, {0, 2, 4, 6, 8, 10}},  {"DIMHW", 8, {0, 1, 3, 4, 6, 7, 9, 10}},
    {"DIMWH", 8, {0, 2, 3, 5, 6, 8, 9, 11}},
};

unsigned jls_count(void) { return JLS_COUNT; }

const struct jln_scale *jls_get(unsigned i) { return i < JLS_COUNT ? &LIST[i] : (const struct jln_scale *)0; }

int jls_index_of(const struct jln_scale *s) { return s >= LIST && s < LIST + JLS_COUNT ? (int)(s - LIST) : -1; }

unsigned jls_mask(unsigned i)
{
    unsigned m = 0;
    for (unsigned k = 0; i < JLS_COUNT && k < LIST[i].n; k++)
        m |= 1u << LIST[i].iv[k];
    return m;
}
