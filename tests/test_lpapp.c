/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of Launchpad Note mode against a Digitone II (core/jank_lpapp.c). Exit status: the number of failed checks. */
#include <ctype.h>
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

/* an app whose Digitone is live, with PLAY and the active-track read (the normal case) */
static struct jlapp fresh(void)
{
    struct jlapp a;
    jlapp_init(&a, &MAJOR);
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
    return a;
}

static struct jlapp fresh_not_live(int state)
{
    struct jlapp a;
    jlapp_init(&a, &MAJOR);
    jlapp_set_live(&a, state, 1, 1);
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

/* the older tests' view of a press: 1 for a note on / off, out = {0x09 | 0x08, status | track, note, velocity}; buttons and the rest give 0 */
static size_t press(struct jlapp *a, struct jlx_ev e, uint8_t out[4])
{
    struct jlapp_act act;
    if (jlapp_on_lpx_acts(a, &e, &act, 1) < 1 || (act.kind != JLAPP_ACT_NOTE_ON && act.kind != JLAPP_ACT_NOTE_OFF))
        return 0;
    out[0] = act.kind == JLAPP_ACT_NOTE_ON ? 0x09 : 0x08;
    out[1] = (uint8_t)((act.kind == JLAPP_ACT_NOTE_ON ? 0x90 : 0x80) | act.track);
    out[2] = act.note;
    out[3] = act.vel;
    return 1;
}

static size_t acts(struct jlapp *a, struct jlx_ev e, struct jlapp_act *o)
{
    return jlapp_on_lpx_acts(a, &e, o, 1);
}

static void say_nothing_yet(struct jlapp *a)
{
    char m[JLAPP_MSG_MAX];
    jlapp_take_message(a, m, sizeof m);
}

/* the message the last press(es) left, "" if none */
static const char *msg(struct jlapp *a)
{
    static char m[JLAPP_MSG_MAX];
    m[0] = 0;
    jlapp_take_message(a, m, sizeof m);
    return m;
}

static uint8_t colour(struct jlapp *a, uint8_t x, uint8_t y)
{
    struct jlx_frame f;
    jlapp_frame(a, &f);
    return f.led[y * 8u + x].color;
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
    jlapp_init(&a, jls_get(JLS_MAJOR));
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
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
    tap(&a, 2, 3);                                           /* the Digitone's scales: two rows of 8 from page row 5 (y = 3): index 2, Dorian */
    CHECK(jls_index_of(a.scale) == 2);
    tap(&a, 6, 2);                                           /* the next row, the seventh: index 8 + 6 = 14, Combo Minor */
    CHECK(jls_index_of(a.scale) == 14);
    tap(&a, 7, 2);                                           /* the last place of the page: index 8 + 7 = 15 */
    CHECK(jls_index_of(a.scale) == 15);
    tap(&a, 1, 4);                                           /* the second page pad (the row above the scales): browsing only, the scale stays */
    CHECK(jls_index_of(a.scale) == 15 && a.scale_page == 1);
    tap(&a, 0, 3);                                           /* the second page: index 16 + 0 */
    CHECK(jls_index_of(a.scale) == 16);
    tap(&a, 7, 2);                                           /* 16 + 8 + 7 = 31, its last */
    CHECK(jls_index_of(a.scale) == 31);
    tap(&a, 2, 4);                                           /* the third page: 4 scales, 32 .. 35 */
    CHECK(jls_index_of(a.scale) == 31 && a.scale_page == 2);
    tap(&a, 3, 3);                                           /* 32 + 3 = 35, the last scale */
    CHECK(jls_index_of(a.scale) == 35);
    tap(&a, 4, 3);                                           /* index 36 does not exist: no pad, nothing changes */
    tap(&a, 0, 2);                                           /* nor does 32 + 8 */
    CHECK(jls_index_of(a.scale) == 35);
    tap(&a, 0, 4);                                           /* the first page again */
    tap(&a, 6, 3);                                           /* Aeolian (Min), index 6 */
    press(&a, top(NOTE_BTN, 0), o);
    CHECK(jls_index_of(a.scale) == 6);
    CHECK(press(&a, pad(2, 0, 100), o) == 1 && o[2] == 51);   /* the third step is a minor third: 48 + 3 */
}

static void test_the_scale_page_opens_on_the_page_of_the_scale_in_use(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(a.scale_page == 0);                                      /* Major: the first page */
    tap(&a, 1, 4);                                                 /* browse to the second page, without choosing */
    CHECK(a.scale_page == 1);
    acts(&a, top(NOTE_BTN, 0), &o);
    acts(&a, top(NOTE_BTN, 127), &o);                              /* opened again: back on the page of the scale in use */
    CHECK(a.scale_page == 0);
    tap(&a, 2, 3);                                                 /* (picking on the first page ...) */
    a.scale = jls_get(30);                                         /* ... the scale became one on the second page (as when the Digitone's scale is taken) */
    acts(&a, top(NOTE_BTN, 0), &o);
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(a.scale_page == 1);
    CHECK(colour(&a, 0, 4) == JLAPP_AMBER_THIRD && colour(&a, 1, 4) == JLAPP_AMBER && colour(&a, 2, 4) == JLAPP_AMBER_THIRD && colour(&a, 3, 4) == 0);   /* the three page pads: AMBER, the shown page bright */
    CHECK(colour(&a, 6, 2) == JLAPP_BLUE);                         /* scale 30 = 16 + 14: the second row (y = 2), x = 6 */
    tap(&a, 2, 4);                                                 /* the third page: scales 32 .. 35 */
    CHECK(colour(&a, 3, 3) == JLAPP_BLUE_THIRD);                   /* the 36th */
    CHECK(colour(&a, 4, 3) == 0 && colour(&a, 7, 3) == 0 && colour(&a, 0, 2) == 0 && colour(&a, 7, 2) == 0);   /* past the 36th: no pads */
    tap(&a, 0, 4);
    CHECK(colour(&a, 0, 4) == JLAPP_AMBER && colour(&a, 6, 2) == JLAPP_BLUE_THIRD && colour(&a, 1, 3) == JLAPP_BLUE_THIRD);
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

/* owner 2026-11-24: the octave row is gone (up / down do the octave); the page's two bottom rows select the track: 1-8 on the upper (y = 1), 9-16 on the lower (y = 0) */
static void test_page_bottom_rows_select_the_track(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(acts(&a, pad(2, 1, 127), &o) == 1 && o.kind == JLAPP_ACT_SELECT && o.track == 2 && a.n.track == 2);   /* track 3 */
    CHECK(strcmp(msg(&a), "TRACK 3") == 0);
    CHECK(acts(&a, pad(2, 1, 0), &o) == 0);                        /* the release is nothing */
    CHECK(acts(&a, pad(5, 0, 127), &o) == 1 && o.kind == JLAPP_ACT_SELECT && o.track == 13 && a.n.track == 13);   /* the lower row: 9 + 5 = track 14 */
    CHECK(strcmp(msg(&a), "TRACK 14") == 0);
    acts(&a, pad(5, 0, 0), &o);
    CHECK(acts(&a, pad(5, 0, 127), &o) == 0 && a.n.track == 13);   /* the same track again: nothing for the Digitone */
    acts(&a, pad(5, 0, 0), &o);
    CHECK(colour(&a, 5, 0) == JLAPP_COLOR_TRACK_CURRENT && colour(&a, 2, 1) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 0, 0) == JLAPP_COLOR_TRACK_IDLE);   /* AMBER selected, WHITE the rest */
    CHECK(a.n.octave == 3 && a.n.key == 0);                        /* nothing else changed */
    acts(&a, pad(7, 7, 127), &o);                                  /* the track pads switch on: the bottom rows still only select while the page is open */
    acts(&a, pad(7, 7, 0), &o);
    CHECK(a.track_pads);
    CHECK(acts(&a, pad(0, 1, 127), &o) == 1 && o.kind == JLAPP_ACT_SELECT && o.track == 0 && a.track_held == 0);
    acts(&a, pad(0, 1, 0), &o);
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
    tap(&a, 3, 2);                                           /* Harmonic Min (index 11 = 8 + 3), in C */
    CHECK(jls_index_of(a.scale) == JLS_HARMONIC_MINOR);
    tap(&a, 2, 6);                                           /* -> E flat major */
    CHECK(jls_index_of(a.scale) == JLS_MAJOR && a.n.key == 3);
    tap(&a, 2, 6);                                           /* the minor it came from, not natural minor: C harmonic minor */
    CHECK(jls_index_of(a.scale) == JLS_HARMONIC_MINOR && a.n.key == 0);
    tap(&a, 0, 2);                                           /* Pentaton. Min (index 8) */
    CHECK(jls_index_of(a.scale) == JLS_MINOR_PENTATONIC);
    tap(&a, 2, 6);
    CHECK(jls_index_of(a.scale) == JLS_MAJOR_PENTATONIC && a.n.key == 3);
    tap(&a, 2, 6);
    CHECK(jls_index_of(a.scale) == JLS_MINOR_PENTATONIC && a.n.key == 0);
    tap(&a, 2, 3);                                           /* Dorian (index 2): no relative scale */
    tap(&a, 2, 6);
    CHECK(jls_index_of(a.scale) == 2 && a.n.key == 0);
}

/* the page's colours are Launchpad 95's (SkinMK2.py Scale.*): keys and Horizontal green, AbsoluteRoot / Mode / Octave red, Modus (the scales) blue, the relative
 * scale and the circle of fifths plain blue; on is the bright one, off its "third" (palette 21/23, 5/7, 45/47) */
static void test_page_lights(void)
{
    struct jlapp a = fresh_list();
    uint8_t o[4];
    CHECK(JLAPP_GREEN == 21 && JLAPP_GREEN_THIRD == 23 && JLAPP_RED == 5 && JLAPP_RED_THIRD == 7 && JLAPP_BLUE == 45 && JLAPP_BLUE_THIRD == 47);
    CHECK(colour(&a, 0, 0) == JLN_COLOR_ROOT && colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);   /* off the page the pads show notes: C blue, D light blue */
    press(&a, top(NOTE_BTN, 127), o);
    CHECK(colour(&a, 5, 7) == JLAPP_RED);                    /* the diatonic layout: Mode on */
    CHECK(colour(&a, 4, 7) == JLAPP_RED_THIRD && colour(&a, 6, 7) == JLAPP_RED_THIRD);
    CHECK(colour(&a, 1, 7) == JLAPP_GREEN);                  /* Horizontal on */
    CHECK(colour(&a, 0, 7) == JLAPP_RED_THIRD);              /* AbsoluteRoot off */
    CHECK(colour(&a, 2, 7) == 0 && colour(&a, 3, 7) == 0);   /* pads this port has no job for */
    CHECK(colour(&a, 7, 7) == JLAPP_GREEN_THIRD);            /* the track pads switch: off */
    CHECK(colour(&a, 0, 5) == JLAPP_GREEN);                  /* the key C: Key on */
    CHECK(colour(&a, 1, 5) == JLAPP_GREEN_THIRD);            /* D: Key off */
    CHECK(colour(&a, 0, 6) == JLAPP_GREEN_THIRD);            /* C#: a black key, the same Key off */
    CHECK(colour(&a, 2, 6) == JLAPP_BLUE && colour(&a, 6, 6) == JLAPP_BLUE && colour(&a, 7, 5) == JLAPP_BLUE);   /* RelativeScale, CircleOfFifths both ways */
    CHECK(colour(&a, 7, 6) == 0);
    CHECK(colour(&a, 1, 3) == JLAPP_BLUE);                   /* Modus: Ionian (Maj), index 1, on */
    CHECK(colour(&a, 0, 3) == JLAPP_BLUE_THIRD && colour(&a, 7, 2) == JLAPP_BLUE_THIRD);   /* Chromatic (index 0) and the 16th, off */
    CHECK(colour(&a, 0, 4) == JLAPP_AMBER && colour(&a, 1, 4) == JLAPP_AMBER_THIRD && colour(&a, 2, 4) == JLAPP_AMBER_THIRD && colour(&a, 3, 4) == 0);   /* the page pads: AMBER, the first page shown */
    CHECK(colour(&a, 0, 1) == JLAPP_COLOR_TRACK_CURRENT && colour(&a, 7, 0) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 0, 0) == JLAPP_COLOR_TRACK_IDLE);   /* the bottom rows: the tracks, track 1 (upper row, left) selected AMBER, the rest WHITE */
    tap(&a, 6, 7);                                           /* chromatic: Horizontal shows its state in every layout, as in LP95 */
    CHECK(colour(&a, 1, 7) == JLAPP_GREEN && colour(&a, 6, 7) == JLAPP_RED && colour(&a, 5, 7) == JLAPP_RED_THIRD);
    tap(&a, 4, 7);                                           /* chords: Horizontal is off */
    CHECK(colour(&a, 4, 7) == JLAPP_RED && colour(&a, 1, 7) == JLAPP_GREEN_THIRD);
    tap(&a, 4, 5);                                           /* G is the key: C is no longer on, G is */
    CHECK(colour(&a, 4, 5) == JLAPP_GREEN && colour(&a, 0, 5) == JLAPP_GREEN_THIRD);
    tap(&a, 1, 6);                                           /* a black key lights, G does not */
    CHECK(colour(&a, 1, 6) == JLAPP_GREEN && colour(&a, 4, 5) == JLAPP_GREEN_THIRD);
    tap(&a, 0, 7);                                           /* AbsoluteRoot on */
    CHECK(colour(&a, 0, 7) == JLAPP_RED);
}

