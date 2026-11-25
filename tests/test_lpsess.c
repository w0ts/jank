/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the Launchpad's Session page (core/jank_lpsess.c) and of the Session button in the app (core/jank_lpapp.c): the layout, the lights, the reads' schedule, a press
 * and the PATTERN request it becomes, the refusal of a Digitone without PATTERN, a short press against a hold. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_dn2live.h"
#include "jank_dn2rpc.h"
#include "jank_lpapp.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

#define UP 0u
#define DOWN 1u
#define SESSION BUTTON_SESSION_IDX
static const struct jln_scale MAJOR = {"Major", 7, {0, 2, 4, 5, 7, 9, 11}};

static struct jlx_ev pad(uint8_t x, uint8_t y, uint8_t v) { return (struct jlx_ev){JLX_PAD, y, x, 0, v}; }
static struct jlx_ev top(uint8_t idx, uint8_t v) { return (struct jlx_ev){JLX_TOP, 0, 0, idx, v}; }
static struct jlx_ev side(uint8_t idx, uint8_t v) { return (struct jlx_ev){JLX_SIDE, 0, 0, idx, v}; }

static size_t acts(struct jlapp *a, struct jlx_ev e, struct jlapp_act *o) { return jlapp_on_lpx_acts(a, &e, o, 1); }

static const char *msg(struct jlapp *a)
{
    static char m[JLAPP_MSG_MAX];
    m[0] = 0;
    jlapp_take_message(a, m, sizeof m);
    return m;
}

static struct jlapp fresh(void)
{
    struct jlapp a;
    jlapp_init(&a, &MAJOR);
    jlapp_set_live(&a, JDL_LIVE, 1, 1);
    jlapp_tick(&a, 1000);
    return a;
}

/* Session pressed and let go after `ms` */
static void session_press(struct jlapp *a, uint32_t ms)
{
    struct jlapp_act o;
    acts(a, top(SESSION, 127), &o);
    jlapp_tick(a, a->now_ms + ms);
    acts(a, top(SESSION, 0), &o);
}

static struct jlx_led led(struct jlapp *a, uint8_t x, uint8_t y)
{
    struct jlx_frame f;
    jlapp_frame(a, &f);
    return f.led[JLX_PAD0 + y * 8u + x];
}

static struct jdl_patterns bank(uint8_t b, uint8_t playing, uint16_t mask) { return (struct jdl_patterns){b, playing, mask}; }

/* ---- the page alone ---- */

static void test_the_layout_on_both_pages(void)
{
    struct jlss s;
    char name[4];
    jlss_init(&s);
    CHECK(jlss_pattern_of_pad(&s, 0, 7) == 0 && jlss_pattern_of_pad(&s, 7, 7) == 7);      /* top row: A01 .. A08 */
    CHECK(jlss_pattern_of_pad(&s, 0, 6) == 8 && jlss_pattern_of_pad(&s, 7, 6) == 15);     /* next: A09 .. A16 */
    CHECK(jlss_pattern_of_pad(&s, 0, 5) == 16 && jlss_pattern_of_pad(&s, 3, 4) == 27);    /* bank B: B01, B12 */
    CHECK(jlss_pattern_of_pad(&s, 0, 3) == 32 && jlss_pattern_of_pad(&s, 7, 0) == 63);    /* C01, and D16 bottom right */
    CHECK(jlss_half_step(&s, -1) == 0 && s.half == 0);                                     /* up at A-D: nothing further */
    CHECK(jlss_half_step(&s, 1) == 1 && s.half == 1);
    CHECK(jlss_pattern_of_pad(&s, 0, 7) == 64 && jlss_pattern_of_pad(&s, 7, 6) == 79);    /* E01, E16 */
    CHECK(jlss_pattern_of_pad(&s, 0, 1) == 112 && jlss_pattern_of_pad(&s, 7, 0) == 127);  /* H01, H16 */
    CHECK(jlss_half_step(&s, 1) == 0 && s.half == 1 && jlss_half_step(&s, -1) == 1 && s.half == 0);
    jlss_name(0, name);
    CHECK(strcmp(name, "A01") == 0);
    jlss_name(37, name);
    CHECK(strcmp(name, "C06") == 0);
    jlss_name(127, name);
    CHECK(strcmp(name, "H16") == 0);
}

