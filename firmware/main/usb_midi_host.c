/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode: the clock to USB-MIDI devices on the port, up to JH_DEVICES of them through a hub. Output only, with
 * ESP-IDF's USB host library (usb_hostlib.c installs it; this is one of its clients); there is no MIDI class driver in
 * it, so this finds the bulk OUT endpoint of the first MIDI streaming interface by itself (USB MIDI 1.0: audio class 1,
 * subclass 3, 4-byte event packets on bulk endpoints). The packets are usb.c's, the same ticks, starts and stops the
 * MIDI OUT jack gets (midi.c's interrupt feeds both). Power is not jank's: on a XIAO the 5V pin is the socket's VBUS with
 * no diode, and a hub has its own; jank mini 2.2 puts only a current-limited 5 V signal on VBUS (usbrole.c). */
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "usb/usb_helpers.h"
#include "usb/usb_host.h"
#include "jank_midiq.h"
#include "usb_host_mode.h"

#define JH_DEVICES 4u
#define EVENTS 8u
#define AUDIO_CLASS 1u
#define MIDI_STREAMING 3u
#define BULK 2u
#define PACKET_MAX 64u
#define STOP_TRIES 20                  /* 5 ms each: a transfer in flight ends when the device has gone */
#define CLIENT_PRIO 23                 /* a sent packet's completion frees the device for the next tick: soon after it */

struct dev {
    usb_device_handle_t h;
    usb_transfer_t *t;
    uint8_t itf;
    uint16_t mps;
    volatile uint8_t busy;
    struct jmq q;                      /* packets that came while the transfer was out (s_lock) */
    char name[JK_USB_NAME_MAX + 1];
};

struct event {
    uint8_t address;                   /* 0: gone */
    usb_device_handle_t gone;
};

static const char *TAG = "usb host";
static struct dev s_dev[JH_DEVICES];
static usb_host_client_handle_t s_client;
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_events;
static uint32_t s_dropped;

/* the waiting packets, up to mps bytes, go out in one transfer; the rest keeps waiting. s_lock held, d->busy 0 */
static void submit_pending(struct dev *d)
{
    size_t len = jmq_take(&d->q, d->t->data_buffer, d->mps);
    if (!len)
        return;
    d->t->num_bytes = (int)len;
    d->busy = 1;
    if (usb_host_transfer_submit(d->t) != ESP_OK) {
        d->busy = 0;
        s_dropped += (uint32_t)(len / 4u);
    }
}

/* (the client task, from usb_host_client_handle_events) the packets that waited go out at once */
static void on_sent(usb_transfer_t *t)
{
    struct dev *d = t->context;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (t->status != USB_TRANSFER_STATUS_COMPLETED)
        s_dropped += (uint32_t)(t->num_bytes / 4);
    d->busy = 0;
    if (d->h)                          /* (a device being removed: nothing more) */
        submit_pending(d);
    xSemaphoreGive(s_lock);
}

static struct dev *free_place(void)
{
    for (uint32_t i = 0; i < JH_DEVICES; i++)
        if (!s_dev[i].h)
            return &s_dev[i];
    return NULL;
}

/* the first MIDI streaming interface's bulk OUT, alternate setting 0 -> 1 */
static int find_midi_out(const usb_config_desc_t *c, uint8_t *itf, uint8_t *ep, uint16_t *mps)
{
    const usb_standard_desc_t *cur = (const usb_standard_desc_t *)c;
    int offset = 0, inside = 0;
    while ((cur = usb_parse_next_descriptor(cur, c->wTotalLength, &offset)) != NULL) {
        if (cur->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t *i = (const usb_intf_desc_t *)cur;
            inside = i->bInterfaceClass == AUDIO_CLASS && i->bInterfaceSubClass == MIDI_STREAMING && i->bAlternateSetting == 0;
            if (inside)
                *itf = i->bInterfaceNumber;
        } else if (inside && cur->bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
            const usb_ep_desc_t *e = (const usb_ep_desc_t *)cur;
            if ((e->bmAttributes & 3u) == BULK && !(e->bEndpointAddress & 0x80u)) {
                *ep = e->bEndpointAddress;
                *mps = e->wMaxPacketSize & 0x7FFu;
                return 1;
            }
        }
    }
    return 0;
}

static esp_err_t claim(struct dev *d, usb_device_handle_t h, uint8_t itf, uint8_t ep, uint16_t mps)
{
    esp_err_t e = usb_host_interface_claim(s_client, h, itf, 0);
    if (e != ESP_OK)
        return e;
    e = usb_host_transfer_alloc(mps, 0, &d->t);
    if (e != ESP_OK) {
        usb_host_interface_release(s_client, h, itf);
        return e;
    }
    d->t->device_handle = h;
    d->t->bEndpointAddress = ep;
    d->t->callback = on_sent;
    d->t->context = d;
    d->itf = itf;
    d->mps = mps;
    d->busy = 0;
    memset(&d->q, 0, sizeof d->q);
    usb_host_product_name(h, d->name, sizeof d->name);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    d->h = h;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

static void add_device(uint8_t address)
{
    usb_device_handle_t h;
    const usb_config_desc_t *c;
    struct dev *d = free_place();
    uint8_t itf = 0, ep = 0;
    uint16_t mps = 0;
    esp_err_t e = usb_host_device_open(s_client, address, &h);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "open %u: %s", address, esp_err_to_name(e));
        return;
    }
    e = usb_host_get_active_config_descriptor(h, &c);
    if (e == ESP_OK && d && find_midi_out(c, &itf, &ep, &mps) && mps >= 4u)
        e = claim(d, h, itf, ep, mps > PACKET_MAX ? PACKET_MAX : mps);
    else
        e = ESP_ERR_NOT_SUPPORTED;      /* no MIDI out, or no place: not ours */
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "device %u left alone: %s", address, esp_err_to_name(e));
        usb_host_device_close(s_client, h);
    } else {
        ESP_LOGI(TAG, "device %u: MIDI out", address);
    }
}

