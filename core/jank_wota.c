/* SPDX-License-Identifier: GPL-3.0-only */
/* The signed Wi-Fi update frames (jank_wota.h). */
#include "jank_wota.h"
#include <string.h>
#include "jank_sha256.h"

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* mac = HMAC(key, nonce || body)[0..16] */
static void mac_of(const uint8_t key[JWO_KEY], const uint8_t nonce[JWO_NONCE], const uint8_t *body, size_t n, uint8_t out[JWO_MAC])
{
    uint8_t m[JSHA_DIGEST];
    jsha_hmac(key, JWO_KEY, nonce, JWO_NONCE, body, n, m);     /* the two parts one after the other: no copy of the frame on the stack */
    memcpy(out, m, JWO_MAC);
}

void jwo_init(struct jwo *s) { memset(s, 0, sizeof *s); }

void jwo_set_key(struct jwo *s, const uint8_t key[JWO_KEY])
{
    memcpy(s->key, key, JWO_KEY);
    s->key_set = 1;
    s->have = 0;
}

void jwo_clear_key(struct jwo *s)
{
    memset(s->key, 0, sizeof s->key);
    s->key_set = 0;
    s->have = 0;
}

/* the bytes the op's arguments take: -1 for an op that has a variable part or is not known (DATA is checked on its own) */
static int args_len(uint8_t op)
{
    switch (op) {
    case JWO_OP_BEGIN: return 9;
    case JWO_OP_END:
    case JWO_OP_ABORT:
    case JWO_OP_CONFIRM: return 0;
    default: return -1;
    }
}

/* the request's fields from a frame whose length and mac are already right; 0: the arguments do not fit the op */
static int parse_args(const uint8_t *f, size_t body, struct jwo_req *r)
{
    const uint8_t *a = f + JWO_HEAD;
    size_t room = body - JWO_HEAD;
    memset(r, 0, sizeof *r);
    r->counter = rd32(f + 2);
    r->op = f[6];
    if (r->op == JWO_OP_DATA) {
        if (room < 10u)
            return 0;
        r->offset = rd32(a);
        r->crc = rd32(a + 4);
        r->n = (uint16_t)(a[8] << 8 | a[9]);
        if (r->n > JWO_DATA_MAX || room != 10u + r->n)
            return 0;
        r->data = a + 10;
        return 1;
    }
    if (r->op == JWO_OP_BEGIN) {
        if (room != 9u)
            return 0;
        r->size = rd32(a);
        r->crc = rd32(a + 4);
        r->target = a[8];
        return 1;
    }
    return room == 0 ? 1 : 0;          /* END, ABORT, CONFIRM, and an op nobody knows (answered "bad op" if the mac was right) */
}

enum jwo_in jwo_input(struct jwo *s, const uint8_t *f, size_t n, uint32_t now_ms, const uint8_t rnd[JWO_NONCE], struct jwo_req *req, uint8_t *out, size_t cap,
                      size_t *out_len)
{
    *out_len = 0;
    if (!s->key_set || n < 2u || f[0] != JWO_MAGIC)
        return JWO_NONE;
    if (f[1] == JWO_T_HELLO) {
        if (n != JWO_HELLO_LEN || cap < JWO_HELLO_R_LEN)
            return JWO_NONE;
        if (s->have && (uint32_t)(now_ms - s->last_ms) < JWO_HELLO_LOCK_MS)
            return JWO_NONE;           /* a session in use: a HELLO from anyone else cannot take it over */
        memcpy(s->nonce, rnd, JWO_NONCE);
        s->have = 1;
        s->last_ctr = 0;
        s->last_ms = now_ms;
        out[0] = JWO_MAGIC;
        out[1] = JWO_T_HELLO_R;
        memcpy(out + 2, s->nonce, JWO_NONCE);
        *out_len = JWO_HELLO_R_LEN;
        return JWO_HELLO;
    }
    if (f[1] == JWO_T_CMD) {
        uint8_t want[JWO_MAC];
        size_t body;
        if (!s->have || n < JWO_HEAD + JWO_MAC || n > JWO_FRAME_MAX)
            return JWO_NONE;
        if ((uint32_t)(now_ms - s->last_ms) > JWO_SESSION_MS) {
            s->have = 0;
            return JWO_NONE;
        }
        body = n - JWO_MAC;
        mac_of(s->key, s->nonce, f, body, want);
        if (!jsha_equal(want, f + body, JWO_MAC))
            return JWO_NONE;           /* the mac first: nothing below is believed before it checks out */
        if (rd32(f + 2) <= s->last_ctr)
            return JWO_NONE;           /* a counter that does not rise is a replay */
        if (!parse_args(f, body, req))
            return JWO_NONE;
        s->last_ctr = req->counter;
        s->last_ms = now_ms;
        return JWO_CMD;
    }
    return JWO_NONE;
}

