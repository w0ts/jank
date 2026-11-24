/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the Launchpad X Programmer mode core (core/jank_lpx.c): what pads and buttons send, and the packets that light
 * them. The numbers are those of the manual. Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_lpx.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static struct jlx_ev dec(uint8_t s, uint8_t a, uint8_t b, int *ok)
{
    const uint8_t m[3] = {s, a, b};
    struct jlx_ev e;
    memset(&e, 0xEE, sizeof e);
    *ok = jlx_decode(m, &e);
    return e;
}

static void test_decode_pads(void)
{
    int ok;
    struct jlx_ev e = dec(0x90, 11, 127, &ok);                 /* lower left */
    CHECK(ok && e.kind == JLX_PAD && e.row == 0 && e.col == 0 && e.value == 127);
    e = dec(0x90, 88, 64, &ok);                                /* upper right */
    CHECK(ok && e.kind == JLX_PAD && e.row == 7 && e.col == 7 && e.value == 64);
    e = dec(0x90, 45, 0, &ok);                                 /* Note On velocity 0 is a release */
    CHECK(ok && e.kind == JLX_PAD && e.row == 3 && e.col == 4 && e.value == 0);
    e = dec(0x80, 45, 90, &ok);                                /* a Note Off is a release whatever its velocity */
    CHECK(ok && e.kind == JLX_PAD && e.value == 0);
    e = dec(0x93, 18, 100, &ok);                               /* any channel */
    CHECK(ok && e.kind == JLX_PAD && e.row == 0 && e.col == 7 && e.value == 100);
}

static void test_decode_not_pads(void)
{
    int ok;
    (void)dec(0x90, 10, 100, &ok);                              /* column 0 does not exist */
    CHECK(!ok);
    (void)dec(0x90, 19, 100, &ok);                              /* column 9 does not exist */
    CHECK(!ok);
    (void)dec(0x90, 9, 100, &ok);
    CHECK(!ok);
    (void)dec(0x90, 89, 100, &ok);                              /* that is the side button's CC number, not a note */
    CHECK(!ok);
    (void)dec(0x90, 0, 100, &ok);
    CHECK(!ok);
    (void)dec(0xA0, 45, 100, &ok);                              /* polyphonic aftertouch */
    CHECK(!ok);
    (void)dec(0xD0, 45, 0, &ok);                                /* channel pressure */
    CHECK(!ok);
    (void)dec(0xE0, 0, 64, &ok);
    CHECK(!ok);
}

static void test_decode_buttons(void)
{
    int ok;
    struct jlx_ev e;
    for (uint8_t i = 0; i < 8; i++) {
        e = dec(0xB0, (uint8_t)(91u + i), 127, &ok);
        CHECK(ok && e.kind == JLX_TOP && e.idx == i && e.value == 127);
        e = dec(0xB0, (uint8_t)(91u + i), 0, &ok);
        CHECK(ok && e.kind == JLX_TOP && e.idx == i && e.value == 0);
    }
    for (uint8_t i = 0; i < 8; i++) {                           /* CC 89 is the top one, CC 19 the bottom one */
        e = dec(0xB0, (uint8_t)(89u - 10u * i), 127, &ok);
        CHECK(ok && e.kind == JLX_SIDE && e.idx == i && e.value == 127);
    }
    (void)dec(0xB0, 99, 127, &ok);                              /* the logo does not send */
    CHECK(!ok);
    (void)dec(0xB0, 90, 127, &ok);
    CHECK(!ok);
    (void)dec(0xB0, 29u + 1u, 127, &ok);                        /* 30 is no button */
    CHECK(!ok);
    (void)dec(0xB0, 7, 127, &ok);                               /* a CC that is not a button's */
    CHECK(!ok);
}