static void remove_device(usb_device_handle_t h)
{
    for (uint32_t i = 0; i < JH_DEVICES; i++) {
        struct dev *d = &s_dev[i];
        if (d->h != h)
            continue;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        d->h = NULL;
        xSemaphoreGive(s_lock);
        for (int tries = 0; d->busy && tries < STOP_TRIES; tries++)
            usb_host_client_handle_events(s_client, pdMS_TO_TICKS(5));
        usb_host_interface_release(s_client, h, d->itf);
        usb_host_transfer_free(d->t);
        d->t = NULL;
        break;
    }
    usb_host_device_close(s_client, h);
}

static void on_event(const usb_host_client_event_msg_t *m, void *arg)
{
    struct event ev = {0, NULL};
    (void)arg;
    if (m->event == USB_HOST_CLIENT_EVENT_NEW_DEV)
        ev.address = m->new_dev.address;
    else if (m->event == USB_HOST_CLIENT_EVENT_DEV_GONE)
        ev.gone = m->dev_gone.dev_hdl;
    else
        return;
    xQueueSend(s_events, &ev, 0);
}

static void client_task(void *arg)
{
    (void)arg;
    for (;;) {
        struct event ev;
        usb_host_client_handle_events(s_client, portMAX_DELAY);
        while (xQueueReceive(s_events, &ev, 0) == pdTRUE) {
            if (ev.address)
                add_device(ev.address);
            else
                remove_device(ev.gone);
        }
    }
}

esp_err_t usb_midi_host_start(void)
{
    usb_host_client_config_t client = {
        .is_synchronous = false,
        .max_num_event_msg = EVENTS,
        .async = {.client_event_callback = on_event},
    };
    esp_err_t e;
    s_lock = xSemaphoreCreateMutex();
    s_events = xQueueCreate(EVENTS, sizeof(struct event));
    if (!s_lock || !s_events)
        return ESP_ERR_NO_MEM;
    e = usb_hostlib_start();
    if (e == ESP_OK)
        e = usb_host_client_register(&client, &s_client);
    if (e != ESP_OK)
        return e;
    return xTaskCreatePinnedToCore(client_task, "usb_midi_cl", 4096, NULL, CLIENT_PRIO, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

/* the devices that take the clock, for `status` (names merged with the audio client's by usbrole.c) */
void usb_midi_host_list(struct jk_usb *u)
{
    if (!s_lock)
        return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (uint32_t i = 0; i < JH_DEVICES; i++)
        if (s_dev[i].h)
            usb_host_list_add(u, s_dev[i].name, 1, 0);
    u->midi_dropped = s_dropped;
    xSemaphoreGive(s_lock);
}

/* each device's packets in order (core/jank_midiq.h): a device still busy with its last transfer gets these in its
 * next one, sent from on_sent as soon as that completes; no F8 is dropped for a busy transfer */
void usb_midi_host_send(const uint8_t *packets, size_t n)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (uint32_t i = 0; i < JH_DEVICES; i++) {
        struct dev *d = &s_dev[i];
        if (!d->h)
            continue;
        s_dropped += jmq_add(&d->q, packets, n);
        if (!d->busy)
            submit_pending(d);
    }
    xSemaphoreGive(s_lock);
}