static void test_arrows_dim_at_their_limits(void)
{
    struct jlapp a = fresh_list();
    struct jlx_frame f;
    uint8_t o[4];
    for (int i = 0; i < 6; i++)
        press(&a, top(0, 127), o);                           /* up to the highest octave of the diatonic layout (5) */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + 0].color == JLAPP_COLOR_ARROW_OFF && f.led[JLX_TOP0 + 1].color == JLAPP_COLOR_ARROW_ON);
    for (int i = 0; i < 6; i++)
        press(&a, top(1, 127), o);                           /* down to octave 0 */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + 0].color == JLAPP_COLOR_ARROW_ON && f.led[JLX_TOP0 + 1].color == JLAPP_COLOR_ARROW_OFF);
    press(&a, top(3, 127), o);                               /* one track to the right: both ways open */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + 2].color == JLAPP_COLOR_ARROW_ON && f.led[JLX_TOP0 + 3].color == JLAPP_COLOR_ARROW_ON);
    for (int i = 0; i < 20; i++)
        press(&a, top(3, 127), o);                           /* to the last track */
    jlapp_frame(&a, &f);
    CHECK(a.n.track == JLN_TRACKS - 1u && f.led[JLX_TOP0 + 2].color == JLAPP_COLOR_ARROW_ON && f.led[JLX_TOP0 + 3].color == JLAPP_COLOR_ARROW_OFF);
}

static void test_page_lights_with_a_scale_that_is_not_in_the_list(void)
{
    struct jlapp a = fresh();                                /* the test's own major scale */
    uint8_t o[4];
    press(&a, top(NOTE_BTN, 127), o);
    for (uint8_t y = 0; y < 4; y++)
        for (uint8_t x = 0; x < 8; x++)
            CHECK(colour(&a, x, y) != JLAPP_BLUE);           /* none of the scales is marked as selected */
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
    CHECK(f.led[JLX_TOP0 + 0].color == JLAPP_COLOR_ARROW_ON);                /* octave 4 of 0..5: up still goes */
}

static void switch_track_pads(struct jlapp *a)
{
    struct jlapp_act o;
    acts(a, top(NOTE_BTN, 127), &o);
    acts(a, pad(7, 7, 127), &o);
    acts(a, pad(7, 7, 0), &o);
    acts(a, top(NOTE_BTN, 0), &o);
    say_nothing_yet(a);
}

/* does the message the app has now say prefix + name, upper case and cut to the 20 characters a message holds? */
static int says(struct jlapp *a, const char *prefix, const char *name)
{
    char want[96];
    size_t i;
    snprintf(want, sizeof want, "%s%s", prefix, name);
    for (i = 0; want[i]; i++)
        want[i] = (char)toupper((unsigned char)want[i]);
    if (strlen(want) > JLAPP_MSG_MAX - 1u)
        want[JLAPP_MSG_MAX - 1u] = 0;
    return strcmp(msg(a), want) == 0;
}

/* ---- Note mode through live control: acts, never MIDI ---- */

static void test_a_pad_plays_a_note_on_the_current_track(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    CHECK(acts(&a, pad(0, 0, 100), &o) == 1 && o.kind == JLAPP_ACT_NOTE_ON && o.track == 0 && o.note == 48 && o.vel == 100);   /* diatonic C major, octave 3 */
    CHECK(acts(&a, pad(0, 0, 0), &o) == 1 && o.kind == JLAPP_ACT_NOTE_OFF && o.track == 0 && o.note == 48);
    acts(&a, top(3, 127), &o);                                    /* right: track 2 */
    CHECK(acts(&a, pad(1, 0, 64), &o) == 1 && o.kind == JLAPP_ACT_NOTE_ON && o.track == 1 && o.note == 50 && o.vel == 64);
}

static void test_release_goes_to_the_note_and_the_track_it_started(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, pad(1, 0, 90), &o);                                  /* 50, track 1 */
    acts(&a, top(0, 127), &o);                                    /* octave up while the pad is held */
    acts(&a, top(3, 127), &o);                                    /* and the next track */
    CHECK(a.n.octave == 4 && a.n.track == 1);
    CHECK(acts(&a, pad(1, 0, 0), &o) == 1 && o.kind == JLAPP_ACT_NOTE_OFF && o.note == 50 && o.track == 0);   /* not 62, not track 2: no stuck note */
}

static void test_same_pitch_on_two_pads(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    jln_set_layout(&a.n, JLN_CHROMATIC);                          /* (5,0) and (0,1) are both 53 */
    CHECK(acts(&a, pad(5, 0, 80), &o) == 1 && o.kind == JLAPP_ACT_NOTE_ON);
    CHECK(acts(&a, pad(0, 1, 80), &o) == 1 && o.kind == JLAPP_ACT_NOTE_ON);
    CHECK(acts(&a, pad(5, 0, 0), &o) == 0);                       /* the other pad still holds it */
    CHECK(acts(&a, pad(0, 1, 0), &o) == 1 && o.kind == JLAPP_ACT_NOTE_OFF && o.note == 53);
}

static void test_pad_outside_midi_range_is_silent(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    jln_set_layout(&a.n, JLN_CHROMATIC);
    a.n.octave = 7;                                               /* base 96: the top rows pass 127 (the buttons stop at 6) */
    CHECK(acts(&a, pad(0, 7, 90), &o) == 0);
    CHECK(acts(&a, pad(0, 7, 0), &o) == 0);
    CHECK(msg(&a)[0] == 0);                                       /* silent, and not a "cannot play" either */
}

static void test_repeated_press_and_stray_release(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    CHECK(acts(&a, pad(2, 2, 70), &o) == 1);
    CHECK(acts(&a, pad(2, 2, 70), &o) == 0);                      /* a pad already down: nothing new */
    CHECK(acts(&a, pad(2, 2, 0), &o) == 1);
    CHECK(acts(&a, pad(2, 2, 0), &o) == 0);                       /* a release with no press: nothing */
    CHECK(acts(&a, pad(3, 3, 0), &o) == 0);
    CHECK(jlapp_on_lpx_acts(&a, &(struct jlx_ev){JLX_PAD, 0, 0, 0, 100}, &o, 0) == 0 && a.held[0].note < 0);   /* no room: nothing changes */
}

static void test_octave_and_other_buttons(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlx_ev side = {JLX_SIDE, 0, 0, 2, 127};   /* scene button 3: no job yet (scene 5 is start / stop) */
    CHECK(acts(&a, top(0, 127), &o) == 0 && a.n.octave == 4);     /* up: octave up, nothing for the Digitone */
    CHECK(strcmp(msg(&a), "OCTAVE 4") == 0);
    acts(&a, top(1, 127), &o);
    acts(&a, top(1, 127), &o);
    CHECK(a.n.octave == 2 && strcmp(msg(&a), "OCTAVE 2") == 0);
    acts(&a, top(0, 0), &o);                                      /* a release does nothing */
    acts(&a, top(1, 0), &o);
    CHECK(a.n.octave == 2 && msg(&a)[0] == 0);
    acts(&a, top(5, 127), &o);                                    /* a scene button with no job does nothing (Session is the colour chart, Capture the mixer) */
    CHECK(jlapp_on_lpx_acts(&a, &side, &o, 1) == 0 && a.n.octave == 2 && msg(&a)[0] == 0);
}

static void test_own_press_lights_the_pad(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, pad(1, 0, 100), &o);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    acts(&a, pad(1, 0, 0), &o);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_buttons_lit(void)
{
    struct jlapp a = fresh();
    struct jlx_frame f;
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + 0].color == JLAPP_COLOR_ARROW_ON && f.led[JLX_TOP0 + 1].color == JLAPP_COLOR_ARROW_ON);   /* octave 3 of 0..5: up and down both go */
    CHECK(f.led[JLX_TOP0 + 2].color == JLAPP_COLOR_ARROW_OFF && f.led[JLX_TOP0 + 3].color == JLAPP_COLOR_ARROW_ON);   /* track 1 of 16: nothing to the left */
    CHECK(f.led[JLX_TOP0 + 5].color == JLAPP_COLOR_MODE);          /* the Note button */
    CHECK(JLAPP_COLOR_ARROW_ON == 21 && JLAPP_COLOR_ARROW_OFF == 23 && JLAPP_COLOR_MODE == 37);   /* GREEN, GREEN_THIRD, LIGHT_BLUE: LP95's Note.Octave and Mode.Note */
    CHECK(f.led[JLX_TOP0 + 4].color == JLAPP_COLOR_PALETTE_OFF);  /* Session is the colour chart: lit, dim grey out of it */
    CHECK(f.led[JLX_TOP0 + 6].color == JLAPP_GREEN_THIRD);        /* Custom does something: lit, dim (melodic step is the one it leads to) */
    CHECK(f.led[JLX_TOP0 + 7].color == JLMX_COLOR_BUTTON_DIM);   /* Capture is the mixer's: lit, dim (MINT_THIRD) */
    for (uint32_t i = 0; i < 8; i++)
        CHECK(f.led[JLX_SIDE0 + i].color == (i == 4 ? JLAPP_GREEN_THIRD : i == 5 ? JLAPP_PINK_THIRD : i == 7 ? JLAPP_RED_THIRD : 0));   /* start / stop (dim green while stopped, owner 2026-11-25), Delete (dim pink) and Record (dim red) are lit */
    CHECK(f.led[JLX_LOGO].color == 0);
    for (uint32_t i = 0; i < JLX_LEDS; i++)
        CHECK(f.led[i].mode == JLX_STATIC);
}

static void test_release_acts(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o[JLAPP_PADS + JLN_TRACKS];
    acts(&a, pad(0, 3, 100), &o[0]);                               /* a note pad */
    switch_track_pads(&a);
    acts(&a, pad(5, 1, 100), &o[0]);                               /* and a track pad */
    CHECK(jlapp_release_acts(&a, o, 1) == 1 && o[0].kind == JLAPP_ACT_NOTE_OFF);   /* room for one: the notes first */
    CHECK(jlapp_release_acts(&a, o, JLAPP_PADS + JLN_TRACKS) == 1 && o[0].kind == JLAPP_ACT_TRACK_OFF && o[0].track == 5);
    CHECK(jlapp_release_acts(&a, o, JLAPP_PADS + JLN_TRACKS) == 0);
    CHECK(a.track_held == 0);
}

/* ---- left / right: the track, and the bottom pads that show it ---- */

static void test_left_right_select_the_track_and_show_it(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    jlapp_tick(&a, 1000);
    CHECK(acts(&a, top(3, 127), &o) == 1 && o.kind == JLAPP_ACT_SELECT && o.track == 1);   /* right: track 2, the Digitone's active one too */
    CHECK(strcmp(msg(&a), "TRACK 2") == 0);
    CHECK(colour(&a, 1, 1) == JLAPP_COLOR_TRACK_CURRENT);          /* the 16 bottom pads show it: tracks 1..8 in the upper row, 9..16 in the lower */
    CHECK(colour(&a, 0, 0) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 7, 0) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 3, 1) == JLAPP_COLOR_TRACK_IDLE);
    jlapp_tick(&a, 1000 + JLAPP_SELECT_MS - 1u);
    CHECK(colour(&a, 1, 1) == JLAPP_COLOR_TRACK_CURRENT);
    jlapp_tick(&a, 1000 + JLAPP_SELECT_MS);                       /* the notes are back */
    CHECK(colour(&a, 0, 0) == JLN_COLOR_ROOT && colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
    jlapp_tick(&a, 3000);
    acts(&a, top(3, 0), &o);
    for (int i = 0; i < 8; i++) {
        acts(&a, top(3, 127), &o);                                /* up to track 10: row 2 */
        acts(&a, top(3, 0), &o);
    }
    CHECK(a.n.track == 9 && colour(&a, 1, 0) == JLAPP_COLOR_TRACK_CURRENT && colour(&a, 1, 1) == JLAPP_COLOR_TRACK_IDLE);
    CHECK(acts(&a, top(2, 127), &o) == 1 && o.track == 8);
}

static void test_a_limit_shows_the_track_but_selects_nothing(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    jlapp_tick(&a, 10);
    CHECK(acts(&a, top(2, 127), &o) == 0);                        /* left on track 1: nothing to select */
    CHECK(colour(&a, 0, 1) == JLAPP_COLOR_TRACK_CURRENT && strcmp(msg(&a), "TRACK 1") == 0);
    CHECK(acts(&a, top(2, 0), &o) == 0 && acts(&a, top(5, 127), &o) == 0);   /* a release, and the Note button, select nothing */
}

