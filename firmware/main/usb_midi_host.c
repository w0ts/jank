/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode: the clock to USB-MIDI devices on the port, up to JH_DEVICES of them through a hub. With ESP-IDF's USB host
 * library (usb_hostlib.c installs it; this is one of its clients); there is no MIDI class driver in it, so this finds the bulk
 * endpoints of one MIDI streaming interface by itself (USB MIDI 1.0: audio class 1, subclass 3, 4-byte event packets on
 * bulk endpoints). The packets are usb.c's, the same ticks, starts and stops the MIDI OUT jack gets (midi.c's interrupt feeds
 * both). Power is not jank's: on a XIAO the 5V pin is the socket's VBUS with no diode, and a hub has its own; jank mini 2.2
 * puts only a current-limited 5 V signal on VBUS (usbrole.c).
 *
 * A Launchpad X and a Digitone (told apart by product name, core/jank_lpapp.h) are also read: their bulk IN endpoint is
 * opened and what they send goes to usb_lp.c, which plays the Digitone from the Launchpad's pads. The Launchpad's MIDI
 * interface is its second one (its first is DAW). Every other device stays output only: the clock. */
#include <string.h>
#include "app.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "usb/usb_helpers.h"
#include "usb/usb_host.h"
#include "jank_fsclamp.h"
#include "jank_midiq.h"
#include "usb_host_mode.h"

#define JH_DEVICES 4u
#define EVENTS 8u
#define AUDIO_CLASS 1u
#define AUDIO_STREAMING 2u
#define MIDI_STREAMING 3u
#define BULK 2u
#define PACKET_MAX 64u
#define STOP_TRIES 20                  /* 5 ms each: a transfer in flight ends when the device has gone */
#define CLIENT_PRIO 23                 /* a sent packet's completion frees the device for the next tick: soon after it */
#define CLIENT_STACK 5120              /* the client task runs the callbacks, usb_lp.c's frame and queues included (4096 was thought tight, 6144 took 2 KB
                                          of a 13 KB internal heap): the margin it leaves is reported in diag (hostdiag_mem client_stack_free) */
#define LEARN_DELAY_MS 3000u          /* after a controller that could not be claimed: the rest of the dock has come by then */
#define IN_RETRIES 3                   /* a stalled or failed IN transfer is tried again at once this many times in a row, */
#define IN_BACKOFF_MS 200u             /* ... then again after a wait that doubles from this, */
#define IN_BACKOFF_MAX_MS 2000u        /* ... up to this, for as long as the device is attached: never given up (a Digitone that stopped
                                        * answering while still listed, reported by its session 2026-11-25: an IN loop given up was a suspect) */

struct dev {
    usb_device_handle_t h;
    usb_transfer_t *t;                 /* OUT */
    usb_transfer_t *in;                /* IN: only for a device with a role */
    uint8_t itf;
    uint8_t in_ep;
    uint16_t mps;
    uint16_t in_mps;
    volatile uint8_t busy;             /* the OUT transfer is out */
    volatile uint8_t in_busy;          /* the IN transfer is out */
    uint8_t in_fails;                  /* IN transfers in a row that ended in an error */
    uint8_t in_waiting;                /* the IN transfer waits for in_retry_at (the backoff) */
    TickType_t in_retry_at;
    uint8_t leaked;                    /* an IN transfer was still out when the device went: its callback still points here, the slot is not reused */
    uint8_t dg;                        /* its line in the probe (g_hostdiag.dev) */
    enum jlapp_role role;
    struct jmq q;                      /* packets that came while the transfer was out (s_lock) */
    char name[JK_USB_NAME_MAX + 1];
};

struct event {
    uint8_t address;                   /* 0: gone */
    usb_device_handle_t gone;
};

struct midi_eps {
    uint8_t index;                     /* which MIDI streaming interface (0 = the first) */
    uint8_t itf, out, in;              /* the interface and its first bulk OUT and IN endpoint addresses (in 0: none) */
    uint8_t neps;                      /* its bNumEndpoints: a claim books all of them */
    uint16_t out_mps, in_mps;
};

