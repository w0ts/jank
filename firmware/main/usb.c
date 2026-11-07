/* SPDX-License-Identifier: GPL-3.0-only */
/* USB: a CDC-NCM network adapter and a CDC-ACM serial, on esp_tinyusb's default descriptors.
 * The address in the NCM descriptor is the computer's adapter's own address, which is why the stick offers the one
 * its radio has: the bridge then never rewrites an address. */
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"
#include "tinyusb_net.h"
#include "tusb.h"

static const char *TAG = "usb";
static usb_frame_fn s_rx;
static usb_free_fn s_tx_done;
static usb_line_fn s_serial_rx;

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

static void on_serial(int itf, cdcacm_event_t *ev)
{
    uint8_t buf[64];
    size_t n = 0;
    (void)ev;
    esp_err_t e = tinyusb_cdcacm_read((tinyusb_cdcacm_itf_t)itf, buf, sizeof buf, &n);
    if (e == ESP_OK && n)
        s_serial_rx(buf, n);
    else if (e != ESP_OK)
        ESP_LOGW(TAG, "serial read: %s", esp_err_to_name(e));
}

esp_err_t usb_start(const uint8_t mac[6], usb_frame_fn rx, usb_free_fn tx_done, usb_line_fn serial_rx)
{
    tinyusb_config_t cfg = TINYUSB_DEFAULT_CONFIG();
    tinyusb_config_cdcacm_t acm = {.cdc_port = TINYUSB_CDC_ACM_0, .callback_rx = on_serial};
    tinyusb_net_config_t net = {.on_recv_callback = on_frame, .free_tx_buffer = on_tx_done};
    esp_err_t e;
    s_rx = rx;
    s_tx_done = tx_done;
    s_serial_rx = serial_rx;
    memcpy(net.mac_addr, mac, 6);
    /* The order matters: tinyusb_driver_install's task overwrites every string descriptor (tinyusb_descriptors_set) and
     * only then returns, so a MAC set earlier is lost and the computer reads an empty one. The host reads strings well
     * after this returns. */
    e = tinyusb_driver_install(&cfg);
    if (e == ESP_OK)
        e = tinyusb_net_init(&net);
    if (e == ESP_OK)
        e = tinyusb_cdcacm_init(&acm);
    return e;
}

int usb_up(void) { return tud_mounted(); }

esp_err_t usb_send(const uint8_t *eth, size_t len, void *token)
{
    return tinyusb_net_send_async((void *)eth, (uint16_t)len, token);
}

void usb_serial_write(const char *text, size_t n)
{
    while (n) {
        size_t did = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, (const uint8_t *)text, n);
        esp_err_t e = tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
        if (!did || e != ESP_OK)
            return;                      /* no terminal listening: nothing to wait for */
        text += did;
        n -= did;
    }
}
