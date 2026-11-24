/* SPDX-License-Identifier: GPL-3.0-only */
/* Novation Launchpad X in Programmer mode (jank_lpx.h). */
#include "jank_lpx.h"
#include <string.h>

#define CIN_NOTE_ON 0x9u
#define CIN_CC 0xBu
#define CIN_SYSEX_CONT 0x4u            /* a SysEx packet carrying three bytes, more to come */
#define CIN_SYSEX_END3 0x7u            /* a SysEx packet carrying the last three bytes */
#define UNKNOWN 0xFFu                  /* a light the Launchpad may show anything on */

uint8_t jlx_number(uint32_t i)
{
    if (i < JLX_TOP0)
        return (uint8_t)(10u * (i / 8u + 1u) + i % 8u + 1u);
    if (i < JLX_SIDE0)
        return (uint8_t)(91u + (i - JLX_TOP0));
    if (i < JLX_LOGO)
        return (uint8_t)(89u - 10u * (i - JLX_SIDE0));
    return 99u;
}

int jlx_decode(const uint8_t msg[3], struct jlx_ev *ev)
{
    uint8_t kind = (uint8_t)(msg[0] & 0xF0u), n = msg[1], v = msg[2];
    memset(ev, 0, sizeof *ev);
    if (kind == 0x80u || kind == 0x90u) {
        uint8_t row = (uint8_t)(n / 10u), col = (uint8_t)(n % 10u);
        if (row < 1u || row > 8u || col < 1u || col > 8u)
            return 0;
        ev->kind = JLX_PAD;
        ev->row = (uint8_t)(row - 1u);
        ev->col = (uint8_t)(col - 1u);
        ev->value = kind == 0x80u ? 0u : v;
        return 1;
    }
    if (kind == 0xB0u) {
        if (n >= 91u && n <= 98u) {
            ev->kind = JLX_TOP;
            ev->idx = (uint8_t)(n - 91u);
        } else if (n >= 19u && n <= 89u && n % 10u == 9u) {
            ev->kind = JLX_SIDE;
            ev->idx = (uint8_t)((89u - n) / 10u);
        } else {
            return 0;
        }
        ev->value = v;
        return 1;
    }
    return 0;
}

size_t jlx_programmer(uint8_t cable, int on, uint8_t *out)
{
    const uint8_t hi = (uint8_t)(cable << 4);
    const uint8_t p[12] = {
        (uint8_t)(hi | CIN_SYSEX_CONT), 0xF0, 0x00, 0x20,
        (uint8_t)(hi | CIN_SYSEX_CONT), 0x29, 0x02, 0x0C,
        (uint8_t)(hi | CIN_SYSEX_END3), 0x0E, (uint8_t)(on ? 1 : 0), 0xF7,
    };
    memcpy(out, p, sizeof p);
    return sizeof p;
}

int jlx_programmer_ack(const uint8_t pkt[4], int *mode)
{
    if ((pkt[0] & 0x0Fu) != CIN_SYSEX_END3 || pkt[1] != 0x0E || pkt[2] > 1u || pkt[3] != 0xF7)
        return 0;
    if (mode)
        *mode = pkt[2];
    return 1;
}

void jlx_forget(struct jlx_frame *shown)
{
    for (uint32_t i = 0; i < JLX_LEDS; i++) {
        shown->led[i].color = UNKNOWN;
        shown->led[i].alt = UNKNOWN;
        shown->led[i].mode = UNKNOWN;
    }
}

static int same(const struct jlx_led *a, const struct jlx_led *b)
{
    return a->mode == b->mode && a->color == b->color && (a->mode != JLX_FLASH || a->alt == b->alt);
}

/* one colour message: channel 1 static, 2 flashing, 3 pulsing; a Note On for a pad, a Control Change for a button */
static uint8_t *put(uint8_t *out, uint8_t cable, uint32_t i, uint8_t channel, uint8_t color)
{
    int pad = i < JLX_TOP0;
    *out++ = (uint8_t)((cable << 4) | (pad ? CIN_NOTE_ON : CIN_CC));
    *out++ = (uint8_t)((pad ? 0x90u : 0xB0u) | channel);
    *out++ = jlx_number(i);
    *out++ = color;
    return out;
}

size_t jlx_diff(uint8_t cable, struct jlx_frame *shown, const struct jlx_frame *want, uint8_t *out, size_t max_packets)
{
    size_t n = 0;
    for (uint32_t i = 0; i < JLX_LEDS; i++) {
        const struct jlx_led *w = &want->led[i];
        size_t need = w->mode == JLX_FLASH ? 2u : 1u;
        if (same(&shown->led[i], w))
            continue;
        if (n + need > max_packets)    /* in order: a later light does not jump over one that waits for room */
            break;
        if (w->mode == JLX_FLASH) {
            out = put(out, cable, i, 0, w->alt);
            out = put(out, cable, i, 1, w->color);
        } else {
            out = put(out, cable, i, w->mode == JLX_PULSE ? 2u : 0u, w->color);
        }
        n += need;
        shown->led[i] = *w;
    }
    return n;
}