static void test_the_lights_for_every_state(void)
{
    struct jlss s;
    struct jlx_frame f;
    struct jdl_patterns b = bank(1, 20, 0x0011u);                                          /* bank B: B01 and B05 hold trigs, B05 (20) plays */
    struct jdl_pattern cue = {20, 66};
    jlss_init(&s);
    jlss_frame(&s, &f);
    for (unsigned i = 0; i < 64; i++)
        CHECK(f.led[JLX_PAD0 + i].color == 0);                                             /* nothing read: dark */
    jlss_on_patterns(&s, &b);
    jlss_cue(&s, 2);                                                                       /* A03 cued by a press */
    jlss_frame(&s, &f);
    CHECK(f.led[JLX_PAD0 + 5 * 8 + 4].color == JLSS_COLOR_PLAYING && f.led[JLX_PAD0 + 5 * 8 + 4].mode == JLX_PULSE);   /* B05: GREEN pulsing */
    CHECK(f.led[JLX_PAD0 + 5 * 8 + 0].color == JLSS_COLOR_FILLED && f.led[JLX_PAD0 + 5 * 8 + 0].mode == JLX_STATIC);   /* B01: trigs, RED_THIRD */
    CHECK(JLSS_COLOR_FILLED == 7u && JLSS_COLOR_PLAYING == 21u && JLSS_COLOR_CUED == 21u);
    CHECK(f.led[JLX_PAD0 + 5 * 8 + 1].color == 0);                                         /* B02: empty, dark */
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 2].color == JLSS_COLOR_CUED && f.led[JLX_PAD0 + 7 * 8 + 2].mode == JLX_FLASH && f.led[JLX_PAD0 + 7 * 8 + 2].alt == 0);   /* A03 flashes green / dark */
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 0].color == 0);                                         /* bank A not read: dark */
    jlss_on_pattern(&s, &cue);                                                             /* the Digitone: E03 is cued (shown on the other page) */
    jlss_frame(&s, &f);
    CHECK(s.cued == 66 && f.led[JLX_PAD0 + 7 * 8 + 2].color == 0);
    jlss_half_step(&s, 1);
    jlss_frame(&s, &f);
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 2].mode == JLX_FLASH && f.led[JLX_PAD0 + 5 * 8 + 4].color == 0);   /* E03 flashes here; B05 is not on this page */
    b = bank(4, 66, 0x0004u);                                                              /* E03 plays now: the cue is done */
    jlss_on_patterns(&s, &b);
    jlss_frame(&s, &f);
    CHECK(s.cued == JDL_NO_PATTERN && f.led[JLX_PAD0 + 7 * 8 + 2].mode == JLX_PULSE);
    cue.playing = 66;
    cue.cued = 66;                                                                         /* "cued" the one that plays: nothing cued */
    jlss_on_pattern(&s, &cue);
    CHECK(s.cued == JDL_NO_PATTERN);
    b = bank(9, 1, 0xFFFFu);                                                               /* a bank that does not exist: not taken */
    jlss_on_patterns(&s, &b);
    CHECK(s.playing == 66);
    jlss_forget(&s);                                                                       /* live control gone: dark, the page shown stays */
    jlss_frame(&s, &f);
    CHECK(f.led[JLX_PAD0 + 7 * 8 + 2].color == 0 && s.half == 1 && s.playing == JLSS_UNKNOWN);
}

