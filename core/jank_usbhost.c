/* SPDX-License-Identifier: GPL-3.0-only */
/* The mini 3's USB host gate (jank_usbhost.h). */
#include "jank_usbhost.h"

void juh_init(struct juh *h, int vbus_raw, uint32_t now_ms)
{
    h->vbus = (uint8_t)(vbus_raw != 0);
    h->low = (uint8_t)(vbus_raw == 0);
    h->t_low = now_ms;
}

void juh_step(struct juh *h, const struct juh_in *in, struct juh_out *out)
{
    uint8_t settled;
    if (in->vbus_raw) {
        h->vbus = 1;                   /* at once */
        h->low = 0;
    } else {
        if (!h->low) {
            h->low = 1;
            h->t_low = in->now_ms;
        }
        if (in->now_ms - h->t_low >= JUH_DEBOUNCE_MS)
            h->vbus = 0;
    }
    settled = (uint8_t)(h->low && in->now_ms - h->t_low >= JUH_DEBOUNCE_MS);
    out->vbus = h->vbus;
    out->mode_host = (uint8_t)(in->setting_host && !h->vbus);
    out->host_en = (uint8_t)(in->want_en && out->mode_host && settled && !in->vbus_raw && in->v5_known
                             && in->now_ms - in->v5_since_ms >= JUH_V5_SETTLE_MS);
}
