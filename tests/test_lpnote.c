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
    struct jln_pad root = {60, 1, 0, 1, 0}, in = {62, 0, 0, 1, 0}, high = {64, 0, 1, 1, 0}, out = {61, 0, 0, 0, 0}, none = {-1, 0, 0, 0, 0};
    CHECK(jln_color(&root, 0) == JLN_COLOR_ROOT && jln_color(&in, 0) == JLN_COLOR_IN_SCALE);
    CHECK(jln_color(&high, 0) == JLN_COLOR_HIGHLIGHT && JLN_COLOR_HIGHLIGHT == 37);          /* the 3rd and 5th: LIGHT_BLUE, between the root and the rest */
    root.highlight = 1;                                          /* a root that is also a highlight (a scale whose 3rd step is the root an octave up): the root wins */
    CHECK(jln_color(&root, 0) == JLN_COLOR_ROOT);
    CHECK(jln_color(&high, 1) == JLN_COLOR_PLAYING);
    CHECK(jln_color(&out, 0) == JLN_COLOR_OUT_OF_SCALE && jln_color(&none, 0) == 0);          /* a note outside the scale is 43 (not white: owner), no note at all is dark */
    CHECK(JLN_COLOR_ROOT == 49 && JLN_COLOR_IN_SCALE == 38 && JLN_COLOR_OUT_OF_SCALE == 43 && JLN_COLOR_MINOR_ONLY == 46 && JLN_COLOR_PLAYING == 21);   /* the owner's picks from the colour chart (2026-11-25): root 49, off scale 43, minor-only 46; LIGHT_BLUE_HALF, GREEN */
    CHECK(JLN_COLOR_OUT_OF_SCALE != 3 && JLN_COLOR_MINOR_ONLY != JLN_COLOR_OUT_OF_SCALE);   /* never white; the two are told apart */
    out.minor_only = 1;
    CHECK(jln_color(&out, 0) == JLN_COLOR_MINOR_ONLY);           /* minor but not major: 46 */
    CHECK(jln_color(&root, 1) == JLN_COLOR_PLAYING && jln_color(&out, 1) == JLN_COLOR_PLAYING);   /* playing wins over everything */
    CHECK(jln_color(&none, 1) == 0);
}

/* Launchpad 95 (ScaleComponent.note): highlight = note == scale[2] or note == scale[4]: the scale's third and fifth STEP, not a fixed interval */
static void test_highlight_is_the_third_and_fifth_step_of_the_scale(void)
{
    static const struct jln_scale PENTA = {"Minor pentatonic", 5, {0, 3, 5, 7, 10}};   /* its 3rd step is the fourth (5), its 5th step the seventh (10) */
    static const struct jln_scale TWO = {"Two notes", 2, {0, 7}};
    struct jln s;
    struct jln_pad p;
    unsigned lit = 0;
    jln_init(&s);                                                /* C, octave 3, diatonic */
    s.layout = JLN_CHROMATIC;                                    /* all 12 notes from C: 48 .. 59 along the row */
    for (uint8_t x = 0; x < 8; x++) {
        jln_pad(&s, &MAJOR, x, 0, &p);                           /* C C# D D# E F F# G: a semitone a pad */
        CHECK(p.highlight == (x == 4 || x == 7));                /* E (52) and G (55): the 3rd and 5th step of C major */
    }
    CHECK(jln_color(&(struct jln_pad){52, 0, 1, 1, 0}, 0) == JLN_COLOR_HIGHLIGHT);
    jln_pad(&s, &PENTA, 5, 0, &p);                               /* x = 5 is F (53), the pentatonic's 3rd step (5) */
    CHECK(p.note == 53 && p.highlight && p.in_scale);
    jln_pad(&s, &PENTA, 10 - 8, 0, &p);                          /* x = 2 is D (50): not in minor pentatonic at all */
    CHECK(p.note == 50 && !p.highlight && !p.in_scale);
    jln_set_layout(&s, JLN_DIATONIC);
    for (uint8_t x = 0; x < 5; x++) {                            /* the diatonic row of 5 steps: the 3rd and 5th step are x = 2 and 4 */
        jln_pad(&s, &PENTA, x, 0, &p);
        CHECK(p.highlight == (x == 2 || x == 4));
    }
    for (uint8_t i = 0; i < 8; i++) {                            /* a two-note scale has no 3rd step: nothing is highlighted */
        jln_pad(&s, &TWO, i, 0, &p);
        lit += p.highlight;
    }
    CHECK(lit == 0);
}

