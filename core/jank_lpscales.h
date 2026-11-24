/* SPDX-License-Identifier: GPL-3.0-only */
/* The scales the Note mode's scale page (jank_lpapp.h) offers: a starter list of 32, standard names and intervals, in the order the
 * page shows them (four rows of eight). It is neither Ableton Live's list (Launchpad 95 takes that one from Live) nor the Digitone II's
 * own, which the unit cannot read yet: when it can, that list replaces this one. Pure, host-tested (tests/test_lpscales.c). */
#ifndef JANK_LPSCALES_H
#define JANK_LPSCALES_H
#include "jank_lpnote.h"

#define JLS_COUNT 32u

/* the places the page's relative-scale pad needs (major <-> natural / harmonic / melodic minor, the two pentatonics) */
enum jls_index {
    JLS_MAJOR = 0,
    JLS_MINOR = 1,
    JLS_MINOR_PENTATONIC = 12,
    JLS_MAJOR_PENTATONIC = 13,
    JLS_HARMONIC_MINOR = 14,
    JLS_MELODIC_MINOR = 16
};

unsigned jls_count(void);
const struct jln_scale *jls_get(unsigned i);               /* NULL past the end */
int jls_index_of(const struct jln_scale *s);               /* its place in the list, -1 if it is not one of them */
#endif