/* ---- the track pads ---- */

static void test_the_track_pads_switch(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(colour(&a, 7, 7) == JLAPP_GREEN_THIRD && a.track_pads == JLAPP_TRACKS_OFF);
    CHECK(acts(&a, pad(7, 7, 127), &o) == 0 && a.track_pads == JLAPP_TRACKS_TRIGGER && colour(&a, 7, 7) == JLAPP_COLOR_TRIG_IDLE);   /* owner 2026-11-25: OFF -> TRIGGER (yellow) */
    CHECK(strcmp(msg(&a), "TRACK PADS TRIGGER") == 0);
    acts(&a, pad(7, 7, 0), &o);
    CHECK(a.track_pads == JLAPP_TRACKS_TRIGGER);                   /* a release changes nothing */
    acts(&a, pad(7, 7, 127), &o);
    CHECK(a.track_pads == JLAPP_TRACKS_SELECT && colour(&a, 7, 7) == JLAPP_COLOR_TRACK_IDLE && strcmp(msg(&a), "TRACK PADS SELECT") == 0);   /* -> SELECT (white) */
    acts(&a, pad(7, 7, 0), &o);
    acts(&a, pad(7, 7, 127), &o);
    CHECK(a.track_pads == JLAPP_TRACKS_OFF && colour(&a, 7, 7) == JLAPP_GREEN_THIRD && strcmp(msg(&a), "TRACK PADS OFF") == 0);   /* -> OFF */
    CHECK(JLAPP_COLOR_TRIG_IDLE == 13 && JLAPP_COLOR_TRIG_CURRENT == 21 && JLAPP_COLOR_TRIG_PLAYING == 5 && JLAPP_COLOR_TRACK_IDLE == 3);   /* yellow as the drum quadrant; white */
}

/* owner 2026-11-25: SELECT mode: a track pad selects its track (nothing sounds), lit WHITE, the current one AMBER */
static void test_the_track_pads_select_mode(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    switch_track_pads(&a);
    switch_track_pads(&a);                                         /* TRIGGER, then SELECT */
    CHECK(a.track_pads == JLAPP_TRACKS_SELECT);
    CHECK(colour(&a, 0, 1) == JLAPP_COLOR_TRACK_CURRENT && colour(&a, 3, 1) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 3, 0) == JLAPP_COLOR_TRACK_IDLE);   /* track 1 current */
    CHECK(acts(&a, pad(3, 0, 100), &o) == 1 && o.kind == JLAPP_ACT_SELECT && o.track == 11 && a.n.track == 11);   /* bottom row, pad 4: track 12 */
    CHECK(colour(&a, 3, 0) == JLAPP_COLOR_TRACK_CURRENT && colour(&a, 0, 1) == JLAPP_COLOR_TRACK_IDLE);
    CHECK(acts(&a, pad(3, 0, 0), &o) == 0);                        /* the release: nothing (no note was started) */
    CHECK(acts(&a, pad(1, 1, 100), &o) == 1 && o.kind == JLAPP_ACT_SELECT && a.n.track == 1);   /* upper track row, pad 2: track 2 */
}

static void test_the_track_pads_play_each_tracks_own_note(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    switch_track_pads(&a);
    CHECK(acts(&a, pad(2, 1, 100), &o) == 1 && o.kind == JLAPP_ACT_TRACK_ON && o.track == 2);
    CHECK(acts(&a, pad(2, 1, 100), &o) == 0);                      /* a second press of the same pad: nothing */
    CHECK(acts(&a, pad(2, 1, 0), &o) == 1 && o.kind == JLAPP_ACT_TRACK_OFF && o.track == 2);
    CHECK(acts(&a, pad(2, 1, 0), &o) == 0);                        /* a stray release */
    CHECK(acts(&a, pad(3, 0, 90), &o) == 1 && o.kind == JLAPP_ACT_TRACK_ON && o.track == 11);   /* the lower row: tracks 9..16 */
    CHECK(colour(&a, 3, 0) == JLAPP_COLOR_TRACK_PLAYING);          /* held: RED */
    CHECK(acts(&a, pad(3, 0, 0), &o) == 1 && o.track == 11);
    CHECK(colour(&a, 3, 0) == JLAPP_COLOR_TRIG_IDLE && colour(&a, 0, 1) == JLAPP_COLOR_TRIG_CURRENT);   /* the current track is blue */
    CHECK(colour(&a, 5, 1) == JLAPP_COLOR_TRIG_IDLE);
}

static void test_the_layout_moves_up_two_rows(void)
{
    struct jlapp a = fresh(), b = fresh(), c = fresh();
    struct jlapp_act o, p;
    struct jln_pad old_pad, new_pad;
    switch_track_pads(&a);
    CHECK(acts(&b, pad(1, 3, 100), &p) == 1);                      /* where the notes were: pad (1,3) */
    CHECK(acts(&a, pad(1, 5, 100), &o) == 1 && o.kind == JLAPP_ACT_NOTE_ON && o.note == p.note);   /* now two rows up */
    CHECK(colour(&a, 1, 5) == JLN_COLOR_PLAYING);
    jln_pad(&c.n, c.scale, 4, 0, &old_pad);
    jln_pad(&a.n, a.scale, 4, 0, &new_pad);
    CHECK(old_pad.note == new_pad.note);
    acts(&a, pad(1, 5, 0), &o);
    CHECK(colour(&a, 0, 2) == JLN_COLOR_ROOT);                     /* the layout's bottom row is pad row 3 now */
}

/* ---- live control missing: the pads say so, and never fall back to MIDI ---- */

static void test_without_live_control_the_pads_are_silent_and_say_why(void)
{
    struct jlapp a = fresh_not_live(JDL_REMOTE_OFF);
    struct jlapp_act o;
    jlapp_tick(&a, 100);
    CHECK(acts(&a, pad(0, 0, 100), &o) == 0 && a.held[0].note < 0);
    CHECK(strcmp(msg(&a), "REMOTE CONTROL OFF") == 0);
    CHECK(acts(&a, pad(1, 0, 100), &o) == 0 && msg(&a)[0] == 0);   /* said once, not on every pad */
    jlapp_tick(&a, 100 + 2000u);
    acts(&a, pad(2, 0, 100), &o);
    CHECK(strcmp(msg(&a), "REMOTE CONTROL OFF") == 0);             /* and again after two seconds */
    CHECK(acts(&a, pad(0, 0, 0), &o) == 0);                        /* the release of a pad that never played */
    jlapp_set_live(&a, JDL_NO_LIVE, 1, 1);
    jlapp_tick(&a, 9000);
    acts(&a, pad(0, 0, 100), &o);
    CHECK(strcmp(msg(&a), "NO LIVE CONTROL") == 0);
    jlapp_set_live(&a, JDL_ASKING, 1, 1);
    jlapp_tick(&a, 12000);
    acts(&a, pad(0, 0, 100), &o);
    CHECK(strcmp(msg(&a), "DN2 NOT READY") == 0);
    jlapp_set_live(&a, JDL_LIVE, 0, 1);                            /* live, but the Digitone refused PLAY */
    jlapp_tick(&a, 15000);
    CHECK(acts(&a, pad(0, 0, 100), &o) == 0 && strcmp(msg(&a), "DN2 HAS NO PLAY") == 0);
}

static void test_the_track_pads_say_why_too(void)
{
    struct jlapp a = fresh_not_live(JDL_NO_LIVE);
    struct jlapp_act o;
    a.track_pads = 1;
    CHECK(acts(&a, pad(2, 0, 100), &o) == 0 && a.track_held == 0);
    CHECK(strcmp(msg(&a), "NO LIVE CONTROL") == 0);
    CHECK(acts(&a, pad(2, 0, 0), &o) == 0);
}

static void test_a_note_down_when_live_goes_still_ends(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, pad(1, 0, 100), &o);
    jlapp_set_live(&a, JDL_ASKING, 1, 1);                          /* the lease ran out with the pad down */
    CHECK(acts(&a, pad(1, 0, 0), &o) == 1 && o.kind == JLAPP_ACT_NOTE_OFF && o.note == 50);   /* the firmware drops it if it cannot be sent; the Digitone ends its notes */
}

/* ---- lights from the Digitone's push ---- */

static void test_pushed_notes_light_pads(void)
{
    struct jlapp a = fresh();
    const struct jdl_note n1[1] = {{0, 4, 50, 100, 8, 0}};
    const struct jdl_note n2[1] = {{1, 4, 50, 100, 8, 0}};
    jlapp_tick(&a, 100);
    jlapp_on_push(&a, n1, 1);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);                  /* D (50) on the current track 1 */
    jlapp_tick(&a, 100 + JLAPP_PUSH_LIGHT_MS - 1u);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
    jlapp_tick(&a, 100 + JLAPP_PUSH_LIGHT_MS);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
    jlapp_tick(&a, 1000);
    jlapp_on_push(&a, n2, 1);                                      /* track 2 plays: not the current track, its pads stay dark */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

static void test_pushed_notes_light_the_track_pads(void)
{
    struct jlapp a = fresh();
    const struct jdl_note n[2] = {{4, 0, 60, 100, 8, 0}, {12, 0, 36, 100, 8, 0}};
    switch_track_pads(&a);
    jlapp_tick(&a, 500);
    jlapp_on_push(&a, n, 2);
    CHECK(colour(&a, 4, 0) == JLAPP_COLOR_TRACK_PLAYING && colour(&a, 4, 1) == JLAPP_COLOR_TRACK_PLAYING);   /* RED: tracks 5 and 13 */
    CHECK(colour(&a, 5, 0) == JLAPP_COLOR_TRIG_IDLE);
    jlapp_tick(&a, 500 + JLAPP_PUSH_LIGHT_MS);
    CHECK(colour(&a, 4, 0) == JLAPP_COLOR_TRIG_IDLE && colour(&a, 4, 1) == JLAPP_COLOR_TRIG_IDLE);
}

static void test_the_overlay_over_the_track_pads_then_back(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    switch_track_pads(&a);
    jlapp_tick(&a, 2000);
    acts(&a, top(3, 127), &o);
    CHECK(colour(&a, 1, 1) == JLAPP_COLOR_TRIG_CURRENT && colour(&a, 0, 1) == JLAPP_COLOR_TRIG_IDLE);
    jlapp_tick(&a, 2000 + JLAPP_SELECT_MS);
    CHECK(colour(&a, 1, 1) == JLAPP_COLOR_TRIG_CURRENT);
}

static void test_a_push_with_junk_and_the_ring(void)
{
    struct jlapp a = fresh();
    const struct jdl_note junk[2] = {{16, 0, 50, 100, 8, 0}, {0, 0, 200, 100, 8, 0}};
    struct jdl_note one = {0, 0, 50, 100, 8, 0};
    jlapp_tick(&a, 10);
    jlapp_on_push(&a, junk, 2);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);                 /* a track past 15 and a note past 127 light nothing */
    for (unsigned i = 0; i < 3u * JLAPP_RECENT; i++) {             /* far more pushes than the ring holds: the newest is still there */
        one.note = (uint8_t)(i % 100u);
        jlapp_on_push(&a, &one, 1);
    }
    one.note = 50;
    jlapp_on_push(&a, &one, 1);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_PLAYING);
}

static void test_the_clock_wrapping(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    const struct jdl_note n[1] = {{0, 0, 50, 100, 8, 0}};
    jlapp_tick(&a, 0xFFFFFF00u);
    jlapp_on_push(&a, n, 1);
    acts(&a, top(3, 127), &o);
    jlapp_tick(&a, 0xFFFFFF00u + 50u);                             /* 50 ms later, past a wrap of the counter below it */
    CHECK(colour(&a, 1, 1) == JLAPP_COLOR_TRACK_CURRENT);           /* the overlay is still up */
    jlapp_tick(&a, 0xFFFFFF00u + JLAPP_SELECT_MS);                 /* 1 s later: past the wrap */
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
}

/* ---- the messages (what Launchpad 95 writes in Live's status bar) ---- */

