/* SPDX-License-Identifier: GPL-3.0-only */
/* The 128 x 64 I2C OLED (oled.h): an SSD1309 module (the mini 2.2's 1.54" on J10, 2.42" on J10B) or, on the XIAO's UI
 * bench, an SSD1306 one. Commands from Solomon Systech's SSD1309 datasheet (Rev 1.4), section 10 "Command descriptions"
 * / Table 9-1 (the SSD1306's are the same codes, plus its charge pump, 8D 14, from its Application Note):
 *   AE / AF display off / on      D5 clock divide and oscillator    A8 multiplex ratio     D3 display offset
 *   40 start line 0               20 02 page addressing             A1 segment remap       C8 COM scan remapped
 *   DA COM pins configuration     81 contrast                       D9 pre-charge          DB VCOMH deselect level
 *   A4 display follows RAM        A6 normal (not inverted)          B0+p page, 00 / 10 column 0
 * Written with the reset values where they serve, so a restart of the ESP32 alone (the 4-pin modules keep their state)
 * finds the controller as at power-up.
 *
 * Power (hardware/mini2 README, "The 2.42" on the cell"): the module's boost may only start while Wi-Fi is idle. The panel
 * is switched on ONCE, in oled_open, which app_main calls before the radio starts; it is never switched off and on again:
 * SCREEN OFF sends a dark frame instead (a dark panel draws next to nothing).
 *
 * Traffic (TIMING.md trap 4): only the pages that changed, from the lowest-priority task on core 0, 400 kHz on the shared
 * bus: a page is 129 bytes, ~2.9 ms of bus. */
#include "oled.h"
#include <string.h>
#include "board.h"
#include "board_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define OLED_HZ 400000u
#define TIMEOUT_MS 50
#define TASK_PRIO 1                     /* the lowest above idle: everything else on core 0 comes first */
#define TASK_STACK 3072

static const char *TAG = "oled";
static i2c_master_dev_handle_t s_dev;
static uint8_t s_addr;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static uint8_t s_next[JO_FB_BYTES], s_shown[JO_FB_BYTES];
static uint8_t s_next_contrast = 0xFF, s_contrast_sent;
static uint8_t s_valid;                 /* s_shown is what the panel holds */
static uint32_t s_pages_sent, s_errors;

static esp_err_t cmds(const uint8_t *c, size_t n)
{
    uint8_t b[24];
    if (n + 1u > sizeof b)
        return ESP_ERR_INVALID_SIZE;
    b[0] = 0x00;                        /* control byte: a command stream */
    memcpy(b + 1, c, n);
    return i2c_master_transmit(s_dev, b, n + 1u, TIMEOUT_MS);
}

static esp_err_t page(uint8_t p, const uint8_t *data)
{
    uint8_t at[3] = {(uint8_t)(0xB0u | p), 0x00, 0x10}, b[1 + JO_W];
    esp_err_t e = cmds(at, sizeof at);
    if (e != ESP_OK)
        return e;
    b[0] = 0x40;                        /* control byte: data */
    memcpy(b + 1, data, JO_W);
    return i2c_master_transmit(s_dev, b, sizeof b, TIMEOUT_MS);
}

static esp_err_t setup(uint8_t contrast)
{
    static const uint8_t INIT[] = {
        0xAE, 0xD5, 0x70, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12,
#if CONFIG_JANK_UI_BENCH_SSD1306
        0xD9, 0xF1, 0xDB, 0x40, 0x8D, 0x14,  /* the SSD1306's own charge pump on (its Application Note) */
#else
        0xD9, 0x22, 0xDB, 0x34,              /* the SSD1309's reset values */
#endif
        0xA4, 0xA6,
    };
    uint8_t c[2] = {0x81, contrast};
    esp_err_t e = cmds(INIT, sizeof INIT);
    if (e == ESP_OK)
        e = cmds(c, sizeof c);
    memset(s_shown, 0, sizeof s_shown);
    for (uint8_t p = 0; p < JO_PAGES && e == ESP_OK; p++)
        e = page(p, s_shown);           /* blank before it lights */
    return e;
}

int oled_open(uint8_t contrast)
{
#if BOARD_OLED
    i2c_master_bus_handle_t bus;
    static const uint8_t ADDRS[2] = {0x3C, 0x3D};
    uint8_t on = 0xAF;
    if (board_i2c_bus(&bus) != ESP_OK)
        return 0;
    for (int i = 0; i < 2 && !s_addr; i++)
        if (i2c_master_probe(bus, ADDRS[i], TIMEOUT_MS) == ESP_OK)
            s_addr = ADDRS[i];
    if (!s_addr) {
        ESP_LOGI(TAG, "no screen: the LED language");
        return 0;
    }
    {
        i2c_device_config_t dc = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = s_addr, .scl_speed_hz = OLED_HZ};
        if (i2c_master_bus_add_device(bus, &dc, &s_dev) != ESP_OK || setup(contrast) != ESP_OK || cmds(&on, 1) != ESP_OK) {
            ESP_LOGE(TAG, "screen at 0x%02x does not take its setup", s_addr);
            s_addr = 0;
            return 0;
        }
    }
    s_contrast_sent = contrast;
    s_valid = 1;
    ESP_LOGI(TAG, "screen at 0x%02x", s_addr);
    return 1;
#else
    (void)contrast;
    return 0;
#endif
}

int oled_present(void) { return s_addr != 0; }

void oled_show(const uint8_t fb[JO_FB_BYTES], uint8_t contrast)
{
    if (!s_task)
        return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_next, fb, JO_FB_BYTES);
    s_next_contrast = contrast;
    xSemaphoreGive(s_lock);
    xTaskNotifyGive(s_task);
}

static void oled_task(void *arg)
{
    static uint8_t fb[JO_FB_BYTES];
    (void)arg;
    for (;;) {
        uint8_t contrast;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        memcpy(fb, s_next, sizeof fb);
        contrast = s_next_contrast;
        xSemaphoreGive(s_lock);
        if (contrast != s_contrast_sent) {
            uint8_t c[2] = {0x81, contrast};
            if (cmds(c, sizeof c) == ESP_OK)
                s_contrast_sent = contrast;
            else
                s_errors++;
        }
        for (uint8_t p = 0; p < JO_PAGES; p++) {
            const uint8_t *src = fb + p * JO_W;
            if (s_valid && !memcmp(src, s_shown + p * JO_W, JO_W))
                continue;
            if (page(p, src) == ESP_OK) {
                memcpy(s_shown + p * JO_W, src, JO_W);
                s_pages_sent++;
            } else {
                s_errors++;              /* sent again with the next frame */
                memset(s_shown + p * JO_W, 0xA5, JO_W);
            }
        }
    }
}

esp_err_t oled_start(void)
{
    if (!s_addr)
        return ESP_OK;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock)
        return ESP_ERR_NO_MEM;
    return xTaskCreatePinnedToCore(oled_task, "oled", TASK_STACK, NULL, TASK_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void oled_stats(uint32_t *pages, uint32_t *errors)
{
    *pages = s_pages_sent;
    *errors = s_errors;
}