static const char *TAG = "usb host";
static struct dev s_dev[JH_DEVICES];
static usb_host_client_handle_t s_client;
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_events;
static uint32_t s_dropped;
static uint8_t s_start_step;           /* the step of usb_midi_host_start under way, for a failure's report */
static TaskHandle_t s_client_task;
static struct jhf_set s_useless;       /* devices seen with neither a MIDI nor an audio interface (client task only): what the unit may learn to ignore */
static uint8_t s_learn_pending;        /* a Launchpad or a Digitone could not be claimed for want of pipes: s_learn_at says when to learn */
static TickType_t s_learn_at;

/* the probe's line for a claimed device (core/jank_hostdiag.h): lines go by arrival, so a device that was left alone keeps its own */
static struct jhd_dev *dg(const struct dev *d) { return &g_hostdiag.dev[d->dg % JHD_DEVS]; }

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

/* (the client task, from usb_host_client_handle_events) the packets that waited go out at once (client_task then lets usb_lp.c
 * send what it has been waiting to) */
static void on_sent(usb_transfer_t *t)
{
    struct dev *d = t->context;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (t->status != USB_TRANSFER_STATUS_COMPLETED) {
        s_dropped += (uint32_t)(t->num_bytes / 4);
        dg(d)->out_fail++;
    } else {
        dg(d)->out_done++;
    }
    d->busy = 0;
    if (d->h)                          /* (a device being removed: nothing more) */
        submit_pending(d);
    xSemaphoreGive(s_lock);
}

/* an IN transfer is out again, unless the device is going: s_lock not held. 1: submitted */
static int submit_in(struct dev *d)
{
    d->in->num_bytes = d->in_mps;
    d->in_busy = 1;
    if (usb_host_transfer_submit(d->in) != ESP_OK) {
        d->in_busy = 0;
        ESP_LOGW(TAG, "IN transfer not submitted: pads / notes from '%s' stop", d->name);
        return 0;
    }
    return 1;
}

/* the wait before IN try number `fails` (past the IN_RETRIES at once): IN_BACKOFF_MS doubled each time, at most IN_BACKOFF_MAX_MS */
static uint32_t in_backoff_ms(uint8_t fails)
{
    unsigned extra = fails > IN_RETRIES ? fails - IN_RETRIES - 1u : 0u;
    uint32_t ms = IN_BACKOFF_MS << (extra < 4u ? extra : 4u);
    return ms < IN_BACKOFF_MAX_MS ? ms : IN_BACKOFF_MAX_MS;
}

/* (the client task) what a device with a role sent: to usb_lp.c, then the next transfer. A stall or an error is tried again (the
 * endpoint cleared): IN_RETRIES times at once, then after a growing wait (in_retry_due), for as long as the device is attached;
 * a cancelled transfer or a device that is gone ends the loop. diag counts the errors (in_err) and keeps the last status (in_status) */
static void on_in(usb_transfer_t *t)
{
    struct dev *d = t->context;
    usb_transfer_status_t st = t->status;
    if (st == USB_TRANSFER_STATUS_COMPLETED) {
        d->in_fails = 0;
        if (t->actual_num_bytes >= 4) {
            size_t whole = (size_t)t->actual_num_bytes & ~3u;
            dg(d)->in_packets += (uint32_t)(whole / 4u);
            memcpy(dg(d)->in_last, t->data_buffer + whole - 4u, 4);
            usb_lp_rx(d->role, t->data_buffer, whole);
        }
    } else if (d->h && (st == USB_TRANSFER_STATUS_STALL || st == USB_TRANSFER_STATUS_ERROR || st == USB_TRANSFER_STATUS_OVERFLOW)) {
        dg(d)->in_errors++;
        dg(d)->in_last_status = (uint8_t)st;
        if (d->in_fails < UINT8_MAX)
            d->in_fails++;
        usb_host_endpoint_clear(d->h, d->in_ep);
        if (d->in_fails > IN_RETRIES) {          /* failing for a while: again after the backoff, from the client task's loop */
            ESP_LOGW(TAG, "IN transfer of '%s' ended with status %d: again in %lu ms", d->name, (int)st, (unsigned long)in_backoff_ms(d->in_fails));
            d->in_retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(in_backoff_ms(d->in_fails));
            d->in_waiting = 1;
            d->in_busy = 0;
            return;
        }
        ESP_LOGW(TAG, "IN transfer of '%s' ended with status %d: try %u", d->name, (int)st, d->in_fails);
    } else {
        if (d->h)
            ESP_LOGW(TAG, "IN transfer of '%s' ended with status %d: no more input from it", d->name, (int)st);
        d->in_busy = 0;
        return;
    }
    if (!d->h || !submit_in(d))
        d->in_busy = 0;
}