static void test_the_scale_page_says_what_it_did(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    acts(&a, top(NOTE_BTN, 127), &o);
    tap(&a, 0, 7);                                                 /* absolute root */
    CHECK(strcmp(msg(&a), "ABSOLUTE ROOT ON") == 0);
    tap(&a, 0, 7);
    CHECK(strcmp(msg(&a), "ABSOLUTE ROOT OFF") == 0);
    tap(&a, 1, 7);                                                 /* horizontal (it starts on) */
    CHECK(strcmp(msg(&a), "HORIZONTAL OFF") == 0);
    tap(&a, 4, 7);
    CHECK(strcmp(msg(&a), "LAYOUT CHORDS") == 0);
    tap(&a, 1, 7);
    CHECK(strcmp(msg(&a), "DIATONIC ONLY") == 0);                  /* horizontal does nothing in this layout */
    tap(&a, 6, 7);
    CHECK(strcmp(msg(&a), "LAYOUT CHROMATIC") == 0);
    tap(&a, 5, 7);
    CHECK(strcmp(msg(&a), "LAYOUT DIATONIC") == 0);
    tap(&a, 7, 7);
    CHECK(strcmp(msg(&a), "TRACK PADS TRIGGER") == 0);
    tap(&a, 0, 6);                                                 /* a black key */
    CHECK(strcmp(msg(&a), "KEY C#") == 0);
    tap(&a, 2, 5);                                                 /* a white key */
    CHECK(strcmp(msg(&a), "KEY E") == 0);
    tap(&a, 7, 5);                                                 /* the circle of fifths forward: B */
    CHECK(strcmp(msg(&a), "KEY B") == 0);
    tap(&a, 6, 6);                                                 /* and back: E */
    CHECK(strcmp(msg(&a), "KEY E") == 0);
    tap(&a, 1, 3);                                                 /* the scales: the second of the first row, Ionian (Maj) */
    CHECK(says(&a, "SCALE ", jls_get(1)->name));
    tap(&a, 4, 2);                                                 /* the second row: 8 + 4 = 12 */
    CHECK(a.scale == jls_get(12) && says(&a, "SCALE ", jls_get(12)->name));
    tap(&a, 1, 4);                                                 /* the page pads say which scales they show */
    CHECK(strcmp(msg(&a), "SCALES 17-32") == 0);
    tap(&a, 2, 4);
    CHECK(strcmp(msg(&a), "SCALES 33-36") == 0);
    tap(&a, 0, 4);
    CHECK(strcmp(msg(&a), "SCALES 1-16") == 0);
    tap(&a, 6, 1);                                                 /* the track pads: a press selects, it says which */
    CHECK(strcmp(msg(&a), "TRACK 7") == 0);
}

static void test_the_relative_scale_says_key_and_scale(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    acts(&a, top(NOTE_BTN, 127), &o);
    tap(&a, 2, 6);                                                 /* C major -> A minor */
    CHECK(says(&a, "A ", jls_get(JLS_MINOR)->name));
    tap(&a, 2, 6);                                                 /* and back: C major */
    CHECK(says(&a, "C ", jls_get(JLS_MAJOR)->name));
    a.scale = jls_get(2);                                          /* a scale with no relative one */
    tap(&a, 2, 6);
    CHECK(strcmp(msg(&a), "NO RELATIVE SCALE") == 0);
}

static void test_messages_are_short_upper_case_and_the_newest_wins(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    char m[JLAPP_MSG_MAX];
    CHECK(jlapp_take_message(&a, m, sizeof m) == 0);               /* nothing said yet */
    acts(&a, top(NOTE_BTN, 127), &o);
    tap(&a, JLS_MINOR_PENTATONIC % 8, 3 - JLS_MINOR_PENTATONIC / 8);   /* a long scale name */
    CHECK(jlapp_take_message(&a, m, sizeof m) == strlen(m) && strlen(m) <= JLAPP_MSG_MAX - 1u && strncmp(m, "SCALE ", 6) == 0);
    for (size_t i = 0; m[i]; i++)
        CHECK(m[i] >= ' ' && m[i] <= '~' && !(m[i] >= 'a' && m[i] <= 'z'));
    CHECK(jlapp_take_message(&a, m, sizeof m) == 0);               /* taken: gone */
    tap(&a, 0, 7);
    tap(&a, 7, 7);                                                 /* two in a row: only the newest */
    CHECK(jlapp_take_message(&a, m, sizeof m) > 0 && strcmp(m, "TRACK PADS TRIGGER") == 0);
    tap(&a, 0, 7);
    CHECK(jlapp_take_message(&a, m, 5) == 4 && strcmp(m, "ABSO") == 0);   /* a small buffer: cut, still terminated */
    CHECK(jlapp_take_message(&a, m, 0) == 0);
}

/* ---- FOLLOW: left and right together ---- */

static void to_track(struct jlapp *a, unsigned t)
{
    struct jlapp_act o;
    while (a->n.track < t) {
        acts(a, top(3, 127), &o);
        acts(a, top(3, 0), &o);
    }
    say_nothing_yet(a);
}

static void test_left_and_right_together_switch_follow_and_make_no_step(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    to_track(&a, 3);
    jlapp_tick(&a, 5000);
    CHECK(acts(&a, top(2, 127), &o) == 1 && o.kind == JLAPP_ACT_SELECT && o.track == 2);   /* left: track 3 (index 2) */
    CHECK(acts(&a, top(3, 127), &o) == 1 && o.kind == JLAPP_ACT_SELECT && o.track == 3);   /* right with left held: the track is put back, the Digitone told */
    CHECK(a.n.track == 3 && a.follow && strcmp(msg(&a), "FOLLOW ON") == 0);
    acts(&a, top(2, 0), &o);
    acts(&a, top(3, 0), &o);
    CHECK(a.n.track == 3 && a.follow);                             /* the releases do nothing */
    CHECK(jlapp_following(&a));
    CHECK(acts(&a, top(3, 127), &o) == 1 && o.track == 4);         /* right alone steps again */
    CHECK(acts(&a, top(2, 127), &o) == 1 && o.kind == JLAPP_ACT_SELECT && o.track == 3 && a.n.track == 3 && !a.follow && strcmp(msg(&a), "FOLLOW OFF") == 0);   /* the other order: off, and right's step put back */
    CHECK(!jlapp_following(&a));
}

static void test_a_pair_at_the_first_track_changes_no_track(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    jlapp_tick(&a, 1);
    CHECK(acts(&a, top(2, 127), &o) == 0);                         /* left at the first track: nothing */
    CHECK(acts(&a, top(3, 127), &o) == 0 && a.n.track == 0 && a.follow);   /* the pair: still track 1, nothing to select */
}

static void test_follow_shows_in_the_arrows(void)
{
    struct jlapp a = fresh();
    struct jlx_frame f;
    struct jlapp_act o;
    acts(&a, top(2, 127), &o);
    acts(&a, top(3, 127), &o);
    acts(&a, top(2, 0), &o);
    acts(&a, top(3, 0), &o);
    CHECK(a.follow);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + 2].color == JLAPP_COLOR_FOLLOW_OFF && f.led[JLX_TOP0 + 3].color == JLAPP_COLOR_FOLLOW_ON);   /* amber; the first track has nothing to its left */
    CHECK(f.led[JLX_TOP0 + 0].color == JLAPP_COLOR_ARROW_ON && f.led[JLX_TOP0 + 1].color == JLAPP_COLOR_ARROW_ON);    /* up / down are as ever */
    CHECK(JLAPP_COLOR_FOLLOW_ON == 9 && JLAPP_COLOR_FOLLOW_OFF == 11);
}

static void test_follow_needs_a_digitone_that_can_be_asked(void)
{
    struct jlapp a = fresh_not_live(JDL_REMOTE_OFF);
    struct jlapp_act o;
    acts(&a, top(2, 127), &o);
    acts(&a, top(3, 127), &o);
    CHECK(!a.follow && strcmp(msg(&a), "DN2 NOT READY") == 0);
    acts(&a, top(2, 0), &o);
    acts(&a, top(3, 0), &o);
    jlapp_set_live(&a, JDL_LIVE, 1, 0);                            /* live, but a Digitone build that cannot say its track */
    acts(&a, top(2, 127), &o);
    acts(&a, top(3, 127), &o);
    CHECK(!a.follow && strcmp(msg(&a), "DN2 CANNOT FOLLOW") == 0);
    acts(&a, top(2, 0), &o);
    acts(&a, top(3, 0), &o);
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
    acts(&a, top(2, 127), &o);
    acts(&a, top(3, 127), &o);
    acts(&a, top(2, 0), &o);
    acts(&a, top(3, 0), &o);
    CHECK(a.follow);
    jlapp_set_live(&a, JDL_ASKING, 1, 1);                          /* live control lapsed: FOLLOW stays on but does not run, and can be turned off */
    CHECK(a.follow && !jlapp_following(&a));
    acts(&a, top(2, 127), &o);
    acts(&a, top(3, 127), &o);
    CHECK(!a.follow && strcmp(msg(&a), "FOLLOW OFF") == 0);
}

static void test_follow_takes_the_digitones_track(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    jlapp_on_active_track(&a, 5);
    CHECK(a.n.track == 0);                                         /* FOLLOW off: ignored */
    jlapp_tick(&a, 1000);
    acts(&a, top(2, 127), &o);
    acts(&a, top(3, 127), &o);
    acts(&a, top(2, 0), &o);
    acts(&a, top(3, 0), &o);
    say_nothing_yet(&a);
    jlapp_tick(&a, 1000 + JLAPP_FOLLOW_HOLD_MS - 1u);
    jlapp_on_active_track(&a, 5);
    CHECK(a.n.track == 0);                                         /* just after the Launchpad's own choice: the Digitone may not have taken it yet */
    jlapp_tick(&a, 1000 + JLAPP_FOLLOW_HOLD_MS);
    jlapp_on_active_track(&a, 5);
    CHECK(a.n.track == 5 && colour(&a, 5, 1) == JLAPP_COLOR_TRACK_CURRENT);   /* followed, and shown on the bottom pads */
    CHECK(msg(&a)[0] == 0);                                        /* no message: the pads say it */
    jlapp_on_active_track(&a, 16);
    CHECK(a.n.track == 5);                                         /* not a track */
    jlapp_on_active_track(&a, 5);
    CHECK(a.n.track == 5);
}

/* ---- the modes: Note, melodic step, drum step ---- */

#define CUSTOM 6u
#define SCENE5 4u

static struct jlx_ev side(uint8_t idx, uint8_t v)
{
    struct jlx_ev e = {JLX_SIDE, 0, 0, idx, v};
    return e;
}

static void read_pages(struct jlapp *a, uint8_t pages, int every4)
{
    struct jdl_page pg;
    memset(&pg, 0, sizeof pg);
    pg.track = a->n.track;
    pg.pages = pages;
    pg.length = (uint8_t)(pages * 16u);
    for (unsigned p = 0; p < 2; p++) {
        pg.page = (uint8_t)(a->step.first_page + p);
        for (unsigned i = 0; i < 16; i++) {
            pg.step[i].trig = every4 && i % 4 == 0;
            pg.step[i].note = (uint8_t)(40 + i);
        }
        jlapp_on_page(a, &pg);
    }
}

static void test_custom_enters_the_step_modes_and_cycles_them(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    CHECK(a.mode == JLAPP_MODE_NOTE);
    acts(&a, top(CUSTOM, 127), &o);
    CHECK(a.mode == JLAPP_MODE_STEP && a.step_kind == JLST_MELODIC && a.step.kind == JLST_MELODIC && strcmp(msg(&a), "MELODIC STEP") == 0);
    acts(&a, top(CUSTOM, 0), &o);
    CHECK(a.mode == JLAPP_MODE_STEP);                              /* a release does nothing */
    acts(&a, top(CUSTOM, 127), &o);
    CHECK(a.step_kind == JLST_DRUM && a.step.kind == JLST_DRUM && strcmp(msg(&a), "DRUM STEP") == 0);
    acts(&a, top(CUSTOM, 127), &o);
    CHECK(a.step_kind == JLST_MELODIC);                           /* the two cycle */
}

static void test_a_tap_on_note_goes_back_to_note_mode_a_hold_does_not(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, top(CUSTOM, 127), &o);
    jlapp_tick(&a, 1000);
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(a.page && a.mode == JLAPP_MODE_STEP);                    /* held: the scale page, in the step mode too */
    jlapp_tick(&a, 1000 + JLAPP_TAP_MS - 1u);
    acts(&a, top(NOTE_BTN, 0), &o);
    CHECK(!a.page && a.mode == JLAPP_MODE_NOTE && strcmp(msg(&a), "NOTE MODE") == 0);   /* a tap */
    acts(&a, top(CUSTOM, 127), &o);
    say_nothing_yet(&a);
    jlapp_tick(&a, 5000);
    acts(&a, top(NOTE_BTN, 127), &o);
    jlapp_tick(&a, 5000 + JLAPP_TAP_MS);
    acts(&a, top(NOTE_BTN, 0), &o);
    CHECK(a.mode == JLAPP_MODE_STEP);                              /* held long: it was the scale page */
    jlapp_tick(&a, 6000);
    acts(&a, top(NOTE_BTN, 127), &o);
    acts(&a, pad(0, 6, 100), &o);                                  /* a black key on the page */
    acts(&a, pad(0, 6, 0), &o);
    jlapp_tick(&a, 6100);
    acts(&a, top(NOTE_BTN, 0), &o);
    CHECK(a.mode == JLAPP_MODE_STEP && a.n.key == 1);              /* a page pad was used: not a tap, even if quick */
    jlapp_tick(&a, 7000);                                          /* a plain tap from a step mode: Note mode */
    acts(&a, top(NOTE_BTN, 127), &o);
    acts(&a, top(NOTE_BTN, 0), &o);
    CHECK(a.mode == JLAPP_MODE_NOTE);
    acts(&a, top(NOTE_BTN, 127), &o);                              /* and in Note mode a tap changes nothing */
    acts(&a, top(NOTE_BTN, 0), &o);
    CHECK(a.mode == JLAPP_MODE_NOTE);
}

