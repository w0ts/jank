/* SPDX-License-Identifier: GPL-3.0-only */
/* The LED language of a unit without a screen (jank_led.h). */
#include "jank_led.h"
#include "jank_core.h"
#include "jank_ui.h"

static int put(uint8_t rgb[3], uint8_t r, uint8_t g, uint8_t b)
{
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
    return r || g || b;
}

uint32_t jlg_batt_period_ms(int pct)
{
    int span = JLG_BATT_LOW_PCT - JLG_BATT_CRIT_PCT, k = pct - JLG_BATT_CRIT_PCT;
    if (k < 0)
        k = 0;
    if (k > span)
        k = span;
    return JLG_BATT_FAST_MS + (JLG_BATT_SLOW_MS - JLG_BATT_FAST_MS) * (uint32_t)k / (uint32_t)span;
}

static uint8_t breath(uint32_t now_ms)
{
    uint32_t t = now_ms % JLG_BREATH_MS, half = JLG_BREATH_MS / 2u;
    uint32_t up = t < half ? t : JLG_BREATH_MS - t;               /* 0 .. half .. 0 */
    return (uint8_t)(JLG_BREATH_MAX * up / half);
}

int jlg_host_lit(uint32_t now_ms)
{
    uint32_t p = now_ms % JLG_HOST_PERIOD_MS;
    return p < JLG_HOST_FLASH_MS || (p >= JLG_HOST_FLASH_MS + JLG_HOST_GAP_MS && p < 2u * JLG_HOST_FLASH_MS + JLG_HOST_GAP_MS);
}

int jlg_host_overlay(int lit, uint32_t now_ms, int host)
{
    return host && jlg_host_lit(now_ms) ? !lit : lit;
}

int jlg_color(const struct jlg_state *s, uint8_t rgb[3])
{
    if (!s->on_usb && s->batt_pct >= 0 && s->batt_pct < JLG_BATT_LOW_PCT &&
        s->now_ms % jlg_batt_period_ms(s->batt_pct) < JLG_BATT_FLASH_MS)
        return put(rgb, JLG_BATT_RGB);
    if (s->host && jlg_host_lit(s->now_ms))
        return put(rgb, JLG_HOST_RGB);
    if (s->running && s->joined) {
        if (!jk_led_lit(JK_NET_JANK, 1, s->beat_ub, s->mpb, s->quantum))
            return put(rgb, 0, 0, 0);
        if (s->low_conf)
            return put(rgb, JLG_LOWCONF_RGB);
        if (s->bar_locked && jk_led_bar_one(s->beat_ub, s->quantum))
            return put(rgb, JK_LED_ONE_RGB);
        return put(rgb, JLG_BEAT_RGB);
    }
    if (s->armed)
        return put(rgb, JLG_ARMED_RGB);
    if (s->joined)
        return put(rgb, 0, breath(s->now_ms), 0);
    return put(rgb, 0, 0, 0);
}
