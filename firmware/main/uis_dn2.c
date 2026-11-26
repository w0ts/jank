/* SPDX-License-Identifier: GPL-3.0-only */
/* The Digitone's screen as the device's UI (uiscreen.h, owner 2026-11-25): between the USB client task (usb_lpmenu.c, which talks to the Digitone) and the ui task
 * (uiscreen.c, which runs the screens and makes the page). The keys come in through a queue; the page goes out through one buffer with a sequence number (odd while
 * the ui task writes it, so the client task reads it again); two flags say whether the page is open and how many OPENs came. */
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "jank_oled_text.h"
#include "uiscreen.h"
#include "usb_host_mode.h"

static const char *TAG = "uis dn2";

#define KEYS 16u                        /* key events waiting for the ui task (the Digitone pushes at most 8 at once) */
static QueueHandle_t s_keys;            /* (button << 1) | down */
static volatile uint8_t s_open;
static volatile uint32_t s_opens;       /* the client task's count of OPENs: each wants a page made afresh */
static uint8_t s_held;                  /* (ui task) the Digitone's keys that are down, a bit per button: let go when the page closes */
struct dn2_page {
    volatile uint32_t seq;              /* odd while it is written; 0: none yet */
    uint32_t opens;                     /* the OPENs count it was made for */
    struct jot_page page;
};
static struct dn2_page *volatile s_page;   /* made at the first page: in PSRAM when the board has it (internal RAM is tight) */

void uis_dn2_init(void) { s_keys = xQueueCreate(KEYS, sizeof(uint8_t)); }   /* (none: the keys are dropped, logged) */

/* ---- the client task ---- */
void uis_dn2_key(enum jo_btn b, int down)
{
    uint8_t v = (uint8_t)(((unsigned)b << 1) | (down != 0));
    if (s_keys && xQueueSend(s_keys, &v, 0) != pdTRUE)
        ESP_LOGW(TAG, "a key lost: the ui task is behind");
}

void uis_dn2_open(int open, uint32_t opens)
{
    s_opens = opens;
    s_open = (uint8_t)(open != 0);
}

int uis_dn2_page(struct jot_page *out, uint32_t *seq, uint32_t *opens)
{
    struct dn2_page *p = s_page;
    for (int tries = 0; p && tries < 3; tries++) {   /* the ui task may be writing it: read again (it writes at most every 30 ms) */
        uint32_t a = p->seq;
        __sync_synchronize();
        if (!a)
            return 0;
        if (a & 1u)
            continue;
        *out = p->page;
        *opens = p->opens;
        __sync_synchronize();
        if (p->seq == a) {
            *seq = a;
            return 1;
        }
    }
    return 0;
}

/* ---- the ui task ---- */
int uis_dn2_is_open(void) { return s_open; }

int uis_dn2_active(void) { return s_open || s_held; }

int uis_dn2_page_wanted(void)
{
    struct dn2_page *p = s_page;
    return s_open && (!p || p->opens != s_opens);
}

/* the next key; a key going down while the page is closed is dropped, one still down when it closes is let go (its repeat would run on) */
int uis_dn2_next_key(enum jo_btn *b, int *down)
{
    uint8_t v;
    while (s_keys && xQueueReceive(s_keys, &v, 0) == pdTRUE) {
        unsigned k = v >> 1;
        int d = v & 1u;
        if (k >= JO_BTNS || (d && !s_open))
            continue;
        if (d)
            s_held |= (uint8_t)(1u << k);
        else
            s_held &= (uint8_t)~(1u << k);
        *b = (enum jo_btn)k;
        *down = d;
        return 1;
    }
    for (unsigned k = 0; !s_open && s_held && k < JO_BTNS; k++)
        if (s_held >> k & 1u) {
            s_held &= (uint8_t)~(1u << k);
            *b = (enum jo_btn)k;
            *down = 0;
            return 1;
        }
    return 0;
}

/* the page, when it changed or an OPEN asks for one; the client task is woken to send it */
void uis_dn2_publish(const struct jot_page *page)
{
    struct dn2_page *s = s_page;
    uint32_t opens = s_opens;
    if (!s) {
        s = heap_caps_calloc(1, sizeof *s, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s)
            s = heap_caps_calloc(1, sizeof *s, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!s) {
            ESP_LOGE(TAG, "no RAM for the page");
            return;
        }
        s_page = s;
    }
    if (s->seq && s->opens == opens && jot_equal(page, &s->page))
        return;
    s->seq++;
    __sync_synchronize();
    s->opens = opens;
    s->page = *page;
    __sync_synchronize();
    s->seq++;
    usb_midi_host_wake();
}
