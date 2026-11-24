/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of Note mode's pad layouts (core/jank_lpnote.c), a port of Launchpad 95's MelodicPattern / ScaleComponent
 * (hdavid/launchpad95). The expected notes are worked out by hand from its formulas: index = steps[0] * (origin[0] + x) +
 * steps[1] * (origin[1] + y), octave = index // size, note = scale[index % size] (floored, so a negative index goes down an
 * octave), midi = (octave + 1) * 12 + 12 * octave_of_index + note. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpnote.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static const struct jln_scale MAJOR = {"Major", 7, {0, 2, 4, 5, 7, 9, 11}};
static const struct jln_scale PENT = {"Pent", 5, {0, 3, 5, 7, 10}};

static struct jln state(enum jln_layout l, uint8_t key, uint8_t octave, uint8_t absolute)
{
    struct jln s;
    jln_init(&s);
    jln_set_layout(&s, l);
    s.key = key;
    s.octave = octave;
    s.absolute = absolute;
    return s;
}

static int16_t note(const struct jln *s, const struct jln_scale *sc, uint8_t x, uint8_t y)
{
    struct jln_pad p;
    jln_pad(s, sc, x, y, &p);
    return p.note;
}

static void test_defaults(void)
{
    struct jln s;
    jln_init(&s);
    CHECK(s.layout == JLN_DIATONIC && s.key == 0 && s.octave == 3 && !s.absolute && s.horizontal && s.track == 0);
}

static void test_chromatic_relative(void)
{
    struct jln s = state(JLN_CHROMATIC, 0, 3, 0);               /* C, octave 3: the bottom left pad is note 48 */
    CHECK(note(&s, &MAJOR, 0, 0) == 48 && note(&s, &MAJOR, 1, 0) == 49 && note(&s, &MAJOR, 7, 0) == 55);
    CHECK(note(&s, &MAJOR, 0, 1) == 53);                         /* a row up is a fourth: the layouts overlap */
    CHECK(note(&s, &MAJOR, 5, 0) == 53);
    CHECK(note(&s, &MAJOR, 0, 7) == 83);                         /* index 35: octave 2, note 11 */
    s = state(JLN_CHROMATIC, 2, 3, 0);                           /* D: the bottom left pad is the root */
    CHECK(note(&s, &MAJOR, 0, 0) == 50);
}

static void test_chromatic_absolute(void)
{
    struct jln s = state(JLN_CHROMATIC, 2, 3, 1);               /* absolute root: the bottom left pad stays C whatever the key */
    CHECK(note(&s, &MAJOR, 0, 0) == 48);                         /* index -2: octave -1, note 12 (floored) */
    CHECK(note(&s, &MAJOR, 2, 0) == 50);                         /* D is at x = 2 */
}

static void test_diatonic(void)
{
    struct jln s = state(JLN_DIATONIC, 0, 3, 0);                /* C major, a row up is a third (3 scale steps) */
    CHECK(note(&s, &MAJOR, 0, 0) == 48 && note(&s, &MAJOR, 1, 0) == 50 && note(&s, &MAJOR, 6, 0) == 59);
    CHECK(note(&s, &MAJOR, 7, 0) == 60);                         /* the 8th pad is the octave */
    CHECK(note(&s, &MAJOR, 0, 1) == 53);                         /* index 3: F */
    CHECK(note(&s, &MAJOR, 0, 3) == 64);                         /* index 9: octave 1, E */
    CHECK(note(&s, &PENT, 5, 0) == 60 && note(&s, &PENT, 4, 0) == 58);   /* five notes an octave */
    s = state(JLN_DIATONIC, 2, 3, 1);                            /* absolute, D major (2 4 6 7 9 11 13): origin = 6 - 7 */
    CHECK(note(&s, &MAJOR, 0, 0) == 49);                         /* index -1: octave -1, scale[6] = 13 */
    s = state(JLN_DIATONIC, 0, 3, 1);                            /* a scale that never reaches 12: origin 0 */
    CHECK(note(&s, &MAJOR, 0, 0) == 48);
}

static void test_chords(void)
{
    struct jln s = state(JLN_CHORDS, 0, 3, 0);                  /* vertical: columns a third apart, rows a step */
    CHECK(!s.horizontal);
    CHECK(note(&s, &MAJOR, 0, 0) == 48 && note(&s, &MAJOR, 0, 1) == 50 && note(&s, &MAJOR, 0, 7) == 60);
    CHECK(note(&s, &MAJOR, 1, 0) == 52 && note(&s, &MAJOR, 2, 0) == 55);   /* C E G: a chord along a row of columns */
}

