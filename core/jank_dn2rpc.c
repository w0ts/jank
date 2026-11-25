/* SPDX-License-Identifier: GPL-3.0-only */
/* The dn2-wishes test channel's framing, PEEK, and SysEx over USB-MIDI packets (jank_dn2rpc.h). */
#include "jank_dn2rpc.h"
#include <string.h>

#define SX_START 0xF0u
#define SX_END 0xF7u
#define REPLY_BIT 0x80u
#define BODY_MIN 4u                    /* u16 seq, u8 cmd, u8 status */
#define CIN_SYSEX_MORE 0x4u            /* three bytes, more to come */
#define CIN_SYSEX_END1 0x5u            /* the last 1 / 2 / 3 bytes: 5 / 6 / 7 */
#define CIN_SYSEX_END3 0x7u

static const uint8_t HEADER[6] = {0xF0, 0x00, 0x20, 0x3C, 0x7D, 0x01};

size_t jd2_pack7(const uint8_t *in, size_t n, uint8_t *out)
{
    size_t o = 0;
    for (size_t i = 0; i < n; i += 7u) {
        size_t g = n - i < 7u ? n - i : 7u, hdr_at = o++;
        uint8_t hdr = 0;
        for (size_t k = 0; k < g; k++) {
            if (in[i + k] & 0x80u)
                hdr |= (uint8_t)(1u << (6u - k));
            out[o++] = in[i + k] & 0x7Fu;
        }
        out[hdr_at] = hdr;
    }
    return o;
}

size_t jd2_unpack7(const uint8_t *in, size_t n, uint8_t *out)
{
    size_t i = 0, o = 0;
    while (i < n) {
        uint8_t hdr = in[i++];
        for (unsigned k = 1; k < 8u && i < n; k++, i++)
            out[o++] = (uint8_t)(in[i] | ((hdr << k) & 0x80u));
    }
    return o;
}

size_t jd2_request(uint16_t seq, uint8_t cmd, const uint8_t *args, size_t nargs, uint8_t *out, size_t cap)
{
    uint8_t body[3u + JD2_ARGS_MAX];
    size_t n, blen = 3u + nargs;
    if (nargs > JD2_ARGS_MAX || (nargs && !args))
        return 0;
    body[0] = (uint8_t)(seq >> 8);
    body[1] = (uint8_t)seq;
    body[2] = cmd;
    if (nargs)
        memcpy(body + 3, args, nargs);
    n = sizeof HEADER + blen + (blen + 6u) / 7u + 1u;
    if (cap < n)
        return 0;
    memcpy(out, HEADER, sizeof HEADER);
    n = sizeof HEADER;
    n += jd2_pack7(body, blen, out + n);
    out[n++] = SX_END;
    return n;
}

size_t jd2_peek_request(uint16_t seq, uint32_t address, uint8_t len, uint8_t *out, size_t cap)
{
    const uint8_t body[8] = {
        (uint8_t)(seq >> 8), (uint8_t)seq, JD2_CMD_PEEK,
        (uint8_t)(address >> 24), (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address, len,
    };
    size_t n = sizeof HEADER + sizeof body + (sizeof body + 6u) / 7u + 1u;
    if (len < 1u || len > JD2_PEEK_MAX || cap < n)
        return 0;
    memcpy(out, HEADER, sizeof HEADER);
    n = sizeof HEADER;
    n += jd2_pack7(body, sizeof body, out + n);
    out[n++] = SX_END;
    return n;
}

int jd2_parse_reply(const uint8_t *sx, size_t n, uint8_t *scratch, size_t cap, struct jd2_reply *r)
{
    size_t body_n, packed;
    if (n < sizeof HEADER + 1u || memcmp(sx, HEADER, sizeof HEADER) || sx[n - 1u] != SX_END)
        return 0;
    packed = n - sizeof HEADER - 1u;
    if (cap < packed)
        return 0;
    body_n = jd2_unpack7(sx + sizeof HEADER, packed, scratch);
    if (body_n < BODY_MIN || !(scratch[2] & REPLY_BIT))
        return 0;
    r->seq = (uint16_t)((scratch[0] << 8) | scratch[1]);
    r->cmd = scratch[2] & (uint8_t)~REPLY_BIT;
    r->status = scratch[3];
    r->data = scratch + BODY_MIN;
    r->n = body_n - BODY_MIN;
    return 1;
}

int jd2_peek_data(const struct jd2_reply *r, uint32_t *address, const uint8_t **bytes, size_t *n)
{
    if (r->n < 4u)
        return 0;
    *address = ((uint32_t)r->data[0] << 24) | ((uint32_t)r->data[1] << 16) | ((uint32_t)r->data[2] << 8) | r->data[3];
    *bytes = r->data + 4;
    *n = r->n - 4u;
    return 1;
}

size_t jd2_sysex_packet(uint8_t cable, const uint8_t *sx, size_t n, size_t k, uint8_t out[4])
{
    size_t i = 3u * k, left, take;
    if (i >= n)
        return 0;
    left = n - i;
    take = left > 3u ? 3u : left;
    out[0] = (uint8_t)((cable << 4) | (left > 3u ? CIN_SYSEX_MORE : CIN_SYSEX_END1 + (take - 1u)));
    out[1] = sx[i];
    out[2] = take > 1u ? sx[i + 1] : 0;
    out[3] = take > 2u ? sx[i + 2] : 0;
    return 1;
}

size_t jd2_sysex_to_packets(uint8_t cable, const uint8_t *sx, size_t n, uint8_t *out, size_t cap)
{
    size_t k = 0;
    if (!n || cap < 4u * ((n + 2u) / 3u))
        return 0;
    while (jd2_sysex_packet(cable, sx, n, k, out + 4u * k))
        k++;
    return 4u * k;
}

void jd2_sx_init(struct jd2_sx *s) { memset(s, 0, sizeof *s); }

int jd2_sx_feed(struct jd2_sx *s, const uint8_t pkt[4])
{
    uint8_t cin = pkt[0] & 0x0Fu;
    size_t take;
    if (s->done) {
        s->done = 0;
        s->active = 0;
        s->n = 0;
    }
    if (cin < CIN_SYSEX_MORE || cin > CIN_SYSEX_END3)
        return 0;                      /* not a SysEx packet: notes, the clock, ... */
    take = cin == CIN_SYSEX_MORE ? 3u : (size_t)(cin - CIN_SYSEX_END1 + 1u);
    if (pkt[1] == SX_START) {          /* a start, also inside a SysEx: the earlier one was cut */
        s->active = 1;
        s->n = 0;
    }
    if (!s->active)
        return 0;
    if (s->n + take > JD2_SX_MAX) {    /* too long for us: dropped whole, up to its end */
        s->active = 0;
        s->n = 0;
        return 0;
    }
    memcpy(s->buf + s->n, pkt + 1, take);
    s->n += take;
    if (cin != CIN_SYSEX_MORE) {
        s->done = 1;
        return 1;
    }
    return 0;
}
