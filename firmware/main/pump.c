/* SPDX-License-Identifier: GPL-3.0-only */
/* The CLK / RST charge pump of jank mini 2.2 (CV_RAIL; hardware/mini2 README "Firmware notes"): GPIO35 through U13 at
 * 416,667 Hz, 50 %, LEDC on the 80 MHz APB with a 1-bit duty, divider exactly 96 (80 MHz / 96 / 2): 10 x the ADC's
 * 41,667 Hz per channel, so its ripple folds to DC (the README's aliasing note). Held by whoever needs CV_RAIL: here the
 * host mode's 5 V signal (U16 HT7550 runs from it); the CLK / RST outputs will be a second holder. Not soft-started: a
 * 1-bit duty has no lower step (the README's 95 mA start-up edges stay; under U13's limits per its notes).
 * Boards without the pump: pump_hold answers ESP_ERR_NOT_SUPPORTED. */
#include "app.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "usb_host_mode.h"
#include "driver/ledc.h"

#define PUMP_HZ 416666u                /* the driver's divider from this is 96 x 256 exactly; 416,667 would give 96 - 1/256 */
#define PUMP_TIMER LEDC_TIMER_3
#define PUMP_CHANNEL LEDC_CHANNEL_7
static const char *TAG = "pump";
static SemaphoreHandle_t s_lock;
static uint32_t s_holders;

static esp_err_t pump_on(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_1_BIT,
        .timer_num = PUMP_TIMER,
        .freq_hz = PUMP_HZ,
        .clk_cfg = LEDC_USE_APB_CLK,
    };
    ledc_channel_config_t c = {
        .gpio_num = BOARD_PUMP_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = PUMP_CHANNEL,
        .timer_sel = PUMP_TIMER,
        .duty = 1,                     /* of 2: 50 % */
        .hpoint = 0,
    };
    esp_err_t e = ledc_timer_config(&t);
    if (e == ESP_OK)
        e = ledc_channel_config(&c);
    if (e == ESP_OK && ledc_get_freq(LEDC_LOW_SPEED_MODE, PUMP_TIMER) != PUMP_HZ)
        ESP_LOGW(TAG, "%lu Hz, not %u: its ripple will not fold to DC", (unsigned long)ledc_get_freq(LEDC_LOW_SPEED_MODE, PUMP_TIMER), PUMP_HZ);
    return e;
}

esp_err_t pump_hold(void)
{
    esp_err_t e = ESP_OK;
    if (BOARD_PUMP_GPIO < 0)
        return ESP_ERR_NOT_SUPPORTED;
    if (!s_lock && !(s_lock = xSemaphoreCreateMutex()))
        return ESP_ERR_NO_MEM;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_holders == 0u)
        e = pump_on();
    if (e == ESP_OK)
        s_holders++;
    xSemaphoreGive(s_lock);
    return e;
}

void pump_release(void)
{
    if (!s_lock)
        return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_holders && --s_holders == 0u) {
        esp_err_t e = ledc_stop(LEDC_LOW_SPEED_MODE, PUMP_CHANNEL, 0);
        if (e != ESP_OK)
            ESP_LOGE(TAG, "stop: %s", esp_err_to_name(e));
    }
    xSemaphoreGive(s_lock);
}