/* every bank once at the opening, 60 ms apart; then the PATTERN read; then every 500 ms the playing bank with a PATTERN read and a bank that never answered */
static void test_the_reads_schedule(void)
{
    struct jlss s;
    struct jlss_req r;
    uint32_t t = 5000;
    jlss_init(&s);
    CHECK(!jlss_due(&s, t, 1, 1, &r));                                                    /* not open: nothing */
    jlss_open(&s);
    for (uint8_t b = 0; b < JDL_BANKS; b++, t += JLSS_OPEN_GAP_MS) {
        CHECK(jlss_due(&s, t, 1, 1, &r) && r.kind == JLSS_REQ_BANK && r.bank == b);
        CHECK(b + 1u == JDL_BANKS || !jlss_due(&s, t + JLSS_OPEN_GAP_MS - 1u, 1, 1, &r));   /* one per gap */
        if (b != 3) {                                                                      /* bank D's answer is lost */
            struct jdl_patterns p = bank(b, 0, 1);
            jlss_on_patterns(&s, &p);
        }
    }
    t -= JLSS_OPEN_GAP_MS;                                                                 /* (the time of the last bank) */
    CHECK(jlss_due(&s, t, 1, 1, &r) && r.kind == JLSS_REQ_CUE_READ);                      /* then PATTERN, once */
    CHECK(!jlss_due(&s, t + JLSS_POLL_MS - 1u, 1, 1, &r));
    CHECK(jlss_due(&s, t + JLSS_POLL_MS, 1, 1, &r) && r.kind == JLSS_REQ_BANK && r.bank == JDL_BANK_PLAYING);
    CHECK(jlss_due(&s, t + JLSS_POLL_MS, 1, 1, &r) && r.kind == JLSS_REQ_CUE_READ);
    CHECK(jlss_due(&s, t + JLSS_POLL_MS, 1, 1, &r) && r.kind == JLSS_REQ_BANK && r.bank == 3);   /* the lost one, again */
    CHECK(!jlss_due(&s, t + JLSS_POLL_MS, 1, 1, &r));
    {
        struct jdl_patterns p = bank(3, 0, 1);
        jlss_on_patterns(&s, &p);
    }
    t += 2u * JLSS_POLL_MS;
    CHECK(jlss_due(&s, t, 1, 0, &r) && r.bank == JDL_BANK_PLAYING);                       /* PATTERN refused: no PATTERN read */
    CHECK(!jlss_due(&s, t, 1, 0, &r));                                                    /* and every bank is in: no retry */
    jlss_open(&s);                                                                         /* a Digitone without PATTERNS: no bank read, only the cue */
    CHECK(jlss_due(&s, t, 0, 1, &r) && r.kind == JLSS_REQ_CUE_READ);
    CHECK(!jlss_due(&s, t + 10u * JLSS_POLL_MS, 0, 1, &r));
}

/* ---- the Session button and the page in the app ---- */

static void test_a_short_press_is_the_page_a_hold_the_chart(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlx_frame f;
    msg(&a);
    acts(&a, top(SESSION, 127), &o);
    CHECK(a.mode == JLAPP_MODE_NOTE);                                                      /* nothing yet while it is down */
    jlapp_tick(&a, a.now_ms + JLAPP_SESSION_HOLD_MS - 1u);
    CHECK(acts(&a, top(SESSION, 0), &o) == 0 && a.mode == JLAPP_MODE_SESSION && strcmp(msg(&a), "SESSION BANKS A-D") == 0);   /* let go in time: the page */
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + SESSION].color == JLAPP_COLOR_PALETTE_ON);
    session_press(&a, 100);                                                                /* again: back to where it came from */
    CHECK(a.mode == JLAPP_MODE_NOTE && strcmp(msg(&a), "SESSION OFF") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + SESSION].color == JLAPP_COLOR_PALETTE_OFF);
    acts(&a, top(SESSION, 127), &o);                                                       /* held: the chart opens at 1 s, still held */
    jlapp_tick(&a, a.now_ms + JLAPP_SESSION_HOLD_MS);
    CHECK(a.mode == JLAPP_MODE_PALETTE && a.palette_page == 0 && strcmp(msg(&a), "COLOURS 0-63") == 0);
    CHECK(acts(&a, top(SESSION, 0), &o) == 0 && a.mode == JLAPP_MODE_PALETTE && msg(&a)[0] == 0);   /* its release: nothing */
    CHECK(acts(&a, pad(3, 6, 100), &o) == 0 && strcmp(msg(&a), "COLOUR 11") == 0);        /* the chart as before */
    acts(&a, top(SESSION, 127), &o);
    CHECK(a.palette_page == 1 && strcmp(msg(&a), "COLOURS 64-127") == 0);
    acts(&a, top(SESSION, 0), &o);
    acts(&a, top(SESSION, 127), &o);
    CHECK(a.mode == JLAPP_MODE_NOTE && strcmp(msg(&a), "COLOURS OFF") == 0);
    acts(&a, top(SESSION, 0), &o);
    CHECK(a.mode == JLAPP_MODE_NOTE);                                                      /* that release opens nothing */
    acts(&a, top(6, 127), &o);                                                             /* from a step mode: the page, then back to it */
    session_press(&a, 50);
    CHECK(a.mode == JLAPP_MODE_SESSION);
    acts(&a, top(SESSION, 127), &o);                                                       /* held on the page: the chart, which goes back to the page */
    jlapp_tick(&a, a.now_ms + JLAPP_SESSION_HOLD_MS + 5u);
    acts(&a, top(SESSION, 0), &o);
    CHECK(a.mode == JLAPP_MODE_PALETTE);
    acts(&a, top(SESSION, 127), &o);
    acts(&a, top(SESSION, 127), &o);
    CHECK(a.mode == JLAPP_MODE_SESSION && a.sess.sweep == 0);                             /* back on the page: read again */
    session_press(&a, 50);
    CHECK(a.mode == JLAPP_MODE_STEP);
}