/* (the client task) the IN transfers whose backoff is over go out again */
static void in_retry_due(void)
{
    for (uint32_t i = 0; i < JH_DEVICES; i++) {
        struct dev *d = &s_dev[i];
        if (!d->in_waiting || (int32_t)(xTaskGetTickCount() - d->in_retry_at) < 0)
            continue;
        d->in_waiting = 0;
        if (d->h && d->in && !d->in_busy)
            submit_in(d);
    }
}

/* the earliest IN backoff that ends, as ticks from now; portMAX_DELAY: none waits */
static TickType_t in_wait_ticks(void)
{
    TickType_t w = portMAX_DELAY;
    for (uint32_t i = 0; i < JH_DEVICES; i++) {
        int32_t left;
        if (!s_dev[i].in_waiting)
            continue;
        left = (int32_t)(s_dev[i].in_retry_at - xTaskGetTickCount());
        if ((TickType_t)(left > 0 ? left : 0) < w)
            w = (TickType_t)(left > 0 ? left : 0);
    }
    return w;
}

static struct dev *free_place(void)
{
    for (uint32_t i = 0; i < JH_DEVICES; i++)
        if (!s_dev[i].h && !s_dev[i].leaked)
            return &s_dev[i];
    return NULL;
}

static int is_midi_streaming(const usb_standard_desc_t *cur)
{
    const usb_intf_desc_t *i = (const usb_intf_desc_t *)cur;
    return cur->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE && i->bInterfaceClass == AUDIO_CLASS
        && i->bInterfaceSubClass == MIDI_STREAMING && i->bAlternateSetting == 0;
}

static unsigned count_midi_interfaces(const usb_config_desc_t *c)
{
    const usb_standard_desc_t *cur = (const usb_standard_desc_t *)c;
    int offset = 0;
    unsigned n = 0;
    while ((cur = usb_parse_next_descriptor(cur, c->wTotalLength, &offset)) != NULL)
        n += (unsigned)is_midi_streaming(cur);
    return n;
}

/* 1: the device has an audio streaming interface (usb_uac_host.c may want it) */
static int has_audio_streaming(const usb_config_desc_t *c)
{
    const usb_standard_desc_t *cur = (const usb_standard_desc_t *)c;
    int offset = 0;
    while ((cur = usb_parse_next_descriptor(cur, c->wTotalLength, &offset)) != NULL) {
        const usb_intf_desc_t *i = (const usb_intf_desc_t *)cur;
        if (cur->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE && i->bInterfaceClass == AUDIO_CLASS && i->bInterfaceSubClass == AUDIO_STREAMING)
            return 1;
    }
    return 0;
}

/* the index-th MIDI streaming interface's (alternate setting 0) first bulk OUT and IN. 1: it has an OUT endpoint */
static int eps_of_interface(const usb_config_desc_t *c, unsigned index, struct midi_eps *m)
{
    const usb_standard_desc_t *cur = (const usb_standard_desc_t *)c;
    int offset = 0, inside = 0;
    unsigned seen = 0;
    memset(m, 0, sizeof *m);
    while ((cur = usb_parse_next_descriptor(cur, c->wTotalLength, &offset)) != NULL) {
        if (cur->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            inside = 0;
            if (is_midi_streaming(cur)) {
                inside = seen++ == index;
                if (inside) {
                    m->itf = ((const usb_intf_desc_t *)cur)->bInterfaceNumber;
                    m->neps = ((const usb_intf_desc_t *)cur)->bNumEndpoints;
                    m->index = (uint8_t)index;
                }
            }
        } else if (inside && cur->bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
            const usb_ep_desc_t *e = (const usb_ep_desc_t *)cur;
            if ((e->bmAttributes & 3u) != BULK)
                continue;
            if (!(e->bEndpointAddress & 0x80u) && !m->out) {
                m->out = e->bEndpointAddress;
                m->out_mps = e->wMaxPacketSize & 0x7FFu;
            } else if ((e->bEndpointAddress & 0x80u) && !m->in) {
                m->in = e->bEndpointAddress;
                m->in_mps = e->wMaxPacketSize & 0x7FFu;
            }
        }
    }
    return m->out != 0;
}

