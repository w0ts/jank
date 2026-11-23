/* SPDX-License-Identifier: GPL-3.0-only */
/* The LED language of a unit without a screen (core/jank_led.h), and the firmware's own mix of the OLED UI's flags
 * (AUDIO mode: the tracker leads, NUDGE MS works, NUDGE BEAT does not) that the emulator never shows. */
#include "jank_led.h"
#include "jank_core.h"
#include "jank_oled.h"
#include "jank_ui.h"
#include <stdio.h>
#include <string.h>

static int s_fail, s_checks;
#define CHECK(c, ...)                                                    \
    do {                                                                 \
        s_checks++;                                                      \
        if (!(c)) {                                                      \
            s_fail++;                                                    \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);         \
            fprintf(stderr, __VA_ARGS__);                                \
            fputc('\n', stderr);                                         \
        }                                                                \
    } while (0)

#define MPB_120 500000                       /* 120 BPM */
static int same(const uint8_t a[3], uint8_t r, uint8_t g, uint8_t b) { return a[0] == r && a[1] == g && a[2] == b; }

static struct jlg_state base(void)
{
    struct jlg_state s;
    memset(&s, 0, sizeof s);
    s.batt_pct = -1;
    s.on_usb = 1;
    s.quantum = 4;
    s.mpb = MPB_120;
    return s;
}

static void test_language(void)
{
    struct jlg_state s = base();
    uint8_t c[3];
    CHECK(!jlg_color(&s, c), "stopped, nobody: dark");
    s.joined = 1;
    s.now_ms = 0;
    CHECK(!jlg_color(&s, c), "breath at its bottom");
    s.now_ms = 2000;
    CHECK(jlg_color(&s, c) && same(c, 0, JLG_BREATH_MAX, 0), "breath at its top: green %u", c[1]);
    s.now_ms = 1000;
    CHECK(jlg_color(&s, c) && same(c, 0, JLG_BREATH_MAX / 2, 0), "breath half way");
    s.armed = 1;
    CHECK(jlg_color(&s, c) && same(c, JLG_ARMED_RGB), "armed: yellow");
    s.armed = 0;
    s.running = 1;
    s.beat_ub = 4000000 + 50000;                 /* beat 4 = the bar's 1, 25 ms in: lit (150 ms) */
    CHECK(jlg_color(&s, c) && same(c, JLG_BEAT_RGB), "the 1: green");
    s.beat_ub = 4000000 + 250000;                /* 125 ms in */
    CHECK(jlg_color(&s, c), "the 1 is longer");
    s.beat_ub = 5000000 + 250000;                /* beat 2, 125 ms in: the 80 ms flash is over */
    CHECK(!jlg_color(&s, c), "beat 2 at 125 ms: dark");
    s.beat_ub = 5000000 + 100000;                /* 50 ms */
    CHECK(jlg_color(&s, c) && same(c, JLG_BEAT_RGB), "beat 2 at 50 ms: green");
    s.bar_locked = 1;
    CHECK(jlg_color(&s, c) && same(c, JLG_BEAT_RGB), "locked: beat 2 stays green");
    s.beat_ub = 8000000;
    CHECK(jlg_color(&s, c) && same(c, JK_LED_ONE_RGB), "locked: the 1 amber");
    s.low_conf = 1;
    CHECK(jlg_color(&s, c) && same(c, JLG_LOWCONF_RGB), "low confidence: red on the beat");
    s.beat_ub = 8000000 + 500000;
    CHECK(!jlg_color(&s, c), "low confidence: dark between beats");
    s.on_usb = 0;
    s.batt_pct = 20;
    s.now_ms = 10;
    CHECK(jlg_color(&s, c) && same(c, JLG_BATT_RGB), "low cell: the red flash over the rest");
    s.now_ms = 150;
    CHECK(!jlg_color(&s, c), "low cell: the beat between the flashes");
    s.batt_pct = 50;
    s.now_ms = 10;
    CHECK(!jlg_color(&s, c), "a full cell: no warning");
    CHECK(jlg_batt_period_ms(30) == JLG_BATT_SLOW_MS && jlg_batt_period_ms(5) == JLG_BATT_FAST_MS && jlg_batt_period_ms(0) == JLG_BATT_FAST_MS &&
              jlg_batt_period_ms(99) == JLG_BATT_SLOW_MS, "battery periods");
}

