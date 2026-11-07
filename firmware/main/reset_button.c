/* SPDX-License-Identifier: GPL-3.0-only */
/* A board whose only button is RST (the Lolin32 Lite): the button goes to the chip's EN pin, so the firmware never sees
 * a press, only the boot after it. Two resets within RESET_WINDOW_MS, which only a finger on RST produces, are the
 * firmware's "long press": the second boot switches MODE JAM <-> the Wi-Fi network before the radio starts.
 * The flag lives in NVS: RTC memory does not survive the EN reset on an ESP32. */
#include "app.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#define RESET_WINDOW_MS 4000u
#define NS "jank"
#define KEY "rst"

static const char *TAG = "reset";

static esp_err_t flag_write(uint8_t armed)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READWRITE, &h);
    if (e != ESP_OK)
        return e;
    e = armed ? nvs_set_u8(h, KEY, 1) : nvs_erase_key(h, KEY);
    if (e == ESP_OK || e == ESP_ERR_NVS_NOT_FOUND)
        e = nvs_commit(h);
    nvs_close(h);
    return e;
}

static int flag_read(void)
{
    nvs_handle_t h;
    uint8_t v = 0;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK)
        return 0;
    esp_err_t e = nvs_get_u8(h, KEY, &v);
    nvs_close(h);
    return e == ESP_OK && v;
}

int reset_button_double(void)
{
    int armed = flag_read();
    int by_button = esp_reset_reason() == ESP_RST_POWERON;    /* EN: the RST button, or a fresh plug-in */
    esp_err_t e = flag_write(1);
    if (e != ESP_OK)
        ESP_LOGE(TAG, "arm: %s", esp_err_to_name(e));
    return armed && by_button;
}

static void disarm_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(RESET_WINDOW_MS));
    esp_err_t e = flag_write(0);
    if (e != ESP_OK)
        ESP_LOGE(TAG, "disarm: %s", esp_err_to_name(e));
    vTaskDelete(NULL);
}

esp_err_t reset_button_arm(void)
{
    return xTaskCreate(disarm_task, "rst", 3072, NULL, 2, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