static void test_the_reads_only_while_the_page_is_shown_and_live(void)
{
    struct jlapp a = fresh();
    struct jlss_req r;
    struct jdl_patterns p = bank(0, 3, 0x000Fu);
    CHECK(!jlapp_session_due(&a, &r));                                                     /* Note mode: nothing */
    session_press(&a, 50);
    CHECK(jlapp_session_due(&a, &r) && r.kind == JLSS_REQ_BANK && r.bank == 0);
    jlapp_on_patterns(&a, &p);
    CHECK(led(&a, 3, 7).mode == JLX_PULSE && led(&a, 0, 7).color == JLSS_COLOR_FILLED);
    jlapp_tick(&a, a.now_ms + JLSS_OPEN_GAP_MS);
    jlapp_set_live(&a, JDL_ASKING, 1, 1);                                                  /* live control lapsed: no reads, nothing shown */
    CHECK(!jlapp_session_due(&a, &r) && led(&a, 3, 7).color == 0);
    jlapp_set_live(&a, JDL_LIVE, 1, 1);                                                    /* back: every bank from the start */
    CHECK(jlapp_session_due(&a, &r) && r.bank == 0);
    session_press(&a, 50);                                                                 /* closed */
    jlapp_tick(&a, a.now_ms + 10u * JLSS_POLL_MS);
    CHECK(!jlapp_session_due(&a, &r));
}

/* a pad press cues its pattern: the act, the PATTERN request it becomes (usb_lp.c: jdl_pattern(act.note, act.vel)), the flash, the message */
static void test_a_press_cues_the_pattern(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jdl d;
    uint8_t sx[64], body[16];
    size_t n;
    session_press(&a, 50);
    msg(&a);
    CHECK(acts(&a, pad(1, 4, 100), &o) == 1 && o.kind == JLAPP_ACT_PATTERN && o.note == 25 && o.vel == JDL_PATTERN_CUE);   /* B10 */
    CHECK(strcmp(msg(&a), "CUE B10") == 0 && led(&a, 1, 4).mode == JLX_FLASH);
    CHECK(acts(&a, pad(1, 4, 0), &o) == 0);                                                /* the release: nothing */
    jdl_init(&d);
    jdl_poll(&d, 0, sx, sizeof sx);
    d.state = JDL_LIVE;
    n = jdl_pattern(&d, 1, o.note, o.vel, sx, sizeof sx);
    CHECK(n > 0 && jd2_unpack7(sx + 6, n - 7, body) == 5 && body[2] == JDL_CMD_PATTERN && body[3] == 25 && body[4] == 0);   /* 60 {25, 0} */
    acts(&a, top(DOWN, 127), &o);
    CHECK(acts(&a, pad(7, 0, 100), &o) == 1 && o.note == 127 && strcmp(msg(&a), "CUE H16") == 0);   /* H16 on the second page */
    {
        struct jlapp b = fresh();                                                          /* no live control: it says why, cues nothing */
        session_press(&b, 50);
        jlapp_set_live(&b, JDL_REMOTE_OFF, 1, 1);
        msg(&b);
        CHECK(acts(&b, pad(0, 7, 100), &o) == 0 && strcmp(msg(&b), "REMOTE CONTROL OFF") == 0);
    }
}

