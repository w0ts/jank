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
    CHECK(colour(&a, 3, 4) == JLAPP_RED);                    /* Octave 3 on */
    CHECK(colour(&a, 0, 4) == JLAPP_RED_THIRD && colour(&a, 5, 4) == JLAPP_RED_THIRD);
    CHECK(colour(&a, 6, 4) == 0 && colour(&a, 7, 4) == 0);   /* diatonic stops at octave 5 */
    CHECK(colour(&a, 0, 3) == JLAPP_BLUE);                   /* Modus: Major on */
    CHECK(colour(&a, 1, 3) == JLAPP_BLUE_THIRD && colour(&a, 7, 0) == JLAPP_BLUE_THIRD);
    tap(&a, 6, 7);                                           /* chromatic: its extra octave lights; Horizontal shows its state in every layout, as in LP95 */
    CHECK(colour(&a, 6, 4) == JLAPP_RED_THIRD && colour(&a, 1, 7) == JLAPP_GREEN && colour(&a, 6, 7) == JLAPP_RED && colour(&a, 5, 7) == JLAPP_RED_THIRD);
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
    struct jlx_ev side = {JLX_SIDE, 0, 0, 4, 127};
    CHECK(acts(&a, top(0, 127), &o) == 0 && a.n.octave == 4);     /* up: octave up, nothing for the Digitone */
    CHECK(strcmp(msg(&a), "OCTAVE 4") == 0);
    acts(&a, top(1, 127), &o);
    acts(&a, top(1, 127), &o);
    CHECK(a.n.octave == 2 && strcmp(msg(&a), "OCTAVE 2") == 0);
    acts(&a, top(0, 0), &o);                                      /* a release does nothing */
    acts(&a, top(1, 0), &o);
    CHECK(a.n.octave == 2 && msg(&a)[0] == 0);
    acts(&a, top(5, 127), &o);                                    /* the other buttons do nothing yet */
    acts(&a, top(7, 127), &o);
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
    CHECK(f.led[JLX_TOP0 + 4].color == 0 && f.led[JLX_TOP0 + 6].color == 0 && f.led[JLX_TOP0 + 7].color == 0);
    for (uint32_t i = 0; i < 8; i++)
        CHECK(f.led[JLX_SIDE0 + i].color == 0);
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
    acts(&a, pad(5, 0, 100), &o[0]);                               /* and a track pad */
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
    CHECK(colour(&a, 1, 0) == JLAPP_COLOR_TRACK_CHOSEN);          /* the 16 bottom pads show it: row 1 tracks 1..8 */
    CHECK(colour(&a, 0, 0) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 7, 0) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 3, 1) == JLAPP_COLOR_TRACK_IDLE);
    jlapp_tick(&a, 1000 + JLAPP_SELECT_MS - 1u);
    CHECK(colour(&a, 1, 0) == JLAPP_COLOR_TRACK_CHOSEN);
    jlapp_tick(&a, 1000 + JLAPP_SELECT_MS);                       /* the notes are back */
    CHECK(colour(&a, 0, 0) == JLN_COLOR_ROOT && colour(&a, 1, 0) == JLN_COLOR_IN_SCALE);
    jlapp_tick(&a, 3000);
    acts(&a, top(3, 0), &o);
    for (int i = 0; i < 8; i++) {
        acts(&a, top(3, 127), &o);                                /* up to track 10: row 2 */
        acts(&a, top(3, 0), &o);
    }
    CHECK(a.n.track == 9 && colour(&a, 1, 1) == JLAPP_COLOR_TRACK_CHOSEN && colour(&a, 1, 0) == JLAPP_COLOR_TRACK_IDLE);
    CHECK(acts(&a, top(2, 127), &o) == 1 && o.track == 8);
}

static void test_a_limit_shows_the_track_but_selects_nothing(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    jlapp_tick(&a, 10);
    CHECK(acts(&a, top(2, 127), &o) == 0);                        /* left on track 1: nothing to select */
    CHECK(colour(&a, 0, 0) == JLAPP_COLOR_TRACK_CHOSEN && strcmp(msg(&a), "TRACK 1") == 0);
    CHECK(acts(&a, top(2, 0), &o) == 0 && acts(&a, top(5, 127), &o) == 0);   /* a release, and the Note button, select nothing */
}

/* ---- the track pads ---- */

static void test_the_track_pads_switch(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    acts(&a, top(NOTE_BTN, 127), &o);
    CHECK(colour(&a, 7, 7) == JLAPP_GREEN_THIRD && !a.track_pads);
    CHECK(acts(&a, pad(7, 7, 127), &o) == 0 && a.track_pads && colour(&a, 7, 7) == JLAPP_GREEN);
    CHECK(strcmp(msg(&a), "TRACK PADS ON") == 0);
    acts(&a, pad(7, 7, 0), &o);
    CHECK(a.track_pads);                                           /* a release changes nothing */
    acts(&a, pad(7, 7, 127), &o);
    CHECK(!a.track_pads && colour(&a, 7, 7) == JLAPP_GREEN_THIRD && strcmp(msg(&a), "TRACK PADS OFF") == 0);
}

