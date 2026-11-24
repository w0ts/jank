/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of the full-speed bulk size clamp (core/jank_fsclamp.c). Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_fsclamp.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

/* a configuration with one MIDI streaming interface and two bulk endpoints of the given size (the Digitone II's shape, measured:
 * mps:512/512, eps:2), preceded by a configuration descriptor header; returns the length */
static size_t config(uint8_t *b, unsigned out_mps, unsigned in_mps)
{
    static const uint8_t head[] = {9, 2, 0, 0, 1, 1, 0, 0x80, 50,                  /* configuration */
                                   9, 4, 1, 0, 2, 1, 3, 0, 0};                     /* interface 1: audio class, MIDI streaming, 2 endpoints */
    size_t n = sizeof head;
    memcpy(b, head, n);
    b[n++] = 7; b[n++] = 5; b[n++] = 0x01; b[n++] = 2; b[n++] = (uint8_t)(out_mps & 0xFF); b[n++] = (uint8_t)(out_mps >> 8); b[n++] = 0;
    b[n++] = 5; b[n++] = 0x25; b[n++] = 1; b[n++] = 1; b[n++] = 1;                  /* a class-specific endpoint descriptor between them */
    b[n++] = 7; b[n++] = 5; b[n++] = 0x81; b[n++] = 2; b[n++] = (uint8_t)(in_mps & 0xFF); b[n++] = (uint8_t)(in_mps >> 8); b[n++] = 0;
    b[2] = (uint8_t)n;
    return n;
}

static unsigned mps_at(const uint8_t *b, size_t n, unsigned which)   /* the which-th endpoint's size */
{
    size_t off = 0;
    while (off + 2u <= n && b[off] >= 2u) {
        if (b[off + 1] == 5 && which-- == 0)
            return (unsigned)(b[off + 4] | (b[off + 5] << 8));
        off += b[off];
    }
    return 0xFFFFu;
}

static void test_the_digitone_shape(void)
{
    uint8_t b[64];
    size_t n = config(b, 512, 512);
    CHECK(jfc_clamp_fs_bulk(b, n) == 2);
    CHECK(mps_at(b, n, 0) == 64 && mps_at(b, n, 1) == 64);
    CHECK(b[2] == n);                                    /* nothing else moved */
    CHECK(jfc_clamp_fs_bulk(b, n) == 0);                 /* clamping again changes nothing */
}

static void test_a_valid_descriptor_is_left_alone(void)
{
    uint8_t b[64], copy[64];
    size_t n = config(b, 64, 64);                        /* the Launchpad X */
    memcpy(copy, b, n);
    CHECK(jfc_clamp_fs_bulk(b, n) == 0);
    CHECK(!memcmp(b, copy, n));
    n = config(b, 32, 16);
    memcpy(copy, b, n);
    CHECK(jfc_clamp_fs_bulk(b, n) == 0 && !memcmp(b, copy, n));
}

static void test_only_the_oversized_one(void)
{
    uint8_t b[64];
    size_t n = config(b, 64, 512);
    CHECK(jfc_clamp_fs_bulk(b, n) == 1);
    CHECK(mps_at(b, n, 0) == 64 && mps_at(b, n, 1) == 64);
}

static void test_high_bandwidth_bits_go_with_it(void)
{
    uint8_t b[64];
    size_t n = config(b, 0x1400, 64);                    /* 1024 with the extra-transactions bits set */
    CHECK(jfc_clamp_fs_bulk(b, n) == 1);
    CHECK(mps_at(b, n, 0) == 64);
}

static void test_other_endpoint_types_are_not_touched(void)
{
    uint8_t b[64];
    size_t n = config(b, 512, 512);
    b[9 + 9 + 3] = 3;                                    /* the first endpoint becomes an interrupt one: its size is not ours to change */
    CHECK(jfc_clamp_fs_bulk(b, n) == 1);
    CHECK(mps_at(b, n, 0) == 512 && mps_at(b, n, 1) == 64);
    n = config(b, 512, 512);
    b[9 + 9 + 3] = 1;                                    /* isochronous: the same */
    CHECK(jfc_clamp_fs_bulk(b, n) == 1 && mps_at(b, n, 0) == 512);
}

static void test_malformed_input(void)
{
    uint8_t b[64];
    size_t n = config(b, 512, 512);
    CHECK(jfc_clamp_fs_bulk(NULL, 10) == 0);
    CHECK(jfc_clamp_fs_bulk(b, 0) == 0 && jfc_clamp_fs_bulk(b, 1) == 0);
    /* cut in the middle of the first endpoint: nothing is read past len, and nothing is changed */
    n = config(b, 512, 512);
    CHECK(jfc_clamp_fs_bulk(b, 9 + 9 + 4) == 0 && mps_at(b, n, 0) == 512);
    /* a zero length stops the walk: the endpoints after it stay as they were */
    n = config(b, 512, 512);
    b[9 + 9 + 7] = 0;                                    /* the class-specific descriptor's bLength */
    CHECK(jfc_clamp_fs_bulk(b, n) == 1);
    CHECK(mps_at(b, n, 0) == 64);
    /* an endpoint descriptor that claims to run past len */
    n = config(b, 512, 512);
    b[9 + 9] = 200;
    CHECK(jfc_clamp_fs_bulk(b, n) == 0);
}

int main(void)
{
    test_the_digitone_shape();
    test_a_valid_descriptor_is_left_alone();
    test_only_the_oversized_one();
    test_high_bandwidth_bits_go_with_it();
    test_other_endpoint_types_are_not_touched();
    test_malformed_input();
    printf("test_fsclamp: %d checks, %d failed\n", checks, fails);
    return fails;
}