/* a Digitone without PATTERN 60 (RECORD's way): said once when it refuses, then at each press, and the pads send nothing */
static void test_a_digitone_without_pattern(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlss_req r;
    session_press(&a, 50);
    acts(&a, pad(0, 7, 100), &o);
    msg(&a);
    jlapp_set_patterns(&a, 1, 0);
    CHECK(strcmp(msg(&a), "PATTERN: DN2 TOO OLD") == 0 && a.sess.cued == JDL_NO_PATTERN && led(&a, 0, 7).color == 0);
    jlapp_set_patterns(&a, 1, 0);
    CHECK(msg(&a)[0] == 0);                                                                /* once */
    CHECK(acts(&a, pad(2, 7, 100), &o) == 0 && strcmp(msg(&a), "PATTERN: DN2 TOO OLD") == 0);
    while (jlapp_session_due(&a, &r))                                                      /* the reads go on, without PATTERN's */
        CHECK(r.kind == JLSS_REQ_BANK);
    jlapp_set_patterns(&a, 1, 1);                                                          /* live control asked for afresh (jdl_init): tried again */
    CHECK(acts(&a, pad(2, 7, 100), &o) == 1 && o.kind == JLAPP_ACT_PATTERN);
}

static void test_the_arrows_and_the_scene_buttons_on_the_page(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    struct jlx_frame f;
    session_press(&a, 50);
    msg(&a);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + UP].color == JLAPP_COLOR_ARROW_OFF && f.led[JLX_TOP0 + DOWN].color == JLAPP_COLOR_ARROW_ON);
    CHECK(acts(&a, top(UP, 127), &o) == 0 && msg(&a)[0] == 0 && a.n.octave == 3);         /* at A-D: nothing, and not the octave */
    CHECK(acts(&a, top(DOWN, 127), &o) == 0 && a.sess.half == 1 && strcmp(msg(&a), "BANKS E-H") == 0);
    jlapp_frame(&a, &f);
    CHECK(f.led[JLX_TOP0 + UP].color == JLAPP_COLOR_ARROW_ON && f.led[JLX_TOP0 + DOWN].color == JLAPP_COLOR_ARROW_OFF);
    CHECK(acts(&a, top(UP, 127), &o) == 0 && a.sess.half == 0 && strcmp(msg(&a), "BANKS A-D") == 0);
    /* the scene buttons: Note mode's start / stop, Delete and Record; the rest dark */
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_START_STOP].color == JLAPP_RED_THIRD && f.led[JLX_SIDE0 + JLAPP_SIDE_DELETE].color == JLAPP_PINK_THIRD);
    CHECK(f.led[JLX_SIDE0 + JLAPP_SIDE_RECORD].color == JLAPP_RED_THIRD);
    for (uint8_t i = 0; i < 4u; i++)
        CHECK(f.led[JLX_SIDE0 + i].color == 0);
    CHECK(f.led[JLX_SIDE0 + 6].color == 0);
    CHECK(acts(&a, side(JLAPP_SIDE_START_STOP, 127), &o) == 1 && o.kind == JLAPP_ACT_TRANSPORT);
    CHECK(acts(&a, side(JLAPP_SIDE_DELETE, 127), &o) == 1 && o.kind == JLAPP_ACT_ERASE && o.vel == 1);
    CHECK(acts(&a, side(JLAPP_SIDE_DELETE, 0), &o) == 1 && o.kind == JLAPP_ACT_ERASE && o.vel == 0);
    CHECK(acts(&a, side(JLAPP_SIDE_RECORD, 127), &o) == 1 && o.kind == JLAPP_ACT_RECORD);
    CHECK(acts(&a, side(0, 127), &o) == 0 && a.mode == JLAPP_MODE_SESSION);
}

static void test_a_note_down_when_the_page_opens_still_ends(void)
{
    struct jlapp a = fresh();
    struct jlapp_act o;
    CHECK(acts(&a, pad(0, 0, 100), &o) == 1 && o.kind == JLAPP_ACT_NOTE_ON);
    session_press(&a, 50);
    CHECK(acts(&a, pad(0, 0, 0), &o) == 1 && o.kind == JLAPP_ACT_NOTE_OFF);
}

int main(void)
{
    test_the_layout_on_both_pages();
    test_the_lights_for_every_state();
    test_the_reads_schedule();
    test_a_short_press_is_the_page_a_hold_the_chart();
    test_the_reads_only_while_the_page_is_shown_and_live();
    test_a_press_cues_the_pattern();
    test_a_digitone_without_pattern();
    test_the_arrows_and_the_scene_buttons_on_the_page();
    test_a_note_down_when_the_page_opens_still_ends();
    printf("test_lpsess: %d checks, %d failed\n", checks, fails);
    return fails;
}