/* The interface to use: a device with a role takes its own (jlapp_midi_interface_index; an index past the last takes the last);
 * any other takes the first one that has a bulk OUT, as before the IN endpoints were read. 1: found */
static int find_midi(const usb_config_desc_t *c, unsigned index, enum jlapp_role role, struct midi_eps *m)
{
    unsigned count = count_midi_interfaces(c);
    if (!count)
        return 0;
    if (index >= count)
        index = count - 1u;
    for (unsigned i = index; i < count; i++) {
        if (eps_of_interface(c, i, m))
            return 1;
        if (role != JLAPP_OTHER)
            break;
    }
    return 0;
}

/* where: how far it got (the probe's claim_where): 1 the interface, 2 the OUT transfer, 3 the IN transfer */
static esp_err_t claim(struct dev *d, usb_device_handle_t h, const struct midi_eps *m, enum jlapp_role role, uint8_t *where)
{
    uint16_t mps = m->out_mps > PACKET_MAX ? PACKET_MAX : m->out_mps;
    esp_err_t e;
    *where = 1;
    e = usb_host_interface_claim(s_client, h, m->itf, 0);
    if (e != ESP_OK)
        return e;
    *where = 2;
    e = usb_host_transfer_alloc(mps, 0, &d->t);
    if (e == ESP_OK && role != JLAPP_OTHER && m->in && m->in_mps >= 4u) {
        *where = 3;
        e = usb_host_transfer_alloc(m->in_mps, 0, &d->in);
        if (e != ESP_OK) {
            usb_host_transfer_free(d->t);
            d->t = NULL;
        }
    }
    if (e != ESP_OK) {
        usb_host_interface_release(s_client, h, m->itf);
        return e;
    }
    d->t->device_handle = h;
    d->t->bEndpointAddress = m->out;
    d->t->callback = on_sent;
    d->t->context = d;
    d->itf = m->itf;
    d->mps = mps;
    d->busy = 0;
    d->in_busy = 0;
    d->in_fails = 0;
    d->in_waiting = 0;
    d->role = d->in ? role : JLAPP_OTHER;      /* a role needs the IN endpoint: without it the device is the clock's only */
    d->in_ep = 0;
    if (d->in) {
        d->in->device_handle = h;
        d->in->bEndpointAddress = m->in;
        d->in->callback = on_in;
        d->in->context = d;
        d->in_ep = m->in;
        d->in_mps = m->in_mps;
    }
    memset(&d->q, 0, sizeof d->q);
    usb_host_product_name(h, d->name, sizeof d->name);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    d->h = h;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

/* A device on a full-speed link may declare its high-speed bulk sizes (the Digitone II: 512 bytes), which the library's claim refuses for good
 * (core/jank_fsclamp.h). The wire allows 64: the declared size is corrected in the library's own heap copy of the configuration descriptor
 * (usbh.c keeps one per device; the claim reads the same one), here in the client task before anything claims it. Returns the endpoints changed. */
static unsigned clamp_full_speed(usb_device_handle_t h, const usb_config_desc_t *c)
{
    usb_device_info_t info;
    if (usb_host_device_info(h, &info) != ESP_OK || info.speed != USB_SPEED_FULL)
        return 0;
    return jfc_clamp_fs_bulk((uint8_t *)(uintptr_t)c, c->wTotalLength);
}

/* a line in the probe for a device that came: who it is and how many MIDI interfaces it has (the claim fills in the rest) */
static struct jhd_dev *probe_begin(usb_device_handle_t h, const char *name, enum jlapp_role role, const usb_config_desc_t *c, uint8_t *idx)
{
    const usb_device_desc_t *dd = NULL;
    struct jhd_dev *r;
    *idx = (uint8_t)(g_hostdiag.add_calls++ % JHD_DEVS);
    r = &g_hostdiag.dev[*idx];
    memset(r, 0, sizeof *r);
    r->used = 1;
    r->role = (uint8_t)role;
    r->claim_err = -1;
    strlcpy(r->name, name, sizeof r->name);
    if (usb_host_get_device_descriptor(h, &dd) == ESP_OK && dd) {
        r->vid = dd->idVendor;
        r->pid = dd->idProduct;
    }
    if (c)
        r->ifs = (uint8_t)count_midi_interfaces(c);
    return r;
}

/* A controller whose claim failed for want of memory (or pipes) is tried again a moment later: the shortage can be a passing one (the Wi-Fi service's
 * buffer, allocated at its first request, landed in the same seconds as the claims: measured 2026-11-24, the Launchpad's claim failed with 257 with 20 KB
 * of heap free, after it had worked on the same build). The library keeps the device attached meanwhile; the retry reuses its line in the probe.
 * 6 tries 1.5 s apart (9 s) was too short: measured 2026-11-24, after a Wi-Fi update's restart the Launchpad's claim failed with 257 for over 30 s (the update
 * client and diag reads were asking the unit questions) and stayed unclaimed until it was replugged, though it claimed at once then. So: every 2 s for two minutes. */
#define CLAIM_RETRY_MS 2000u
#define CLAIM_RETRIES 60u
static uint8_t s_retry_addr, s_retry_left;   /* the device to try again, and how many tries remain (0: none waiting) */
static int s_retry_idx = -1;                 /* its line in the probe */
static TickType_t s_retry_at;

/* reuse: -1 for a device that has just come, else its line in the probe (a retry); tries_left: retries still allowed after this one */
static void add_device(uint8_t address, int reuse, uint8_t tries_left)
{
    usb_device_handle_t h;
    const usb_config_desc_t *c;
    struct dev *d = free_place();
    struct midi_eps m;
    char name[JK_USB_NAME_MAX + 1];
    enum jlapp_role role;
    struct jhd_dev *r;
    uint8_t idx;
    unsigned fixed;
    esp_err_t e = usb_host_device_open(s_client, address, &h);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "open %u: %s", address, esp_err_to_name(e));
        return;
    }
    usb_host_product_name(h, name, sizeof name);
    role = jlapp_role_from_name(name);
    e = usb_host_get_active_config_descriptor(h, &c);
    if (reuse >= 0) {                  /* a retry: the same line, not a new one */
        idx = (uint8_t)reuse;
        r = &g_hostdiag.dev[idx % JHD_DEVS];
        r->claim_err = -1;
    } else {
        fixed = e == ESP_OK ? clamp_full_speed(h, c) : 0;
        r = probe_begin(h, name, role, e == ESP_OK ? c : NULL, &idx);
        r->clamped = (uint8_t)(fixed < UINT8_MAX ? fixed : UINT8_MAX);
        if (e == ESP_OK && role == JLAPP_OTHER && !r->ifs && !has_audio_streaming(c) && (r->vid || r->pid))
            jhf_set_add(&s_useless, r->vid, r->pid);     /* of no use to jank, and it holds a pipe of the controller's 8 (core/jank_hostfilter.h) */
    }
    if (e == ESP_OK && d && find_midi(c, jlapp_midi_interface_index(role), role, &m) && m.out_mps >= 4u) {
        r->out_mps = m.out_mps;        /* what the library is about to be asked to book (its FIFO limits are checked against these) */
        r->in_mps = m.in_mps;
        r->neps = m.neps;
        e = claim(d, h, &m, role, &r->claim_where);
        r->itf_index = m.index;
        r->out_ep = m.out;
        r->in_ep = d->in ? m.in : 0;
        if (e == ESP_OK)
            d->dg = idx;
    } else {
        e = ESP_ERR_NOT_SUPPORTED;      /* no MIDI out, or no place: not ours */
    }
    r->claim_err = (int32_t)e;
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "device %u left alone: %s", address, esp_err_to_name(e));
        usb_host_device_close(s_client, h);
        if (role != JLAPP_OTHER) {     /* what the heap looked like (diag fail_dma): a claim books two 512-aligned DMA descriptor lists per endpoint */
            void *probe = heap_caps_aligned_alloc(512, 512, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            g_hostdiag.fail_aligned = probe != NULL;
            heap_caps_free(probe);
            g_hostdiag.fail_dma_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            g_hostdiag.fail_dma_big = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        }
        if (role != JLAPP_OTHER && (e == ESP_ERR_NO_MEM || e == ESP_ERR_NOT_SUPPORTED)) {   /* a controller, and no pipes (or memory) for it */
            if (!s_learn_pending) {
                s_learn_pending = 1;
                s_learn_at = xTaskGetTickCount() + pdMS_TO_TICKS(LEARN_DELAY_MS);
            }
            if (tries_left > 0) {      /* maybe a passing shortage: the same device again soon */
                s_retry_addr = address;
                s_retry_idx = idx;
                s_retry_left = tries_left;
                s_retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(CLAIM_RETRY_MS);
            }
        }
        return;
    }
    ESP_LOGI(TAG, "device %u: MIDI out%s", address, d->in ? " and in" : "");
    if (d->in) {
        usb_lp_attach(d->role);
        submit_in(d);
    }
}

