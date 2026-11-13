/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB audio speaker's receive path (usb_audio.h): TinyUSB's audio class calls tud_audio_rx_done_isr for each 1 ms packet,
 * which mixes it to mono int16 into a ring the tracker task drains, and keeps the offset between the sample count and the
 * local clock: off = arrival time - samples / 44100, smoothed, so that a sample's local time is off + s / 44100 whatever the
 * packets' jitter. The computer's clock is the stream's; ours drifts against it a little, which the smoothing follows. */
#include <string.h>
#include "app.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "tusb.h"
#include "usb_audio.h"

#define RING 4096u                      /* mono samples, a power of two: 93 ms; the tracker wakes every 5 ms */
#define SMOOTH_SHIFT 6                  /* the offset moves 1/64 of the way to each packet's value ... */
#define FAST_PACKETS 64u                /* ... and 1/8 for the first packets */
#define GAP_US 1500

static int16_t s_ring[RING];              /* (bss: DRAM_ATTR on an unset array would put 16 KB of zeros in the image) */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
/* all below under s_mux */
static uint32_t s_head, s_tail, s_base;     /* samples ever written, read, and the head when the stream's sample 0 arrived */
static int64_t s_off_us;
static int64_t s_last_us;
static uint8_t s_have_off;
static uint32_t s_seen;                     /* packets since the offset started */
static struct uac_stats s_st;
static volatile uint32_t s_epoch;

static int64_t sample_us(uint32_t samples) { return (int64_t)samples * 10000 / 441; }

/* the host's side of the descriptor: which alternate setting is on */
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;
    if ((p_request->wValue & 0xFFu) != 0u) {
        s_st.open = 1;
        s_epoch++;
    }
    return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;
    (void)p_request;
    s_st.open = 0;
    s_epoch++;
    return true;
}

/* GET CUR / RANGE of the clock source: 44.1 kHz and nothing else; the clock is valid */
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *rq)
{
    uint8_t entity = TU_U16_HIGH(rq->wIndex);
    uint8_t control = TU_U16_HIGH(rq->wValue);
    if (entity != UAC_CLOCK_ID)
        return false;
    if (control == AUDIO20_CS_CTRL_SAM_FREQ && rq->bRequest == AUDIO20_CS_REQ_CUR) {
        uint32_t hz = UAC_RATE;
        return tud_audio_buffer_and_schedule_control_xfer(rhport, rq, &hz, sizeof hz);
    }
    if (control == AUDIO20_CS_CTRL_SAM_FREQ && rq->bRequest == AUDIO20_CS_REQ_RANGE) {
        struct __attribute__((packed)) {
            uint16_t n;
            uint32_t min, max, res;
        } range = {1, UAC_RATE, UAC_RATE, 0};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, rq, &range, sizeof range);
    }
    if (control == AUDIO20_CS_CTRL_CLK_VALID && rq->bRequest == AUDIO20_CS_REQ_CUR) {
        uint8_t valid = 1;
        return tud_audio_buffer_and_schedule_control_xfer(rhport, rq, &valid, sizeof valid);
    }
    return false;
}

/* a computer that sets the (fixed) rate gets 44.1 kHz anyway */
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *rq, uint8_t *buf)
{
    (void)rhport;
    (void)buf;
    return TU_U16_HIGH(rq->wIndex) == UAC_CLOCK_ID;
}

/* each packet: into the ring as mono, and the offset updated. In the USB interrupt. */
bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting)
{
    uint8_t pk[UAC_EP_SIZE + 4] __attribute__((aligned(4)));
    uint16_t got = tud_audio_read(pk, sizeof pk);
    uint32_t frames = got / 4u;
    int64_t now = midi_now_us();
    (void)rhport, (void)n_bytes_received, (void)func_id, (void)ep_out, (void)cur_alt_setting;
    if (!frames)
        return true;
    portENTER_CRITICAL_ISR(&s_mux);
    if (RING - (s_head - s_tail) < frames) {
        s_st.overflows++;
        s_epoch++;                      /* the tracker starts again from here */
    } else {
        for (uint32_t i = 0; i < frames; i++) {
            const int16_t *f = (const int16_t *)(pk + 4u * i);
            s_ring[(s_head + i) & (RING - 1u)] = (int16_t)(((int32_t)f[0] + (int32_t)f[1]) / 2);
        }
        s_head += frames;
        {
            int64_t x = now - sample_us(s_head - s_base);
            if (!s_have_off) {
                s_off_us = x;
                s_have_off = 1;
            } else {
                s_off_us += (x - s_off_us) >> (s_seen < FAST_PACKETS ? 3 : SMOOTH_SHIFT);
            }
            if (s_last_us && now - s_last_us > GAP_US)
                s_st.gaps += (uint32_t)((now - s_last_us + 500) / 1000) - 1u;
            s_last_us = now;
            s_seen++;
        }
        s_st.packets++;
    }
    portEXIT_CRITICAL_ISR(&s_mux);
    return true;
}

uint32_t uac_epoch(void) { return s_epoch; }

void uac_restart(void)
{
    portENTER_CRITICAL(&s_mux);
    s_tail = s_head;
    s_base = s_head;
    s_have_off = 0;
    s_seen = 0;
    s_last_us = 0;
    s_st.resets++;
    portEXIT_CRITICAL(&s_mux);
}

size_t uac_pull(int16_t *dst, size_t max)
{
    size_t n;
    uint32_t tail;
    portENTER_CRITICAL(&s_mux);
    n = s_head - s_tail;
    tail = s_tail;
    portEXIT_CRITICAL(&s_mux);
    if (n > max)
        n = max;
    for (size_t i = 0; i < n; i++)
        dst[i] = s_ring[(tail + i) & (RING - 1u)];
    portENTER_CRITICAL(&s_mux);
    s_tail = tail + (uint32_t)n;
    portEXIT_CRITICAL(&s_mux);
    return n;
}

int uac_sample_time(int64_t s, int64_t *us)
{
    int ok;
    int64_t off;
    portENTER_CRITICAL(&s_mux);
    ok = s_have_off;
    off = s_off_us;
    portEXIT_CRITICAL(&s_mux);
    if (!ok)
        return 0;
    *us = off + s * 10000 / 441;
    return 1;
}

void uac_stats(struct uac_stats *out)
{
    portENTER_CRITICAL(&s_mux);
    *out = s_st;
    portEXIT_CRITICAL(&s_mux);
}

int32_t uac_offset_us(void)
{
    int64_t off;
    portENTER_CRITICAL(&s_mux);
    off = s_off_us;
    portEXIT_CRITICAL(&s_mux);
    return (int32_t)off;      /* (the timer started at boot: hours of it fit) */
}
