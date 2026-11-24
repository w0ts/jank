/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the Digitone's scale list (core/jank_lpscales.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpscales.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

/* how many notes each of the Digitone's 36 scales has, as the Digitone session listed them */
static const uint8_t NOTES[36] = {12, 7, 7, 7, 7, 7, 7, 7, 5, 5, 7, 7, 6, 6, 8, 7, 5, 5, 5, 5, 7, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

static void test_every_scale_is_well_formed(void)
{
    CHECK(jls_count() == JLS_COUNT && JLS_COUNT == 36);
    for (unsigned i = 0; i < jls_count(); i++) {
        const struct jln_scale *s = jls_get(i);
        CHECK(s && s->name && s->name[0] && strlen(s->name) <= 14);   /* "SCALE " + the name must fit a 20-character message */
        CHECK(s->n >= 1 && s->n <= JLN_SCALE_MAX);
        CHECK(s->n == NOTES[i]);
        CHECK(s->iv[0] == 0);                                   /* the first is the root */
        for (unsigned k = 1; k < s->n; k++)
            CHECK(s->iv[k] > s->iv[k - 1] && s->iv[k] < 12);    /* strictly rising, inside the octave */
        for (unsigned j = i + 1; j < jls_count(); j++)
            CHECK(strcmp(s->name, jls_get(j)->name) != 0);      /* no name twice */
    }
}

static void test_the_intervals_are_the_masks(void)
{
    const struct jln_scale *major = jls_get(1), *dorian = jls_get(2), *pent = jls_get(8), *chrom = jls_get(0), *last = jls_get(35);
    static const uint8_t MAJOR[7] = {0, 2, 4, 5, 7, 9, 11}, DORIAN[7] = {0, 2, 3, 5, 7, 9, 10}, PENT[5] = {0, 3, 5, 7, 10};
    CHECK(jls_mask(1) == 0xAB5 && jls_mask(35) == 0x367 && jls_mask(0) == 0xFFF && jls_mask(36) == 0 && jls_mask(1000) == 0);
    CHECK(!memcmp(major->iv, MAJOR, 7) && !memcmp(dorian->iv, DORIAN, 7) && !memcmp(pent->iv, PENT, 5));
    CHECK(chrom->n == 12 && chrom->iv[11] == 11);
    CHECK(last->n == 7 && last->iv[1] == 1 && last->iv[2] == 2 && last->iv[3] == 5 && last->iv[4] == 6 && last->iv[5] == 8 && last->iv[6] == 9);   /* 0x367 = bits 0 1 2 5 6 8 9 */
}

static void test_the_places_the_relative_pad_uses(void)
{
    CHECK(JLS_CHROMATIC == 0 && JLS_MAJOR == 1 && JLS_MINOR == 6 && JLS_MINOR_PENTATONIC == 8 && JLS_MAJOR_PENTATONIC == 9 && JLS_MELODIC_MINOR == 10 && JLS_HARMONIC_MINOR == 11);
    CHECK(!strcmp(jls_get(JLS_MAJOR)->name, "Ionian (Maj)"));
    CHECK(!strcmp(jls_get(JLS_MINOR)->name, "Aeolian (Min)"));
    CHECK(!strcmp(jls_get(JLS_MINOR_PENTATONIC)->name, "Pentaton. Min"));
    CHECK(!strcmp(jls_get(JLS_MAJOR_PENTATONIC)->name, "Pentaton. Maj"));
    CHECK(!strcmp(jls_get(JLS_HARMONIC_MINOR)->name, "Harmonic Min"));
    CHECK(!strcmp(jls_get(JLS_MELODIC_MINOR)->name, "Melodic Minor"));
    CHECK(!strcmp(jls_get(JLS_CHROMATIC)->name, "Chromatic"));
}

static void test_lookup(void)
{
    static const struct jln_scale own = {"Mine", 1, {0}};
    CHECK(jls_get(JLS_COUNT) == NULL && jls_get(1000) == NULL);
    CHECK(jls_index_of(jls_get(5)) == 5 && jls_index_of(jls_get(0)) == 0 && jls_index_of(jls_get(JLS_COUNT - 1)) == (int)JLS_COUNT - 1);
    CHECK(jls_index_of(&own) == -1 && jls_index_of(NULL) == -1);
}

int main(void)
{
    test_every_scale_is_well_formed();
    test_the_intervals_are_the_masks();
    test_the_places_the_relative_pad_uses();
    test_lookup();
    printf("test_lpscales: %d checks, %d failed\n", checks, fails);
    return fails;
}
