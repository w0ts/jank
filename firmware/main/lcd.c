/* SPDX-License-Identifier: GPL-3.0-only */
/* The Waveshare ESP32-S3-LCD-2's screen (lcd.h). Pins: board.h, from the schematic; the panel setup follows the vendor's
 * ESP-IDF demo (ESP-IDF/06_lvgl_example, ESP32-S3-LCD-2-Demo.zip): esp_lcd's ST7789 driver, RGB element order, colours
 * inverted; landscape is swap_xy plus the two mirror switches (Kconfig). docs/lcd2.md.
 *
 * Traffic: the frame is sent in chunks of 4 destination rows (2.5 KB) from two buffers, so the SPI's DMA sends one while
 * the next is filled: the task runs the rows' expansion, the DMA moves the bytes. Only the frame pages that changed (the
 * pixels or their ink) are sent (a page is 16 destination rows). The task is core 0, the lowest priority above idle: the MIDI
 * clock's interrupt on core 1 (docs/midi.md) is not in its way.
 *
 * RAM: the four ink planes (the UI's, the pending one, the one being sent, the one shown: 8 KB each) are allocated from PSRAM
 * when the board has it (the lcd2's 8 MB); the chunk buffers (5 KB) are internal, the DMA reads them. */
#include "lcd.h"
#include <string.h>
#include "board.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jank_lcd.h"

#define LCD_HOST SPI2_HOST
#define PLANES 4
#define LCD_HZ (40 * 1000 * 1000)       /* the vendor demo runs 80 MHz; half of it leaves margin on an unmeasured board */
#define CHUNK_ROWS 4
#if CONFIG_JANK_LCD_MIRROR_X               /* a bool that is off is not defined at all */
#define MIRROR_X true
#else
#define MIRROR_X false
#endif
#if CONFIG_JANK_LCD_MIRROR_Y
#define MIRROR_Y true
#else
#define MIRROR_Y false
#endif
#define CHUNK_PIXELS (CHUNK_ROWS * JLC_W)
#define TASK_PRIO 1                     /* the lowest above idle: everything else on core 0 comes first */
#define TASK_STACK 3072

static const char *TAG = "lcd";
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_free;        /* counts the free chunk buffers (2) */
static DMA_ATTR uint16_t s_buf[2][CHUNK_PIXELS];
static unsigned s_next_buf;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static struct jlc_pal s_pal;
static uint8_t *s_planes;               /* PLANES ink planes in one block */
static uint8_t *s_ui_ink, *s_pend_ink, *s_work_ink, *s_shown_ink;
static uint8_t s_pend_fb[JO_FB_BYTES], s_work_fb[JO_FB_BYTES], s_shown_fb[JO_FB_BYTES];
static uint8_t *s_pend = s_pend_fb, *s_work = s_work_fb;   /* swapped, not copied, when the task takes the pending frame */
static uint8_t s_valid;                 /* s_shown_* is what the panel holds */

static bool chunk_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    BaseType_t woken = pdFALSE;
    (void)io;
    (void)ev;
    (void)ctx;
    xSemaphoreGiveFromISR(s_free, &woken);
    return woken == pdTRUE;
}

static esp_err_t send_rows(const uint8_t *fb, const uint8_t *ink, unsigned y, unsigned n)
{
    for (unsigned r = 0; r < n; r += CHUNK_ROWS) {
        uint16_t *buf = s_buf[s_next_buf];
        xSemaphoreTake(s_free, portMAX_DELAY);
        for (unsigned i = 0; i < CHUNK_ROWS; i++)
            jlc_row(fb, ink, &s_pal, y + r + i, buf + i * JLC_W);
        esp_err_t e = esp_lcd_panel_draw_bitmap(s_panel, 0, (int)(y + r), JLC_W, (int)(y + r + CHUNK_ROWS), buf);
        if (e != ESP_OK) {
            xSemaphoreGive(s_free);
            return e;
        }
        s_next_buf ^= 1u;
    }
    return ESP_OK;
}

static int page_changed(const uint8_t *fb, const uint8_t *ink, unsigned p)
{
    return memcmp(fb + p * JO_W, s_shown_fb + p * JO_W, JO_W) || memcmp(ink + p * 8u * JO_W, s_shown_ink + p * 8u * JO_W, 8u * JO_W);
}

static esp_err_t send_changed_pages(const uint8_t *fb, const uint8_t *ink)
{
    for (unsigned p = 0; p < JO_PAGES; p++) {
        if (!page_changed(fb, ink, p))
            continue;
        esp_err_t e = send_rows(fb, ink, jlc_page_y(p), jlc_page_rows());
        if (e != ESP_OK)
            return e;
    }
    return ESP_OK;
}

