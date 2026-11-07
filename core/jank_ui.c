/* SPDX-License-Identifier: GPL-3.0-only */
/* The button, the group scan's bookkeeping, the LED's event patterns (jank_ui.h). */
#include "jank_ui.h"
#include "jank_core.h"
#include <stdio.h>
#include <string.h>

enum { ST_IDLE, ST_DOWN1, ST_WAIT2, ST_DOWN2, ST_LONG };

void jb_init(struct jb_button *b) { memset(b, 0, sizeof *b); }

/* 1 when the debounced level changed */
static int debounce(struct jb_button *b, int pressed, uint32_t now_ms)
{
    if (pressed != b->raw) {
        b->raw = (uint8_t)pressed;
        b->t_raw = now_ms;
    }
    if (b->raw == b->level || now_ms - b->t_raw < JB_DEBOUNCE_MS)
        return 0;
    b->level = b->raw;
    return 1;
}

enum jb_event jb_step(struct jb_button *b, int pressed, uint32_t now_ms)
{
    int edge = debounce(b, pressed != 0, now_ms);
    switch (b->state) {
    case ST_IDLE:
        if (edge && b->level) {
            b->state = ST_DOWN1;
            b->t_edge = now_ms;
            b->t_down = now_ms;
        }
        break;
    case ST_DOWN1:
        if (edge) {
            b->state = ST_WAIT2;
            b->t_edge = now_ms;
        } else if (now_ms - b->t_edge >= JB_DOWNLOAD_MS) {
            b->state = ST_LONG;
            return JB_DOWNLOAD;
        }
        break;
    case ST_WAIT2:
        if (b->t_edge - b->t_down >= JB_LONG_MS) {     /* the release of a long press */
            b->state = ST_IDLE;
            return JB_LONG;
        }
        if (edge && b->level) {
            b->state = ST_DOWN2;
            return JB_DOUBLE;
        }
        if (now_ms - b->t_edge >= JB_DOUBLE_MS) {
            b->state = ST_IDLE;
            return JB_CLICK;
        }
        break;
    default:                          /* ST_DOWN2, ST_LONG: wait for the release */
        if (edge && !b->level)
            b->state = ST_IDLE;
        break;
    }
    return JB_NONE;
}

void js_clear(struct js_scan *s) { memset(s, 0, sizeof *s); }

uint32_t js_group_of(const uint8_t *air, size_t len, const uint8_t own_mac[6])
{
    static const uint8_t prefix[5] = {0x02, 0x66, 0x6D, 0x31, 0x6A};
    if (len < JK_DOT11_LEN || (air[0] & 0x0Cu) != 0x08u || memcmp(air + 16, prefix, 5) || !memcmp(air + 10, own_mac, 6))
        return 0;
    return air[21] >= JK_GROUP_MIN && air[21] <= JK_GROUP_MAX ? air[21] : 0u;
}

void js_note(struct js_scan *s, uint32_t group, uint32_t now_ms)
{
    if (group < JK_GROUP_MIN || group > JK_GROUP_MAX)
        return;
    s->heard |= (uint16_t)(1u << group);
    s->t_heard[group] = now_ms;
}

uint32_t js_plan(uint32_t home_group, uint8_t out[2])
{
    static const uint8_t all[3] = {1, 6, 11};
    uint32_t n = 0, home = jk_channel(home_group);
    for (uint32_t i = 0; i < 3u; i++)
        if (all[i] != home)
            out[n++] = all[i];
    return n;
}

static int heard_lately(const struct js_scan *s, uint32_t g, uint32_t now_ms)
{
    return (s->heard >> g & 1u) && now_ms - s->t_heard[g] < JS_HEARD_MS;
}

uint32_t js_pick(const struct js_scan *s, uint32_t current, uint32_t now_ms)
{
    uint32_t g = current;
    for (uint32_t i = 1; i < JK_GROUP_MAX; i++) {
        g = jk_next_group(g);
        if (g != current && heard_lately(s, g, now_ms))
            return g;
    }
    return 0;
}

void js_table_clear(struct js_table *t) { memset(t, 0, sizeof *t); }

static int row_knows(const struct js_row *r, const uint8_t sender[6])
{
    for (uint32_t i = 0; i < r->units; i++)
        if (!memcmp(r->mac[i], sender, 6))
            return 1;
    return 0;
}

void js_table_note(struct js_table *t, uint32_t group, const uint8_t sender[6], int8_t rssi)
{
    struct js_row *r;
    if (group < JK_GROUP_MIN || group > JK_GROUP_MAX)
        return;
    r = &t->row[group];
    if (!r->frames || rssi > r->rssi)
        r->rssi = rssi;
    r->frames++;
    if (r->units < JS_UNITS && !row_knows(r, sender)) {
        memcpy(r->mac[r->units++], sender, 6);
        r->janks += (uint8_t)jk_mac_is_jank(sender);
    }
}

size_t js_table_report(const struct js_table *t, char *out, size_t cap)
{
    size_t n = 0;
    int r;
    for (uint32_t g = JK_GROUP_MIN; g <= JK_GROUP_MAX; g++) {
        const struct js_row *w = &t->row[g];
        if (!w->frames)
            continue;
        r = snprintf(out + n, cap - n, "scan=%lu,%u,%lu,%u,%u,%d\n", (unsigned long)g, jk_channel(g), (unsigned long)w->frames, w->units, w->janks, w->rssi);
        if (r < 0 || (size_t)r >= cap - n)
            return 0;
        n += (size_t)r;
    }
    r = snprintf(out + n, cap - n, "%sok\n", n ? "" : "scan=none\n");
    return r < 0 || (size_t)r >= cap - n ? 0u : n + (size_t)r;
}

static void set_rgb(uint8_t rgb[3], uint8_t r, uint8_t g, uint8_t b)
{
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}

int jl_pattern(enum jl_event e, uint32_t group, uint32_t t_ms, uint8_t rgb[3])
{
    set_rgb(rgb, 0, 0, 0);
    switch (e) {
    case JL_SCANNING:
        set_rgb(rgb, 40, 0, 40);
        return 1;
    case JL_JOINED:
        if (t_ms >= group * JL_FLASH_MS)
            return 0;
        if (t_ms % JL_FLASH_MS < JL_FLASH_MS / 2u)
            set_rgb(rgb, 40, 40, 40);
        return 1;
    case JL_NO_GROUP:
        if (t_ms >= 3u * JL_PULSE_MS)
            return 0;
        if (t_ms % JL_PULSE_MS < JL_PULSE_MS * 2u / 3u)
            set_rgb(rgb, 40, 0, 0);
        return 1;
    default:
        return 0;
    }
}
