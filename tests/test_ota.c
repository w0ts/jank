/* SPDX-License-Identifier: GPL-3.0-only */
/* The update protocol's pure half: CRC-32, base64, line parsing, the transfer's state machine. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "jank_ota.h"

static void test_crc(void)
{
    assert(jo_crc32(0, (const uint8_t *)"123456789", 9) == 0xCBF43926u);   /* zlib's check value */
    assert(jo_crc32(0, (const uint8_t *)"", 0) == 0);
    uint32_t a = jo_crc32(0, (const uint8_t *)"1234", 4);
    assert(jo_crc32(a, (const uint8_t *)"56789", 5) == 0xCBF43926u);
}

static void test_base64(void)
{
    uint8_t out[16];
    char txt[32];
    assert(jo_b64_encode((const uint8_t *)"foobar", 6, txt, sizeof txt) == 8 && !memcmp(txt, "Zm9vYmFy", 8));
    assert(jo_b64_encode((const uint8_t *)"fooba", 5, txt, sizeof txt) == 8 && !memcmp(txt, "Zm9vYmE=", 8));
    assert(jo_b64_encode((const uint8_t *)"foob", 4, txt, sizeof txt) == 8 && !memcmp(txt, "Zm9vYg==", 8));
    assert(jo_b64_encode((const uint8_t *)"foob", 4, txt, 7) == 0);
    assert(jo_b64_decode("Zm9vYmFy", 8, out, sizeof out) == 6 && !memcmp(out, "foobar", 6));
    assert(jo_b64_decode("Zm9vYmE=", 8, out, sizeof out) == 5);
    assert(jo_b64_decode("Zm9vYg==", 8, out, sizeof out) == 4);
    assert(jo_b64_decode("Zm9vYg=", 7, out, sizeof out) == -1);       /* not a whole group */
    assert(jo_b64_decode("Zm9v=mFy", 8, out, sizeof out) == -1);       /* padding inside */
    assert(jo_b64_decode("Zm9vYg=A", 8, out, sizeof out) == -1);
    assert(jo_b64_decode("Zm9vYh==", 8, out, sizeof out) == -1);       /* stray bits */
    assert(jo_b64_decode("Zm9v!mFy", 8, out, sizeof out) == -1);
    assert(jo_b64_decode("Zm9vYmFy", 8, out, 5) == -1);                /* no room */
    assert(jo_b64_decode("", 0, out, sizeof out) == -1);
    for (size_t n = 1; n <= JO_CHUNK_MAX; ++n) {                       /* every length round-trips */
        uint8_t in[JO_CHUNK_MAX], back[JO_CHUNK_MAX];
        char t[200];
        for (size_t i = 0; i < n; ++i)
            in[i] = (uint8_t)(i * 37u + n);
        size_t k = jo_b64_encode(in, n, t, sizeof t);
        assert(k == (n + 2) / 3 * 4);
        assert(jo_b64_decode(t, k, back, sizeof back) == (int)n && !memcmp(in, back, n));
    }
}

static void test_parse(void)
{
    struct jo_cmd c;
    assert(jo_parse("get", &c) == JO_NOT_OTA);
    assert(jo_parse("otaX", &c) == JO_NOT_OTA);
    assert(jo_parse("ota", &c) == JO_BAD);
    assert(jo_parse("ota end", &c) == JO_END && jo_parse("ota abort", &c) == JO_ABORT);
    assert(jo_parse("ota end now", &c) == JO_BAD);
    assert(jo_parse("ota begin 834000 CBF43926", &c) == JO_BEGIN && c.size == 834000 && c.crc == 0xCBF43926u);
    assert(jo_parse("ota begin 834000 cbf43926", &c) == JO_BEGIN && c.crc == 0xCBF43926u);
    assert(jo_parse("ota begin 834000", &c) == JO_BAD);
    assert(jo_parse("ota begin 83x000 CBF43926", &c) == JO_BAD);
    assert(jo_parse("ota begin 99999999999 CBF43926", &c) == JO_BAD);
    assert(jo_parse("ota begin 834000 CBF4392", &c) == JO_BAD);
    assert(jo_parse("ota begin 834000 CBF43926 x", &c) == JO_BAD);
    assert(jo_parse("ota d 240 CBF43926 Zm9vYmFy", &c) == JO_DATA && c.offset == 240 && c.n == 6 && !memcmp(c.data, "foobar", 6));
    assert(jo_parse("ota d 240 CBF43926", &c) == JO_BAD);
    assert(jo_parse("ota d 240 CBF43926 Zm9vYmF", &c) == JO_BAD);
    assert(jo_parse("ota d 240 CBF43926 Zm9vYmFy extra", &c) == JO_BAD);
    assert(jo_parse("ota bogus", &c) == JO_BAD);
}

static void image(uint8_t *img, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        img[i] = (uint8_t)(i * 131u + 7u);
    img[0] = 0xE9; img[1] = 5; img[12] = 9; img[13] = 0;
}

static struct jo_cmd chunk(const uint8_t *img, uint32_t off, size_t n)
{
    struct jo_cmd c = {.kind = JO_DATA, .offset = off, .n = n, .crc = jo_crc32(0, img + off, n)};
    memcpy(c.data, img + off, n);
    return c;
}

static void test_header(void)
{
    uint8_t h[JO_HEADER_LEN];
    image(h, sizeof h);
    assert(jo_header_ok(h, sizeof h));
    assert(!jo_header_ok(h, sizeof h - 1));
    h[0] = 0xE8; assert(!jo_header_ok(h, sizeof h)); h[0] = 0xE9;
    h[1] = 0; assert(!jo_header_ok(h, sizeof h)); h[1] = 17; assert(!jo_header_ok(h, sizeof h)); h[1] = 5;
    h[12] = 5; assert(!jo_header_ok(h, sizeof h));          /* an ESP32's image */
}

