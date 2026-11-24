/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the starter scale list (core/jank_lpscales.c). Exit status: the number of failed checks. */
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

static void test_every_scale_is_well_formed(void)
{
    CHECK(jls_count() == JLS_COUNT);
    for (unsigned i = 0; i < jls_count(); i++) {
        const struct jln_scale *s = jls_get(i);
        CHECK(s && s->name && s->name[0]);
        CHECK(s->n >= 1 && s->n <= JLN_SCALE_MAX);
        CHECK(s->iv[0] == 0);                                   /* the first is the root */
        for (unsigned k = 1; k < s->n; k++)
            CHECK(s->iv[k] > s->iv[k - 1] && s->iv[k] < 12);    /* strictly rising, inside the octave */
        for (unsigned j = i + 1; j < jls_count(); j++)
            CHECK(strcmp(s->name, jls_get(j)->name) != 0);      /* no name twice */
    }
}

static void test_the_places_the_relative_pad_uses(void)
{
    CHECK(!strcmp(jls_get(JLS_MAJOR)->name, "Major"));
    CHECK(!strcmp(jls_get(JLS_MINOR)->name, "Minor"));
    CHECK(!strcmp(jls_get(JLS_MINOR_PENTATONIC)->name, "Minor Pentatonic"));
    CHECK(!strcmp(jls_get(JLS_MAJOR_PENTATONIC)->name, "Major Pentatonic"));
    CHECK(!strcmp(jls_get(JLS_HARMONIC_MINOR)->name, "Harmonic Minor"));
    CHECK(!strcmp(jls_get(JLS_MELODIC_MINOR)->name, "Melodic Minor"));
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
    test_the_places_the_relative_pad_uses();
    test_lookup();
    printf("test_lpscales: %d checks, %d failed\n", checks, fails);
    return fails;
}