static void remove_device(usb_device_handle_t h)
{
    for (uint32_t i = 0; i < JH_DEVICES; i++) {
        struct dev *d = &s_dev[i];
        enum jlapp_role role = d->role;
        if (d->h != h)
            continue;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        d->h = NULL;
        xSemaphoreGive(s_lock);
        if (d->in) {                   /* ends the IN transfer that waits for the device (its callback runs with a status that is not completed) */
            usb_host_endpoint_halt(h, d->in_ep);
            usb_host_endpoint_flush(h, d->in_ep);
        }
        for (int tries = 0; (d->busy || d->in_busy) && tries < STOP_TRIES; tries++)
            usb_host_client_handle_events(s_client, pdMS_TO_TICKS(5));
        if (d->in) {
            if (d->in_busy) {          /* never free a transfer that is out: keep it, and keep its slot out of use (its callback points here) */
                ESP_LOGW(TAG, "IN transfer still out: not freed, its slot stays unused");
                d->leaked = 1;
            } else {
                usb_host_transfer_free(d->in);
                d->in = NULL;
            }
            usb_lp_detach(role);
        }
        usb_host_interface_release(s_client, h, d->itf);
        usb_host_transfer_free(d->t);
        d->t = NULL;
        d->role = JLAPP_OTHER;
        break;
    }
    usb_host_device_close(s_client, h);
}

