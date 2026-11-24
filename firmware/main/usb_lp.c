/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode: a Launchpad X plays a Digitone II (Note mode, stock MIDI). The logic is core/jank_lpapp.h's (host-tested); this
 * is the part that owns the one state, feeds it what the two devices send and puts what it makes into their queues
 * (usb_midi_host.c). Everything here runs in that file's client task (the USB Host Library's callbacks come from it), so the
 * state needs no lock; the queues take their own. Nothing here waits: the client task also serves the clock's transfers.
 *
 * The packets for the Digitone go through a FIFO (core/jank_lpout.h) that is drained as its queue has room, so a Note Off is
 * never lost to a full queue; the Launchpad's lights are only what changed (jlx_diff), a batch at a time.
 *
 * Not verified on hardware yet: the product names the roles are told apart by, that the Digitone's sequencer notes come out of
 * its USB port, its cable number (0 is assumed), the Launchpad taking lights SETTLE_MS after the Programmer mode message. */
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_lpapp.h"
#include "jank_lpout.h"
#include "jank_lpscales.h"
#include "jank_lpx.h"
#include "usb_host_mode.h"

#define LPX_CABLE 0u                   /* the Launchpad's MIDI interface has one port */
#define DN2_CABLE 0u
#define RESERVE 4u                     /* packets left free in a queue that also carries the clock: the clock must not wait */
#define BATCH 16u                      /* lights per call: what fits one bulk packet */
#define SYSEX_PACKETS 3u               /* the Programmer mode message */
#define SETTLE_MS 400                  /* after the Launchpad's answer to the Programmer mode message, before the first lights: still a guess (it was 150,
                                          and the lights stayed dark: 2026-11-24, with the answer in and the packets right) */
#define REFRESH_MS 2000                /* every light again this often: the Launchpad may take none while it changes modes, and the diff would otherwise
                                          never send a light it believes delivered; the whole frame is about 330 bytes */
#define RETRY_MS 10                    /* the Programmer mode message waits for room this long between tries */
/* A Launchpad that was replugged stayed dark (measured 2026-11-24 over Wi-Fi diag: attached, claimed, answering the clock, no lights; the
 * message had gone out the moment it was claimed). The cause is not proven: a Launchpad whose firmware is not up yet may drop the message.
 * So: a wait before the first message, then the message again every ACK_WAIT_MS until the Launchpad answers it (it sends the same message
 * back: jlx_programmer_ack); lights wait for the answer. After ACK_TRIES messages with no answer the lights go anyway (a Launchpad that never
 * answers would otherwise stay dark for good). diag shows messages sent and answers. */
#define ATTACH_WAIT_MS 500
#define ACK_WAIT_MS 400
#define ACK_TRIES 4u
#define CIN_NOTE_OFF 0x8u
#define CIN_NOTE_ON 0x9u
#define CIN_CC 0xBu

static const char *TAG = "usb lp";

static struct jlapp s_app;
static struct jlx_frame s_shown;
static struct jlo s_out;               /* for the Digitone */
static uint8_t s_ready, s_lpx_up, s_dn2_up, s_programmer_pending;   /* pending: the Launchpad has not answered the Programmer mode message yet */
static uint8_t s_tries;                /* Programmer mode messages sent to this Launchpad */
static TickType_t s_settle_at;         /* no lights before this tick */
static TickType_t s_try_at;            /* the next Programmer mode message is due at this tick */
static TickType_t s_refresh_at;        /* every light is sent again at this tick (REFRESH_MS) */

static void ensure_init(void)
{
    if (s_ready)
        return;
    jlapp_init(&s_app, jls_get(JLS_MAJOR), DN2_CABLE);   /* the starter list's major scale; the scale page (hold Note) changes it */
    jlo_init(&s_out);
    s_ready = 1;
}

/* the waiting packets go out as far as the Digitone's queue has room (beyond the clock's reserve) */
static void dn2_drain(void)
{
    uint8_t out[4 * BATCH];
    size_t room = usb_midi_host_room(JLAPP_DN2), n;
    if (!s_dn2_up || room <= RESERVE)
        return;
    n = jlo_drain(&s_out, room - RESERVE < BATCH ? room - RESERVE : BATCH, out);
    if (n)
        usb_midi_host_send_to(JLAPP_DN2, out, 4u * n);
}

/* bytes: whole packets, in order; with no Digitone there is nobody to play and they are dropped */
static void dn2_send(const uint8_t *packets, size_t bytes)
{
    if (!s_dn2_up)
        return;
    for (size_t i = 0; i + 4u <= bytes; i += 4u)
        if (!jlo_push(&s_out, packets + i))
            ESP_LOGW(TAG, "packets for the Digitone: FIFO full");
    dn2_drain();
}

void usb_lp_attach(enum jlapp_role r)
{
    ensure_init();
    if (r == JLAPP_LPX) {
        s_lpx_up = 1;
        g_hostdiag.lpx_up = 1;
        s_programmer_pending = 1;      /* sent by usb_lp_pump, after a wait and when its queue has room */
        s_tries = 0;
        s_try_at = xTaskGetTickCount() + pdMS_TO_TICKS(ATTACH_WAIT_MS);
        ESP_LOGI(TAG, "Launchpad X");
    } else if (r == JLAPP_DN2) {
        s_dn2_up = 1;
        g_hostdiag.dn2_up = 1;
        jlo_clear(&s_out);
        ESP_LOGI(TAG, "Digitone");
    }
}

