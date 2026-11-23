/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode: one hosted device's audio into the beat tracker. A client of ESP-IDF's USB Host Library (usb_hostlib.c),
 * with jank's own small UAC 1.0 / 2.0 capture driver (core/jank_uacdesc.h says why not espressif/usb_host_uac).
 *
 * The first device with a usable isochronous IN stream (PCM Type I, 44.1 or 48 kHz, 2 .. 4-byte subframes, up to 8
 * channels, a packet that fits the host's IN FIFO) is streamed; others are left alone. Its packets come back here in the
 * client task (the USB Host Library calls a transfer's callback from usb_host_client_handle_events, usb_host.h), are
 * mixed to mono int16 ((L + R) / 2, core/jank_pcm.h), resampled to 44.1 kHz when they are 48 kHz, and go into the same
 * ring the device stack's speaker fills (usb_audio.h uac_feed), stamped with the local clock when their transfer
 * completes: so beat.c, the latency accounting (beat_latency) and the `beat` numbers are the same in both modes. The
 * stamp is the transfer's completion as the client task sees it, PACKETS ms of audio at a time; usb_audio.c's offset
 * filter (1/64 a step) smooths the task's jitter, and what stays constant (a transfer's length, the resampler's 0.25 ms)
 * is in beat_latency like any other constant.
 *
 * Setting it up (the worker task, because a control transfer's completion needs the client task running):
 *   UAC 2.0: RANGE of each clock -> the rates; choose; claim; SET_CUR the clock's rate; SET_INTERFACE.
 *   UAC 1.0: choose from the descriptors; claim; SET_INTERFACE; SET_CUR the endpoint's rate (when it has the control).
 * (usb_host_interface_claim only books the interface and its endpoints: the SET_INTERFACE request is ours to send, as
 * espressif/usb_host_uac does too, uac_host.c uac_host_interface_resume.) */
#include <string.h>
#include "app.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jank_pcm.h"
#include "jank_uacdesc.h"
#include "usb/usb_helpers.h"
#include "usb/usb_host.h"
#include "usb_audio.h"
#include "usb_host_mode.h"

#define EVENTS 8u
#define XFERS 4u                         /* transfers in flight: 8 ms of audio queued at the host */
#define PACKETS 2u                       /* packets (1 ms frames) a transfer */
#define CTRL_TIMEOUT_MS 1000u
#define CTRL_DATA_MAX 128u               /* a RANGE answer (98 bytes) rounded up to EP0's packet size, 64 at most */
#define OPEN_HANDLES 8u                      /* handles the worker holds open at once (setting up, streaming) */
#define WAITING_MAX 4u                   /* devices passed over while another streams: tried again when it leaves */
#define STOP_WAIT_MS 200u
#define CLIENT_PRIO 22                   /* the packets' callbacks run in it: above Wi-Fi's app tasks, under the MIDI task */
#define WORKER_PRIO 5
#define MONO_MAX (PACKETS * JU_MPS_MAX / 4u)   /* frames: the smallest frame is 2 channels x 2 bytes or 1 x 4 */
#define OUT_MAX (MONO_MAX + 8u)

enum { EV_NEW = 1, EV_GONE = 2 };

struct event {
    uint8_t kind, address;
    usb_device_handle_t h;
};

static const char *TAG = "usb audio";
static usb_host_client_handle_t s_client;
static QueueHandle_t s_events;
static SemaphoreHandle_t s_ctrl_done;
static usb_transfer_t *s_ctrl;
static volatile uint8_t s_ctrl_out;      /* a control transfer is with the library (set before submit, cleared by on_ctrl) */
static usb_device_handle_t s_open[OPEN_HANDLES];   /* the worker's: what this client has open, so a gone handle is closed once */
static uint8_t s_waiting[WAITING_MAX];   /* the worker's: addresses passed over */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* the device being streamed; h set = in use. Fields below `active` are the worker's, read by the client task's callbacks
 * only while active is 1. */
static struct {
    usb_device_handle_t h;
    volatile uint8_t active;              /* resubmit transfers */
    uint32_t inflight;                    /* __atomic: the worker adds, the callbacks take away */
    uint8_t itf, channels, subframe, bits;
    uint16_t mps;
    uint32_t rate;
    uint32_t packets, errors;             /* under s_mux */
    char name[JK_USB_NAME_MAX + 1];
    uint16_t vid, pid;                    /* its beat_latency's key (core/jank_lat.h); with name, under s_mux */
    usb_transfer_t *x[XFERS];
} s_dev;

/* the client task's while a stream runs, on the heap only then (8.8 KB, the resampler's table most of it): internal RAM
 * first, PSRAM when that is short */
struct work {
    struct jp_rs rs;
    int16_t mono[MONO_MAX];
    int16_t out[OUT_MAX];
};
static struct work *s_w;

static void on_event(const usb_host_client_event_msg_t *m, void *arg)
{
    struct event ev = {0, 0, NULL};
    (void)arg;
    if (m->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        ev.kind = EV_NEW;
        ev.address = m->new_dev.address;
    } else if (m->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ev.kind = EV_GONE;
        ev.h = m->dev_gone.dev_hdl;
    } else {
        return;
    }
    if (xQueueSend(s_events, &ev, 0) != pdTRUE)
        ESP_LOGW(TAG, "event dropped");
}

/* a transfer back, in the client task: its packets into the tracker's ring, then out again */
static void on_audio(usb_transfer_t *t)
{
    int64_t now = midi_now_us();
    size_t mono = 0, out;
    uint32_t bad = 0, good = 0;
    if (t->status == USB_TRANSFER_STATUS_COMPLETED) {
        for (int i = 0; i < t->num_isoc_packets; i++) {
            const usb_isoc_packet_desc_t *p = &t->isoc_packet_desc[i];
            if (p->status != USB_TRANSFER_STATUS_COMPLETED || p->actual_num_bytes > p->num_bytes) {
                bad++;
                continue;
            }
            good++;
            mono += jp_to_mono16(t->data_buffer + (size_t)i * s_dev.mps, (size_t)p->actual_num_bytes, s_dev.channels, s_dev.subframe, s_w->mono + mono, MONO_MAX - mono);
        }
    } else {
        bad = (uint32_t)t->num_isoc_packets;
    }
    if (mono) {
        out = jp_rs_run(&s_w->rs, s_w->mono, mono, s_w->out, OUT_MAX);
        uac_feed(s_w->out, (uint32_t)out, now);
    }
    portENTER_CRITICAL(&s_mux);
    s_dev.packets += good;
    s_dev.errors += bad;
    portEXIT_CRITICAL(&s_mux);
    if (s_dev.active && t->status != USB_TRANSFER_STATUS_NO_DEVICE && t->status != USB_TRANSFER_STATUS_CANCELED
        && usb_host_transfer_submit(t) == ESP_OK)
        return;
    __atomic_sub_fetch(&s_dev.inflight, 1u, __ATOMIC_RELEASE);
}

static void on_ctrl(usb_transfer_t *t)
{
    (void)t;
    s_ctrl_out = 0;
    xSemaphoreGive(s_ctrl_done);
}

static uint16_t ep0_mps(usb_device_handle_t h)
{
    usb_device_info_t info;
    return usb_host_device_info(h, &info) == ESP_OK && info.bMaxPacketSize0 ? info.bMaxPacketSize0 : 64u;
}

/* a control request with its data stage (out: data to send; in: room for the answer); the worker task only */
static esp_err_t control(usb_device_handle_t h, const uint8_t setup[8], const uint8_t *out, uint8_t *in, size_t *got)
{
    uint16_t len = (uint16_t)(setup[6] | setup[7] << 8), mps = ep0_mps(h);
    /* an IN data stage asks for whole packets of EP0's size (usb_types_stack.h, as usb_host.c's own requests do) */
    size_t want = (setup[0] & 0x80u) ? (size_t)(len + mps - 1u) / mps * mps : len;
    esp_err_t e;
    if (s_ctrl_out)
        return ESP_ERR_INVALID_STATE;    /* a timed-out transfer is still out: its buffer is not ours */
    if (8u + want > s_ctrl->data_buffer_size)
        return ESP_ERR_INVALID_SIZE;
    xSemaphoreTake(s_ctrl_done, 0);      /* a late completion's token, if one was left */
    memcpy(s_ctrl->data_buffer, setup, 8);
    if (out && len)
        memcpy(s_ctrl->data_buffer + 8, out, len);
    s_ctrl->device_handle = h;
    s_ctrl->bEndpointAddress = 0;
    s_ctrl->callback = on_ctrl;
    s_ctrl->context = NULL;
    s_ctrl->num_bytes = (int)(8u + want);
    s_ctrl_out = 1;
    e = usb_host_transfer_submit_control(s_client, s_ctrl);
    if (e != ESP_OK) {
        s_ctrl_out = 0;
        return e;
    }
    /* No recovery on a timeout: the library has no control timeout and refuses to halt EP0 (usbh.c check_ep_addr), so the
     * transfer stays out until the device answers or leaves; until then no other control request goes (s_ctrl_out). */
    if (xSemaphoreTake(s_ctrl_done, pdMS_TO_TICKS(CTRL_TIMEOUT_MS)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    if (s_ctrl->status != USB_TRANSFER_STATUS_COMPLETED)
        return ESP_ERR_INVALID_RESPONSE;
    if (in && got) {
        size_t n = s_ctrl->actual_num_bytes > 8 ? (size_t)s_ctrl->actual_num_bytes - 8u : 0u;
        if (n > len)
            n = len;                     /* never past wLength: the caller's buffer holds that much */
        memcpy(in, s_ctrl->data_buffer + 8, n);
        *got = n;
    }
    return ESP_OK;
}

/* 2.0: every clock's rates, by RANGE */
static void ask_clocks(usb_device_handle_t h, struct ju_dev *d)
{
    uint8_t asked[JU_ALTS_MAX], n_asked = 0;
    for (uint8_t i = 0; i < d->n; i++) {
        uint8_t clock = d->alt[i].clock, setup[8], range[JU_RANGE_LEN];
        size_t got = 0;
        int seen = 0;
        for (uint8_t k = 0; k < n_asked; k++)
            seen |= asked[k] == clock;
        if (!clock || seen)
            continue;
        asked[n_asked++] = clock;
        ju_req_uac2_range(setup, clock, d->ac_itf);
        if (control(h, setup, NULL, range, &got) == ESP_OK)
            ESP_LOGI(TAG, "clock %u: %u rates", clock, ju_clock_rates(d, clock, range, got));
        else
            ESP_LOGW(TAG, "clock %u: no RANGE answer", clock);
    }
}

/* the rate, then the alternate setting (2.0); the alternate setting, then the rate (1.0). A device with a single rate
 * may refuse to be told it: that is not an error then. */
static esp_err_t select_stream(usb_device_handle_t h, const struct ju_dev *d, const struct ju_alt *a, uint32_t rate)
{
    uint8_t setup[8], data[4];
    esp_err_t e;
    int only = !a->continuous && a->nrates == 1u;
    if (d->uac2 && a->clock) {
        ju_req_uac2_rate(setup, data, a->clock, d->ac_itf, rate);
        e = control(h, setup, data, NULL, NULL);
        if (e != ESP_OK && !only)
            return e;
    }
    ju_req_set_interface(setup, a->itf, a->alt);
    e = control(h, setup, NULL, NULL, NULL);
    if (e != ESP_OK)
        return e;
    if (!d->uac2 && a->freq_ctrl) {
        ju_req_uac1_rate(setup, data, a->ep, rate);
        e = control(h, setup, data, NULL, NULL);
        if (e != ESP_OK && !only)
            return e;
    }
    return ESP_OK;
}

static void free_transfers(void)
{
    for (uint32_t i = 0; i < XFERS; i++)
        if (s_dev.x[i]) {
            usb_host_transfer_free(s_dev.x[i]);
            s_dev.x[i] = NULL;
        }
}

static esp_err_t start_stream(usb_device_handle_t h, const struct ju_alt *a, uint32_t rate)
{
    esp_err_t e = ESP_OK;
    if (!s_w)
        s_w = heap_caps_malloc(sizeof *s_w, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_w)
        s_w = heap_caps_malloc(sizeof *s_w, MALLOC_CAP_8BIT);
    if (!s_w)
        return ESP_ERR_NO_MEM;
    if (jp_rs_init(&s_w->rs, rate))
        return ESP_ERR_NOT_SUPPORTED;
    for (uint32_t i = 0; i < XFERS && e == ESP_OK; i++) {
        e = usb_host_transfer_alloc((size_t)a->mps * PACKETS, PACKETS, &s_dev.x[i]);
        if (e != ESP_OK)
            break;
        s_dev.x[i]->device_handle = h;
        s_dev.x[i]->bEndpointAddress = a->ep;
        s_dev.x[i]->callback = on_audio;
        s_dev.x[i]->context = NULL;
        s_dev.x[i]->num_bytes = (int)(a->mps * PACKETS);
        for (uint32_t p = 0; p < PACKETS; p++)
            s_dev.x[i]->isoc_packet_desc[p].num_bytes = a->mps;   /* IN packets ask for a whole MPS (usb_types_stack.h) */
    }
    if (e != ESP_OK) {
        free_transfers();
        return e;
    }
    s_dev.mps = a->mps;
    s_dev.channels = a->channels;
    s_dev.subframe = a->subframe;
    s_dev.bits = a->bits;
    s_dev.rate = rate;
    s_dev.active = 1;
    uac_host_stream(1);
    for (uint32_t i = 0; i < XFERS; i++) {
        __atomic_add_fetch(&s_dev.inflight, 1u, __ATOMIC_RELEASE);
        if (usb_host_transfer_submit(s_dev.x[i]) != ESP_OK) {
            __atomic_sub_fetch(&s_dev.inflight, 1u, __ATOMIC_RELEASE);
            ESP_LOGW(TAG, "transfer %u not submitted", (unsigned)i);
        }
    }
    return ESP_OK;
}

static int track(usb_device_handle_t h, int add)
{
    for (uint32_t i = 0; i < OPEN_HANDLES; i++)
        if (add ? !s_open[i] : s_open[i] == h) {
            s_open[i] = add ? h : NULL;
            return 1;
        }
    return 0;
}

static esp_err_t open_dev(uint8_t address, usb_device_handle_t *h)
{
    esp_err_t e = usb_host_device_open(s_client, address, h);
    if (e == ESP_OK && !track(*h, 1)) {
        usb_host_device_close(s_client, *h);
        e = ESP_ERR_NO_MEM;
    }
    return e;
}

/* closes a handle this client holds, once: a device gone while add_device had it may already be closed and freed */
static void close_dev(usb_device_handle_t h)
{
    if (track(h, 0))
        usb_host_device_close(s_client, h);
}

static void wait_for_stream(uint8_t address)
{
    for (uint32_t i = 0; i < WAITING_MAX; i++)
        if (!s_waiting[i] || s_waiting[i] == address) {
            s_waiting[i] = address;
            return;
        }
}

static void add_device(uint8_t address)
{
    usb_device_handle_t h;
    const usb_config_desc_t *c;
    static struct ju_dev d;               /* the worker's alone */
    char name[JK_USB_NAME_MAX + 1];
    struct ju_pick pick;
    const struct ju_alt *a;
    esp_err_t e;
    if (s_dev.h) {
        wait_for_stream(address);        /* one stream: the first device keeps it; this one is tried when it leaves */
        return;
    }
    if (open_dev(address, &h) != ESP_OK)
        return;
    usb_host_product_name(h, name, sizeof name);
    if (jlapp_role_from_name(name) != JLAPP_OTHER) {
        /* A Launchpad X and a Digitone are played over MIDI (usb_lp.c): the controller has 8 pipes (usb_hostlib.c), and a hub (2),
         * the Launchpad (control, OUT, IN) and the Digitone (control, OUT, IN) already take all 8: no room for the Digitone's
         * audio. Deterministic: whichever device enumerates first would otherwise win the pipes. */
        close_dev(h);
        return;
    }
    if (usb_host_get_active_config_descriptor(h, &c) != ESP_OK || !ju_parse((const uint8_t *)c, c->wTotalLength, &d)) {
        close_dev(h);
        return;                          /* no audio out of it: not ours */
    }
    if (d.uac2)
        ask_clocks(h, &d);
    if (ju_choose(&d, &pick)) {
        ESP_LOGW(TAG, "device %u: audio, but no 44.1 / 48 kHz stream that fits", address);
        close_dev(h);
        return;
    }
    a = &d.alt[pick.index];
    e = usb_host_interface_claim(s_client, h, a->itf, a->alt);
    if (e == ESP_OK) {
        e = select_stream(h, &d, a, pick.rate);
        if (e == ESP_OK) {
            const usb_device_desc_t *dd = NULL;
            s_dev.h = h;
            s_dev.itf = a->itf;
            usb_host_product_name(h, s_dev.name, sizeof s_dev.name);
            if (usb_host_get_device_descriptor(h, &dd) == ESP_OK && dd) {
                portENTER_CRITICAL(&s_mux);
                s_dev.vid = dd->idVendor;
                s_dev.pid = dd->idProduct;
                portEXIT_CRITICAL(&s_mux);
            }
            e = start_stream(h, a, pick.rate);
            if (e != ESP_OK)
                s_dev.h = NULL;
        }
        if (e != ESP_OK)
            usb_host_interface_release(s_client, h, a->itf);
    }
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "device %u: %s", address, esp_err_to_name(e));
        close_dev(h);
        return;
    }
    ESP_LOGI(TAG, "device %u '%s': UAC %s, itf %u alt %u, %lu Hz, %u ch, %u bits, mps %u", address, s_dev.name, d.uac2 ? "2.0" : "1.0",
             a->itf, a->alt, (unsigned long)pick.rate, a->channels, a->bits, a->mps);
}

static void remove_device(usb_device_handle_t h)
{
    if (h == s_dev.h) {
        s_dev.active = 0;
        uac_host_stream(0);
        /* the transfers come back with NO_DEVICE once the device is gone; then the interface can be released */
        for (uint32_t t = 0; __atomic_load_n(&s_dev.inflight, __ATOMIC_ACQUIRE) && t < STOP_WAIT_MS; t += 5u)
            vTaskDelay(pdMS_TO_TICKS(5));
        if (__atomic_load_n(&s_dev.inflight, __ATOMIC_ACQUIRE))
            ESP_LOGE(TAG, "%u transfers still out: kept, not freed", (unsigned)s_dev.inflight);
        else {
            free_transfers();
            heap_caps_free(s_w);
            s_w = NULL;
        }
        usb_host_interface_release(s_client, h, s_dev.itf);
        portENTER_CRITICAL(&s_mux);
        s_dev.h = NULL;
        s_dev.rate = 0;
        s_dev.name[0] = 0;
        s_dev.vid = s_dev.pid = 0;
        portEXIT_CRITICAL(&s_mux);
        close_dev(h);
        /* the devices passed over get their turn (a stale address fails to open, or is just another candidate) */
        for (uint32_t i = 0; i < WAITING_MAX; i++)
            if (s_waiting[i]) {
                struct event ev = {EV_NEW, s_waiting[i], NULL};
                s_waiting[i] = 0;
                xQueueSend(s_events, &ev, 0);
            }
        return;
    }
    close_dev(h);
}

static void worker_task(void *arg)
{
    (void)arg;
    for (;;) {
        struct event ev;
        if (xQueueReceive(s_events, &ev, portMAX_DELAY) != pdTRUE)
            continue;
        if (ev.kind == EV_NEW)
            add_device(ev.address);
        else
            remove_device(ev.h);
    }
}

static void client_task(void *arg)
{
    (void)arg;
    for (;;)
        usb_host_client_handle_events(s_client, portMAX_DELAY);
}

esp_err_t usb_uac_host_start(void)
{
    usb_host_client_config_t client = {
        .is_synchronous = false,
        .max_num_event_msg = EVENTS,
        .async = {.client_event_callback = on_event},
    };
    esp_err_t e;
    s_events = xQueueCreate(EVENTS, sizeof(struct event));
    s_ctrl_done = xSemaphoreCreateBinary();
    if (!s_events || !s_ctrl_done)
        return ESP_ERR_NO_MEM;
    e = usb_hostlib_start();
    if (e == ESP_OK)
        e = usb_host_client_register(&client, &s_client);
    if (e == ESP_OK)
        e = usb_host_transfer_alloc(8u + CTRL_DATA_MAX, 0, &s_ctrl);
    if (e != ESP_OK)
        return e;
    if (xTaskCreatePinnedToCore(client_task, "usb_uac_cl", 3072, NULL, CLIENT_PRIO, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    return xTaskCreatePinnedToCore(worker_task, "usb_uac", 4096, NULL, WORKER_PRIO, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

int usb_uac_host_id(uint16_t *vid, uint16_t *pid)
{
    int on;
    portENTER_CRITICAL(&s_mux);
    on = s_dev.h && s_dev.rate && (s_dev.vid || s_dev.pid);
    *vid = s_dev.vid;
    *pid = s_dev.pid;
    portEXIT_CRITICAL(&s_mux);
    return on;
}

void usb_uac_host_list(struct jk_usb *u)
{
    char name[JK_USB_NAME_MAX + 1];
    uint32_t rate;
    portENTER_CRITICAL(&s_mux);
    rate = s_dev.h ? s_dev.rate : 0u;
    memcpy(name, s_dev.name, sizeof name);
    u->audio_packets = s_dev.packets;
    u->audio_errors = s_dev.errors;
    portEXIT_CRITICAL(&s_mux);
    if (!rate)
        return;
    u->audio_rate = rate;
    u->audio_channels = s_dev.channels;
    u->audio_bits = s_dev.bits;
    usb_host_list_add(u, name, 0, 1);
}