/* the colour of the 12 notes above C (octave 3, the chromatic row's first 12 pads) against a scale */
static void colours_of_a_row(const struct jln_scale *sc, uint8_t out[12])
{
    struct jln s = state(JLN_CHROMATIC, 0, 3, 0);
    struct jln_pad p;
    for (uint8_t x = 0; x < 8; x++) {
        jln_pad(&s, sc, x, 0, &p);
        out[x] = jln_color(&p, 0);
    }
    for (uint8_t x = 0; x < 4; x++) {                            /* row 1 starts a fourth (5 semitones) higher: its pads x = 3..6 are semitones 8, 9, 10, 11 */
        jln_pad(&s, sc, (uint8_t)(x + 3), 1, &p);
        out[8 + x] = jln_color(&p, 0);
    }
}

/* owner 2026-11-24: against major (the Digitone's Chromatic uses the major scale of the key as reference) the notes outside it are DARK_GREY, and the minor-but-not-major ones a very dark purple */
static void test_chromatic_scale_is_coloured_against_major(void)
{
    static const struct jln_scale CHROM = {"Chromatic", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
    static const struct jln_scale MINOR = {"Minor", 7, {0, 2, 3, 5, 7, 8, 10}};
    uint8_t c[12];
    colours_of_a_row(&CHROM, c);                                 /* C C# D D# E F F# G G# A A# B */
    CHECK(c[0] == JLN_COLOR_ROOT);
    CHECK(c[2] == JLN_COLOR_IN_SCALE && c[5] == JLN_COLOR_IN_SCALE && c[9] == JLN_COLOR_IN_SCALE && c[11] == JLN_COLOR_IN_SCALE);   /* D F A B: in major */
    CHECK(c[4] == JLN_COLOR_HIGHLIGHT && c[7] == JLN_COLOR_HIGHLIGHT);                                                             /* E and G: major's 3rd and 5th */
    CHECK(c[3] == JLN_COLOR_MINOR_ONLY && c[8] == JLN_COLOR_MINOR_ONLY && c[10] == JLN_COLOR_MINOR_ONLY);                          /* D# G# A#: minor, not major */
    CHECK(c[1] == JLN_COLOR_OUT_OF_SCALE && c[6] == JLN_COLOR_OUT_OF_SCALE);                                                       /* C# F#: in neither */
    colours_of_a_row(&MAJOR, c);                                 /* a real major scale gives the same picture */
    CHECK(c[3] == JLN_COLOR_MINOR_ONLY && c[1] == JLN_COLOR_OUT_OF_SCALE && c[4] == JLN_COLOR_HIGHLIGHT);
    colours_of_a_row(&MINOR, c);                                 /* any other scale: its own notes, everything else DARK_GREY, no purple */
    CHECK(c[3] == JLN_COLOR_HIGHLIGHT && c[4] == JLN_COLOR_OUT_OF_SCALE && c[1] == JLN_COLOR_OUT_OF_SCALE && c[8] == JLN_COLOR_IN_SCALE);
    for (int i = 0; i < 12; i++)
        CHECK(c[i] != JLN_COLOR_MINOR_ONLY);
    colours_of_a_row(&PENT, c);
    for (int i = 0; i < 12; i++)
        CHECK(c[i] != JLN_COLOR_MINOR_ONLY);
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
    test_highlight_is_the_third_and_fifth_step_of_the_scale();
    test_chromatic_scale_is_coloured_against_major();
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
