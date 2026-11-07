/* SPDX-License-Identifier: GPL-3.0-only */
/* The button, the LED and the group scan.
 * Click: with MIDI on, the classic resync (jc_resync: stop now, start on the next bar line); with MIDI off, the next GROUP.
 * Double click: to the next GROUP heard on the air. Held 1.5 s and let go: MODE JAM <-> the Wi-Fi network, a restart.
 * Held 10 s: download mode, for flashing. The LED: the session's beat, inverted on the Wi-Fi network. */
#include <string.h>
#include "app.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"

#define POLL_MS 5u
#define LED_EVERY_MS 20u

void app_bootloader(void);
void app_reboot(void);

static const char *TAG = "ui";
#if BOARD_LED_RGB_GPIO >= 0
static led_strip_handle_t s_strip;
#endif
static SemaphoreHandle_t s_scanning;     /* one sweep at a time: the button's or the page's */
static enum jl_event s_event;
static uint32_t s_event_at, s_event_group;

static void led_show(uint8_t r, uint8_t g, uint8_t b)
{
#if BOARD_LED_RGB_GPIO >= 0
    if (r || g || b)
        led_strip_set_pixel(s_strip, 0, r, g, b);
    else
        led_strip_clear(s_strip);
    led_strip_refresh(s_strip);
#else
    gpio_set_level(BOARD_LED_MONO_GPIO, !(r || g || b));   /* active low */
#endif
}

static void led_event(enum jl_event e, uint32_t group)
{
    s_event = e;
    s_event_at = bridge_ms();
    s_event_group = group;
}

static void led_step(uint32_t now)
{
    static uint32_t last;
    uint8_t c[3];
    struct jk_cfg cfg;
    int64_t beat = 0;
    int32_t mpb = 0;
    int joined;
    if (now - last < LED_EVERY_MS)
        return;
    last = now;
    if (jl_pattern(s_event, s_event_group, now - s_event_at, c)) {
        led_show(c[0], c[1], c[2]);
        return;
    }
    s_event = JL_NONE;
    app_cfg_get(&cfg);
    joined = bridge_beat(&beat, &mpb);
    if (!jk_led_lit(cfg.net, joined, beat, mpb, cfg.quantum))
        led_show(0, 0, 0);
    else if (cfg.net == JK_NET_WIFI)
        led_show(0, 0, 40);              /* (an RGB LED: blue on a network, green in MODE JAM) */
    else
        led_show(0, 30, 0);
}

#if BOARD_BUTTON_GPIO >= 0
/* MODE JAM <-> the Wi-Fi network: the radio is set up at start-up, so a restart. No network saved: red pulses */
static void switch_network(void)
{
    struct jk_cfg cfg;
    app_cfg_get(&cfg);
    if (cfg.net == JK_NET_JAM && !cfg.ssid[0]) {
        led_event(JL_NO_GROUP, cfg.group);
        return;
    }
    cfg.net = cfg.net == JK_NET_WIFI ? JK_NET_JAM : JK_NET_WIFI;
    app_cfg_set(&cfg);
    app_reboot();
}

static void move_to(uint32_t group)
{
    struct jk_cfg cfg;
    app_cfg_get(&cfg);
    cfg.group = (uint8_t)group;
    app_cfg_set(&cfg);
}
#endif

/* Off the home channel for two dwells: this is all of the time the stick is away. The MIDI clock's timer is not touched */
static void sweep(uint32_t home_group)
{
    uint8_t plan[2];
    uint32_t n = js_plan(home_group, plan);
    esp_err_t e;
    bridge_away(1);
    for (uint32_t i = 0; i < n; i++) {
        e = radio_channel(plan[i]);
        if (e != ESP_OK)
            ESP_LOGE(TAG, "scan channel %u: %s", plan[i], esp_err_to_name(e));
        vTaskDelay(pdMS_TO_TICKS(JS_DWELL_MS));
    }
    e = radio_channel(jk_channel(home_group));
    if (e != ESP_OK)
        ESP_LOGE(TAG, "back home: %s", esp_err_to_name(e));
    bridge_away(0);
}

