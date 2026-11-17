/* SPDX-License-Identifier: GPL-3.0-only */
/* The SETUP screen's QR code: the emulator's minimal encoder (byte mode, error correction L, versions 1-5, one block
 * each), its mask choice by penalty included, so the modules are the same as the page's. */
#include "jank_oled.h"
#include <stdlib.h>
#include <string.h>

static uint8_t EXP[512], LOG[256];
static int s_tables;

static void tables(void)
{
    int x = 1;
    if (s_tables)
        return;
    for (int i = 0; i < 255; i++) {
        EXP[i] = (uint8_t)x;
        LOG[x] = (uint8_t)i;
        x <<= 1;
        if (x & 256)
            x ^= 0x11d;
    }
    for (int i = 255; i < 512; i++)
        EXP[i] = EXP[i - 255];
    s_tables = 1;
}

static uint8_t mul(uint8_t a, uint8_t b) { return a && b ? EXP[LOG[a] + LOG[b]] : 0; }

/* the remainder of data(x) * x^n by the generator polynomial */
static void rs_encode(const uint8_t *data, int nd, int n, uint8_t *res)
{
    uint8_t g[32] = {1}, ng[32];
    int gl = 1;
    for (int i = 0; i < n; i++) {
        memset(ng, 0, sizeof ng);
        for (int j = 0; j < gl; j++) {
            ng[j] ^= g[j];
            ng[j + 1] ^= mul(g[j], EXP[i]);
        }
        gl++;
        memcpy(g, ng, sizeof g);
    }
    memset(res, 0, (size_t)n);
    for (int k = 0; k < nd; k++) {
        uint8_t f = data[k] ^ res[0];
        memmove(res, res + 1, (size_t)n - 1u);
        res[n - 1] = 0;
        for (int i = 0; i < n; i++)
            res[i] ^= mul(g[i + 1], f);
    }
}

static const uint8_t CAP[6][2] = {{0, 0}, {19, 7}, {34, 10}, {55, 15}, {80, 20}, {108, 26}};   /* data, ecc codewords */

static int codewords(const uint8_t *bytes, int len, int version, uint8_t *cw)
{
    int nd = CAP[version][0], ne = CAP[version][1], nbits = 0;
    uint8_t bits[108 * 8];
    memset(bits, 0, sizeof bits);
#define PUT(v, n)                                       \
    for (int i_ = (n) - 1; i_ >= 0; i_--)              \
        bits[nbits++] = (uint8_t)(((v) >> i_) & 1);
    PUT(4, 4);
    PUT(len, 8);
    for (int i = 0; i < len; i++) {
        PUT(bytes[i], 8);
    }
#undef PUT
    for (int i = 0; i < 4 && nbits < nd * 8; i++)
        bits[nbits++] = 0;
    while (nbits % 8)
        bits[nbits++] = 0;
    for (int i = 0; i < nbits; i += 8) {
        uint8_t b = 0;
        for (int k = 0; k < 8; k++)
            b = (uint8_t)(b << 1 | bits[i + k]);
        cw[i / 8] = b;
    }
    for (int i = nbits / 8, pad = 0xec; i < nd; i++, pad ^= 0xec ^ 0x11)
        cw[i] = (uint8_t)pad;
    rs_encode(cw, nd, ne, cw + nd);
    return nd + ne;
}

struct grid {
    int size;
    uint8_t mod[JO_QR_MAX][JO_QR_MAX], fn[JO_QR_MAX][JO_QR_MAX];
};

static void set(struct grid *q, int x, int y, int v)
{
    if (x >= 0 && x < q->size && y >= 0 && y < q->size) {
        q->mod[y][x] = (uint8_t)(v != 0);
        q->fn[y][x] = 1;
    }
}

static int masked(int mask, int x, int y)
{
    switch (mask) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (x / 3 + y / 2) % 2 == 0;
    case 5: return (x * y) % 2 + (x * y) % 3 == 0;
    case 6: return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
    default: return ((x + y) % 2 + (x * y) % 3) % 2 == 0;
    }
}