static void test_up_and_down_are_the_octave_except_in_drum_step(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, top(CUSTOM, 127), &o);                                /* melodic step */
    acts(&a, top(0, 127), &o);
    CHECK(a.n.octave == 4);
    acts(&a, top(CUSTOM, 127), &o);                                /* drum step */
    acts(&a, top(0, 127), &o);
    acts(&a, top(1, 127), &o);
    CHECK(a.n.octave == 4);                                        /* nothing */
    {
        struct jlx_frame f;
        jlapp_frame(&a, &f);
        CHECK(f.led[JLX_TOP0 + 0].color == 0 && f.led[JLX_TOP0 + 1].color == 0);   /* and dark */
    }
}

/* owner 2026-11-24: "in drum step, i cannot change track when transport is stopped. it should select the track and play it". The press makes TWO acts (select, play): the
 * firmware's caller has to give room for JLAPP_ACTS_MAX; with room for one the library drops the whole press (it was called with 1 and nothing happened) */
static void test_a_drum_pad_on_another_track_selects_and_plays_while_stopped(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o[JLAPP_ACTS_MAX];
    struct jlx_ev down = pad(3, 0, 127), up = pad(3, 0, 0);          /* the quadrant's corner: track 16 */
    acts(&a, top(CUSTOM, 127), o);                                   /* melodic step */
    acts(&a, top(CUSTOM, 127), o);                                   /* drum step */
    CHECK(JLAPP_ACTS_MAX == 2 && a.mode == JLAPP_MODE_STEP && a.step_kind == JLST_DRUM && !a.transport && a.n.track == 0);
    CHECK(jlapp_on_lpx_acts(&a, &down, o, 1) == 0 && a.n.track == 0);   /* room for one act: nothing happens, as the owner saw */
    CHECK(jlapp_on_lpx_acts(&a, &down, o, JLAPP_ACTS_MAX) == 2);
    CHECK(o[0].kind == JLAPP_ACT_SELECT && o[0].track == 15 && o[1].kind == JLAPP_ACT_TRACK_ON && o[1].track == 15 && a.n.track == 15);
    CHECK(jlapp_on_lpx_acts(&a, &up, o, JLAPP_ACTS_MAX) == 1 && o[0].kind == JLAPP_ACT_TRACK_OFF && o[0].track == 15);
    CHECK(jlapp_on_lpx_acts(&a, &down, o, JLAPP_ACTS_MAX) == 1 && o[0].kind == JLAPP_ACT_TRACK_ON && o[0].track == 15);   /* the track it is on: only the sound */
    jlapp_on_lpx_acts(&a, &up, o, JLAPP_ACTS_MAX);
    jlapp_set_transport(&a, 1);                                      /* running: it only selects */
    down = pad(0, 3, 127);
    CHECK(jlapp_on_lpx_acts(&a, &down, o, JLAPP_ACTS_MAX) == 1 && o[0].kind == JLAPP_ACT_SELECT && o[0].track == 0 && a.n.track == 0);
}

static void test_the_start_stop_scene_button(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlx_frame f;
    CHECK(acts(&a, side(SCENE5, 127), &o) == 1 && o.kind == JLAPP_ACT_TRANSPORT && strcmp(msg(&a), "START") == 0);
    CHECK(acts(&a, side(SCENE5, 0), &o) == 0);                     /* a release */
    CHECK(acts(&a, side(1, 127), &o) == 0);                        /* the other scene buttons have no job yet */
    jlapp_set_transport(&a, 1);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + SCENE5].color == JLAPP_GREEN);         /* running: green */
    CHECK(acts(&a, side(SCENE5, 127), &o) == 1 && strcmp(msg(&a), "STOP") == 0);
    jlapp_set_transport(&a, 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + SCENE5].color == JLAPP_GREEN_THIRD);   /* stopped: green, third (owner 2026-11-25) */
}

/* owner 2026-11-24: Delete on scene 6, held: the Digitone erases the track's steps as its own playhead passes (LIVE ERASE); Note and step modes */
static void test_the_delete_scene_button_is_held(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlx_frame f;
    CHECK(JLAPP_SIDE_DELETE == 5);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_DELETE].color == JLAPP_PINK_THIRD && f.led[JLX_SIDE0 + 7].color == JLAPP_RED_THIRD);   /* dim; scene 8 is Record, dim red */
    acts(&a, top(3, 127), &o);                                     /* the track: the next one (index 1) */
    CHECK(a.n.track == 1);
    say_nothing_yet(&a);
    CHECK(acts(&a, side(JLAPP_SIDE_DELETE, 127), &o) == 1 && o.kind == JLAPP_ACT_ERASE && o.track == 1 && o.vel == 1 && strcmp(msg(&a), "DELETE TRACK 2") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_DELETE].color == JLAPP_PINK);                                          /* bright while held */
    CHECK(acts(&a, side(JLAPP_SIDE_DELETE, 127), &o) == 0);        /* a second press while held: nothing */
    acts(&a, top(3, 127), &o);                                     /* the track changes while it is held ... */
    CHECK(a.n.track == 2);
    CHECK(acts(&a, side(JLAPP_SIDE_DELETE, 0), &o) == 1 && o.kind == JLAPP_ACT_ERASE && o.track == 1 && o.vel == 0);   /* ... the release ends the track it erased */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_DELETE].color == JLAPP_PINK_THIRD);
    CHECK(acts(&a, side(JLAPP_SIDE_DELETE, 0), &o) == 0);          /* a release with nothing held */
}

static void test_delete_in_a_step_mode_and_in_the_mixer_and_without_live_control(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o[JLAPP_ACTS_MAX + 1u];
    struct jlx_ev ev = {JLX_SIDE, 0, 0, JLAPP_SIDE_DELETE, 127};
    acts(&a, top(6, 127), o);                                      /* a step mode */
    CHECK(jlapp_on_lpx_acts(&a, &ev, o, 1) == 1 && o[0].kind == JLAPP_ACT_ERASE && o[0].vel == 1);
    acts(&a, top(BUTTON_CAPTURE_IDX, 127), o);                     /* the mixer opens with Delete held */
    ev.value = 0;
    CHECK(jlapp_on_lpx_acts(&a, &ev, o, 1) == 1 && o[0].kind == JLAPP_ACT_ERASE && o[0].vel == 0);   /* its release still ends it */
    ev.value = 127;
    CHECK(jlapp_on_lpx_acts(&a, &ev, o, 1) == 0 && a.mix.view == JLMX_MUTE);   /* in the mixer scene 6 is a view, not Delete */
    {
        struct jlapp b = fresh_not_live(JDL_REMOTE_OFF);           /* no live control: it says why and erases nothing */
        say_nothing_yet(&b);
        CHECK(jlapp_on_lpx_acts(&b, &ev, o, 1) == 0 && !b.erase_on && msg(&b)[0] != 0);
    }
}

static void test_delete_ends_when_the_launchpad_goes_away(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o[JLAPP_PADS + JLN_TRACKS + 1u];
    struct jlx_ev ev = {JLX_SIDE, 0, 0, JLAPP_SIDE_DELETE, 127};
    CHECK(jlapp_on_lpx_acts(&a, &ev, o, 1) == 1 && a.erase_on);
    CHECK(jlapp_release_acts(&a, o, JLAPP_PADS + JLN_TRACKS + 1u) == 1 && o[0].kind == JLAPP_ACT_ERASE && o[0].vel == 0 && !a.erase_on);
    CHECK(jlapp_release_acts(&a, o, JLAPP_PADS + JLN_TRACKS + 1u) == 0);
}

/* owner 2026-11-24: Record on scene 8 (red): the Digitone's own live record (RECORD, 58); it arms, the Digitone records while its sequencer plays */
static void test_the_record_button(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlx_frame f;
    jlapp_tick(&a, 1000);
    CHECK(JLAPP_SIDE_RECORD == 7);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_RECORD].color == JLAPP_RED_THIRD);   /* dim red: off */
    say_nothing_yet(&a);
    CHECK(acts(&a, side(JLAPP_SIDE_RECORD, 127), &o) == 1 && o.kind == JLAPP_ACT_RECORD && o.vel == 1 && strcmp(msg(&a), "RECORD ON") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_RECORD].color == JLAPP_RED);          /* bright at once */
    CHECK(acts(&a, side(JLAPP_SIDE_RECORD, 0), &o) == 0);                    /* a release is nothing */
    jlapp_set_record(&a, 1, 1, 0, 0);                                        /* an older LIVE STATUS right after the press: not taken */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_RECORD].color == JLAPP_RED);
    jlapp_tick(&a, 1000u + JLAPP_RECORD_HOLD_MS);
    jlapp_set_record(&a, 1, 1, JDL_REC_ARMED, 0);                            /* the Digitone says armed: it stays on */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_RECORD].color == JLAPP_RED);
    jlapp_set_record(&a, 1, 1, JDL_REC_RECORDING | JDL_REC_ARMED, 0);        /* and recording */
    CHECK(a.record_on);
    jlapp_set_record(&a, 1, 1, 0, 0);                                        /* the Digitone's own REC key turned it off: the light follows */
    CHECK(!a.record_on);
    jlapp_set_record(&a, 1, 1, JDL_REC_ARMED, 0);                            /* ... or on */
    CHECK(a.record_on);
    CHECK(acts(&a, side(JLAPP_SIDE_RECORD, 127), &o) == 1 && o.vel == 0 && strcmp(msg(&a), "RECORD OFF") == 0);   /* a press while on: off */
    jlapp_tick(&a, 5000);
    jlapp_set_record(&a, 1, 0, 0, 0);                                        /* protocol 5: no record byte: the press stays */
    CHECK(!a.record_on);
    acts(&a, side(JLAPP_SIDE_RECORD, 127), &o);                              /* armed again by the press */
    jlapp_tick(&a, 9000);
    jlapp_set_record(&a, 1, 0, 0, 0);
    CHECK(a.record_on);
    say_nothing_yet(&a);
    jlapp_set_record(&a, 1, 0, 0, 1);                                        /* GRID or STEP RECORDING is on: refused */
    CHECK(!a.record_on && strcmp(msg(&a), "GRID/STEP REC IS ON") == 0);
    jlapp_set_record(&a, 1, 0, 0, 1);
    CHECK(msg(&a)[0] == 0);                                                  /* said once per refusal */
    jlapp_set_record(&a, 1, 0, 0, 0);                                        /* the counter restarted (live control came back): no message */
    CHECK(msg(&a)[0] == 0);
    jlapp_set_record(&a, 0, 0, 0, 0);                                        /* a Digitone without RECORD */
    CHECK(strcmp(msg(&a), "RECORD: DN2 TOO OLD") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_RECORD].color == 0);                  /* dark */
    CHECK(acts(&a, side(JLAPP_SIDE_RECORD, 127), &o) == 0 && strcmp(msg(&a), "RECORD: DN2 TOO OLD") == 0);
    {
        struct jlapp b = fresh_not_live(JDL_REMOTE_OFF);                     /* no live control: it says why, arms nothing */
        say_nothing_yet(&b);
        CHECK(acts(&b, side(JLAPP_SIDE_RECORD, 127), &o) == 0 && !b.record_on && msg(&b)[0] != 0);
    }
    {
        struct jlapp c = fresh();                                            /* the step modes: the same button */
        acts(&c, top(6, 127), &o);
        CHECK(acts(&c, side(JLAPP_SIDE_RECORD, 127), &o) == 1 && o.kind == JLAPP_ACT_RECORD && o.vel == 1);
        acts(&c, top(7, 127), &o);                                           /* the mixer: scene 8 has no job there */
        CHECK(acts(&c, side(JLAPP_SIDE_RECORD, 127), &o) == 0);
    }
}

/* Session held for JLAPP_SESSION_HOLD_MS (owner 2026-11-25: a short press is the Session page now): the chart opens while it is still down */
static void hold_session(struct jlapp *a)
{
    struct jlapp_act o;
    uint32_t t = a->now_ms;
    acts(a, top(BUTTON_SESSION_IDX, 127), &o);
    jlapp_tick(a, t + JLAPP_SESSION_HOLD_MS);
}