static void test_numbers_round_trip(void)
{
    uint8_t seen[128];
    memset(seen, 0, sizeof seen);
    for (uint32_t i = 0; i < JLX_LEDS; i++) {
        uint8_t n = jlx_number(i);
        CHECK(n < 128 && !seen[n]);                             /* one number each */
        seen[n] = 1;
    }
    CHECK(jlx_number(JLX_PAD0 + 0) == 11 && jlx_number(JLX_PAD0 + 7) == 18 && jlx_number(JLX_PAD0 + 63) == 88);
    CHECK(jlx_number(JLX_TOP0) == 91 && jlx_number(JLX_TOP0 + 7) == 98);
    CHECK(jlx_number(JLX_SIDE0) == 89 && jlx_number(JLX_SIDE0 + 7) == 19);
    CHECK(jlx_number(JLX_LOGO) == 99);
    for (uint32_t i = 0; i < 64; i++) {                         /* a pad's note decodes back to the same pad */
        const uint8_t m[3] = {0x90, jlx_number(i), 100};
        struct jlx_ev e;
        CHECK(jlx_decode(m, &e) && e.kind == JLX_PAD && e.row * 8u + e.col == i);
    }
}

static void test_programmer_sysex(void)
{
    uint8_t out[16];
    const uint8_t on[12] = {0x04, 0xF0, 0x00, 0x20, 0x04, 0x29, 0x02, 0x0C, 0x07, 0x0E, 0x01, 0xF7};
    const uint8_t off[12] = {0x04, 0xF0, 0x00, 0x20, 0x04, 0x29, 0x02, 0x0C, 0x07, 0x0E, 0x00, 0xF7};
    CHECK(jlx_programmer(0, 1, out) == 12 && !memcmp(out, on, 12));
    CHECK(jlx_programmer(0, 0, out) == 12 && !memcmp(out, off, 12));
    CHECK(jlx_programmer(2, 1, out) == 12 && out[0] == 0x24 && out[4] == 0x24 && out[8] == 0x27);   /* the cable in the high nibble */
}

static struct jlx_frame blank(void)
{
    struct jlx_frame f;
    memset(&f, 0, sizeof f);
    return f;
}

static void test_diff_nothing_to_send(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[400];
    CHECK(jlx_diff(0, &shown, &want, out, 100) == 0);
}

static void test_diff_static_pad_and_button(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[16];
    const uint8_t pad[4] = {0x09, 0x90, 11, 5};
    const uint8_t top[4] = {0x0B, 0xB0, 91, 21};
    want.led[JLX_PAD0 + 0].color = 5;
    CHECK(jlx_diff(0, &shown, &want, out, 4) == 1 && !memcmp(out, pad, 4));
    CHECK(shown.led[0].color == 5);
    CHECK(jlx_diff(0, &shown, &want, out, 4) == 0);                  /* now the same: nothing */
    want.led[JLX_TOP0].color = 21;
    CHECK(jlx_diff(1, &shown, &want, out, 4) == 1 && out[0] == 0x1B && !memcmp(out + 1, top + 1, 3));   /* cable 1 */
}

static void test_diff_off_is_static_zero(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[16];
    const uint8_t off[4] = {0x09, 0x90, 12, 0};
    shown.led[1].color = 9;
    shown.led[1].mode = JLX_PULSE;                                  /* a pulsing light is stopped by a static 0 */
    CHECK(jlx_diff(0, &shown, &want, out, 4) == 1 && !memcmp(out, off, 4));
    CHECK(shown.led[1].mode == JLX_STATIC && shown.led[1].color == 0);
}

static void test_diff_flash_and_pulse(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[16];
    const uint8_t a[4] = {0x09, 0x90, 11, 3};                       /* A, static */
    const uint8_t b[4] = {0x09, 0x91, 11, 5};                       /* then B, flashing */
    const uint8_t p[4] = {0x09, 0x92, 12, 45};
    want.led[0].mode = JLX_FLASH;
    want.led[0].alt = 3;
    want.led[0].color = 5;
    CHECK(jlx_diff(0, &shown, &want, out, 8) == 2 && !memcmp(out, a, 4) && !memcmp(out + 4, b, 4));
    CHECK(jlx_diff(0, &shown, &want, out, 8) == 0);
    want.led[1].mode = JLX_PULSE;
    want.led[1].color = 45;
    CHECK(jlx_diff(0, &shown, &want, out, 8) == 1 && !memcmp(out, p, 4));
    want.led[0].alt = 7;                                            /* a changed A sends the pair again */
    CHECK(jlx_diff(0, &shown, &want, out, 8) == 2 && out[3] == 7);
}

