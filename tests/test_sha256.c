/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tests of SHA-256 and HMAC-SHA-256 (core/jank_sha256.c) against the published vectors (FIPS 180-4 examples, RFC 4231).
 * Exit status: the number of failed checks. */
#include <stdio.h>
#include <string.h>
#include "jank_sha256.h"

static int fails, checks;
#define CHECK(c)                                                  \
    do {                                                          \
        checks++;                                                 \
        if (!(c)) {                                               \
            printf("FAIL %s:%d: %s\n", __func__, __LINE__, #c);   \
            fails++;                                              \
        }                                                         \
    } while (0)

static void hex(const uint8_t *d, size_t n, char *out)
{
    static const char H[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[2 * i] = H[d[i] >> 4];
        out[2 * i + 1] = H[d[i] & 15];
    }
    out[2 * n] = 0;
}

static void sha(const void *msg, size_t n, char out[65])
{
    struct jsha s;
    uint8_t d[JSHA_DIGEST];
    jsha_init(&s);
    jsha_update(&s, msg, n);
    jsha_final(&s, d);
    hex(d, sizeof d, out);
}

static void test_sha256_vectors(void)
{
    char h[65];
    sha("", 0, h);
    CHECK(!strcmp(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    sha("abc", 3, h);
    CHECK(!strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, h);
    CHECK(!strcmp(h, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    sha("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 112, h);
    CHECK(!strcmp(h, "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"));
}

static void test_sha256_pieces_and_a_million_a(void)
{
    struct jsha s;
    uint8_t d[JSHA_DIGEST], block[1000];
    char h[65];
    jsha_init(&s);                                   /* one message in uneven pieces gives the same digest */
    jsha_update(&s, (const uint8_t *)"a", 1);
    jsha_update(&s, (const uint8_t *)"bc", 2);
    jsha_final(&s, d);
    hex(d, sizeof d, h);
    CHECK(!strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    memset(block, 'a', sizeof block);                /* FIPS 180-4: one million 'a' */
    jsha_init(&s);
    for (unsigned i = 0; i < 1000; i++)
        jsha_update(&s, block, sizeof block);
    jsha_final(&s, d);
    hex(d, sizeof d, h);
    CHECK(!strcmp(h, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

static void mac(const uint8_t *key, size_t kl, const void *m, size_t n, char out[65])
{
    uint8_t d[JSHA_DIGEST];
    jsha_hmac(key, kl, m, n, NULL, 0, d);
    hex(d, sizeof d, out);
}

static void test_hmac_rfc4231(void)
{
    uint8_t key[131];
    char h[65];
    memset(key, 0x0b, 20);                           /* test case 1 */
    mac(key, 20, "Hi There", 8, h);
    CHECK(!strcmp(h, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
    mac((const uint8_t *)"Jefe", 4, "what do ya want for nothing?", 28, h);            /* test case 2 */
    CHECK(!strcmp(h, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
    memset(key, 0xaa, 20);                           /* test case 3 */
    {
        uint8_t data[50];
        memset(data, 0xdd, sizeof data);
        mac(key, 20, data, sizeof data, h);
        CHECK(!strcmp(h, "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"));
    }
    memset(key, 0xaa, 131);                          /* test case 6: a key longer than a block */
    mac(key, 131, "Test Using Larger Than Block-Size Key - Hash Key First", 54, h);
    CHECK(!strcmp(h, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));
}

static void test_hmac_two_parts_and_equal(void)
{
    uint8_t d1[JSHA_DIGEST], d2[JSHA_DIGEST];
    jsha_hmac((const uint8_t *)"key", 3, (const uint8_t *)"abc", 3, (const uint8_t *)"def", 3, d1);
    jsha_hmac((const uint8_t *)"key", 3, (const uint8_t *)"abcdef", 6, NULL, 0, d2);
    CHECK(!memcmp(d1, d2, sizeof d1));               /* two parts are one message */
    CHECK(jsha_equal(d1, d2, sizeof d1));
    d2[31] ^= 1;
    CHECK(!jsha_equal(d1, d2, sizeof d1));
    d2[31] ^= 1;
    d2[0] ^= 0x80;
    CHECK(!jsha_equal(d1, d2, sizeof d1));
    CHECK(jsha_equal(d1, d2, 0));                    /* nothing to compare is equal */
}

int main(void)
{
    test_sha256_vectors();
    test_sha256_pieces_and_a_million_a();
    test_hmac_rfc4231();
    test_hmac_two_parts_and_equal();
    printf("test_sha256: %d checks, %d failed\n", checks, fails);
    return fails;
}
