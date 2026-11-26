/* SPDX-License-Identifier: GPL-3.0-only */
/* Another unit's scale list, for tests/test_lpunit.c: the FM-1's 16 (its short names, its order), in place of jank's core/jank_lpscales.h, as that unit replaces the file.
 * The contract (docs/launchpad.md, "Using the core on another unit"): JLS_COUNT, the JLS_ indexes below (a unit without one of them gives it JLS_COUNT) and the four
 * functions. */
#ifndef JANK_LPSCALES_H
#define JANK_LPSCALES_H
#include "jank_lpnote.h"

#define JLS_COUNT 16u

enum jls_index {
    JLS_CHROMATIC = 0,
    JLS_MAJOR = 1,
    JLS_MINOR = 2,
    JLS_MAJOR_PENTATONIC = 5,
    JLS_MINOR_PENTATONIC = 6,
    JLS_HARMONIC_MINOR = 7,
    JLS_MELODIC_MINOR = 11
};

unsigned jls_count(void);
const struct jln_scale *jls_get(unsigned i);               /* NULL past the end */
int jls_index_of(const struct jln_scale *s);               /* its place in the list, -1 if it is not one of them */
unsigned jls_mask(unsigned i);                             /* bit k: the note k semitones above the root (0 past the end) */
#endif