static void test_diff_limit_and_resume(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[4 * JLX_LEDS * 2];
    size_t total = 0, n;
    for (uint32_t i = 0; i < JLX_LEDS; i++)
        want.led[i].color = (uint8_t)(1 + i);
    n = jlx_diff(0, &shown, &want, out, 16);
    CHECK(n == 16);
    total += n;
    while ((n = jlx_diff(0, &shown, &want, out, 16)) != 0)
        total += n;
    CHECK(total == JLX_LEDS);                                       /* every light sent once, none twice */
    CHECK(!memcmp(&shown, &want, sizeof shown));
}

static void test_diff_flash_needs_two_slots(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[16];
    want.led[0].mode = JLX_FLASH;
    want.led[0].color = 5;
    CHECK(jlx_diff(0, &shown, &want, out, 1) == 0);                 /* no half a flash: it waits for room */
    CHECK(shown.led[0].mode == JLX_STATIC && shown.led[0].color == 0);
    want.led[1].color = 9;                                          /* a later light that fits does not jump the queue */
    CHECK(jlx_diff(0, &shown, &want, out, 1) == 0);
    CHECK(jlx_diff(0, &shown, &want, out, 3) == 3);
}

static void test_forget_sends_all(void)
{
    struct jlx_frame shown = blank(), want = blank();
    uint8_t out[4 * JLX_LEDS];
    CHECK(jlx_diff(0, &shown, &want, out, JLX_LEDS) == 0);
    jlx_forget(&shown);
    CHECK(jlx_diff(0, &shown, &want, out, JLX_LEDS) == JLX_LEDS);   /* all dark lights are sent once, to be sure */
    CHECK(jlx_diff(0, &shown, &want, out, JLX_LEDS) == 0);
}

/* the Launchpad's answer to the Programmer mode message: the same SysEx back, its last packet 07 0E 01 F7 (measured) */
static void test_programmer_ack(void)
{
    const uint8_t on[4] = {0x07, 0x0E, 0x01, 0xF7}, off[4] = {0x07, 0x0E, 0x00, 0xF7}, cable2[4] = {0x27, 0x0E, 0x01, 0xF7};
    const uint8_t first[4] = {0x04, 0xF0, 0x00, 0x20}, bad_mode[4] = {0x07, 0x0E, 0x02, 0xF7}, bad_end[4] = {0x07, 0x0E, 0x01, 0x00};
    const uint8_t not_end[4] = {0x04, 0x0E, 0x01, 0xF7}, pad[4] = {0x09, 0x90, 0x18, 0x00}, other[4] = {0x07, 0x0F, 0x01, 0xF7};
    uint8_t sent[12];
    int mode = -1;
    CHECK(jlx_programmer_ack(on, &mode) && mode == 1);
    CHECK(jlx_programmer_ack(off, &mode) && mode == 0);
    CHECK(jlx_programmer_ack(cable2, &mode) && mode == 1);       /* any cable */
    CHECK(jlx_programmer_ack(on, NULL));                         /* the mode is optional */
    CHECK(!jlx_programmer_ack(first, &mode));                    /* the first packet of the same SysEx is not its end */
    CHECK(!jlx_programmer_ack(bad_mode, &mode) && !jlx_programmer_ack(bad_end, &mode) && !jlx_programmer_ack(not_end, &mode));
    CHECK(!jlx_programmer_ack(pad, &mode) && !jlx_programmer_ack(other, &mode));
    CHECK(jlx_programmer(0, 1, sent) == 12 && jlx_programmer_ack(sent + 8, &mode) && mode == 1);   /* our own message ends the way its answer does */
}

int main(void)
{
    test_programmer_ack();
    test_decode_pads();
    test_decode_not_pads();
    test_decode_buttons();
    test_numbers_round_trip();
    test_programmer_sysex();
    test_diff_nothing_to_send();
    test_diff_static_pad_and_button();
    test_diff_off_is_static_zero();
    test_diff_flash_and_pulse();
    test_diff_limit_and_resume();
    test_diff_flash_needs_two_slots();
    test_forget_sends_all();
    printf("test_lpx: %d checks, %d failed\n", checks, fails);
    return fails;
}