static void test_flags(void)
{
    struct jln_pad p;
    struct jln s = state(JLN_CHROMATIC, 0, 3, 0);
    jln_pad(&s, &MAJOR, 0, 0, &p);
    CHECK(p.root && p.in_scale);
    jln_pad(&s, &MAJOR, 1, 0, &p);                               /* C# is not in C major */
    CHECK(!p.root && !p.in_scale);
    jln_pad(&s, &MAJOR, 2, 0, &p);
    CHECK(!p.root && p.in_scale);
    jln_pad(&s, &MAJOR, 2, 2, &p);                               /* index 12: the root again, an octave up */
    CHECK(p.root && p.note == 60);
}

static void test_outside_midi_range(void)
{
    struct jln s = state(JLN_CHROMATIC, 0, 7, 0);               /* octave 7: base 96 */
    CHECK(note(&s, &MAJOR, 7, 0) == 103);
    CHECK(note(&s, &MAJOR, 0, 7) == -1);                         /* 131: a pad that plays nothing */
}

static void test_octave_and_layout_limits(void)
{
    struct jln s;
    jln_init(&s);
    for (int i = 0; i < 20; i++)
        jln_octave_step(&s, 1);
    CHECK(s.octave == 5);                                        /* diatonic: octaves 0..5 */
    for (int i = 0; i < 20; i++)
        jln_octave_step(&s, -1);
    CHECK(s.octave == 0);
    jln_set_layout(&s, JLN_CHROMATIC);
    for (int i = 0; i < 20; i++)
        jln_octave_step(&s, 1);
    CHECK(s.octave == 6);                                        /* chromatic and chords: 0..6 */
    jln_set_layout(&s, JLN_DIATONIC);                            /* back to diatonic: pulled down to its top */
    CHECK(s.octave == 5);
}

static void test_track_step(void)
{
    struct jln s;
    jln_init(&s);
    jln_track_step(&s, -1);
    CHECK(s.track == 0);                                         /* no wrap: the first track stays */
    for (int i = 0; i < 40; i++)
        jln_track_step(&s, 1);
    CHECK(s.track == 15);
    jln_track_step(&s, -1);
    CHECK(s.track == 14);
}

static void test_pads_with_note(void)
{
    struct jln s = state(JLN_CHROMATIC, 0, 3, 0);
    uint64_t m = jln_pads_with_note(&s, &MAJOR, 53);
    CHECK(m == ((1ull << 5) | (1ull << 8)));                     /* pad (5,0) and pad (0,1): one note lights every pad with it */
    CHECK(jln_pads_with_note(&s, &MAJOR, 20) == 0);              /* no pad has it */
    CHECK(jln_pads_with_note(&s, &MAJOR, -1) == 0 && jln_pads_with_note(&s, &MAJOR, 200) == 0);
    s = state(JLN_DIATONIC, 0, 3, 0);
    m = jln_pads_with_note(&s, &MAJOR, 53);                      /* F: (0,1) and (3,0) */
    CHECK(m & (1ull << 8) && m & (1ull << 3));
}

static void test_colours(void)
{
    struct jln_pad root = {60, 1, 1}, in = {62, 0, 1}, out = {61, 0, 0}, none = {-1, 0, 0};
    CHECK(jln_color(&root, 0) == JLN_COLOR_ROOT && jln_color(&in, 0) == JLN_COLOR_IN_SCALE);
    CHECK(jln_color(&out, 0) == 0 && jln_color(&none, 0) == 0);
    CHECK(jln_color(&root, 1) == JLN_COLOR_PLAYING && jln_color(&out, 1) == JLN_COLOR_PLAYING);   /* playing wins over everything */
    CHECK(jln_color(&none, 1) == 0);
}

/* the scale page sets an octave directly: inside the layout's octaves, a place past the last one changes nothing */
static void test_set_octave(void)
{
    struct jln s;
    jln_init(&s);
    CHECK(jln_octaves(&s) == 6);                                 /* diatonic: octaves 0..5 */
    jln_set_octave(&s, 5);
    CHECK(s.octave == 5);
    jln_set_octave(&s, 6);
    CHECK(s.octave == 5);
    jln_set_octave(&s, 0);
    CHECK(s.octave == 0);
    jln_set_layout(&s, JLN_CHROMATIC);
    CHECK(jln_octaves(&s) == 7);
    jln_set_octave(&s, 6);
    CHECK(s.octave == 6);
    jln_set_octave(&s, 7);
    CHECK(s.octave == 6);
    jln_set_layout(&s, JLN_CHORDS);
    CHECK(jln_octaves(&s) == 7);
}

int main(void)
{
    test_set_octave();
    test_defaults();
    test_chromatic_relative();
    test_chromatic_absolute();
    test_diatonic();
    test_chords();
    test_flags();
    test_outside_midi_range();
    test_octave_and_layout_limits();
    test_track_step();
    test_pads_with_note();
    test_colours();
    printf("test_lpnote: %d checks, %d failed\n", checks, fails);
    return fails;
}