static void patterns(struct grid *q, int version, int mask)
{
    int size = q->size, data = (1 << 3) | mask, rem = data, f;
    const int C[3][2] = {{3, 3}, {size - 4, 3}, {3, size - 4}};
    for (int i = 0; i < size; i++) {
        set(q, 6, i, i % 2 == 0);
        set(q, i, 6, i % 2 == 0);
    }
    for (int k = 0; k < 3; k++)
        for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4; dx <= 4; dx++) {
                int d = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
                set(q, C[k][0] + dx, C[k][1] + dy, d != 2 && d != 4);
            }
    if (version >= 2) {
        int c = size - 7;
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++)
                set(q, c + dx, c + dy, (abs(dx) > abs(dy) ? abs(dx) : abs(dy)) != 1);
    }
    for (int i = 0; i < 10; i++)                      /* level L = 01 */
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    f = ((data << 10) | rem) ^ 0x5412;
#define BIT(i) (((f >> (i)) & 1) != 0)
    for (int i = 0; i <= 5; i++)
        set(q, 8, i, BIT(i));
    set(q, 8, 7, BIT(6));
    set(q, 8, 8, BIT(7));
    set(q, 7, 8, BIT(8));
    for (int i = 9; i < 15; i++)
        set(q, 14 - i, 8, BIT(i));
    for (int i = 0; i < 8; i++)
        set(q, size - 1 - i, 8, BIT(i));
    for (int i = 8; i < 15; i++)
        set(q, 8, size - 15 + i, BIT(i));
#undef BIT
    set(q, 8, size - 8, 1);
}

static void build(struct grid *q, int version, const uint8_t *cw, int ncw, int mask)
{
    int size = 17 + 4 * version, k = 0;
    memset(q, 0, sizeof *q);
    q->size = size;
    patterns(q, version, mask);
    for (int right = size - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5;
        for (int vert = 0; vert < size; vert++)
            for (int j = 0; j < 2; j++) {
                int x = right - j, y = ((right + 1) & 2) == 0 ? size - 1 - vert : vert;
                if (!q->fn[y][x] && k < ncw * 8) {
                    q->mod[y][x] = (uint8_t)((cw[k >> 3] >> (7 - (k & 7))) & 1);
                    k++;
                }
            }
    }
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            if (!q->fn[y][x] && masked(mask, x, y))
                q->mod[y][x] ^= 1;
}

/* the line's runs of 5+ (3 + the length over 5 each) and its 1:1:3:1:1 finder look-alikes (40 each, overlapping) */
static int line_penalty(const uint8_t *l, int n)
{
    static const uint8_t A[11] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0}, B[11] = {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1};
    int p = 0;
    for (int i = 0; i < n;) {
        int j = i;
        while (j < n && l[j] == l[i])
            j++;
        if (j - i >= 5)
            p += 3 + (j - i) - 5;
        i = j;
    }
    for (int i = 0; i + 11 <= n; i++)
        if (!memcmp(l + i, A, 11) || !memcmp(l + i, B, 11))
            p += 40;
    return p;
}

static int penalty(const struct grid *q)
{
    int size = q->size, p = 0, dark = 0, a;
    uint8_t col[JO_QR_MAX];
    for (int i = 0; i < size; i++) {
        p += line_penalty(q->mod[i], size);
        for (int y = 0; y < size; y++)
            col[y] = q->mod[y][i];
        p += line_penalty(col, size);
    }
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            uint8_t m = q->mod[y][x];
            dark += m;
            if (x < size - 1 && y < size - 1 && m == q->mod[y][x + 1] && m == q->mod[y + 1][x] && m == q->mod[y + 1][x + 1])
                p += 3;
        }
    a = abs(dark * 20 - size * size * 10);
    return p + 10 * ((a + size * size - 1) / (size * size) - 1);
}

int jo_qr_encode(const char *text, struct jo_qr *out)
{
    static struct grid q, best;
    uint8_t cw[134];
    int len = (int)strlen(text), version = 0, ncw, best_p = 0;
    tables();
    memset(out, 0, sizeof *out);
    for (int v = 1; v <= 5 && !version; v++)
        if (len <= CAP[v][0] - 2)
            version = v;
    if (!version)
        return -1;
    ncw = codewords((const uint8_t *)text, len, version, cw);
    for (int m = 0; m < 8; m++) {
        int p;
        build(&q, version, cw, ncw, m);
        p = penalty(&q);
        if (m == 0 || p < best_p) {
            best_p = p;
            best = q;
        }
    }
    out->size = (uint8_t)best.size;
    for (int y = 0; y < best.size; y++)
        for (int x = 0; x < best.size; x++)
            if (best.mod[y][x])
                out->mod[y][x >> 3] |= (uint8_t)(1u << (x & 7));
    return 0;
}