size_t jwo_reply(const struct jwo *s, uint32_t counter, uint8_t status, uint32_t offset, uint16_t chunk, uint8_t *out, size_t cap)
{
    if (!s->key_set || !s->have || cap < JWO_REPLY_LEN)
        return 0;
    out[0] = JWO_MAGIC;
    out[1] = JWO_T_CMD_R;
    wr32(out + 2, counter);
    out[6] = status;
    wr32(out + 7, offset);
    out[11] = (uint8_t)(chunk >> 8);
    out[12] = (uint8_t)chunk;
    mac_of(s->key, s->nonce, out, JWO_REPLY_LEN - JWO_MAC, out + JWO_REPLY_LEN - JWO_MAC);
    return JWO_REPLY_LEN;
}

size_t jwo_build(const uint8_t key[JWO_KEY], const uint8_t nonce[JWO_NONCE], const struct jwo_req *r, uint8_t *out, size_t cap)
{
    size_t n = JWO_HEAD;
    int fixed = args_len(r->op);
    if (r->op == JWO_OP_DATA) {
        if (r->n > JWO_DATA_MAX || (r->n && !r->data))
            return 0;
    } else if (fixed < 0) {
        fixed = 0;                     /* an op nobody knows: no arguments (the tests send one to see it answered "bad op") */
    }
    {
        size_t total = JWO_HEAD + (r->op == JWO_OP_DATA ? 10u + r->n : (size_t)fixed) + JWO_MAC;
        if (cap < total)
            return 0;
    }
    out[0] = JWO_MAGIC;
    out[1] = JWO_T_CMD;
    wr32(out + 2, r->counter);
    out[6] = r->op;
    if (r->op == JWO_OP_DATA) {
        wr32(out + n, r->offset);
        wr32(out + n + 4, r->crc);
        out[n + 8] = (uint8_t)(r->n >> 8);
        out[n + 9] = (uint8_t)r->n;
        n += 10;
        if (r->n)
            memcpy(out + n, r->data, r->n);
        n += r->n;
    } else if (r->op == JWO_OP_BEGIN) {
        wr32(out + n, r->size);
        wr32(out + n + 4, r->crc);
        out[n + 8] = r->target;
        n += 9;
    }
    mac_of(key, nonce, out, n, out + n);
    return n + JWO_MAC;
}

int jwo_check_reply(const uint8_t key[JWO_KEY], const uint8_t nonce[JWO_NONCE], const uint8_t *f, size_t n, uint32_t *counter, uint8_t *status,
                    uint32_t *offset, uint16_t *chunk)
{
    uint8_t want[JWO_MAC];
    if (n != JWO_REPLY_LEN || f[0] != JWO_MAGIC || f[1] != JWO_T_CMD_R)
        return 0;
    mac_of(key, nonce, f, JWO_REPLY_LEN - JWO_MAC, want);
    if (!jsha_equal(want, f + JWO_REPLY_LEN - JWO_MAC, JWO_MAC))
        return 0;
    *counter = rd32(f + 2);
    *status = f[6];
    *offset = rd32(f + 7);
    *chunk = (uint16_t)(f[11] << 8 | f[12]);
    return 1;
}
