/* SPDX-License-Identifier: GPL-3.0-only */
/* USB: a CDC-NCM network adapter, a USB MIDI port and a drive. The MIDI port carries the clock to the computer and
 * the control line in SysEx (core/jank_sysex.h); the drive holds the control page (drive.c), so there is no link to remember.
 * No CDC-ACM serial: the S3 has 5 IN endpoints, EP0 included (tinyusb dwc2_esp32.h ep_in_count). NCM takes 2, MIDI 1 and
 * the drive 1, which is all of them; more fails when the host configures the device.
 * Device or host is decided at each start and kept: as a device first, and when no computer has spoken within HOST_WAIT_MS
 * it restarts once as host (usb_midi_host.c). A computer on this port is what powers the stick, so it is there from the
 * start; a mode switched later, with no restart, would have to break the clock whenever the port is idle.
 * The address in the NCM descriptor is the computer's adapter's own address, which is why the stick offers the one
 * its radio has: the bridge then never rewrites an address. */
#include <string.h>
#include "app.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "jank_sysex.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_net.h"
#include "tusb.h"

#define RING_SIZE 64u                  /* a power of two; a clock tick is one byte */
#define TEXT_OUT_SIZE 2048u
#define WRITE_TRIES 50                 /* milliseconds a full MIDI buffer may hold us up */
#define MIDI_TASK_CORE 0              /* core 1 is the MIDI clock's alone (midi.c) */
#define MIDI_TASK_PRIO 24             /* above Wi-Fi (23) on core 0: it runs a few us a tick, straight into the endpoint (measured 2026-11-09: at 6, USB clock sd 220 us) */
#define HOST_WAIT_MS 3000
#define HOST_NEXT 0x4A4B484Fu          /* in RTC memory, for one restart: garbage after power-on is not this */

/* tusb_get_mac_string_id() with CDC off and MSC on: esp_tinyusb writes the MAC there, so STR_MAC has to be that index */
enum { STR_LANG, STR_MAKER, STR_PRODUCT, STR_SERIAL, STR_DISK, STR_NET, STR_MAC, STR_MIDI, STR_COUNT };
enum { ITF_NET, ITF_NET_DATA, ITF_MIDI, ITF_MIDI_STREAM, ITF_DISK, ITF_TOTAL };
enum { EP_NET_NOTIF = 1, EP_NET_DATA, EP_MIDI, EP_DISK };
#define CONFIG_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_MSC_DESC_LEN)

static const char *STRINGS[STR_COUNT] = {
    [STR_LANG] = "\x09\x04", [STR_MAKER] = "jank", [STR_PRODUCT] = "jank", [STR_SERIAL] = NULL,
    [STR_DISK] = "jank page", [STR_NET] = "jank network", [STR_MAC] = "", [STR_MIDI] = "jank",
};

static const uint8_t CONFIG_DESC[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_TOTAL, 0, CONFIG_LEN, 0, 500),
    TUD_CDC_NCM_DESCRIPTOR(ITF_NET, STR_NET, STR_MAC, 0x80 | EP_NET_NOTIF, 64, EP_NET_DATA, 0x80 | EP_NET_DATA, 64, CFG_TUD_NET_MTU),
    TUD_MIDI_DESCRIPTOR(ITF_MIDI, STR_MIDI, EP_MIDI, 0x80 | EP_MIDI, 64),
    TUD_MSC_DESCRIPTOR(ITF_DISK, STR_DISK, EP_DISK, 0x80 | EP_DISK, 64),
};

static const char *TAG = "usb";
static usb_frame_fn s_rx;
static usb_free_fn s_tx_done;
static usb_line_fn s_line_rx;
static DRAM_ATTR TaskHandle_t s_task;
static StreamBufferHandle_t s_text_out;
static SemaphoreHandle_t s_text_lock;

/* the timer's interrupt fills it, the MIDI task empties it */
static DRAM_ATTR uint8_t s_ring[RING_SIZE];
static DRAM_ATTR uint32_t s_head, s_tail, s_dropped;
static RTC_NOINIT_ATTR uint32_t s_start_as;
static uint8_t s_host;

static esp_err_t on_frame(void *buf, uint16_t len, void *ctx)
{
    (void)ctx;
    s_rx(buf, len);
    return ESP_OK;
}

static void on_tx_done(void *token, void *ctx)
{
    (void)ctx;
    s_tx_done(token);
}

bool IRAM_ATTR usb_midi_isr(const uint8_t *bytes, size_t n)
{
    BaseType_t woken = pdFALSE;
    uint32_t head = s_head;
    uint32_t tail = __atomic_load_n(&s_tail, __ATOMIC_ACQUIRE);
    if (!n || !s_task)
        return false;
    if (RING_SIZE - (head - tail) < n) {
        ++s_dropped;
        return false;
    }
    for (size_t i = 0; i < n; ++i)
        s_ring[(head + i) & (RING_SIZE - 1u)] = bytes[i];
    __atomic_store_n(&s_head, head + n, __ATOMIC_RELEASE);
    vTaskNotifyGiveFromISR(s_task, &woken);
    return woken == pdTRUE;
}

/* TinyUSB's task: something arrived from the computer */
void tud_midi_rx_cb(uint8_t itf)
{
    (void)itf;
    if (s_task)                        /* the host can be quicker than usb_start */
        xTaskNotifyGive(s_task);
}

static void write_all(const uint8_t *data, size_t n)
{
    for (int tries = 0; n && tries < WRITE_TRIES; ++tries) {
        uint32_t did = tud_midi_stream_write(0, data, (uint32_t)n);
        data += did;
        n -= did;
        if (n)
            vTaskDelay(1);
    }
}