#if BOARD_BUTTON_GPIO >= 0
/* The button. The home channel needs no dwell: the radio listens there all the time, so what it heard in the last
 * JS_HEARD_MS counts. */
static void scan_groups(void)
{
    struct jk_cfg cfg;
    struct js_scan seen;
    uint32_t next;
    app_cfg_get(&cfg);
    if (cfg.net == JK_NET_WIFI)
        return;                          /* groups are MODE JAM's */
    xSemaphoreTake(s_scanning, portMAX_DELAY);
    led_event(JL_SCANNING, cfg.group);
    led_step(bridge_ms() + LED_EVERY_MS);
    sweep(cfg.group);
    bridge_note_scan_groups(&seen);
    next = js_pick(&seen, cfg.group, bridge_ms());
    if (next) {
        move_to(next);
        led_event(JL_JOINED, next);
    } else {
        led_event(JL_NO_GROUP, cfg.group);
    }
    xSemaphoreGive(s_scanning);
}
#endif

/* The control page. The table counts from the start of a sweep, so the home channel gets a dwell of its own, at home */
esp_err_t ui_scan_table(struct js_table *out)
{
    struct jk_cfg cfg;
    app_cfg_get(&cfg);
    if (!s_scanning || cfg.net == JK_NET_WIFI)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_scanning, portMAX_DELAY);
    led_event(JL_SCANNING, cfg.group);
    bridge_table_start();
    vTaskDelay(pdMS_TO_TICKS(JS_DWELL_MS));
    sweep(cfg.group);
    bridge_table_stop(out);
    led_event(JL_NONE, cfg.group);
    xSemaphoreGive(s_scanning);
    return ESP_OK;
}

#if BOARD_BUTTON_GPIO >= 0
static void on_button(enum jb_event e)
{
    switch (e) {
    case JB_CLICK:
        midi_resync();
        break;
    case JB_DOUBLE:
        scan_groups();
        break;
    case JB_LONG:
        switch_network();
        break;
    case JB_DOWNLOAD:
        app_bootloader();
        break;
    default:
        break;
    }
}
#endif

static void ui_task(void *arg)
{
    struct jb_button b;
    (void)arg;
    jb_init(&b);
    for (;;) {
        uint32_t now = bridge_ms();
#if BOARD_BUTTON_GPIO >= 0
        int level = gpio_get_level(BOARD_BUTTON_GPIO);
        on_button(jb_step(&b, BOARD_BUTTON_ACTIVE_LOW ? !level : level, now));
#else
        (void)b;                         /* the board's only button is RST: reset_button.c */
#endif
        led_step(now);
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

static esp_err_t led_up(void)
{
#if BOARD_LED_RGB_GPIO >= 0
    led_strip_config_t strip = {.strip_gpio_num = BOARD_LED_RGB_GPIO, .max_leds = 1, .led_model = LED_MODEL_WS2812, .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB};
    led_strip_rmt_config_t rmt = {.clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10000000};
    return led_strip_new_rmt_device(&strip, &rmt, &s_strip);
#else
    esp_err_t e = gpio_set_direction(BOARD_LED_MONO_GPIO, GPIO_MODE_OUTPUT);
    return e == ESP_OK ? gpio_set_level(BOARD_LED_MONO_GPIO, 1) : e;
#endif
}

esp_err_t ui_start(void)
{
    esp_err_t e = ESP_OK;
#if BOARD_BUTTON_GPIO >= 0
    gpio_config_t c = {.pin_bit_mask = 1ull << BOARD_BUTTON_GPIO, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    e = gpio_config(&c);
#endif
    s_scanning = xSemaphoreCreateMutex();
    if (!s_scanning)
        return ESP_ERR_NO_MEM;
    if (e == ESP_OK)
        e = led_up();
    if (e != ESP_OK)
        return e;
    return xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 5, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