esp_err_t lcd_present(const uint8_t fb[JO_FB_BYTES], const uint8_t ink[JO_INK_BYTES])
{
    esp_err_t e = s_valid ? send_changed_pages(fb, ink) : send_rows(fb, ink, 0, JLC_H);
    if (e == ESP_OK) {
        memcpy(s_shown_fb, fb, JO_FB_BYTES);
        memcpy(s_shown_ink, ink, JO_INK_BYTES);
        s_valid = 1;
    } else {
        if (s_valid)
            ESP_LOGW(TAG, "send failed: %s", esp_err_to_name(e));
        s_valid = 0;                     /* the whole frame again with the next one */
    }
    return e;
}

static esp_err_t panel_up(void)
{
    spi_bus_config_t bus = {.mosi_io_num = BOARD_LCD_MOSI_GPIO, .miso_io_num = -1, .sclk_io_num = BOARD_LCD_SCLK_GPIO,
                            .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = CHUNK_PIXELS * 2};
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {.dc_gpio_num = BOARD_LCD_DC_GPIO, .cs_gpio_num = BOARD_LCD_CS_GPIO, .pclk_hz = LCD_HZ,
                                            .lcd_cmd_bits = 8, .lcd_param_bits = 8, .spi_mode = 0, .trans_queue_depth = 4,
                                            .on_color_trans_done = chunk_done};
    esp_lcd_panel_dev_config_t panel_cfg = {.reset_gpio_num = BOARD_LCD_RST_GPIO, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16};
    esp_err_t e = spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO);
    if (e == ESP_OK)
        e = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io);
    if (e == ESP_OK)
        e = esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel);
    if (e == ESP_OK)
        e = esp_lcd_panel_reset(s_panel);
    if (e == ESP_OK)
        e = esp_lcd_panel_init(s_panel);
    if (e == ESP_OK)
        e = esp_lcd_panel_invert_color(s_panel, true);
    if (e == ESP_OK)
        e = esp_lcd_panel_swap_xy(s_panel, true);
    if (e == ESP_OK)
        e = esp_lcd_panel_mirror(s_panel, MIRROR_X, MIRROR_Y);
    return e;
}

static esp_err_t backlight_on(void)
{
    gpio_config_t c = {.pin_bit_mask = 1ull << BOARD_LCD_BL_GPIO, .mode = GPIO_MODE_OUTPUT};
    esp_err_t e = gpio_config(&c);
    return e == ESP_OK ? gpio_set_level(BOARD_LCD_BL_GPIO, 1) : e;
}

static int planes_up(void)
{
    s_planes = heap_caps_calloc(PLANES, JO_INK_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_planes)
        s_planes = heap_caps_calloc(PLANES, JO_INK_BYTES, MALLOC_CAP_8BIT);   /* no PSRAM: internal RAM, 32 KB */
    if (!s_planes)
        return 0;
    s_ui_ink = s_planes;
    s_pend_ink = s_planes + JO_INK_BYTES;
    s_work_ink = s_planes + 2u * JO_INK_BYTES;
    s_shown_ink = s_planes + 3u * JO_INK_BYTES;
    return 1;
}

int lcd_open(void)
{
    static const uint8_t BLANK[JO_FB_BYTES];
    esp_err_t e;
    s_free = xSemaphoreCreateCounting(2, 2);
    if (!s_free || !planes_up())
        return 0;
    jlc_palette(&s_pal, 1);               /* the panel takes the high byte first */
    e = panel_up();
    if (e == ESP_OK)
        e = lcd_present(BLANK, s_ui_ink);
    if (e == ESP_OK)
        e = esp_lcd_panel_disp_on_off(s_panel, true);
    if (e == ESP_OK)
        e = backlight_on();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "screen does not start: %s", esp_err_to_name(e));
        s_panel = NULL;
        heap_caps_free(s_planes);
        s_planes = s_ui_ink = NULL;
        return 0;
    }
    return 1;
}

int lcd_found(void) { return s_panel != NULL; }

uint8_t *lcd_ink(void) { return s_ui_ink; }

void lcd_show(const uint8_t fb[JO_FB_BYTES])
{
    if (!s_task)
        return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_pend, fb, JO_FB_BYTES);
    memcpy(s_pend_ink, s_ui_ink, JO_INK_BYTES);
    xSemaphoreGive(s_lock);
    xTaskNotifyGive(s_task);
}

/* the pending frame becomes the one being sent: the pointers change places, nothing is copied under the lock */
static void take_pending(void)
{
    uint8_t *fb = s_work, *ink = s_work_ink;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_work = s_pend;
    s_work_ink = s_pend_ink;
    s_pend = fb;
    s_pend_ink = ink;
    xSemaphoreGive(s_lock);
}

static void lcd_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        take_pending();
        lcd_present(s_work, s_work_ink);
    }
}

esp_err_t lcd_start(void)
{
    if (!s_panel)
        return ESP_OK;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock)
        return ESP_ERR_NO_MEM;
    return xTaskCreatePinnedToCore(lcd_task, "lcd", TASK_STACK, NULL, TASK_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