static void on_event(const usb_host_client_event_msg_t *m, void *arg)
{
    struct event ev = {0, NULL};
    (void)arg;
    if (m->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        ev.address = m->new_dev.address;
        g_hostdiag.new_dev++;
    } else if (m->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ev.gone = m->dev_gone.dev_hdl;
        g_hostdiag.gone_dev++;
    } else {
        return;
    }
    xQueueSend(s_events, &ev, 0);
}

/* A controller could not be claimed (add_device), and a while later the useless devices seen go on the list that is kept in flash, and the unit
 * restarts: the next start leaves them alone (usb_hostlib.c's filter) and their pipes are free. A restart only when something new was learned and
 * the list was saved: a list that is full, or already holds them, ends it (core/jank_hostfilter.h). */
static void learn_and_restart(void)
{
    s_learn_pending = 0;
    if (!jhf_set_merge(&g_hostdiag.filter.deny, &s_useless))
        return;                        /* nothing new: the controller failed for another reason */
    if (hostdiag_skiplist_save(&g_hostdiag.filter.deny) != ESP_OK) {
        ESP_LOGW(TAG, "list of ignored devices not saved: no restart");
        return;
    }
    ESP_LOGW(TAG, "%u device(s) of no use hold pipes the controllers need: saved, restarting to leave them alone", g_hostdiag.filter.deny.n);
    usbrole_host_inflight(0);          /* a restart of ours, not a crash */
    vTaskDelay(pdMS_TO_TICKS(100));
    restart_why_set("host-learn");
    esp_restart();
}

/* how long the client task may sleep: usb_lp.c's wish, and no longer than the wait for learning */
static TickType_t client_wait_ticks(void)
{
    TickType_t w = usb_lp_wait_ticks();
    if (s_learn_pending) {
        int32_t left = (int32_t)(s_learn_at - xTaskGetTickCount());
        TickType_t t = left > 0 ? (TickType_t)left : 0;
        if (t < w)
            w = t;
    }
    if (s_retry_left) {
        int32_t left = (int32_t)(s_retry_at - xTaskGetTickCount());
        TickType_t t = left > 0 ? (TickType_t)left : 0;
        if (t < w)
            w = t;
    }
    if (in_wait_ticks() < w)
        w = in_wait_ticks();
    return w;
}

