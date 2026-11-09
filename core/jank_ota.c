/* SPDX-License-Identifier: GPL-3.0-only */
#include "jank_ota.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t jo_crc32(uint32_t crc, const uint8_t *data, size_t n)
{
    static const uint32_t nib[16] = {0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
                                     0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        crc = (crc >> 4) ^ nib[crc & 15u];
        crc = (crc >> 4) ^ nib[crc & 15u];
    }
    return ~crc;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t jo_b64_encode(const uint8_t *in, size_t n, char *out, size_t cap)
{
    size_t at = 0;
    if (cap < (n + 2u) / 3u * 4u)
        return 0;
    for (size_t i = 0; i < n; i += 3u) {
        uint32_t v = (uint32_t)in[i] << 16 | (i + 1u < n ? (uint32_t)in[i + 1u] << 8 : 0u) | (i + 2u < n ? in[i + 2u] : 0u);
        out[at++] = B64[v >> 18 & 63u];
        out[at++] = B64[v >> 12 & 63u];
        out[at++] = i + 1u < n ? B64[v >> 6 & 63u] : '=';
        out[at++] = i + 2u < n ? B64[v & 63u] : '=';
    }
    return at;
}

static int b64_value(char c)
{
    const char *p = c ? strchr(B64, c) : NULL;
    return p ? (int)(p - B64) : -1;
}

/* one group of four characters -> up to 3 bytes; the padding only in the last group */
static int b64_group(const char *g, int last, uint8_t *out)
{
    int pad = g[2] == '=' ? (g[3] == '=' ? 2 : -1) : (g[3] == '=' ? 1 : 0);
    int v[4] = {0, 0, 0, 0};
    uint32_t w;
    if (pad < 0 || (pad && !last))
        return -1;
    for (int i = 0; i < 4 - pad; ++i)
        if ((v[i] = b64_value(g[i])) < 0)
            return -1;
    w = (uint32_t)v[0] << 18 | (uint32_t)v[1] << 12 | (uint32_t)v[2] << 6 | (uint32_t)v[3];
    if ((pad == 2 && (w & 0xFFFFu)) || (pad == 1 && (w & 0xFFu)))
        return -1;                       /* stray bits: not what an encoder writes */
    out[0] = (uint8_t)(w >> 16);
    out[1] = (uint8_t)(w >> 8);
    out[2] = (uint8_t)w;
    return 3 - pad;
}

int jo_b64_decode(const char *in, size_t len, uint8_t *out, size_t cap)
{
    size_t at = 0;
    if (!len || len % 4u)
        return -1;
    for (size_t i = 0; i < len; i += 4u) {
        uint8_t b[3];
        int k = b64_group(in + i, i + 4u == len, b);
        if (k < 0 || at + (size_t)k > cap)
            return -1;
        memcpy(out + at, b, (size_t)k);
        at += (size_t)k;
    }
    return (int)at;
}

/* a plain decimal number, nothing else */
static int num(const char *s, size_t len, uint32_t *v)
{
    uint64_t x = 0;
    if (!len || len > 10u)
        return -1;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] < '0' || s[i] > '9')
            return -1;
        x = x * 10u + (uint64_t)(s[i] - '0');
    }
    if (x > 0xFFFFFFFFu)
        return -1;
    *v = (uint32_t)x;
    return 0;
}

static int hex8(const char *s, size_t len, uint32_t *v)
{
    uint32_t x = 0;
    if (len != 8u)
        return -1;
    for (size_t i = 0; i < 8u; ++i) {
        char c = s[i];
        uint32_t d = c >= '0' && c <= '9' ? (uint32_t)(c - '0') : c >= 'a' && c <= 'f' ? (uint32_t)(c - 'a' + 10) : c >= 'A' && c <= 'F' ? (uint32_t)(c - 'A' + 10) : 16u;
        if (d > 15u)
            return -1;
        x = x << 4 | d;
    }
    *v = x;
    return 0;
}

/* the length of the word at s (up to a space or the end) */
static size_t word(const char *s)
{
    size_t n = 0;
    while (s[n] && s[n] != ' ')
        ++n;
    return n;
}

/* past n characters and the spaces after them */
static const char *next(const char *s, size_t n)
{
    s += n;
    while (*s == ' ')
        ++s;
    return s;
}

static enum jo_kind parse_begin(const char *s, struct jo_cmd *c)
{
    size_t a = word(s);
    const char *t = next(s, a);
    size_t b = word(t);
    if (num(s, a, &c->size) || hex8(t, b, &c->crc) || t[b])
        return JO_BAD;
    return JO_BEGIN;
}

