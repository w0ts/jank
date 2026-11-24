/* SPDX-License-Identifier: GPL-3.0-only */
/* SHA-256 (FIPS 180-4) and HMAC-SHA-256 (RFC 2104), small and dependency-free, for the signed Wi-Fi update (jank_wota.h): the same
 * code runs in the firmware and in the host tests, which check it against the published vectors (tests/test_sha256.c). Pure. */
#ifndef JANK_SHA256_H
#define JANK_SHA256_H
#include <stddef.h>
#include <stdint.h>

#define JSHA_DIGEST 32u
#define JSHA_BLOCK 64u

struct jsha {
    uint32_t h[8];
    uint64_t bits;                     /* message length so far, in bits */
    uint8_t buf[JSHA_BLOCK];
    size_t n;                          /* bytes waiting in buf */
};

void jsha_init(struct jsha *s);
void jsha_update(struct jsha *s, const uint8_t *data, size_t n);
void jsha_final(struct jsha *s, uint8_t out[JSHA_DIGEST]);

/* HMAC-SHA-256 of the two parts, one after the other (m2 may be NULL with n2 0): key of any length */
void jsha_hmac(const uint8_t *key, size_t klen, const uint8_t *m1, size_t n1, const uint8_t *m2, size_t n2, uint8_t out[JSHA_DIGEST]);

/* 1: a and b (n bytes) are equal; the time it takes does not depend on where they differ */
int jsha_equal(const uint8_t *a, const uint8_t *b, size_t n);
#endif