static void send_clock(void)
{
    uint32_t head = __atomic_load_n(&s_head, __ATOMIC_ACQUIRE);
    uint32_t tail = s_tail;
    uint8_t b[RING_SIZE];
    size_t n = 0;
    while (tail != head)
        b[n++] = s_ring[tail++ & (RING_SIZE - 1u)];
    __atomic_store_n(&s_tail, tail, __ATOMIC_RELEASE);
    if (n && s_host) {
        uint8_t packets[RING_SIZE * 4u];
        usb_midi_host_send(packets, jx_realtime_packets(b, n, packets, sizeof packets));
    } else if (n && tud_midi_mounted()) {
        write_all(b, n);
    }
}

static void read_host(struct jx_in *in)
{
    uint8_t b[64];
    uint32_t n;
    while ((n = tud_midi_stream_read(b, sizeof b)) != 0) {
        uint8_t text[sizeof b];
        size_t got = 0;
        for (uint32_t i = 0; i < n; ++i) {
            int c = jx_feed(in, b[i]);
            if (c >= 0)
                text[got++] = (uint8_t)c;
        }
        if (got)
            s_line_rx(text, got);
    }
}

/* one frame at a time, so that the clock goes first between frames */
static void send_text(void)
{
    char text[JX_TEXT_MAX];
    uint8_t frame[JX_FRAME_MAX];
    size_t n;
    while ((n = xStreamBufferReceive(s_text_out, text, sizeof text, 0)) != 0) {
        if (tud_midi_mounted())
            write_all(frame, jx_frame(text, n, frame));
        send_clock();
    }
}

/* the only task that writes to the MIDI port: TinyUSB's byte stream parser holds state between calls */
static void midi_task(void *arg)
{
    struct jx_in in;
    (void)arg;
    jx_init(&in);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5));
        send_clock();
        if (!s_host) {
            read_host(&in);
            send_text();
        }
    }
}

static void wait_for_a_computer(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(HOST_WAIT_MS));
    if (!tud_connected()) {              /* its first SETUP sets this */
        ota_mark_valid();                /* a new image that got this far ran for 3 s with every task up: it is not a bad one */
        s_start_as = HOST_NEXT;
        esp_restart();
    }
    vTaskDelete(NULL);
}

static int start_as_host(void)
{
    int host = s_start_as == HOST_NEXT;
    s_start_as = 0;
    return host;
}

esp_err_t usb_start(const uint8_t mac[6], usb_frame_fn rx, usb_free_fn tx_done, usb_line_fn line_rx)
{
    tinyusb_config_t cfg = TINYUSB_DEFAULT_CONFIG();
    cfg.task.xCoreID = 0;             /* TinyUSB's task and the USB interrupt it installs: off the clock's core 1 */
    tinyusb_net_config_t net = {.on_recv_callback = on_frame, .free_tx_buffer = on_tx_done};
    esp_err_t e;
    s_host = (uint8_t)start_as_host();
    if (s_host) {
        e = usb_midi_host_start();
        if (e == ESP_OK && xTaskCreatePinnedToCore(midi_task, "usb_midi", 4096, NULL, MIDI_TASK_PRIO, &s_task, MIDI_TASK_CORE) != pdPASS)
            e = ESP_ERR_NO_MEM;
        return e;
    }
    s_rx = rx;
    s_tx_done = tx_done;
    s_line_rx = line_rx;
    s_text_out = xStreamBufferCreate(TEXT_OUT_SIZE, 1);
    s_text_lock = xSemaphoreCreateMutex();
    if (!s_text_out || !s_text_lock)
        return ESP_ERR_NO_MEM;
    cfg.descriptor.full_speed_config = CONFIG_DESC;
    cfg.descriptor.string = STRINGS;
    cfg.descriptor.string_count = STR_COUNT;
    memcpy(net.mac_addr, mac, 6);
    e = drive_start();
    if (e != ESP_OK)                     /* the page on a web site still works: the network and MIDI go on without it */
        ESP_LOGE(TAG, "drive: %s", esp_err_to_name(e));
    /* The order matters: tinyusb_driver_install's task overwrites every string descriptor (tinyusb_descriptors_set) and
     * only then returns, so a MAC set earlier is lost and the computer reads an empty one. The host reads strings well
     * after this returns. */
    e = tinyusb_driver_install(&cfg);
    if (e == ESP_OK)
        e = tinyusb_net_init(&net);
    if (e == ESP_OK && xTaskCreatePinnedToCore(midi_task, "usb_midi", 4096, NULL, MIDI_TASK_PRIO, &s_task, MIDI_TASK_CORE) != pdPASS)
        e = ESP_ERR_NO_MEM;
    if (e == ESP_OK && xTaskCreatePinnedToCore(wait_for_a_computer, "usb_wait", 2048, NULL, 2, NULL, 0) != pdPASS)
        e = ESP_ERR_NO_MEM;
    if (e != ESP_OK)
        ESP_LOGE(TAG, "start: %s", esp_err_to_name(e));
    return e;
}

int usb_up(void) { return !s_host && tud_mounted(); }

esp_err_t usb_send(const uint8_t *eth, size_t len, void *token)
{
    return tinyusb_net_send_async((void *)eth, (uint16_t)len, token);
}

void usb_serial_write(const char *text, size_t n)
{
    if (s_host || !tud_midi_mounted())
        return;                          /* nobody listening: nothing to wait for */
    xSemaphoreTake(s_text_lock, portMAX_DELAY);
    xStreamBufferSend(s_text_out, text, n, pdMS_TO_TICKS(50));
    xSemaphoreGive(s_text_lock);
    xTaskNotifyGive(s_task);
}