static void test_transfer(void)
{
    enum { SIZE = 1000 };
    static uint8_t img[SIZE];
    struct jo j;
    struct jo_cmd c;
    image(img, SIZE);
    jo_init(&j);
    assert(jo_data(&j, &(struct jo_cmd){0}, 0) == JO_E_STATE && jo_end(&j) == JO_E_STATE);
    assert(jo_begin(&j, 0, 0, 4096, 0) == JO_E_SIZE && jo_begin(&j, 4097, 0, 4096, 0) == JO_E_SIZE && jo_begin(&j, 10, 0, 4096, 0) == JO_E_SIZE);
    assert(jo_begin(&j, SIZE, jo_crc32(0, img, SIZE), 4096, 100) == JO_OK);
    assert(jo_begin(&j, SIZE, 0, 4096, 100) == JO_E_BUSY);
    c = chunk(img, 120, 120);
    assert(jo_data(&j, &c, 101) == JO_E_GAP && j.offset == 0);                /* a lost chunk: resend from the offset */
    c = chunk(img, 0, 120);
    c.data[5] ^= 1;
    assert(jo_data(&j, &c, 102) == JO_E_CRC && j.offset == 0);                /* damaged in transit */
    c = chunk(img, 0, 120);
    assert(jo_data(&j, &c, 103) == JO_OK && j.offset == 120);
    assert(jo_data(&j, &c, 104) == JO_OK_DUP && j.offset == 120);             /* the ack was lost: sent again */
    assert(jo_end(&j) == JO_E_SHORT && j.active);
    for (uint32_t off = 120; off < SIZE; off += 120) {
        size_t n = SIZE - off < 120 ? SIZE - off : 120;
        c = chunk(img, off, n);
        assert(jo_data(&j, &c, 105 + off) == JO_OK);
    }
    assert(j.offset == SIZE);
    assert(jo_end(&j) == JO_OK && !j.active);
    assert(jo_end(&j) == JO_E_STATE);
}

static void test_failures(void)
{
    uint8_t img[400];
    struct jo j;
    struct jo_cmd c;
    image(img, sizeof img);
    jo_init(&j);
    assert(jo_begin(&j, 400, jo_crc32(0, img, 400), 4096, 0) == JO_OK);
    img[0] = 0x00;                                                            /* not an image */
    c = chunk(img, 0, 120);
    assert(jo_data(&j, &c, 1) == JO_E_HEADER && j.offset == 0 && j.active);
    jo_abort(&j);
    assert(!j.active);
    /* the chunks run past the size: the transfer is dropped */
    image(img, sizeof img);
    assert(jo_begin(&j, 100, 0, 4096, 0) == JO_OK);
    c = chunk(img, 0, 120);
    assert(jo_data(&j, &c, 1) == JO_E_SIZE && !j.active);
    /* the image's CRC is wrong */
    assert(jo_begin(&j, 120, 1234, 4096, 0) == JO_OK);
    c = chunk(img, 0, 120);
    assert(jo_data(&j, &c, 1) == JO_OK && jo_end(&j) == JO_E_CRC && !j.active);
    /* a silent sender */
    assert(jo_begin(&j, 120, 0, 4096, 1000) == JO_OK);
    assert(jo_idle(&j, 1000 + JO_IDLE_MS - 1) == JO_OK && j.active);
    assert(jo_idle(&j, 1000 + JO_IDLE_MS) == JO_E_IDLE && !j.active);
    assert(jo_idle(&j, 1000 + 2 * JO_IDLE_MS) == JO_OK);
    /* the clock wraps */
    assert(jo_begin(&j, 120, 0, 4096, 0xFFFFFF00u) == JO_OK);
    assert(jo_idle(&j, 100) == JO_OK && j.active);
}

static void test_reply(void)
{
    char out[40];
    assert(jo_reply(JO_OK, 120, out, sizeof out) == 7 && !strcmp(out, "ok 120\n"));
    assert(jo_reply(JO_OK_DUP, 120, out, sizeof out) == 7 && !strcmp(out, "ok 120\n"));
    assert(jo_reply(JO_E_GAP, 240, out, sizeof out) && !strcmp(out, "err gap 240\n"));
    assert(jo_reply(JO_E_CRC, 0, out, sizeof out) && !strcmp(out, "err crc 0\n"));
    assert(jo_reply(JO_E_ARG, 0, out, 4) == 0);
}

/* UPDATE.TXT at a start, from the note kept in flash across the restart */
static void test_note(void)
{
    char out[160];
    /* the new image runs, not confirmed yet: say it, keep the note (a crash would roll back) */
    assert(jo_note_at_start("ota_1", "ota_1", 1, "installed, restarted", out, sizeof out) == 0);
    assert(!strcmp(out, "installed, restarted"));
    /* confirmed (or a refusal on the same image): say it and forget */
    assert(jo_note_at_start("ota_1", "ota_1", 0, "jank.bin not installed: too big", out, sizeof out) == 1);
    assert(!strcmp(out, "jank.bin not installed: too big"));
    /* the old image after a rollback */
    assert(jo_note_at_start("ota_1", "ota_0", 0, "installed, restarted", out, sizeof out) == 1);
    assert(strstr(out, "did not start") && strstr(out, "rolled back"));
    /* a short buffer still ends in a NUL */
    assert(jo_note_at_start("ota_1", "ota_0", 0, "x", out, 8) == 1 && strlen(out) == 7);
}

int main(void)
{
    test_crc();
    test_base64();
    test_parse();
    test_header();
    test_transfer();
    test_failures();
    test_reply();
    test_note();
    puts("ota: ok");
    return 0;
}
