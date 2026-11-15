/* SPDX-License-Identifier: GPL-3.0-only */
/* The phone link's state and the HOME line (jank_phone.h). */
#include "jank_phone.h"
#include <stdio.h>
#include <string.h>

void jph_init(struct jph *p, uint32_t mode, uint32_t now_ms)
{
    memset(p, 0, sizeof *p);
    p->mode = (uint8_t)(mode <= JK_BLE_AUTO ? mode : JK_BLE_PAIR);
    if (p->mode == JK_BLE_AUTO) {
        p->state = JPH_WINDOW;
        p->until_ms = now_ms + JPH_WINDOW_MS;
    }
}

static void open_window(struct jph *p, uint32_t now_ms, uint32_t ms)
{
    uint32_t until = now_ms + ms;
    if (p->state != JPH_WINDOW || (int32_t)(until - p->until_ms) > 0)
        p->until_ms = until;
    p->state = JPH_WINDOW;
}

int jph_pair(struct jph *p, uint32_t now_ms)
{
    if (p->mode == JK_BLE_OFF)
        return -1;
    if (p->state != JPH_CONNECTED)
        open_window(p, now_ms, JPH_WINDOW_MS);
    return 0;
}

void jph_close(struct jph *p) { p->state = JPH_OFF; }

void jph_set_mode(struct jph *p, uint32_t mode)
{
    p->mode = (uint8_t)(mode <= JK_BLE_AUTO ? mode : JK_BLE_PAIR);
    if (p->mode == JK_BLE_OFF)
        p->state = JPH_OFF;
}

void jph_connected(struct jph *p, uint32_t now_ms)
{
    (void)now_ms;
    if (p->state != JPH_OFF)           /* a link that comes up as the window closes is not kept */
        p->state = JPH_CONNECTED;
}

void jph_disconnected(struct jph *p, uint32_t now_ms)
{
    if (p->state == JPH_CONNECTED && p->mode != JK_BLE_OFF)
        open_window(p, now_ms, JPH_REJOIN_MS);
}

void jph_step(struct jph *p, uint32_t now_ms)
{
    if (p->state == JPH_WINDOW && (int32_t)(now_ms - p->until_ms) >= 0)
        p->state = JPH_OFF;
}

int jph_radio_on(const struct jph *p) { return p->state != JPH_OFF; }

int jph_advertising(const struct jph *p) { return p->state == JPH_WINDOW; }

uint32_t jph_left_s(const struct jph *p, uint32_t now_ms)
{
    int32_t left = (int32_t)(p->until_ms - now_ms);
    return p->state == JPH_WINDOW && left > 0 ? ((uint32_t)left + 999u) / 1000u : 0u;
}

const char *jph_state_name(const struct jph *p)
{
    static const char *const names[] = {"off", "window", "connected"};
    return p->state <= JPH_CONNECTED ? names[p->state] : "off";
}

/* "-" or the number */
static void field(char *out, size_t cap, int have, long v)
{
    if (have)
        snprintf(out, cap, "%ld", v);
    else
        snprintf(out, cap, "-");
}

size_t jph_home_line(const struct jk_cfg *c, const struct jk_snapshot *s, uint32_t now_ms, char *out, size_t cap)
{
    char net[8], bar[12], batt[8], level[8], conf[8];
    uint32_t q = c->quantum >= 1u && c->quantum <= JK_QUANTUM_MAX ? c->quantum : 4u;
    uint32_t bpm_x100 = s->mpb > 0 ? (uint32_t)((6000000000ull + (uint64_t)s->mpb / 2u) / (uint32_t)s->mpb) : 0u;
    unsigned transport = s->armed ? 2u : s->playing ? 1u : 0u;
    int r;
    if (c->net == JK_NET_WIFI)
        snprintf(net, sizeof net, "w%u", s->wifi_up ? 1u : 0u);
    else
        snprintf(net, sizeof net, "j%u", s->b.group);
    if (s->mpb > 0) {
        /* the beat in hundredths, floored, then into the bar (Link's phase: beat mod quantum, negative beats too) */
        int64_t hb = s->beat_ub >= 0 ? s->beat_ub / 10000 : -((-s->beat_ub + 9999) / 10000);
        int64_t m = (int64_t)q * 100;
        field(bar, sizeof bar, 1, (long)(((hb % m) + m) % m));
    } else {
        field(bar, sizeof bar, 0, 0);
    }
    field(batt, sizeof batt, s->batt_pct >= 0, s->batt_pct);
    field(level, sizeof level, s->beat.audio, s->beat.level_db);
    field(conf, sizeof conf, s->beat.audio, s->beat.conf_pct);
    r = snprintf(out, cap, "h %lu %s %u %s %lu %lu %lu %s %s %s", (unsigned long)bpm_x100, net, transport, bar, (unsigned long)q,
                 (unsigned long)s->link_peers, (unsigned long)(c->net == JK_NET_WIFI ? 0u : jk_peers_heard(&s->b, now_ms)), batt, level, conf);
    return r < 0 || (size_t)r >= cap ? 0u : (size_t)r;
}

void jph_rate_init(struct jph_rate *r) { memset(r, 0, sizeof *r); }

int jph_home_due(struct jph_rate *r, const char *line, size_t n, uint32_t now_ms)
{
    uint32_t since = now_ms - r->last_ms;
    int same = r->sent && n < sizeof r->last && strlen(r->last) == n && !memcmp(r->last, line, n);
    if (r->sent && (since < JPH_HOME_EVERY_MS || (same && since < JPH_HOME_IDLE_MS)))
        return 0;
    r->sent = 1;
    r->last_ms = now_ms;
    if (n >= sizeof r->last)
        n = sizeof r->last - 1u;
    memcpy(r->last, line, n);
    r->last[n] = 0;
    return 1;
}
