/* SPDX-License-Identifier: GPL-3.0-only */
/* SHA-256 and HMAC-SHA-256 (jank_sha256.h). */
#include "jank_sha256.h"
#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu,
    0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau,
    0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u,
    0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }

static void block(struct jsha *s, const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (unsigned i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (unsigned i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0];
    b = s->h[1];
    c = s->h[2];
    d = s->h[3];
    e = s->h[4];
    f = s->h[5];
    g = s->h[6];
    h = s->h[7];
    for (unsigned i = 0; i < 64; i++) {
        uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}

void jsha_init(struct jsha *s)
{
    static const uint32_t H0[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    memcpy(s->h, H0, sizeof H0);
    s->bits = 0;
    s->n = 0;
}

void jsha_update(struct jsha *s, const uint8_t *data, size_t n)
{
    s->bits += (uint64_t)n * 8u;
    while (n) {
        size_t k = JSHA_BLOCK - s->n;
        if (k > n)
            k = n;
        memcpy(s->buf + s->n, data, k);
        s->n += k;
        data += k;
        n -= k;
        if (s->n == JSHA_BLOCK) {
            block(s, s->buf);
            s->n = 0;
        }
    }
}

void jsha_final(struct jsha *s, uint8_t out[JSHA_DIGEST])
{
    uint64_t bits = s->bits;
    uint8_t pad = 0x80u, zero = 0, len[8];
    jsha_update(s, &pad, 1);
    while (s->n != JSHA_BLOCK - 8u)
        jsha_update(s, &zero, 1);
    for (unsigned i = 0; i < 8; i++)
        len[i] = (uint8_t)(bits >> (56u - 8u * i));
    jsha_update(s, len, 8);
    for (unsigned i = 0; i < 8; i++) {
        out[4 * i] = (uint8_t)(s->h[i] >> 24);
        out[4 * i + 1] = (uint8_t)(s->h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(s->h[i] >> 8);
        out[4 * i + 3] = (uint8_t)s->h[i];
    }
}

void jsha_hmac(const uint8_t *key, size_t klen, const uint8_t *m1, size_t n1, const uint8_t *m2, size_t n2, uint8_t out[JSHA_DIGEST])
{
    uint8_t k[JSHA_BLOCK], ipad[JSHA_BLOCK], opad[JSHA_BLOCK], inner[JSHA_DIGEST];
    struct jsha s;
    memset(k, 0, sizeof k);
    if (klen > JSHA_BLOCK) {           /* a key longer than a block is hashed first */
        jsha_init(&s);
        jsha_update(&s, key, klen);
        jsha_final(&s, k);
    } else if (klen) {
        memcpy(k, key, klen);
    }
    for (unsigned i = 0; i < JSHA_BLOCK; i++) {
        ipad[i] = (uint8_t)(k[i] ^ 0x36u);
        opad[i] = (uint8_t)(k[i] ^ 0x5Cu);
    }
    jsha_init(&s);
    jsha_update(&s, ipad, sizeof ipad);
    if (n1)
        jsha_update(&s, m1, n1);
    if (n2)
        jsha_update(&s, m2, n2);
    jsha_final(&s, inner);
    jsha_init(&s);
    jsha_update(&s, opad, sizeof opad);
    jsha_update(&s, inner, sizeof inner);
    jsha_final(&s, out);
    memset(k, 0, sizeof k);
    memset(ipad, 0, sizeof ipad);
    memset(opad, 0, sizeof opad);
}

int jsha_equal(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++)
        d |= (uint8_t)(a[i] ^ b[i]);
    return d == 0;
}