/* the controller that could not be claimed is tried again (it may fail again, and then wait once more, up to CLAIM_RETRIES times in all) */
static void retry_due(void)
{
    uint8_t tries = s_retry_left;
    s_retry_left = 0;                  /* add_device sets it again if the claim fails again */
    add_device(s_retry_addr, s_retry_idx, (uint8_t)(tries - 1u));
}

static void client_task(void *arg)
{
    (void)arg;
    for (;;) {
        struct event ev;
        usb_host_client_handle_events(s_client, client_wait_ticks());   /* (the callbacks run in here) */
        while (xQueueReceive(s_events, &ev, 0) == pdTRUE) {
            if (ev.address)
                add_device(ev.address, -1, CLAIM_RETRIES);
            else
                remove_device(ev.gone);
        }
        if (s_retry_left && (int32_t)(xTaskGetTickCount() - s_retry_at) >= 0)
            retry_due();
        in_retry_due();
        usb_lp_pump();                 /* what a callback, a device or the clock has made room for */
        if (s_learn_pending && (int32_t)(xTaskGetTickCount() - s_learn_at) >= 0)
            learn_and_restart();
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
    s_start_step = JHD_STEP_HOSTLIB;
    s_lock = xSemaphoreCreateMutex();
    s_events = xQueueCreate(EVENTS, sizeof(struct event));
    if (!s_lock || !s_events)
        return ESP_ERR_NO_MEM;
    hostdiag_start();
    s_start_step = JHD_STEP_HOSTLIB;
    e = usb_hostlib_start();
    g_hostdiag.lib_up = (uint8_t)(e == ESP_OK);
    if (e == ESP_OK) {
        s_start_step = JHD_STEP_CLIENT;
        e = usb_host_client_register(&client, &s_client);
    }
    if (e != ESP_OK)
        return e;
    s_start_step = JHD_STEP_CLIENT_TASK;
    if (xTaskCreatePinnedToCore(client_task, "usb_midi_cl", CLIENT_STACK, NULL, CLIENT_PRIO, &s_client_task, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    s_start_step = JHD_STEP_NONE;
    return ESP_OK;
}

/* which step of usb_midi_host_start failed (JHD_STEP_NONE: none did) */
uint8_t usb_midi_host_start_step(void) { return s_start_step; }

/* the client task's stack never used, bytes (0: no task) */
uint16_t usb_midi_host_stack_free(void)
{
    UBaseType_t free_bytes;
    if (!s_client_task)
        return 0;
    free_bytes = uxTaskGetStackHighWaterMark(s_client_task);
    return free_bytes > 0xFFFFu ? 0xFFFFu : (uint16_t)free_bytes;
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

/* the device that has the role (s_lock held) */
static struct dev *by_role(enum jlapp_role r)
{
    for (uint32_t i = 0; i < JH_DEVICES; i++)
        if (s_dev[i].h && s_dev[i].role == r)
            return &s_dev[i];
    return NULL;
}

/* packets (n bytes, whole packets) to the device that has the role, in order with what else goes to it (the clock) */
void usb_midi_host_send_to(enum jlapp_role r, const uint8_t *packets, size_t n)
{
    struct dev *d;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    d = by_role(r);
    if (d) {
        s_dropped += jmq_add(&d->q, packets, n);
        if (!d->busy)
            submit_pending(d);
    }
    xSemaphoreGive(s_lock);
}

/* (any task) the client task leaves its wait for one pass (usb_lp_pump): the ui task's new page for the Digitone goes out now, not at the next transfer. The client
 * stays registered for good once registered, so the handle never goes stale */
void usb_midi_host_wake(void)
{
    if (s_client)
        usb_host_client_unblock(s_client);
}

/* the packets still fitting in the queue of the device that has the role */
size_t usb_midi_host_room(enum jlapp_role r)
{
    struct dev *d;
    size_t room = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    d = by_role(r);
    if (d)
        room = (JMQ_BYTES - d->q.n) / 4u;
    xSemaphoreGive(s_lock);
    return room;
}
