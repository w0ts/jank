/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB audio speaker's receive path (usb_audio.h): TinyUSB's audio class calls tud_audio_rx_done_isr for each 1 ms packet,
 * which mixes it to mono int16 into a ring the tracker task drains, and keeps the offset between the sample count and the
 * local clock: off = arrival time - samples / 44100, smoothed, so that a sample's local time is off + s / 44100 whatever the
 * packets' jitter. The computer's clock is the stream's; ours drifts against it a little, which the smoothing follows. */
#include <string.h>
#include "esp_timer.h"
#include "app.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "tusb.h"
#include "usb_audio.h"

#define RING 4096u                      /* mono samples, a power of two: 93 ms; the tracker wakes every 5 ms */
#define SMOOTH_SHIFT 6                  /* the offset moves 1/64 of the way to each packet's value ... */
#define FAST_PACKETS 64u                /* ... and 1/8 for the first packets */
#define GAP_SLACK_US 500           /* 44 samples (998 us) + this = the 1.5 ms a missing 1 ms packet shows as */

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
static uint8_t s_stream;                    /* a stream is open: the computer's (alt 1) or the hosted device's */
static uint8_t s_route = 1;                 /* the audio source is USB (usbrole.c): else the samples are dropped */

static int64_t sample_us(uint32_t samples) { return (int64_t)samples * 10000 / 441; }

/* the stream as the tracker sees it: open and routed to it; any change restarts the tracker */
static void set_stream(int stream, int route)
{
    portENTER_CRITICAL_SAFE(&s_mux);
    if (stream >= 0)
        s_stream = (uint8_t)stream;
    if (route >= 0)
        s_route = (uint8_t)route;
    if (s_st.open != (s_stream && s_route) || stream > 0) {
        s_st.open = (uint8_t)(s_stream && s_route);
        s_epoch++;
    }
    portEXIT_CRITICAL_SAFE(&s_mux);
}

/* the host's side of the descriptor: which alternate setting is on */
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;
    if ((p_request->wValue & 0xFFu) != 0u)
        set_stream(1, -1);
    return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;
    (void)p_request;
    set_stream(0, -1);
    return true;
}

void uac_host_stream(int open) { set_stream(open ? 1 : 0, -1); }

void uac_route(int usb) { set_stream(-1, usb ? 1 : 0); }

int uac_stream_open(void) { return s_stream; }

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

/* mono samples that arrived at `now` (local us): into the ring, and the offset updated. The USB interrupt (device) or the
 * USB host client's task (host, usb_uac_host.c). */
void uac_feed(const int16_t *pk, uint32_t frames, int64_t now)
{
    if (!frames)
        return;
    portENTER_CRITICAL_SAFE(&s_mux);
    if (!s_route) {
        /* the tracker hears the line in: nothing of this goes to it */
    } else if (RING - (s_head - s_tail) < frames) {
        s_st.overflows++;
        s_epoch++;                      /* the tracker starts again from here */
    } else {
        for (uint32_t i = 0; i < frames; i++)
            s_ring[(s_head + i) & (RING - 1u)] = pk[i];
        s_head += frames;
        {
            int64_t x = now - sample_us(s_head - s_base);
            if (!s_have_off) {
                s_off_us = x;
                s_have_off = 1;
            } else {
                s_off_us += (x - s_off_us) >> (s_seen < FAST_PACKETS ? 3 : SMOOTH_SHIFT);
            }
            /* a gap: longer since the last feed than this feed's own audio and half a millisecond (device: 1 ms packets, so
             * over 1.5 ms; host: a transfer of a few packets at a time) */
            if (s_last_us && now - s_last_us > sample_us(frames) + GAP_SLACK_US) {
                uint32_t len = (uint32_t)((now - s_last_us - sample_us(frames) + GAP_SLACK_US) / 1000);
                s_st.gaps += len;
                memmove(s_st.gap_at_ms, s_st.gap_at_ms + 1, sizeof s_st.gap_at_ms - sizeof s_st.gap_at_ms[0]);
                memmove(s_st.gap_len_ms, s_st.gap_len_ms + 1, sizeof s_st.gap_len_ms - sizeof s_st.gap_len_ms[0]);
                s_st.gap_at_ms[5] = (uint32_t)(esp_timer_get_time() / 1000);   /* (`now` is the Link host clock: its own origin) */
                s_st.gap_len_ms[5] = len;
            }
            s_last_us = now;
            s_seen++;
        }
        s_st.packets++;
    }
    portEXIT_CRITICAL_SAFE(&s_mux);
}

/* each packet of the computer's stream: mono already. In the USB interrupt. */
bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting)
{
    int16_t pk[UAC_EP_SIZE / 2u + 2u];
    uint16_t got = tud_audio_read(pk, sizeof pk);
    (void)rhport, (void)n_bytes_received, (void)func_id, (void)ep_out, (void)cur_alt_setting;
    uac_feed(pk, got / 2u, midi_now_us());
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
