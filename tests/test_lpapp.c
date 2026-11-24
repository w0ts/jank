/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of Launchpad Note mode against a Digitone II (core/jank_lpapp.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpapp.h"
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

static const struct jln_scale MAJOR = {"Major", 7, {0, 2, 4, 5, 7, 9, 11}};

static struct jlapp fresh(void)
{
    struct jlapp a;
    jlapp_init(&a, &MAJOR, 0);
    return a;
}

static struct jlx_ev pad(uint8_t x, uint8_t y, uint8_t v)
{
    struct jlx_ev e = {JLX_PAD, y, x, 0, v};
    return e;
}

static struct jlx_ev top(uint8_t idx, uint8_t v)
{
    struct jlx_ev e = {JLX_TOP, 0, 0, idx, v};
    return e;
}

static size_t press(struct jlapp *a, struct jlx_ev e, uint8_t out[4])
{
    return jlapp_on_lpx(a, &e, out, 1);
}

static void dn2(struct jlapp *a, uint8_t status, uint8_t d1, uint8_t d2)
{
    const uint8_t p[4] = {(uint8_t)(status >> 4), status, d1, d2};
    jlapp_on_dn2(a, p);
}

static uint8_t colour(struct jlapp *a, uint8_t x, uint8_t y)
{
    struct jlx_frame f;
    jlapp_frame(a, &f);
    return f.led[y * 8u + x].color;
}

static void test_pad_plays_note_on_track_channel(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    CHECK(press(&a, pad(0, 0, 100), o) == 1);                     /* diatonic C major, octave 3: 48 */
    CHECK(o[0] == 0x09 && o[1] == 0x90 && o[2] == 48 && o[3] == 100);
    CHECK(press(&a, pad(0, 0, 0), o) == 1);
    CHECK(o[0] == 0x08 && o[1] == 0x80 && o[2] == 48 && o[3] == 0);
    a.dn2_cable = 2;                                              /* the cable of the DN2's port */
    jlapp_on_lpx(&a, &(struct jlx_ev){JLX_TOP, 0, 0, 3, 127}, o, 1);   /* right: track 2, channel 2 */
    CHECK(press(&a, pad(1, 0, 64), o) == 1);
    CHECK(o[0] == 0x29 && o[1] == 0x91 && o[2] == 50 && o[3] == 64);
}

static void test_release_goes_to_the_note_it_started(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    press(&a, pad(1, 0, 90), o);                                  /* 50 */
    press(&a, top(0, 127), o);                                    /* octave up while the pad is held */
    press(&a, top(0, 0), o);
    CHECK(a.n.octave == 4);
    CHECK(press(&a, pad(1, 0, 0), o) == 1 && o[2] == 50 && o[1] == 0x80);   /* not 62 */
}

static void test_release_goes_to_the_channel_it_started(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    press(&a, pad(0, 0, 90), o);                                  /* track 1, channel 1 */
    press(&a, top(3, 127), o);                                    /* track 2 */
    CHECK(a.n.track == 1);
    CHECK(press(&a, pad(0, 0, 0), o) == 1 && o[1] == 0x80 && o[2] == 48);   /* still channel 1: no stuck note */
}

static void test_same_pitch_on_two_pads(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    jln_set_layout(&a.n, JLN_CHROMATIC);                          /* (5,0) and (0,1) are both 53 */
    CHECK(press(&a, pad(5, 0, 80), o) == 1 && o[1] == 0x90);
    CHECK(press(&a, pad(0, 1, 80), o) == 1 && o[1] == 0x90);
    CHECK(press(&a, pad(5, 0, 0), o) == 0);                       /* the other pad still holds it */
    CHECK(press(&a, pad(0, 1, 0), o) == 1 && o[1] == 0x80 && o[2] == 53);
}

static void test_pad_outside_midi_range_is_silent(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    jln_set_layout(&a.n, JLN_CHROMATIC);
    a.n.octave = 7;                                               /* base 96: the top rows pass 127 (the buttons stop at 6) */
    CHECK(press(&a, pad(0, 7, 90), o) == 0);
    CHECK(press(&a, pad(0, 7, 0), o) == 0);
}

static void test_repeated_press_and_stray_release(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    CHECK(press(&a, pad(2, 2, 70), o) == 1);
    CHECK(press(&a, pad(2, 2, 70), o) == 0);                      /* a pad already down: nothing new */
    CHECK(press(&a, pad(2, 2, 0), o) == 1);
    CHECK(press(&a, pad(2, 2, 0), o) == 0);                       /* a release with no press: nothing */
    CHECK(press(&a, pad(3, 3, 0), o) == 0);
}

static void test_buttons(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    CHECK(press(&a, top(0, 127), o) == 0 && a.n.octave == 4);     /* up: octave up, no MIDI */
    press(&a, top(1, 127), o);
    press(&a, top(1, 127), o);
    CHECK(a.n.octave == 2);
    press(&a, top(0, 0), o);                                      /* a release does nothing */
    press(&a, top(1, 0), o);
    CHECK(a.n.octave == 2);
    press(&a, top(2, 127), o);                                    /* left at the first track: stays */
    CHECK(a.n.track == 0);
    press(&a, top(3, 127), o);
    press(&a, top(3, 127), o);
    CHECK(a.n.track == 2);
    press(&a, top(2, 127), o);
    CHECK(a.n.track == 1);
    press(&a, top(5, 127), o);                                    /* the other buttons do nothing yet */
    press(&a, top(7, 127), o);
    CHECK(a.n.track == 1 && a.n.octave == 2);
    {
        struct jlx_ev side = {JLX_SIDE, 0, 0, 4, 127};
        CHECK(jlapp_on_lpx(&a, &side, o, 1) == 0);
    }
}

static void test_dn2_notes_light_the_pads(void)
{
    struct jlapp a = fresh();
    CHECK(colour(&a, 0, 0) == JLN_COLOR_ROOT);                    /* 48 is C: the root */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);                /* D */
    dn2(&a, 0x90, 50, 100);                                       /* the DN2 plays D (50) on channel 1 */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    CHECK(colour(&a, 0, 0) == JLN_COLOR_ROOT);
    dn2(&a, 0x80, 50, 0);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
    dn2(&a, 0x90, 50, 100);
    dn2(&a, 0x90, 50, 0);                                         /* a Note On with velocity 0 is an off */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_dn2_notes_per_channel(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    dn2(&a, 0x91, 50, 100);                                       /* channel 2: not the current track */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
    press(&a, top(3, 127), o);                                    /* track 2: it is lit now */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    press(&a, top(2, 127), o);                                    /* back to track 1: dark again */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_dn2_same_pitch_lights_every_pad(void)
{
    struct jlapp a = fresh();
    jln_set_layout(&a.n, JLN_CHROMATIC);
    dn2(&a, 0x90, 53, 100);
    CHECK(colour(&a, 5, 0) == JLN_COLOR_PLAYING && colour(&a, 0, 1) == JLN_COLOR_PLAYING);
}

static void test_dn2_all_notes_off_and_noise(void)
{
    struct jlapp a = fresh();
    dn2(&a, 0x90, 50, 100);
    dn2(&a, 0x90, 52, 100);
    dn2(&a, 0xB0, 7, 100);                                        /* a volume CC: ignored */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    dn2(&a, 0xB0, 123, 0);                                        /* All Notes Off on channel 1 */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE && colour(&a, 2, 0) == JLN_COLOR_IN_SCALE);
    {
        const uint8_t clock[4] = {0x0F, 0xF8, 0, 0};              /* a clock byte */
        const uint8_t sysex[4] = {0x04, 0xF0, 0x7D, 0x01};
        const uint8_t pressure[4] = {0x0D, 0xD0, 50, 0};
        jlapp_on_dn2(&a, clock);
        jlapp_on_dn2(&a, sysex);
        jlapp_on_dn2(&a, pressure);
    }
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_own_press_lights_the_pad(void)
{
    struct jlapp a = fresh();
    uint8_t o[4];
    press(&a, pad(1, 0, 100), o);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    press(&a, pad(1, 0, 0), o);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_buttons_lit(void)
{
    struct jlapp a = fresh();
    struct jlx_frame f;
    jlapp_frame(&a, &f);
    for (uint32_t i = 0; i < 4; i++)
        CHECK(f.led[JLX_TOP0 + i].color == JLAPP_COLOR_ARROW);
    CHECK(f.led[JLX_TOP0 + 5].color == JLAPP_COLOR_MODE);          /* the Note button */
    CHECK(f.led[JLX_TOP0 + 4].color == 0 && f.led[JLX_TOP0 + 6].color == 0 && f.led[JLX_TOP0 + 7].color == 0);
    for (uint32_t i = 0; i < 8; i++)
        CHECK(f.led[JLX_SIDE0 + i].color == 0);
    CHECK(f.led[JLX_LOGO].color == 0);
    for (uint32_t i = 0; i < JLX_LEDS; i++)
        CHECK(f.led[i].mode == JLX_STATIC);
}

static void test_release_all(void)
{
    struct jlapp a = fresh();
    uint8_t o[4 * 8];
    uint8_t one[4];
    press(&a, pad(0, 0, 100), one);
    press(&a, pad(1, 0, 100), one);
    press(&a, top(3, 127), one);
    press(&a, pad(2, 0, 100), one);                               /* a third one, on channel 2 */
    CHECK(jlapp_release_all(&a, o, 8) == 3);
    CHECK(o[1] == 0x80 && o[2] == 48 && o[5] == 0x80 && o[6] == 50 && o[9] == 0x81 && o[10] == 52);
    CHECK(jlapp_release_all(&a, o, 8) == 0);                        /* all up now */
    CHECK(press(&a, pad(0, 0, 0), one) == 0);
}

static void test_release_all_respects_the_room(void)
{
    struct jlapp a = fresh();
    uint8_t o[4 * 2], one[4];
    press(&a, pad(0, 0, 100), one);
    press(&a, pad(1, 0, 100), one);
    press(&a, pad(2, 0, 100), one);
    CHECK(jlapp_release_all(&a, o, 2) == 2);
    CHECK(jlapp_release_all(&a, o, 2) == 1);                        /* the rest waits for the next call */
    CHECK(jlapp_release_all(&a, o, 2) == 0);
}

static void test_roles_by_product_name(void)
{
    CHECK(jlapp_role_from_name("Launchpad X") == JLAPP_LPX);
    CHECK(jlapp_role_from_name("Novation Launchpad X") == JLAPP_LPX);
    CHECK(jlapp_role_from_name("Digitone II") == JLAPP_DN2);
    CHECK(jlapp_role_from_name("Elektron Digitone II") == JLAPP_DN2);
    CHECK(jlapp_role_from_name("Launchpad Mini MK3") == JLAPP_OTHER);   /* other Launchpads have other layouts */
    CHECK(jlapp_role_from_name("Launchpad Pro MK3") == JLAPP_OTHER);
    CHECK(jlapp_role_from_name("OP-Z") == JLAPP_OTHER);
    CHECK(jlapp_role_from_name("") == JLAPP_OTHER && jlapp_role_from_name(NULL) == JLAPP_OTHER);
}

static void test_midi_interface_choice(void)
{
    CHECK(jlapp_midi_interface_index(JLAPP_LPX) == 1);                  /* the Launchpad X's MIDI interface is its second, the first is DAW */
    CHECK(jlapp_midi_interface_index(JLAPP_DN2) == 0);
    CHECK(jlapp_midi_interface_index(JLAPP_OTHER) == 0);
}

/* ---- the scale page (hold Note) ---- the pad coordinates are (x, y) with y = 0 the bottom row; the page's rows count from the top (y = 7 - row) */

#define NOTE_BTN 5u

static struct jlapp fresh_list(void)
{
    struct jlapp a;
    jlapp_init(&a, jls_get(JLS_MAJOR), 0);
    return a;
}

static void tap(struct jlapp *a, uint8_t x, uint8_t y)       /* press and release a pad on the page */
{
    uint8_t o[4];
    CHECK(press(a, pad(x, y, 100), o) == 0);                 /* a page pad never plays */
    CHECK(press(a, pad(x, y, 0), o) == 0);
}

static void test_the_note_button_opens_the_page_while_held(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    CHECK(!a.page);
    press(&a, top(NOTE_BTN, 127), o);
    CHECK(a.page);
    press(&a, top(NOTE_BTN, 0), o);
    CHECK(!a.page);
    CHECK(press(&a, pad(0, 0, 100), o) == 1 && o[2] == 48);   /* back to playing */
}

static void test_page_pads_edit_and_do_not_play(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    press(&a, top(NOTE_BTN, 127), o);
    tap(&a, 4, 5);                                           /* white keys, row 3: the fifth one, G */
    CHECK(a.n.key == 7);
    tap(&a, 0, 6);                                           /* black keys, row 2: the one right of C, C# */
    CHECK(a.n.key == 1);
    tap(&a, 3, 6);
    CHECK(a.n.key == 6);                                     /* F# */
    tap(&a, 6, 5);                                           /* B */
    CHECK(a.n.key == 11);
    tap(&a, 2, 5);                                           /* an E */
    CHECK(a.n.key == 4);
    tap(&a, 7, 6);                                           /* pad (8, row 2) does nothing */
    CHECK(a.n.key == 4);
    press(&a, top(NOTE_BTN, 0), o);
    CHECK(press(&a, pad(0, 0, 100), o) == 1 && o[2] == 52);   /* off the page the same pad plays: key E, relative, octave 3: 48 + 4 */
}

static void test_page_picks_the_scale(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    press(&a, top(NOTE_BTN, 127), o);
    tap(&a, 1, 3);                                           /* the scales start at row 5 (y = 3): index 1, Minor */
    CHECK(jls_index_of(a.scale) == 1);
    tap(&a, 6, 2);                                           /* row 6, the seventh: index 8 + 6 = 14 */
    CHECK(jls_index_of(a.scale) == 14);
    tap(&a, 7, 0);                                           /* the last place of the last row: index 31 */
    CHECK(jls_index_of(a.scale) == 31);
    tap(&a, 1, 3);
    press(&a, top(NOTE_BTN, 0), o);
    CHECK(press(&a, pad(2, 0, 100), o) == 1 && o[2] == 51);   /* Minor: the third step is a minor third: 48 + 3 */
}

static void test_page_picks_the_layout(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    press(&a, top(NOTE_BTN, 127), o);
    tap(&a, 6, 7);                                           /* row 1, pad 7: chromatic */
    CHECK(a.n.layout == JLN_CHROMATIC);
    tap(&a, 1, 7);                                           /* horizontal only means something in the diatonic layout */
    CHECK(a.n.horizontal == 1);
    tap(&a, 4, 7);                                           /* pad 5: chords */
    CHECK(a.n.layout == JLN_CHORDS && a.n.horizontal == 0);
    tap(&a, 5, 7);                                           /* pad 6: diatonic */
    CHECK(a.n.layout == JLN_DIATONIC && a.n.horizontal == 1);
    tap(&a, 1, 7);
    CHECK(a.n.horizontal == 0);
    tap(&a, 1, 7);
    CHECK(a.n.horizontal == 1);
    tap(&a, 0, 7);                                           /* absolute root */
    CHECK(a.n.absolute == 1);
    tap(&a, 0, 7);
    CHECK(a.n.absolute == 0);
    tap(&a, 2, 7);                                           /* the unused pads change nothing */
    tap(&a, 3, 7);
    tap(&a, 7, 7);
    CHECK(a.n.layout == JLN_DIATONIC && a.n.absolute == 0);
}

static void test_page_picks_the_octave(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    press(&a, top(NOTE_BTN, 127), o);
    tap(&a, 5, 4);                                           /* row 4, pad 6: octave 5, the diatonic layout's last */
    CHECK(a.n.octave == 5);
    tap(&a, 6, 4);                                           /* past it: nothing */
    CHECK(a.n.octave == 5);
    tap(&a, 0, 4);
    CHECK(a.n.octave == 0);
    tap(&a, 6, 7);                                           /* chromatic has one more */
    tap(&a, 6, 4);
    CHECK(a.n.octave == 6);
    tap(&a, 5, 7);                                           /* back to diatonic: the octave comes down under its top */
    CHECK(a.n.octave == 5);
}

static void test_page_circle_of_fifths(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    press(&a, top(NOTE_BTN, 127), o);
    tap(&a, 7, 5);                                           /* forward: a fifth up */
    CHECK(a.n.key == 7);
    tap(&a, 7, 5);
    CHECK(a.n.key == 2);                                     /* D */
    tap(&a, 6, 6);                                           /* back: a fifth down */
    CHECK(a.n.key == 7);
    tap(&a, 6, 6);
    CHECK(a.n.key == 0);
    tap(&a, 6, 6);
    CHECK(a.n.key == 5);                                     /* F, the other way round */
}

static void test_page_relative_scale(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    press(&a, top(NOTE_BTN, 127), o);
    tap(&a, 2, 6);                                           /* C major -> A minor */
    CHECK(jls_index_of(a.scale) == JLS_MINOR && a.n.key == 9);
    tap(&a, 2, 6);                                           /* and back */
    CHECK(jls_index_of(a.scale) == JLS_MAJOR && a.n.key == 0);
    tap(&a, 6, 2);                                           /* Harmonic Minor, in C */
    CHECK(jls_index_of(a.scale) == JLS_HARMONIC_MINOR);
    tap(&a, 2, 6);                                           /* -> E flat major */
    CHECK(jls_index_of(a.scale) == JLS_MAJOR && a.n.key == 3);
    tap(&a, 2, 6);                                           /* the minor it came from, not natural minor: C harmonic minor */
    CHECK(jls_index_of(a.scale) == JLS_HARMONIC_MINOR && a.n.key == 0);
    tap(&a, 4, 2);                                           /* Minor Pentatonic */
    CHECK(jls_index_of(a.scale) == JLS_MINOR_PENTATONIC);
    tap(&a, 2, 6);
    CHECK(jls_index_of(a.scale) == JLS_MAJOR_PENTATONIC && a.n.key == 3);
    tap(&a, 2, 6);
    CHECK(jls_index_of(a.scale) == JLS_MINOR_PENTATONIC && a.n.key == 0);
    tap(&a, 2, 3);                                           /* Dorian: no relative scale */
    tap(&a, 2, 6);
    CHECK(jls_index_of(a.scale) == 2 && a.n.key == 0);
}

static void test_page_lights(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    CHECK(colour(&a, 0, 5) != JLAPP_PAGE_ON);                /* off the page the pads show notes */
    press(&a, top(NOTE_BTN, 127), o);
    CHECK(colour(&a, 5, 7) == JLAPP_PAGE_ON);                /* diatonic */
    CHECK(colour(&a, 4, 7) == JLAPP_PAGE_LAYOUT && colour(&a, 6, 7) == JLAPP_PAGE_LAYOUT);
    CHECK(colour(&a, 1, 7) == JLAPP_PAGE_ON);                /* horizontal */
    CHECK(colour(&a, 0, 7) == JLAPP_PAGE_LAYOUT);            /* relative root: absolute is off */
    CHECK(colour(&a, 2, 7) == 0 && colour(&a, 3, 7) == 0 && colour(&a, 7, 7) == 0);
    CHECK(colour(&a, 0, 5) == JLAPP_PAGE_ON);                /* C */
    CHECK(colour(&a, 1, 5) == JLAPP_PAGE_WHITE_KEY);
    CHECK(colour(&a, 0, 6) == JLAPP_PAGE_BLACK_KEY);
    CHECK(colour(&a, 2, 6) == JLAPP_PAGE_SHIFT && colour(&a, 6, 6) == JLAPP_PAGE_SHIFT && colour(&a, 7, 5) == JLAPP_PAGE_SHIFT);
    CHECK(colour(&a, 2, 6) != 0 && colour(&a, 7, 6) == 0);
    CHECK(colour(&a, 3, 4) == JLAPP_PAGE_ON);                /* octave 3 */
    CHECK(colour(&a, 0, 4) == JLAPP_PAGE_OCTAVE && colour(&a, 5, 4) == JLAPP_PAGE_OCTAVE);
    CHECK(colour(&a, 6, 4) == 0 && colour(&a, 7, 4) == 0);   /* diatonic stops at octave 5 */
    CHECK(colour(&a, 0, 3) == JLAPP_PAGE_ON);                /* Major */
    CHECK(colour(&a, 1, 3) == JLAPP_PAGE_SCALE && colour(&a, 7, 0) == JLAPP_PAGE_SCALE);
    tap(&a, 6, 7);                                           /* chromatic: its extra octave lights, horizontal goes dark */
    CHECK(colour(&a, 6, 4) == JLAPP_PAGE_OCTAVE && colour(&a, 1, 7) == 0);
    tap(&a, 4, 5);                                           /* G is the key: C is no longer lit, G is */
    CHECK(colour(&a, 4, 5) == JLAPP_PAGE_ON && colour(&a, 0, 5) == JLAPP_PAGE_WHITE_KEY);
    tap(&a, 1, 6);                                           /* a black key lights */
    CHECK(colour(&a, 1, 6) == JLAPP_PAGE_ON && colour(&a, 4, 5) == JLAPP_PAGE_WHITE_KEY);
}

static void test_page_lights_with_a_scale_that_is_not_in_the_list(void)
{
    struct jlapp a = fresh();                                /* the test's own major scale */
    uint8_t o[4];
    press(&a, top(NOTE_BTN, 127), o);
    for (uint8_t y = 0; y < 4; y++)
        for (uint8_t x = 0; x < 8; x++)
            CHECK(colour(&a, x, y) != JLAPP_PAGE_ON);        /* none of the scales is marked as selected */
    tap(&a, 0, 6);
    tap(&a, 2, 6);                                           /* a relative scale for a scale the page does not know: nothing */
    CHECK(a.scale == &MAJOR);
}

static void test_a_note_down_before_the_page_still_ends(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    CHECK(press(&a, pad(0, 0, 100), o) == 1);
    press(&a, top(NOTE_BTN, 127), o);
    CHECK(press(&a, pad(0, 0, 0), o) == 1 && o[0] == 0x08 && o[2] == 48);   /* its Note Off still goes out with the page open */
    press(&a, top(NOTE_BTN, 0), o);
    CHECK(press(&a, pad(0, 0, 0), o) == 0);                  /* and a second release is nothing */
}

static void test_page_buttons_still_work_and_note_stays_lit(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    struct jlx_frame f;
    press(&a, top(NOTE_BTN, 127), o);
    press(&a, top(0, 127), o);                               /* up: octave 4 */
    CHECK(a.n.octave == 4);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + NOTE_BTN].color == JLAPP_COLOR_MODE);
    CHECK(f.led[JLX_TOP0 + 0].color == JLAPP_COLOR_ARROW);
}

int main(void)
{
    test_the_note_button_opens_the_page_while_held();
    test_page_pads_edit_and_do_not_play();
    test_page_picks_the_scale();
    test_page_picks_the_layout();
    test_page_picks_the_octave();
    test_page_circle_of_fifths();
    test_page_relative_scale();
    test_page_lights();
    test_page_lights_with_a_scale_that_is_not_in_the_list();
    test_a_note_down_before_the_page_still_ends();
    test_page_buttons_still_work_and_note_stays_lit();
    test_roles_by_product_name();
    test_midi_interface_choice();
    test_pad_plays_note_on_track_channel();
    test_release_goes_to_the_note_it_started();
    test_release_goes_to_the_channel_it_started();
    test_same_pitch_on_two_pads();
    test_pad_outside_midi_range_is_silent();
    test_repeated_press_and_stray_release();
    test_buttons();
    test_dn2_notes_light_the_pads();
    test_dn2_notes_per_channel();
    test_dn2_same_pitch_lights_every_pad();
    test_dn2_all_notes_off_and_noise();
    test_own_press_lights_the_pad();
    test_buttons_lit();
    test_release_all();
    test_release_all_respects_the_room();
    printf("test_lpapp: %d checks, %d failed\n", checks, fails);
    return fails;
}
