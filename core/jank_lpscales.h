/* SPDX-License-Identifier: GPL-3.0-only */
/* The Digitone II's own scales (owner 2026-11-24: the Launchpad's scale follows the Digitone's, so it uses ITS list, in ITS order): 36 of them, index 0 Chromatic ("no
 * scale") .. 35. The index is the one the Digitone's live control uses (SCALE 50 reads it, SCALE SET 55 writes it). The intervals are stock OS 1.12's table as the
 * Digitone session read it (12-bit masks, bit k = the note k semitones above the root; read from the firmware's data, not measured on the unit). Pure, host-tested
 * (tests/test_lpscales.c). */
#ifndef JANK_LPSCALES_H
#define JANK_LPSCALES_H
#include "jank_lpnote.h"

#define JLS_COUNT 36u

/* the places the page's relative-scale pad needs (major <-> natural / harmonic / melodic minor, the two pentatonics) */
enum jls_index {
    JLS_CHROMATIC = 0,
    JLS_MAJOR = 1,                     /* Ionian (Maj) */
    JLS_MINOR = 6,                     /* Aeolian (Min) */
    JLS_MINOR_PENTATONIC = 8,
    JLS_MAJOR_PENTATONIC = 9,
    JLS_MELODIC_MINOR = 10,
    JLS_HARMONIC_MINOR = 11
};

unsigned jls_count(void);
const struct jln_scale *jls_get(unsigned i);               /* NULL past the end */
int jls_index_of(const struct jln_scale *s);               /* its place in the list, -1 if it is not one of them */
unsigned jls_mask(unsigned i);                             /* the 12-bit interval mask of scale i (0 past the end) */
#endif
