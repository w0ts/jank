/* SPDX-License-Identifier: GPL-3.0-only */
/* The control line in SysEx on the S3's USB MIDI port: framing and reading back. Exit status: non-zero on a failure. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "jank_sysex.h"

static size_t feed(struct jx_in *in, const uint8_t *data, size_t n, char *out)
{
    size_t got = 0;
    for (size_t i = 0; i < n; ++i) {
        int c = jx_feed(in, data[i]);
        if (c >= 0)
            out[got++] = (char)c;
    }
    return got;
}

static void test_round_trip(void)
{
    uint8_t wire[JX_FRAME_MAX];
    char out[JX_TEXT_MAX];
    struct jx_in in;
    size_t n = jx_frame("get\n", 4, wire);
    assert(n == 9);
    assert(wire[0] == 0xF0 && wire[1] == 0x7D && wire[2] == 0x4A && wire[3] == 0x4B && wire[8] == 0xF7);
    jx_init(&in);
    assert(feed(&in, wire, n, out) == 4 && !memcmp(out, "get\n", 4));
}

static void test_no_high_bit_on_the_wire(void)
{
    uint8_t wire[JX_FRAME_MAX];
    size_t n = jx_frame("a\xC3" "b", 3, wire);
    assert(n == 8 && wire[4] == 'a' && wire[5] == '?' && wire[6] == 'b');
}

static void test_clock_inside_a_frame_is_no_text(void)
{
    const uint8_t wire[] = {0xF0, 0x7D, 0xF8, 0x4A, 0x4B, 'o', 0xF8, 'k', 0xFA, 0xF7};
    char out[8];
    struct jx_in in;
    jx_init(&in);
    assert(feed(&in, wire, sizeof wire, out) == 2 && !memcmp(out, "ok", 2));
}

static void test_other_midi_is_ignored(void)
{
    const uint8_t other[] = {0x90, 0x3C, 0x40, 0xF0, 0x7D, 0x41, 0x42, 'x', 0xF7, 'y', 0xF0, 0x41, 'z', 0xF7};
    char out[8];
    struct jx_in in;
    jx_init(&in);
    assert(feed(&in, other, sizeof other, out) == 0);
}

static void test_a_cut_frame_ends_at_the_next_status(void)
{
    const uint8_t wire[] = {0xF0, 0x7D, 0x4A, 0x4B, 'a', 0x90, 'b', 0xF0, 0x7D, 0x4A, 0x4B, 'c', 0xF7};
    char out[8];
    struct jx_in in;
    jx_init(&in);
    assert(feed(&in, wire, sizeof wire, out) == 2 && !memcmp(out, "ac", 2));
}

static void test_clock_as_usb_midi_packets(void)
{
    const uint8_t bytes[] = {0xFA, 0xF8, 0xFC};
    uint8_t p[12];
    assert(jx_realtime_packets(bytes, 3, p, sizeof p) == 12);
    assert(!memcmp(p, "\x0F\xFA\0\0\x0F\xF8\0\0\x0F\xFC\0\0", 12));
    assert(jx_realtime_packets(bytes, 3, p, 8) == 8);    /* whole packets that fit */
    assert(jx_realtime_packets(bytes, 3, p, 3) == 0);
}

int main(void)
{
    test_round_trip();
    test_no_high_bit_on_the_wire();
    test_clock_inside_a_frame_is_no_text();
    test_other_midi_is_ignored();
    test_a_cut_frame_ends_at_the_next_status();
    test_clock_as_usb_midi_packets();
    puts("sysex: ok");
    return 0;
}
