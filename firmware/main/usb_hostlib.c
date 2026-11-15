/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode: ESP-IDF's USB Host Library (components/usb, usb_host.h), installed once when the port starts as host, and
 * the task that handles its library events. Its clients are usb_midi_host.c (the clock out to MIDI devices) and
 * usb_uac_host.c (a device's audio into the beat tracker); each registers on its own (usb_host_client_register) and opens
 * the devices it wants: the library lets several clients open one device and claim different interfaces of it (a synth's
 * MIDI and audio).
 * - usb_host_install creates the PHY in host mode itself (usb_host.c: usb_new_phy with USB_OTG_MODE_HOST unless
 *   skip_phy_setup); the device stack (TinyUSB) is never installed in the same start (usb.c).
 * - Hubs: CONFIG_USB_HOST_HUBS_SUPPORTED (sdkconfig.usb). The S3's controller has 8 host channels (soc/esp32s3
 *   usb_dwc_cfg.h OTG_NUM_HOST_CHAN 8): each device's control pipe, a hub's interrupt pipe and every open endpoint take
 *   one, which is the real limit on what a hub can bring (e.g. a hub, a synth with MIDI + audio, a MIDI interface: 2 + 3
 *   + 2 = 7). */
#include <string.h>
#include "app.h"
#include "jank_uacdesc.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb_host_mode.h"

#define LIB_STACK 4096
#define LIB_PRIO 22                    /* under the MIDI task (24, usb.c) and the clients (21 .. 23), over Wi-Fi's app tasks */

static const char *TAG = "usb host";
static volatile uint8_t s_up;

static void lib_task(void *arg)
{
    usb_host_config_t c = {.intr_flags = ESP_INTR_FLAG_LOWMED};
    TaskHandle_t parent = arg;
    esp_err_t e = usb_host_install(&c);
    if (e == ESP_OK)
        s_up = 1;                      /* before the caller hears of it: a second start sees it installed */
    xTaskNotify(parent, (uint32_t)e, eSetValueWithOverwrite);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "install: %s", esp_err_to_name(e));
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        uint32_t flags = 0;
        e = usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (e != ESP_OK && e != ESP_ERR_TIMEOUT)
            ESP_LOGW(TAG, "events: %s", esp_err_to_name(e));
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            ESP_LOGW(TAG, "no clients left");
    }
}

esp_err_t usb_hostlib_start(void)
{
    uint32_t r = (uint32_t)ESP_FAIL;
    if (s_up)
        return ESP_OK;
    if (xTaskCreatePinnedToCore(lib_task, "usb_lib", LIB_STACK, xTaskGetCurrentTaskHandle(), LIB_PRIO, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    xTaskNotifyWait(0, UINT32_MAX, &r, portMAX_DELAY);
    return (esp_err_t)r;
}

int usb_hostlib_up(void) { return s_up; }

void usb_host_product_name(usb_device_handle_t h, char *out, size_t cap)
{
    usb_device_info_t info;
    if (!cap)
        return;
    out[0] = 0;
    if (usb_host_device_info(h, &info) == ESP_OK && info.str_desc_product)
        ju_string_ascii((const uint8_t *)info.str_desc_product, info.str_desc_product->bLength, out, cap);
}

void usb_host_list_add(struct jk_usb *u, const char *name, int midi, int audio)
{
    for (uint32_t i = 0; i < u->ndev; i++)
        if (name[0] && !strcmp(u->dev[i].name, name)) {   /* one device, two clients: one line */
            u->dev[i].midi |= (uint8_t)(midi != 0);
            u->dev[i].audio |= (uint8_t)(audio != 0);
            return;
        }
    if (u->ndev >= JK_USB_DEVS)
        return;
    strlcpy(u->dev[u->ndev].name, name, sizeof u->dev[u->ndev].name);
    u->dev[u->ndev].midi = (uint8_t)(midi != 0);
    u->dev[u->ndev].audio = (uint8_t)(audio != 0);
    u->ndev++;
}