/* owner 2026-11-24: the Session button is a colour chart: the 128 palette indexes on the pads, two pages (owner 2026-11-25: held) */
static void test_the_session_button_is_a_colour_chart(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlx_frame f;
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + BUTTON_SESSION_IDX].color == JLAPP_COLOR_PALETTE_OFF);
    say_nothing_yet(&a);
    hold_session(&a);
    CHECK(a.mode == JLAPP_MODE_PALETTE && strcmp(msg(&a), "COLOURS 0-63") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + BUTTON_SESSION_IDX].color == JLAPP_COLOR_PALETTE_ON);
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 0].color == 0 && f.led[JLX_PAD0 + 7 * 8 + 1].color == 1 && f.led[JLX_PAD0 + 7 * 8 + 7].color == 7);   /* the top row: 0 .. 7, index 0 top left */
    CHECK(f.led[JLX_PAD0 + 6 * 8 + 3].color == 11 && f.led[JLX_PAD0 + 0].color == 56 && f.led[JLX_PAD0 + 7].color == 63);   /* the bottom row: 56 .. 63 */
    for (uint32_t i = 0; i < 64; i++)
        CHECK(f.led[JLX_PAD0 + i].color < 64);
    CHECK(acts(&a, pad(3, 6, 100), &o) == 0 && strcmp(msg(&a), "COLOUR 11") == 0);   /* a pad says its index, plays nothing */
    CHECK(acts(&a, pad(3, 6, 0), &o) == 0);
    acts(&a, top(BUTTON_SESSION_IDX, 0), &o);                                /* a release of Session: nothing */
    CHECK(a.mode == JLAPP_MODE_PALETTE && a.palette_page == 0);
    acts(&a, top(BUTTON_SESSION_IDX, 127), &o);                              /* the second page */
    CHECK(a.palette_page == 1 && strcmp(msg(&a), "COLOURS 64-127") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 0].color == 64 && f.led[JLX_PAD0 + 0].color == 120 && f.led[JLX_PAD0 + 7].color == 127);
    CHECK(acts(&a, pad(0, 0, 100), &o) == 0 && strcmp(msg(&a), "COLOUR 120") == 0);
    acts(&a, top(BUTTON_SESSION_IDX, 127), &o);                              /* a third press: back to where it came from */
    CHECK(a.mode == JLAPP_MODE_NOTE && strcmp(msg(&a), "COLOURS OFF") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_PAD0].color == JLN_COLOR_ROOT && f.led[JLX_TOP0 + BUTTON_SESSION_IDX].color == JLAPP_COLOR_PALETTE_OFF);   /* the note pads again */
    acts(&a, top(6, 127), &o);                                               /* from a step mode ... */
    hold_session(&a);
    acts(&a, top(BUTTON_SESSION_IDX, 0), &o);
    acts(&a, top(BUTTON_SESSION_IDX, 127), &o);
    acts(&a, top(BUTTON_SESSION_IDX, 127), &o);
    CHECK(a.mode == JLAPP_MODE_STEP);                                        /* ... back to it */
    hold_session(&a);
    acts(&a, top(NOTE_BTN, 127), &o);                                        /* a tap on Note leaves the chart for Note mode */
    jlapp_tick(&a, a.now_ms + 100u);
    acts(&a, top(NOTE_BTN, 0), &o);
    CHECK(a.mode == JLAPP_MODE_NOTE);
}

static void test_the_mode_buttons_light(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlx_frame f;
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + NOTE_BTN].color == JLAPP_COLOR_MODE && f.led[JLX_TOP0 + CUSTOM].color == JLAPP_GREEN_THIRD);   /* a button that does something is lit: dim when its mode is off */
    acts(&a, top(CUSTOM, 127), &o);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + NOTE_BTN].color == JLAPP_COLOR_MODE_DIM && f.led[JLX_TOP0 + CUSTOM].color == JLAPP_GREEN);   /* melodic: green */
    acts(&a, top(CUSTOM, 127), &o);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + CUSTOM].color == JLAPP_AMBER);          /* drum: amber */
    acts(&a, top(NOTE_BTN, 127), &o);                              /* a tap on Note: Note mode again; Custom stays lit, dim, in the colour of the step mode last used */
    acts(&a, top(NOTE_BTN, 0), &o);
    jlapp_frame(&a, &f);
    CHECK(a.mode == JLAPP_MODE_NOTE && f.led[JLX_TOP0 + NOTE_BTN].color == JLAPP_COLOR_MODE && f.led[JLX_TOP0 + CUSTOM].color == JLAPP_AMBER_THIRD);
}

/* the whole path in the app: Delete (scene 6) held in a step mode: a step pad press clears that step (a trig is cleared, an empty step stays empty) while the track wipe runs */
static void test_a_step_press_with_delete_held_in_the_app(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, top(CUSTOM, 127), &o);                                /* melodic step */
    read_pages(&a, 4, 1);                                          /* trigs on steps 0, 4, 8, 12 ... */
    CHECK(acts(&a, side(JLAPP_SIDE_DELETE, 127), &o) == 1 && o.kind == JLAPP_ACT_ERASE && o.vel == 1);
    CHECK(acts(&a, pad(0, 7, 100), &o) == 1 && o.kind == JLAPP_ACT_STEP && o.step == 0 && o.op == JDL_STEP_CLEAR);   /* step 1 had a trig: cleared */
    CHECK(acts(&a, pad(1, 7, 100), &o) == 0);                      /* step 2 is empty: nothing, and no trig is set */
    CHECK(colour(&a, 1, 7) == JLST_COLOR_OFF);
    acts(&a, side(JLAPP_SIDE_DELETE, 0), &o);                      /* let go: presses set steps again */
    CHECK(acts(&a, pad(1, 7, 100), &o) == 1 && o.kind == JLAPP_ACT_STEP && o.step == 1 && o.op != JDL_STEP_CLEAR);
    acts(&a, top(CUSTOM, 127), &o);                                /* the drum step mode: the same */
    acts(&a, side(JLAPP_SIDE_DELETE, 127), &o);
    CHECK(acts(&a, pad(0, 6, 100), &o) == 1 && o.kind == JLAPP_ACT_STEP && o.step == 8 && o.op == JDL_STEP_CLEAR);   /* (the second row of steps: step 9 .. 16, x = 0 is step 9: has a trig at 8) */
}

static void test_a_step_press_in_the_app(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    uint8_t pages[2];
    acts(&a, top(CUSTOM, 127), &o);
    CHECK(jlapp_step_wanted(&a, pages) == 1 && pages[0] == 0);     /* page 0 first: it says the length */
    CHECK(acts(&a, pad(1, 7, 100), &o) == 0);                      /* nothing read: nothing to toggle */
    read_pages(&a, 4, 1);
    CHECK(jlapp_step_wanted(&a, pages) == 2 && pages[0] == 0 && pages[1] == 1);
    CHECK(acts(&a, pad(1, 7, 100), &o) == 1 && o.kind == JLAPP_ACT_STEP && o.step == 1 && o.op == JDL_STEP_TRIG && o.track == 0);
    CHECK(colour(&a, 1, 7) == JLST_COLOR_ON && colour(&a, 0, 7) == JLST_COLOR_ON && colour(&a, 2, 7) == JLST_COLOR_OFF);
    acts(&a, top(CUSTOM, 127), &o);                                /* Custom again: the drum step mode */
    CHECK(colour(&a, 0, 3) == JLST_COLOR_TRACK_CURRENT);           /* the quadrant is the tracks: track 1 top left of it */
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(colour(&a, 7, 7) == JLAPP_GREEN_THIRD);                  /* held: the scale page covers the steps */
    jlapp_tick(&a, JLAPP_TAP_MS);
    acts(&a, top(NOTE_BTN, 0), &o);                                /* held long: still the step mode */
    CHECK(a.mode == JLAPP_MODE_STEP && jlapp_step_wanted(&a, pages) == 2);
    {
        struct jlapp b = fresh_not_live(JDL_ASKING);
        acts(&b, top(CUSTOM, 127), &o);
        CHECK(jlapp_step_wanted(&b, pages) == 0);                  /* no live control: nothing to read */
        CHECK(acts(&b, pad(1, 7, 100), &o) == 0 && strcmp(msg(&b), "DN2 NOT READY") == 0);   /* and why the step does nothing */
    }
    {
        struct jlapp c = fresh();
        acts(&c, top(5, 127), &o);
        CHECK(jlapp_step_wanted(&c, pages) == 0);                  /* Note mode: no reads */
    }
}

static void test_a_drum_track_pad_changes_the_current_track(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o[2];
    acts(&a, top(CUSTOM, 127), &o[0]);
    acts(&a, top(CUSTOM, 127), &o[0]);                             /* drum */
    say_nothing_yet(&a);
    CHECK(jlapp_on_lpx_acts(&a, &(struct jlx_ev){JLX_PAD, 0, 3, 0, 100}, o, 2) == 2);   /* pad row 0, column 3: track 16 */
    CHECK(a.n.track == 15 && o[0].kind == JLAPP_ACT_SELECT && o[1].kind == JLAPP_ACT_TRACK_ON && strcmp(msg(&a), "TRACK 16") == 0);
    CHECK(a.hold_on);                                              /* FOLLOW does not take it back at once */
}

static void test_a_note_held_in_a_step_mode_still_ends_after_the_mode_changes(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, top(CUSTOM, 127), &o);                                /* melodic: the keyboard plays while stopped */
    CHECK(acts(&a, pad(1, 0, 100), &o) == 1 && o.kind == JLAPP_ACT_NOTE_ON);
    jlapp_tick(&a, 100);
    acts(&a, top(NOTE_BTN, 127), &o);
    jlapp_tick(&a, 150);
    acts(&a, top(NOTE_BTN, 0), &o);                                /* a tap: Note mode */
    CHECK(a.mode == JLAPP_MODE_NOTE);
    CHECK(acts(&a, pad(1, 0, 0), &o) == 1 && o.kind == JLAPP_ACT_NOTE_OFF);   /* the release still reaches the step mode's note */
}

static void test_the_playhead_comes_from_the_push(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    const struct jdl_note n[2] = {{0, 4, 60, 100, 8, 0}, {3, 5, 60, 100, 8, 0}};
    acts(&a, top(CUSTOM, 127), &o);
    read_pages(&a, 4, 1);
    jlapp_tick(&a, 100);
    jlapp_on_push(&a, n, 2);
    CHECK(colour(&a, 4, 7) == JLST_COLOR_HEAD);                    /* step 4 of the current track */
    CHECK(colour(&a, 5, 7) == JLST_COLOR_OFF);                     /* another track's step 5 is not shown */
}