/* the firmware in AUDIO mode: YES / UP / DOWN say TRACKER LEADS, YES + UP nudges the AUDIO IN offset, NO + UP says
 * TRACKER LEADS */
static int s_calls, s_inoff;
static struct jo_ctx s_ctx;
static void set_field(void *u, enum jo_field f, int v)
{
    (void)u;
    s_calls++;
    if (f == JO_F_INOFF)
        s_ctx.in_off = s_inoff = v;
}
static void shift(void *u, int h)
{
    (void)u;
    (void)h;
    s_calls += 100;
}
static const struct jo_ctx *ctx(void *u, uint32_t now)
{
    (void)u;
    (void)now;
    return &s_ctx;
}

static void test_firmware_flags(void)
{
    struct jo_ops ops = {NULL, NULL, set_field, NULL, NULL, NULL, shift};
    struct jo_host h = {&ops, ctx};
    struct jo_ui ui;
    struct jo_input in;
    memset(&s_ctx, 0, sizeof s_ctx);
    s_ctx.line_in = 1;
    s_ctx.tracker_leads = 1;
    s_ctx.no_beat_nudge = 1;
    s_ctx.in_off = 25;
    jo_ui_init(&ui);
    jo_input_init(&in, 0);
    jo_input_down(&in, &ui, JO_YES, 0, &h);
    jo_input_down(&in, &ui, JO_UP, 10, &h);
    CHECK(s_inoff == 26 && !strcmp(ui.overlay.title, "NUDGE MS") && !strcmp(ui.overlay.value, "+26"), "NUDGE MS works: %s %s", ui.overlay.title,
          ui.overlay.value);
    jo_input_up(&in, &ui, JO_UP, 20, &h);
    jo_input_up(&in, &ui, JO_YES, 30, &h);
    CHECK(s_calls == 1, "the combo's YES does nothing on release");
    jo_input_down(&in, &ui, JO_NO, 40, &h);
    jo_input_down(&in, &ui, JO_DOWN, 50, &h);
    CHECK(!strcmp(ui.overlay.title, "TRACKER LEADS") && s_calls == 1, "NUDGE BEAT is not there: no shift");
    jo_input_up(&in, &ui, JO_DOWN, 60, &h);
    jo_input_up(&in, &ui, JO_NO, 70, &h);
    CHECK(ui.screen == JO_HOME, "the combo's NO does not open the menu");
    jo_input_down(&in, &ui, JO_YES, 2000, &h);
    jo_input_up(&in, &ui, JO_YES, 2050, &h);
    CHECK(!strcmp(ui.overlay.title, "TRACKER LEADS") && ui.overlay.until == 3050u, "YES alone: TRACKER LEADS");
}

/* USB host mode shows as two short flashes every JLG_HOST_PERIOD_MS, over the rest (a unit in host mode has no screen and no
 * control line: the LED is how one tells it is hosting) */