static void test_the_track_pads_play_each_tracks_own_note(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    switch_track_pads(&a);
    CHECK(acts(&a, pad(2, 0, 100), &o) == 1 && o.kind == JLAPP_ACT_TRACK_ON && o.track == 2);
    CHECK(acts(&a, pad(2, 0, 100), &o) == 0);                      /* a second press of the same pad: nothing */
    CHECK(acts(&a, pad(2, 0, 0), &o) == 1 && o.kind == JLAPP_ACT_TRACK_OFF && o.track == 2);
    CHECK(acts(&a, pad(2, 0, 0), &o) == 0);                        /* a stray release */
    CHECK(acts(&a, pad(3, 1, 90), &o) == 1 && o.kind == JLAPP_ACT_TRACK_ON && o.track == 11);   /* row 2: tracks 9..16 */
    CHECK(colour(&a, 3, 1) == JLN_COLOR_PLAYING);                  /* held: lit */
    CHECK(acts(&a, pad(3, 1, 0), &o) == 1 && o.track == 11);
    CHECK(colour(&a, 3, 1) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 0, 0) == JLAPP_COLOR_TRACK_CURRENT);   /* the current track is blue */
    CHECK(colour(&a, 5, 1) == JLAPP_COLOR_TRACK_IDLE);
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
    CHECK(colour(&a, 4, 0) == JLN_COLOR_PLAYING && colour(&a, 4, 1) == JLN_COLOR_PLAYING);   /* track 5 is pad (4,0), track 13 is (4,1) */
    CHECK(colour(&a, 5, 0) == JLAPP_COLOR_TRACK_IDLE);
    jlapp_tick(&a, 500 + JLAPP_PUSH_LIGHT_MS);
    CHECK(colour(&a, 4, 0) == JLAPP_COLOR_TRACK_IDLE && colour(&a, 4, 1) == JLAPP_COLOR_TRACK_IDLE);
}

static void test_the_overlay_over_the_track_pads_then_back(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    switch_track_pads(&a);
    jlapp_tick(&a, 2000);
    acts(&a, top(3, 127), &o);
    CHECK(colour(&a, 1, 0) == JLAPP_COLOR_TRACK_CHOSEN && colour(&a, 0, 0) == JLAPP_COLOR_TRACK_IDLE);
    jlapp_tick(&a, 2000 + JLAPP_SELECT_MS);
    CHECK(colour(&a, 1, 0) == JLAPP_COLOR_TRACK_CURRENT);
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
    CHECK(colour(&a, 1, 0) == JLAPP_COLOR_TRACK_CHOSEN);           /* the overlay is still up */
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
    CHECK(strcmp(msg(&a), "TRACK PADS ON") == 0);
    tap(&a, 0, 6);                                                 /* a black key */
    CHECK(strcmp(msg(&a), "KEY C#") == 0);
    tap(&a, 2, 5);                                                 /* a white key */
    CHECK(strcmp(msg(&a), "KEY E") == 0);
    tap(&a, 7, 5);                                                 /* the circle of fifths forward: B */
    CHECK(strcmp(msg(&a), "KEY B") == 0);
    tap(&a, 6, 6);                                                 /* and back: E */
    CHECK(strcmp(msg(&a), "KEY E") == 0);
    tap(&a, 3, 4);                                                 /* the octaves */
    CHECK(strcmp(msg(&a), "OCTAVE 3") == 0);
    tap(&a, 1, 4);
    CHECK(strcmp(msg(&a), "OCTAVE 1") == 0);
    tap(&a, 1, 3);                                                 /* the scales: the second of the first row */
    CHECK(says(&a, "SCALE ", jls_get(1)->name));
    tap(&a, 4, 0);                                                 /* the last row */
    CHECK(a.scale == jls_get(24 + 4) && says(&a, "SCALE ", jls_get(28)->name));
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
    CHECK(jlapp_take_message(&a, m, sizeof m) > 0 && strcmp(m, "TRACK PADS ON") == 0);
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
    CHECK(a.n.track == 5 && colour(&a, 5, 0) == JLAPP_COLOR_TRACK_CHOSEN);   /* followed, and shown on the bottom pads */
    CHECK(msg(&a)[0] == 0);                                        /* no message: the pads say it */
    jlapp_on_active_track(&a, 16);
    CHECK(a.n.track == 5);                                         /* not a track */
    jlapp_on_active_track(&a, 5);
    CHECK(a.n.track == 5);
}

int main(void)
{
    test_roles_by_product_name();
    test_midi_interface_choice();
    test_the_note_button_opens_the_page_while_held();
    test_page_pads_edit_and_do_not_play();
    test_page_picks_the_scale();
    test_page_picks_the_layout();
    test_page_picks_the_octave();
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