static void test_the_playhead_and_the_flashes_reach_the_step_pads(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    const struct jdl_note n[1] = {{0, 3, 50, 100, 8, 0}};
    acts(&a, top(CUSTOM, 127), &o);                                /* melodic step */
    read_pages(&a, 4, 0);
    jlapp_set_transport(&a, 1);
    jlapp_set_beat(&a, 1, 2);                                      /* the session's beat: sixteenth 2 */
    CHECK(colour(&a, 2, 7) == JLST_COLOR_PLAYHEAD);
    jlapp_set_beat(&a, 0, 0);                                      /* outside a session: no clock to go by */
    CHECK(colour(&a, 2, 7) == JLST_COLOR_OFF);
    jlapp_set_beat(&a, 1, 2);
    jlapp_set_transport(&a, 0);                                    /* stopped: no playhead either */
    CHECK(colour(&a, 2, 7) == JLST_COLOR_OFF);
    jlapp_tick(&a, 100);
    jlapp_on_push(&a, n, 1);                                       /* the current track plays D: the keyboard flashes it, RED */
    CHECK(colour(&a, 1, 0) == JLST_COLOR_TRACK_PLAYING);
    jlapp_tick(&a, 100 + JLST_HEAD_MS);
    CHECK(colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
    acts(&a, top(CUSTOM, 127), &o);                                /* drum: the track's own pad flashes */
    jlapp_tick(&a, 1000);
    jlapp_on_push(&a, n, 1);
    CHECK(colour(&a, 0, 3) == JLST_COLOR_TRACK_PLAYING);
    jlapp_tick(&a, 1000 + JLST_HEAD_MS);
    CHECK(colour(&a, 0, 3) == JLST_COLOR_TRACK_CURRENT);
}

static void test_release_acts_in_a_step_mode(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o[4];
    acts(&a, top(CUSTOM, 127), &o[0]);
    acts(&a, pad(1, 0, 100), &o[0]);
    CHECK(jlapp_release_acts(&a, o, 4) == 1 && o[0].kind == JLAPP_ACT_NOTE_OFF);
}

/* ---- the scale, synced with the Digitone ---- */

static void test_the_digitones_scale_is_taken_when_it_is_first_read_and_when_it_changes(void)
{
    struct jlapp a = fresh_list();
    CHECK(jls_index_of(a.scale) == 1 && a.n.key == 0);             /* Ionian (Maj), C, until the Digitone says */
    jlapp_on_dn2_scale(&a, 6, 1, 9);                               /* its first reading: Aeolian (Min), root A */
    CHECK(jls_index_of(a.scale) == 6 && a.n.key == 9 && a.scale_page == 0);
    jlapp_on_dn2_scale(&a, 30, 1, 2);                              /* it changes the scale and the root */
    CHECK(jls_index_of(a.scale) == 30 && a.n.key == 2 && a.scale_page == 1);   /* scale 30 is on the second page */
    jlapp_on_dn2_scale(&a, 3, 1, 2);                               /* only the scale changes */
    CHECK(jls_index_of(a.scale) == 3 && a.n.key == 2 && a.scale_page == 0);
    CHECK(strcmp(msg(&a), "") == 0);                               /* no message: the Digitone's screen says it */
    jlapp_on_dn2_scale(&a, 99, 1, 0);                              /* not a scale: ignored */
    CHECK(jls_index_of(a.scale) == 3);
}

static void test_an_unchanged_reading_leaves_the_launchpads_choice_alone(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    jlapp_on_dn2_scale(&a, 1, 1, 0);
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(acts(&a, pad(2, 3, 100), &o) == 1 && o.kind == JLAPP_ACT_SCALE && o.note == 2);   /* Dorian, chosen here */
    acts(&a, top(NOTE_BTN, 0), &o);
    jlapp_on_dn2_scale(&a, 1, 1, 0);                               /* the Digitone still says Ionian (an older build that cannot be set, or the write not yet applied) */
    jlapp_on_dn2_scale(&a, 1, 1, 0);
    CHECK(jls_index_of(a.scale) == 2);                             /* the Launchpad is not put back */
    jlapp_on_dn2_scale(&a, 2, 1, 0);                               /* the write was applied: the same as ours, nothing to do */
    CHECK(jls_index_of(a.scale) == 2);
    jlapp_on_dn2_scale(&a, 5, 1, 0);                               /* the Digitone changes it: that wins */
    CHECK(jls_index_of(a.scale) == 5);
}

static void test_the_first_reading_after_live_control_comes_back_is_taken_again(void)
{
    struct jlapp a = fresh_list();
    jlapp_on_dn2_scale(&a, 4, 1, 7);
    CHECK(jls_index_of(a.scale) == 4 && a.n.key == 7);
    a.scale = jls_get(9);                                          /* the Launchpad's own, meanwhile */
    a.n.key = 1;
    jlapp_on_dn2_scale(&a, 4, 1, 7);                               /* unchanged: stays */
    CHECK(jls_index_of(a.scale) == 9 && a.n.key == 1);
    jlapp_set_live(&a, JDL_ASKING, 1, 1);                          /* live control lapsed ... */
    jlapp_set_live(&a, JDL_LIVE, 1, 1);                            /* ... and came back */
    jlapp_on_dn2_scale(&a, 4, 1, 7);                               /* a first reading again: the Digitone's word */
    CHECK(jls_index_of(a.scale) == 4 && a.n.key == 7);
}

static void test_a_digitone_without_the_root_leaves_the_key(void)
{
    struct jlapp a = fresh_list();
    a.n.key = 5;
    jlapp_on_dn2_scale(&a, 6, 0, 0);                               /* protocol 4: a scale and no root */
    CHECK(jls_index_of(a.scale) == 6 && a.n.key == 5);
    jlapp_on_dn2_scale(&a, 6, 1, 3);                               /* a root appears (an upgraded Digitone): taken */
    CHECK(a.n.key == 3);
    jlapp_on_dn2_scale(&a, 6, 1, 3);
    a.n.key = 8;                                                   /* chosen here */
    jlapp_on_dn2_scale(&a, 6, 1, 3);                               /* unchanged root: stays */
    CHECK(a.n.key == 8);
    jlapp_on_dn2_scale(&a, 6, 1, 10);                              /* the Digitone's root changes */
    CHECK(a.n.key == 10);
    jlapp_on_dn2_scale(&a, 6, 1, 12);                              /* a root that does not exist: ignored */
    CHECK(a.n.key == 10);
}

static void test_scale_and_key_acts_carry_both(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(acts(&a, pad(2, 5, 100), &o) == 1 && o.kind == JLAPP_ACT_SCALE && o.note == 1 && o.vel == 4);   /* white key E: scale 1, root 4 */
    CHECK(acts(&a, pad(2, 5, 100), &o) == 0);                      /* the same key again: nothing changed, nothing sent */
    CHECK(acts(&a, pad(7, 5, 100), &o) == 1 && o.vel == 11);       /* the circle of fifths forward: E + 7 = B */
    CHECK(acts(&a, pad(6, 6, 100), &o) == 1 && o.vel == 4);        /* ... and back: E */
    CHECK(acts(&a, pad(0, 6, 100), &o) == 1 && o.vel == 1);        /* a black key: C# */
    CHECK(acts(&a, pad(2, 6, 100), &o) == 1 && o.note == 6 && o.vel == 10);   /* the relative scale: Ionian in C# -> Aeolian in A# (10) */
    CHECK(acts(&a, pad(3, 2, 100), &o) == 1 && o.note == 11 && o.vel == 10);   /* Harmonic Min (11 = 8 + 3, the second row): the key stays A# */
}

static void test_page_presses_that_change_nothing_send_nothing(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(acts(&a, pad(1, 3, 100), &o) == 0);                      /* the scale already in use (Ionian, on the first page) */
    CHECK(acts(&a, pad(1, 4, 100), &o) == 0 && a.scale_page == 1); /* a page pad only browses ... */
    CHECK(acts(&a, pad(0, 4, 100), &o) == 0 && a.scale_page == 0); /* ... and back */
    CHECK(acts(&a, pad(4, 4, 100), &o) == 0 && a.scale_page == 0); /* past the three page pads: nothing */
    CHECK(acts(&a, pad(5, 7, 100), &o) == 0);                      /* the layouts */
    CHECK(jlapp_on_lpx_acts(&a, &(struct jlx_ev){JLX_PAD, 3, 2, 0, 100}, &o, 0) == 0);   /* no room for the act: nothing is sent ... */
    CHECK(jls_index_of(a.scale) == 2);                             /* ... but the scale still changes on the Launchpad */
}

/* ---- the mixer screen in the app ---- */

#define CAPTURE 7u

static struct jdl_mixer app_mixer(void)
{
    struct jdl_mixer m;
    memset(&m, 0, sizeof m);
    for (unsigned i = 0; i < JDL_TRACKS; i++) {
        m.t[i].vol = (uint8_t)(i * 8u);
        m.t[i].pan = 64;
        m.t[i].del = 127;
    }
    m.mute = 0x0001u;
    return m;
}

static void test_capture_enters_the_mixer_and_the_buttons_follow(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    struct jlx_frame f;
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + CAPTURE].color == JLMX_COLOR_BUTTON_DIM);   /* lit, dim, before it is on */
    acts(&a, top(CAPTURE, 127), &o);
    CHECK(a.mode == JLAPP_MODE_MIXER && strcmp(msg(&a), "MIXER") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + CAPTURE].color == JLMX_COLOR_BUTTON && f.led[JLX_TOP0 + NOTE_BTN].color == JLAPP_COLOR_MODE_DIM);
    CHECK(f.led[JLX_SIDE0 + 0].color == JLMX_COLOR_SCENE_OFF && f.led[JLX_SIDE0 + 1].color == JLMX_COLOR_SCENE_OFF && f.led[JLX_SIDE0 + 2].color == JLMX_COLOR_SCENE_OFF);   /* the overview is on: no sub mode is the one shown */
    CHECK(f.led[JLX_SIDE0 + 5].color == JLMX_COLOR_MUTED && f.led[JLX_SIDE0 + 6].color == JLMX_COLOR_NOSOLO && f.led[JLX_SIDE0 + 7].color == 0);
    acts(&a, top(NOTE_BTN, 127), &o);                              /* a tap on Note: Note mode */
    jlapp_tick(&a, 100);
    acts(&a, top(NOTE_BTN, 0), &o);
    CHECK(a.mode == JLAPP_MODE_NOTE);
    acts(&a, top(CAPTURE, 127), &o);
    acts(&a, top(6, 127), &o);                                     /* Custom: the step modes */
    CHECK(a.mode == JLAPP_MODE_STEP);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + CAPTURE].color == JLMX_COLOR_BUTTON_DIM);   /* dim again */
}

static void test_scene_buttons_pick_the_view_and_say_it(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    static const char *const NAME[7] = {"VOLUME", "PAN", "DELAY SEND", "REVERB SEND", "CHORUS SEND", "MUTE AND SOLO", "MUTE AND SOLO"};   /* scene buttons 6 and 7: one screen */
    acts(&a, top(CAPTURE, 127), &o);
    say_nothing_yet(&a);
    for (uint8_t i = 0; i < 7; i++) {
        CHECK(acts(&a, side(i, 127), &o) == 0 && a.mix.view == i && strcmp(msg(&a), NAME[i]) == 0);
        CHECK(acts(&a, side(i, 0), &o) == 0 && msg(&a)[0] == 0);   /* a release: nothing */
    }
    CHECK(acts(&a, side(7, 127), &o) == 0 && a.mix.view == JLMX_SOLO && msg(&a)[0] == 0);   /* the eighth has no job */
    CHECK(acts(&a, side(SCENE5, 127), &o) == 0 && a.mix.view == JLMX_CHR);   /* scene 5 is chorus here, not start / stop */
}

/* owner 2026-11-24: the mixer opens on an overview (Launchpad 95's); the scene buttons switch to the sub modes; a press on a value row opens that sub mode; Capture again is the overview */
static void test_the_mixer_opens_on_the_overview(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    struct jdl_mixer m = app_mixer();
    struct jlx_frame f;
    static const char *const NAME[5] = {"VOLUME", "PAN", "DELAY SEND", "REVERB SEND", "CHORUS SEND"};
    jlapp_tick(&a, 1000);
    acts(&a, top(CAPTURE, 127), &o);
    jlapp_on_mixer(&a, &m);
    CHECK(a.mix.view == JLMX_OVERVIEW);
    jlapp_frame(&a, &f);
    for (uint8_t x = 0; x < 8; x++) {
        for (uint8_t y = 3; y < 8; y++)                            /* rows 1-5 from the top: volume .. chorus, each in its fader's colour */
            CHECK(f.led[JLX_PAD0 + y * 8 + x].color == (y == 7 ? JLMX_COLOR_VOL : y == 6 ? JLMX_COLOR_PAN : JLMX_COLOR_SEND));
        CHECK(f.led[JLX_PAD0 + 2 * 8 + x].color == (x == 0 ? JLMX_COLOR_MUTED : JLMX_COLOR_UNMUTED));   /* row 6: mute */
        CHECK(f.led[JLX_PAD0 + 1 * 8 + x].color == JLMX_COLOR_NOSOLO);                                  /* row 7: solo */
        CHECK(f.led[JLX_PAD0 + 0 * 8 + x].color == 0);                                                  /* row 8: nothing */
    }
    for (uint8_t i = 0; i < 5; i++) {                              /* a press on a value row opens its sub mode: no write */
        acts(&a, top(CAPTURE, 127), &o);
        CHECK(a.mix.view == JLMX_OVERVIEW);
        say_nothing_yet(&a);
        CHECK(acts(&a, pad(2, (uint8_t)(7 - i), 100), &o) == 0 && a.mix.view == i && strcmp(msg(&a), NAME[i]) == 0);
        CHECK(acts(&a, pad(2, (uint8_t)(7 - i), 0), &o) == 0);
    }
    CHECK(acts(&a, pad(2, 7, 100), &o) == 0 && a.mix.view == JLMX_CHR);   /* in a sub mode a press is a fader press, not another opening: held ... */
    CHECK(acts(&a, pad(2, 7, 0), &o) == 1 && o.kind == JLAPP_ACT_MIX && a.mix.view == JLMX_CHR);   /* ... and its release writes */
    acts(&a, top(CAPTURE, 127), &o);                               /* Capture again: back on the overview */
    CHECK(a.mix.view == JLMX_OVERVIEW && a.mode == JLAPP_MODE_MIXER);
    acts(&a, side(1, 127), &o);                                    /* a scene button switches from the overview */
    CHECK(a.mix.view == JLMX_PAN);
}

static void test_the_overviews_mute_and_solo_rows_toggle_tracks_and_row_8_does_nothing(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    struct jdl_mixer m = app_mixer();
    jlapp_tick(&a, 1000);
    acts(&a, top(CAPTURE, 127), &o);
    jlapp_on_mixer(&a, &m);
    CHECK(acts(&a, pad(2, 2, 100), &o) == 1 && o.kind == JLAPP_ACT_MUTE && o.op == 0 && o.note == 0x05 && a.mix.view == JLMX_OVERVIEW);   /* row 6: track 3 muted too */
    CHECK(acts(&a, pad(0, 2, 100), &o) == 1 && o.kind == JLAPP_ACT_MUTE && o.op == 0 && o.note == 0x04);                                  /* and track 1 unmuted */
    CHECK(acts(&a, pad(1, 1, 100), &o) == 1 && o.kind == JLAPP_ACT_MUTE && o.op == 1 && o.note == 0x02);                                  /* row 7: track 2 soloed */
    CHECK(acts(&a, pad(1, 0, 100), &o) == 0 && a.mix.view == JLMX_OVERVIEW);                                                              /* row 8: nothing */
    jlapp_set_mixer_writes(&a, 0);
    say_nothing_yet(&a);
    CHECK(acts(&a, pad(4, 7, 100), &o) == 0 && a.mix.view == JLMX_VOL);   /* opening a sub mode needs no write: it works on a read-only build */
    CHECK(strcmp(msg(&a), "VOLUME") == 0);
}