void usb_lp_detach(enum jlapp_role r)
{
    ensure_init();
    if (r == JLAPP_LPX) {
        uint8_t off[4 * JLAPP_PADS];
        s_lpx_up = 0;
        g_hostdiag.lpx_up = 0;
        s_programmer_pending = 0;
        /* its pads that were down end their notes: nothing stays sounding on the Digitone */
        dn2_send(off, 4u * jlapp_release_all(&s_app, off, JLAPP_PADS));
    } else if (r == JLAPP_DN2) {
        s_dn2_up = 0;
        g_hostdiag.dn2_up = 0;
        jlo_clear(&s_out);
        memset(s_app.playing, 0, sizeof s_app.playing);
    }
}

static void rx_lpx(const uint8_t p[4])
{
    uint8_t cin = p[0] & 0x0Fu, out[4];
    struct jlx_ev ev;
    const uint8_t msg[3] = {p[1], p[2], p[3]};
    int mode;
    if (jlx_programmer_ack(p, &mode)) {   /* its answer to the Programmer mode message, or to the mode query: the mode it is in now */
        g_hostdiag.lp_mode = (uint8_t)(mode ? 2 : 1);
        if (mode && s_programmer_pending) {   /* now the lights can follow */
            s_programmer_pending = 0;
            g_hostdiag.programmer_acked++;
            jlx_forget(&s_shown);      /* a Launchpad that just entered the mode shows nothing we know of */
            s_settle_at = xTaskGetTickCount() + pdMS_TO_TICKS(SETTLE_MS);
            s_refresh_at = s_settle_at + pdMS_TO_TICKS(REFRESH_MS);
        } else if (!mode && !s_programmer_pending) {   /* it is in Live mode (it took no lights there): enter Programmer mode again */
            if (g_hostdiag.lp_reentered < UINT8_MAX)
                g_hostdiag.lp_reentered++;
            s_programmer_pending = 1;
            s_tries = 0;
            s_try_at = xTaskGetTickCount();
        }
        return;
    }
    if (cin != CIN_NOTE_ON && cin != CIN_NOTE_OFF && cin != CIN_CC)
        return;
    if (!jlx_decode(msg, &ev))
        return;
    g_hostdiag.pads++;
    if (jlapp_on_lpx(&s_app, &ev, out, 1))
        dn2_send(out, 4);
}

void usb_lp_rx(enum jlapp_role r, const uint8_t *packets, size_t n)
{
    ensure_init();
    for (size_t i = 0; i + 4u <= n; i += 4u) {
        if (r == JLAPP_LPX)
            rx_lpx(packets + i);
        else if (r == JLAPP_DN2) {
            g_hostdiag.dn2_msgs++;
            jlapp_on_dn2(&s_app, packets + i);
        }
    }
}

void usb_lp_pump(void)
{
    struct jlx_frame want;
    uint8_t out[4 * BATCH];
    size_t room, n;
    dn2_drain();
    if (!s_lpx_up)
        return;
    room = usb_midi_host_room(JLAPP_LPX);
    if (s_programmer_pending) {        /* the whole message or none of it: a cut SysEx would leave the Launchpad in Live mode */
        if ((int32_t)(xTaskGetTickCount() - s_try_at) < 0 || room < SYSEX_PACKETS)
            return;
        usb_midi_host_send_to(JLAPP_LPX, out, jlx_programmer(LPX_CABLE, 1, out));
        g_hostdiag.programmer_sent++;
        s_tries++;
        s_try_at = xTaskGetTickCount() + pdMS_TO_TICKS(ACK_WAIT_MS);
        if (s_tries >= ACK_TRIES) {    /* it never answered: the lights go anyway, as before */
            s_programmer_pending = 0;
            jlx_forget(&s_shown);
            s_settle_at = xTaskGetTickCount() + pdMS_TO_TICKS(SETTLE_MS);
            s_refresh_at = s_settle_at + pdMS_TO_TICKS(REFRESH_MS);
        }
        return;
    }
    if ((int32_t)(xTaskGetTickCount() - s_settle_at) < 0 || room <= RESERVE)
        return;
    if ((int32_t)(xTaskGetTickCount() - s_refresh_at) >= 0) {   /* all the lights again, in case one was lost; and which mode is it in? (a Launchpad in Live mode takes no lights) */
        jlx_forget(&s_shown);
        s_refresh_at = xTaskGetTickCount() + pdMS_TO_TICKS(REFRESH_MS);
        if (room >= SYSEX_PACKETS + RESERVE) {      /* the whole query, and the clock's reserve left: else it waits for the next refresh */
            usb_midi_host_send_to(JLAPP_LPX, out, jlx_mode_query(LPX_CABLE, out));
            return;                    /* the lights follow on the next pass: the queue has 3 packets fewer room now */
        }
    }
    jlapp_frame(&s_app, &want);
    n = jlx_diff(LPX_CABLE, &s_shown, &want, out, room - RESERVE < BATCH ? room - RESERVE : BATCH);
    if (n) {
        usb_midi_host_send_to(JLAPP_LPX, out, n * 4u);
        g_hostdiag.lights_sent += (uint32_t)n;
    }
}

/* how long the client task may sleep: until the lights may start, or the next try for room; else until an event */
uint32_t usb_lp_wait_ticks(void)
{
    int32_t left;
    if (!s_lpx_up)
        return portMAX_DELAY;
    if (s_programmer_pending) {        /* until the next message is due, then every RETRY_MS while it waits for room */
        left = (int32_t)(s_try_at - xTaskGetTickCount());
        if (left > 0)
            return (uint32_t)left;
        return pdMS_TO_TICKS(RETRY_MS) ? pdMS_TO_TICKS(RETRY_MS) : 1u;
    }
    left = (int32_t)(s_settle_at - xTaskGetTickCount());
    if (left > 0)
        return (uint32_t)left;
    left = (int32_t)(s_refresh_at - xTaskGetTickCount());   /* settled: until the next refresh (the clock's transfers wake the task far oftener) */
    return left > 0 ? (uint32_t)left : 1u;
}