static enum jo_kind parse_data(const char *s, struct jo_cmd *c)
{
    size_t a = word(s);
    const char *t = next(s, a);
    size_t b = word(t);
    const char *u = next(t, b);
    size_t d = word(u);
    int got;
    if (num(s, a, &c->offset) || hex8(t, b, &c->crc) || u[d])
        return JO_BAD;
    got = jo_b64_decode(u, d, c->data, sizeof c->data);
    if (got <= 0)
        return JO_BAD;
    c->n = (size_t)got;
    return JO_DATA;
}

enum jo_kind jo_parse(const char *line, struct jo_cmd *c)
{
    const char *s = line;
    memset(c, 0, sizeof *c);
    if (strncmp(s, "ota", 3) || (s[3] && s[3] != ' '))
        return JO_NOT_OTA;
    s = next(s, 3);
    if (!strcmp(s, "end"))
        c->kind = JO_END;
    else if (!strcmp(s, "abort"))
        c->kind = JO_ABORT;
    else if (!strncmp(s, "begin ", 6))
        c->kind = parse_begin(next(s, 5), c);
    else if (!strncmp(s, "d ", 2))
        c->kind = parse_data(next(s, 1), c);
    else
        c->kind = JO_BAD;
    return c->kind;
}

int jo_header_ok(const uint8_t *first, size_t n)
{
    uint32_t chip;
    if (n < JO_HEADER_LEN || first[0] != 0xE9u || first[1] < 1u || first[1] > 16u)
        return 0;
    chip = (uint32_t)first[12] | (uint32_t)first[13] << 8;
    return chip == JO_CHIP_ESP32S3;
}

void jo_init(struct jo *j) { memset(j, 0, sizeof *j); }

enum jo_err jo_begin(struct jo *j, uint32_t size, uint32_t crc, uint32_t slot_size, uint32_t now_ms)
{
    if (j->active)
        return JO_E_BUSY;
    if (!size || size < JO_HEADER_LEN || size > slot_size)
        return JO_E_SIZE;
    jo_init(j);
    j->active = 1;
    j->size = size;
    j->crc_want = crc;
    j->last_ms = now_ms;
    return JO_OK;
}

enum jo_err jo_data(struct jo *j, const struct jo_cmd *c, uint32_t now_ms)
{
    if (!j->active)
        return JO_E_STATE;
    j->last_ms = now_ms;
    if (c->offset < j->offset)
        return JO_OK_DUP;
    if (c->offset > j->offset)
        return JO_E_GAP;
    if (c->n > JO_CHUNK_MAX || jo_crc32(0, c->data, c->n) != c->crc)
        return JO_E_CRC;
    if (c->n > j->size - j->offset) {
        jo_abort(j);
        return JO_E_SIZE;
    }
    if (!j->offset && !jo_header_ok(c->data, c->n))
        return JO_E_HEADER;
    j->crc_run = jo_crc32(j->crc_run, c->data, c->n);
    j->offset += (uint32_t)c->n;
    return JO_OK;
}

enum jo_err jo_end(struct jo *j)
{
    enum jo_err e = JO_OK;
    if (!j->active)
        return JO_E_STATE;
    if (j->offset != j->size)
        return JO_E_SHORT;               /* the sender may go on from the offset, or abort */
    if (j->crc_run != j->crc_want)
        e = JO_E_CRC;
    j->active = 0;
    return e;
}

void jo_abort(struct jo *j) { j->active = 0; }

enum jo_err jo_idle(struct jo *j, uint32_t now_ms)
{
    if (!j->active || now_ms - j->last_ms < JO_IDLE_MS)
        return JO_OK;
    j->active = 0;
    return JO_E_IDLE;
}

static const char *const NAMES[] = {"ok", "ok", "busy", "size", "arg", "state", "gap", "crc", "header", "short", "idle"};

size_t jo_reply(enum jo_err e, uint32_t offset, char *out, size_t cap)
{
    int r;
    if ((unsigned)e >= sizeof NAMES / sizeof NAMES[0])
        e = JO_E_ARG;
    if (e == JO_OK || e == JO_OK_DUP)
        r = snprintf(out, cap, "ok %lu\n", (unsigned long)offset);
    else
        r = snprintf(out, cap, "err %s %lu\n", NAMES[e], (unsigned long)offset);
    return r > 0 && (size_t)r < cap ? (size_t)r : 0;
}