static void test_up_and_down_are_the_mixers_banks(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    struct jlx_frame f;
    acts(&a, top(CAPTURE, 127), &o);
    say_nothing_yet(&a);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + 0].color == JLAPP_COLOR_ARROW_OFF && f.led[JLX_TOP0 + 1].color == JLAPP_COLOR_ARROW_ON);   /* bank 1-8: nothing further up */
    CHECK(acts(&a, top(1, 127), &o) == 0 && a.mix.bank == 1 && strcmp(msg(&a), "TRACKS 9-16") == 0);   /* down: tracks 9-16, as the Digitone's lower row */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + 0].color == JLAPP_COLOR_ARROW_ON && f.led[JLX_TOP0 + 1].color == JLAPP_COLOR_ARROW_OFF);
    CHECK(acts(&a, top(1, 127), &o) == 0 && a.mix.bank == 1 && msg(&a)[0] == 0);   /* nothing below: no message */
    CHECK(acts(&a, top(0, 127), &o) == 0 && a.mix.bank == 0 && strcmp(msg(&a), "TRACKS 1-8") == 0);
    CHECK(a.n.octave == 3);                                        /* and the octave was not touched */
}

/* owner 2026-11-25: Launchpad 95's slow faders through the app: the release writes the first step, jlapp_pump_acts the rest; live control lost ends the slides */
static void test_a_held_fader_pad_slides_slowly(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o, out[JLMX_SLIDES];
    struct jdl_mixer m = app_mixer();
    struct jlx_frame f;
    jlapp_tick(&a, 1000);
    acts(&a, top(CAPTURE, 127), &o);
    jlapp_on_mixer(&a, &m);
    acts(&a, side(0, 127), &o);                                    /* volume */
    CHECK(acts(&a, pad(0, 7, 100), &o) == 0);                      /* track 1 held at its top pad */
    jlapp_tick(&a, 1400);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 0].color == 13);               /* 0.4 s held: the third hold colour (YELLOW) */
    jlapp_tick(&a, 2000);
    CHECK(acts(&a, pad(0, 7, 0), &o) == 1 && o.kind == JLAPP_ACT_MIX && o.track == 0 && o.op == JDL_MIX_VOL && o.vel == 1);   /* 1 s held: a 5 s sweep */
    jlapp_tick(&a, 2050);
    CHECK(jlapp_pump_acts(&a, out, JLMX_SLIDES) == 1 && out[0].kind == JLAPP_ACT_MIX && out[0].track == 0 && out[0].vel == 2);
    jlapp_tick(&a, 2100);
    CHECK(jlapp_pump_acts(&a, out, JLMX_SLIDES) == 0);            /* every 100 ms */
    acts(&a, top(NOTE_BTN, 127), &o);                              /* Note mode: the slide goes on */
    jlapp_tick(&a, 4450);
    CHECK(jlapp_pump_acts(&a, out, JLMX_SLIDES) == 1 && out[0].vel == 63);
    jlapp_set_live(&a, JDL_NO_LIVE, 1, 1);                         /* live control gone: the slide ends (it would jump later) */
    jlapp_tick(&a, 4550);
    CHECK(jlapp_pump_acts(&a, out, JLMX_SLIDES) == 0);
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
    jlapp_tick(&a, 4650);
    CHECK(jlapp_pump_acts(&a, out, JLMX_SLIDES) == 0);            /* and does not come back */
}

static void test_the_faders_follow_the_digitone_and_a_tap_writes(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    struct jdl_mixer m = app_mixer();
    struct jlx_frame f;
    jlapp_tick(&a, 1000);
    acts(&a, top(CAPTURE, 127), &o);
    jlapp_on_mixer(&a, &m);
    CHECK(a.mix.view == JLMX_OVERVIEW && jlapp_mixer_wanted(&a));
    acts(&a, side(0, 127), &o);                                    /* the volume sub mode */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_PAD0 + 0 * 8 + 3].color == JLMX_COLOR_VOL && f.led[JLX_PAD0 + 2 * 8 + 3].color == JLMX_COLOR_VOL && f.led[JLX_PAD0 + 3 * 8 + 3].color == 0);   /* track 4: volume 24: up to Launchpad 95's step 39: 3 pads */
    CHECK(acts(&a, pad(3, 5, 100), &o) == 0);                      /* a fader pad held: nothing yet (Launchpad 95's slow faders) */
    CHECK(acts(&a, pad(3, 5, 0), &o) == 1 && o.kind == JLAPP_ACT_MIX && o.track == 3 && o.op == JDL_MIX_VOL && o.vel == 89);   /* let go at once: pad 6 of 8, its step (-6 dB: 0.70 of the range) */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_PAD0 + 5 * 8 + 3].color == JLMX_COLOR_VOL && f.led[JLX_PAD0 + 6 * 8 + 3].color == 0);   /* the fader moved at once */
    acts(&a, side(1, 127), &o);                                    /* pan */
    acts(&a, pad(0, 7, 100), &o);
    CHECK(acts(&a, pad(0, 7, 0), &o) == 1 && o.kind == JLAPP_ACT_MIX && o.op == JDL_MIX_PAN && o.vel == 127 && o.track == 0);
    acts(&a, side(5, 127), &o);                                    /* the mute / solo screen */
    CHECK(acts(&a, pad(2, 3, 100), &o) == 1 && o.kind == JLAPP_ACT_MUTE && o.op == 0 && o.note == 0x05 && o.step == 0);   /* mute 1-8: track 3 muted too: tracks 1 and 3 */
    acts(&a, side(6, 127), &o);                                    /* the other scene button: the same screen */
    CHECK(acts(&a, pad(7, 0, 100), &o) == 1 && o.kind == JLAPP_ACT_MUTE && o.op == 1 && o.note == 0x00 && o.step == 0x80);   /* solo 9-16: track 16 soloed: 0x8000 */
    CHECK(acts(&a, pad(0, 4, 100), &o) == 0);                      /* the top four rows have no job */
}

static void test_a_mixer_without_writes_or_without_live_control_says_so(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    struct jdl_mixer m = app_mixer();
    jlapp_tick(&a, 1000);
    acts(&a, top(CAPTURE, 127), &o);
    jlapp_on_mixer(&a, &m);
    acts(&a, side(0, 127), &o);                                    /* a fader screen: its pads write */
    say_nothing_yet(&a);
    jlapp_set_mixer_writes(&a, 0);                               /* a Digitone without the mixer writes (stage 5) */
    CHECK(acts(&a, pad(3, 5, 100), &o) == 0 && strcmp(msg(&a), "MIXER READ ONLY") == 0);
    CHECK(acts(&a, pad(3, 6, 100), &o) == 0 && msg(&a)[0] == 0);   /* said once, not on every press */
    jlapp_tick(&a, 1000 + 2000u);
    CHECK(acts(&a, pad(3, 6, 100), &o) == 0 && strcmp(msg(&a), "MIXER READ ONLY") == 0);
    jlapp_set_mixer_writes(&a, 1);
    jlapp_set_live(&a, JDL_REMOTE_OFF, 1, 1);                      /* live control gone: the reason, and no read is wanted */
    jlapp_tick(&a, 9000);
    CHECK(acts(&a, pad(3, 5, 100), &o) == 0 && strcmp(msg(&a), "REMOTE CONTROL OFF") == 0);
    CHECK(!jlapp_mixer_wanted(&a));
}

static void test_the_mixer_is_not_wanted_in_the_other_modes(void)
{
    struct jlapp a = fresh_list();
    CHECK(!jlapp_mixer_wanted(&a));                                /* Note mode */
}

static void test_a_note_held_when_the_mixer_opens_still_ends(void)
{
    struct jlapp a = fresh_list();
    struct jlapp_act o;
    acts(&a, top(6, 127), &o);                                     /* melodic step: the keyboard plays while stopped */
    CHECK(acts(&a, pad(1, 0, 100), &o) == 1 && o.kind == JLAPP_ACT_NOTE_ON);
    acts(&a, top(CAPTURE, 127), &o);                               /* the mixer opens with the pad down */
    CHECK(acts(&a, pad(1, 0, 0), &o) == 1 && o.kind == JLAPP_ACT_NOTE_OFF);   /* its release still ends the note */
}

int main(void)
{
    test_capture_enters_the_mixer_and_the_buttons_follow();
    test_scene_buttons_pick_the_view_and_say_it();
    test_the_mixer_opens_on_the_overview();
    test_the_overviews_mute_and_solo_rows_toggle_tracks_and_row_8_does_nothing();
    test_up_and_down_are_the_mixers_banks();
    test_the_faders_follow_the_digitone_and_a_tap_writes();
    test_a_held_fader_pad_slides_slowly();
    test_a_mixer_without_writes_or_without_live_control_says_so();
    test_the_mixer_is_not_wanted_in_the_other_modes();
    test_a_note_held_when_the_mixer_opens_still_ends();
    test_the_digitones_scale_is_taken_when_it_is_first_read_and_when_it_changes();
    test_an_unchanged_reading_leaves_the_launchpads_choice_alone();
    test_the_first_reading_after_live_control_comes_back_is_taken_again();
    test_a_digitone_without_the_root_leaves_the_key();
    test_scale_and_key_acts_carry_both();
    test_page_presses_that_change_nothing_send_nothing();
    test_custom_enters_the_step_modes_and_cycles_them();
    test_a_tap_on_note_goes_back_to_note_mode_a_hold_does_not();
    test_up_and_down_are_the_octave_except_in_drum_step();
    test_a_drum_pad_on_another_track_selects_and_plays_while_stopped();
    test_the_start_stop_scene_button();
    test_the_record_button();
    test_the_session_button_is_a_colour_chart();
    test_the_delete_scene_button_is_held();
    test_delete_in_a_step_mode_and_in_the_mixer_and_without_live_control();
    test_delete_ends_when_the_launchpad_goes_away();
    test_the_mode_buttons_light();
    test_a_step_press_in_the_app();
    test_a_step_press_with_delete_held_in_the_app();
    test_a_drum_track_pad_changes_the_current_track();
    test_a_note_held_in_a_step_mode_still_ends_after_the_mode_changes();
    test_the_playhead_comes_from_the_push();
    test_the_playhead_and_the_flashes_reach_the_step_pads();
    test_release_acts_in_a_step_mode();
    test_roles_by_product_name();
    test_midi_interface_choice();
    test_the_note_button_opens_the_page_while_held();
    test_page_pads_edit_and_do_not_play();
    test_page_picks_the_scale();
    test_the_scale_page_opens_on_the_page_of_the_scale_in_use();
    test_page_picks_the_layout();
    test_page_bottom_rows_select_the_track();
    test_page_circle_of_fifths();
    test_page_relative_scale();
    test_page_lights();
    test_arrows_dim_at_their_limits();
    test_page_lights_with_a_scale_that_is_not_in_the_list();
    test_a_note_down_before_the_page_still_ends();
    test_page_buttons_still_work_and_note_stays_lit();
    test_a_pad_plays_a_note_on_the_current_track();
    test_release_goes_to_the_note_and_the_track_it_started();
    test_same_pitch_on_two_pads();
    test_pad_outside_midi_range_is_silent();
    test_repeated_press_and_stray_release();
    test_octave_and_other_buttons();
    test_own_press_lights_the_pad();
    test_buttons_lit();
    test_release_acts();
    test_left_right_select_the_track_and_show_it();
    test_a_limit_shows_the_track_but_selects_nothing();
    test_the_track_pads_switch();
    test_the_track_pads_play_each_tracks_own_note();
    test_the_track_pads_select_mode();
    test_the_layout_moves_up_two_rows();
    test_without_live_control_the_pads_are_silent_and_say_why();
    test_the_track_pads_say_why_too();
    test_a_note_down_when_live_goes_still_ends();
    test_pushed_notes_light_pads();
    test_pushed_notes_light_the_track_pads();
    test_the_overlay_over_the_track_pads_then_back();
    test_a_push_with_junk_and_the_ring();
    test_the_clock_wrapping();
    test_the_scale_page_says_what_it_did();
    test_the_relative_scale_says_key_and_scale();
    test_messages_are_short_upper_case_and_the_newest_wins();
    test_left_and_right_together_switch_follow_and_make_no_step();
    test_a_pair_at_the_first_track_changes_no_track();
    test_follow_shows_in_the_arrows();
    test_follow_needs_a_digitone_that_can_be_asked();
    test_follow_takes_the_digitones_track();
    printf("test_lpapp: %d checks, %d failed\n", checks, fails);
    return fails;
}