static void test_host_double_flash(void)
{
    struct jlg_state s = base();
    uint8_t c[3];
    const uint32_t t0 = JLG_HOST_PERIOD_MS * 5u;                 /* a window start, whatever the uptime */
    s.host = 1;
    s.now_ms = t0 + 10u;
    CHECK(jlg_color(&s, c) && same(c, JLG_HOST_RGB), "host: the first flash");
    s.now_ms = t0 + JLG_HOST_FLASH_MS + 10u;
    CHECK(!jlg_color(&s, c), "host: dark between the two flashes");
    s.now_ms = t0 + JLG_HOST_FLASH_MS + JLG_HOST_GAP_MS + 10u;
    CHECK(jlg_color(&s, c) && same(c, JLG_HOST_RGB), "host: the second flash");
    s.now_ms = t0 + 2u * JLG_HOST_FLASH_MS + JLG_HOST_GAP_MS + 10u;
    CHECK(!jlg_color(&s, c), "host: dark after the pair");
    s.now_ms = t0 + 1000u;
    CHECK(!jlg_color(&s, c), "host: quiet for the rest of the period");
    s.host = 0;
    s.now_ms = t0 + 10u;
    CHECK(!jlg_color(&s, c), "not host: no flash");
    CHECK(jlg_host_lit(t0 + 10u) && !jlg_host_lit(t0 + JLG_HOST_FLASH_MS + 10u) && jlg_host_lit(t0 + JLG_HOST_FLASH_MS + JLG_HOST_GAP_MS + 10u),
          "jlg_host_lit: the same pattern");
    /* over the beat: the pair shows even where the beat is dark, the beat shows outside it */
    s.host = 1;
    s.joined = 1;
    s.running = 1;
    s.now_ms = t0 + 10u;
    s.beat_ub = 5000000 + 250000;                                /* beat 2, 125 ms in: dark */
    CHECK(jlg_color(&s, c) && same(c, JLG_HOST_RGB), "host over a dark beat: the flash shows");
    s.now_ms = t0 + 1000u;
    s.beat_ub = 5000000 + 100000;                                /* 50 ms in: lit */
    CHECK(jlg_color(&s, c) && same(c, JLG_BEAT_RGB), "host outside the pair: the beat as ever");
    /* the low battery's red blink comes first */
    s.on_usb = 0;
    s.batt_pct = 10;
    s.now_ms = 16800u;                                           /* 10 %: a blink every 840 ms; 16800 is a multiple of it and of the host period: both start */
    CHECK(jlg_host_lit(s.now_ms), "(precondition) the host flash is lit there");
    CHECK(jlg_color(&s, c) && same(c, JLG_BATT_RGB), "low battery over the host flash");
}

/* A one-colour LED rests lit in MODE WIFI (short offs for the beat) and dark otherwise: the host flash must show against either, so inside
 * its two windows the LED is the opposite of what it would be. */
static void test_host_overlay(void)
{
    const uint32_t t0 = JLG_HOST_PERIOD_MS * 7u;
    const uint32_t first = t0 + 10u, gap = t0 + JLG_HOST_FLASH_MS + 10u, second = t0 + JLG_HOST_FLASH_MS + JLG_HOST_GAP_MS + 10u, quiet = t0 + 1000u;
    CHECK(jlg_host_overlay(0, first, 1) == 1 && jlg_host_overlay(1, first, 1) == 0, "host, first window: the opposite of the resting state");
    CHECK(jlg_host_overlay(0, gap, 1) == 0 && jlg_host_overlay(1, gap, 1) == 1, "host, between the flashes: as it would be");
    CHECK(jlg_host_overlay(0, second, 1) == 1 && jlg_host_overlay(1, second, 1) == 0, "host, second window: the opposite");
    CHECK(jlg_host_overlay(0, quiet, 1) == 0 && jlg_host_overlay(1, quiet, 1) == 1, "host, the rest of the period: as it would be");
    CHECK(jlg_host_overlay(0, first, 0) == 0 && jlg_host_overlay(1, first, 0) == 1, "not host: never changed");
    CHECK(jlg_host_overlay(7, first, 1) == 0 && jlg_host_overlay(7, quiet, 1) == 7, "any non-zero lit is lit: the opposite is 0, untouched stays as given");
}

int main(void)
{
    test_language();
    test_firmware_flags();
    test_host_double_flash();
    test_host_overlay();
    if (s_fail) {
        fprintf(stderr, "test_led: %d of %d checks failed\n", s_fail, s_checks);
        return 1;
    }
    printf("test_led: %d checks\n", s_checks);
    return 0;
}
