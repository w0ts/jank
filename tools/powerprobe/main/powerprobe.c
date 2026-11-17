/* SPDX-License-Identifier: GPL-3.0-only */
/* powerprobe: what a MIDI-powered jank would cost (docs/dongle.md). Not jank: a test firmware for the XIAO ESP32-S3.
 *
 * Five phases of PHASE_S seconds, each announced by the LED: n+1 short blinks for phase n. Read a USB power meter's
 * average current during each phase (the XIAO on a charger through the meter). Timings are measured on the chip, kept in
 * NVS and printed on the USB console at the next start (plug it into the Mac and read the first seconds).
 *   0  Wi-Fi listening all the time (promiscuous, the busiest channel nearby): the receiver's current
 *   1  each second: Wi-Fi stopped, light sleep, wake, Wi-Fi started and listening for WINDOW_MS: wake-to-listen time,
 *      time to the first frame heard
 *   2  the same with Wi-Fi left started across the light sleep (does it survive? does it listen right away?)
 *   3  Wi-Fi stopped, light sleep in 1 s steps: the floor
 *   4  Wi-Fi stopped, the CPU woken every 20.8 ms (MIDI clock at 120 BPM) for 1 ms: what the ticks cost
 * The frames heard are anything on air (home Wi-Fi beacons, about 10 a second per access point). */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define PHASE_S 40
#define WINDOW_MS 10
#define FIRST_WAIT_MS 120                  /* longer than a beacon interval (102.4 ms): a frame is always due */
#define LED_GPIO 21                        /* the XIAO's yellow LED, active low */
#define TICK_US 20833
#define RESULT_MAX 1400

static volatile uint32_t s_frames;
static volatile int64_t s_first_rx;
static uint8_t s_channel = 1;
static char s_result[RESULT_MAX];
static size_t s_len;

static void note(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void note(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(s_result + s_len, sizeof s_result - s_len, fmt, ap);
    va_end(ap);
    if (n > 0)
        s_len = s_len + (size_t)n < sizeof s_result ? s_len + (size_t)n : sizeof s_result - 1;
    printf("%s", s_result + s_len - (n > 0 ? (size_t)n : 0));
}

static void rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    (void)buf;
    (void)type;
    s_frames++;
    if (!s_first_rx)
        s_first_rx = esp_timer_get_time();
}

static void led(int on) { gpio_set_level(LED_GPIO, on ? 0 : 1); }

static void announce(int phase)
{
    for (int i = 0; i <= phase; i++) {
        led(1);
        vTaskDelay(pdMS_TO_TICKS(150));
        led(0);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void listen_on(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(rx_cb));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
}

/* the busiest channel nearby, from one scan: the probe's frames */
static void pick_channel(void)
{
    uint16_t n = 0;
    wifi_scan_config_t sc = {0};
    static wifi_ap_record_t ap[20];
    int count[14] = {0}, best = 1;
    if (esp_wifi_scan_start(&sc, true) == ESP_OK && esp_wifi_scan_get_ap_num(&n) == ESP_OK) {
        uint16_t got = n > 20 ? 20 : n;
        esp_wifi_scan_get_ap_records(&got, ap);
        for (int i = 0; i < got; i++)
            if (ap[i].primary >= 1 && ap[i].primary <= 13)
                count[ap[i].primary]++;
        for (int c = 1; c <= 13; c++)
            if (count[c] > count[best])
                best = c;
    }
    s_channel = (uint8_t)best;
    note("channel %u (%d access points heard there)\n", s_channel, count[best]);
}

static void light_sleep_us(int64_t us)
{
    led(0);
    gpio_hold_en(LED_GPIO);
    esp_sleep_enable_timer_wakeup((uint64_t)us);
    esp_err_t e = esp_light_sleep_start();
    gpio_hold_dis(LED_GPIO);
    if (e != ESP_OK)
        note("  light sleep refused: %s\n", esp_err_to_name(e));
}

static void phase_listen(void)
{
    uint32_t f0 = s_frames;
    listen_on();
    vTaskDelay(pdMS_TO_TICKS(PHASE_S * 1000));
    note("0 listening: %lu frames in %d s\n", (unsigned long)(s_frames - f0), PHASE_S);
}

/* stop: Wi-Fi stopped over each sleep (else left started, promiscuous off) */
static void phase_windows(int phase, int stop)
{
    int64_t start_sum = 0, start_max = 0, first_sum = 0, first_max = 0, awake_sum = 0;
    int heard = 0, cycles = 0;
    int64_t end = esp_timer_get_time() + (int64_t)PHASE_S * 1000000;
    while (esp_timer_get_time() < end) {
        int64_t t0 = esp_timer_get_time(), tw, tr;
        if (stop)
            esp_wifi_stop();
        else
            esp_wifi_set_promiscuous(false);
        light_sleep_us(1000000 - WINDOW_MS * 1000);
        tw = esp_timer_get_time();
        s_first_rx = 0;
        if (stop)
            esp_wifi_start();
        listen_on();
        tr = esp_timer_get_time();
        while (!s_first_rx && esp_timer_get_time() - tr < FIRST_WAIT_MS * 1000)
            esp_rom_delay_us(100);
        if (s_first_rx) {
            int64_t f = s_first_rx - tr;
            first_sum += f;
            first_max = f > first_max ? f : first_max;
            heard++;
        }
        start_sum += tr - tw;
        start_max = tr - tw > start_max ? tr - tw : start_max;
        awake_sum += esp_timer_get_time() - tw;
        cycles++;
        (void)t0;
    }
    note("%d windows (%s): %d cycles; wake to listening mean %lld us max %lld; first frame after %d of %d, mean %lld us"
         " max %lld; awake per cycle mean %lld us\n",
         phase, stop ? "Wi-Fi stopped over the sleep" : "Wi-Fi kept", cycles, cycles ? start_sum / cycles : 0, start_max,
         heard, cycles, heard ? first_sum / heard : 0, first_max, cycles ? awake_sum / cycles : 0);
}

static void phase_floor(void)
{
    esp_wifi_stop();
    int64_t end = esp_timer_get_time() + (int64_t)PHASE_S * 1000000;
    int n = 0;
    while (esp_timer_get_time() < end) {
        light_sleep_us(1000000);
        n++;
    }
    note("3 floor: %d light sleeps of 1 s\n", n);
}

static void phase_ticks(void)
{
    int64_t late_max = 0, late_sum = 0, next = esp_timer_get_time() + TICK_US;
    int n = 0;
    int64_t end = esp_timer_get_time() + (int64_t)PHASE_S * 1000000;
    while (esp_timer_get_time() < end) {
        int64_t now = esp_timer_get_time();
        if (next - now > 1500)
            light_sleep_us(next - now - 1500);  /* wake 1.5 ms early, then wait for the microsecond */
        while (esp_timer_get_time() < next)
            ;
        int64_t late = esp_timer_get_time() - next;
        late_sum += late;
        late_max = late > late_max ? late : late_max;
        esp_rom_delay_us(300);                   /* (a MIDI byte's worth of work) */
        next += TICK_US;
        n++;
    }
    note("4 ticks: %d, late mean %lld us max %lld (woken 1.5 ms early)\n", n, n ? late_sum / n : 0, late_max);
}

static void show_last(void)
{
    nvs_handle_t h;
    size_t n = sizeof s_result;
    if (nvs_open("probe", NVS_READONLY, &h) != ESP_OK)
        return;
    if (nvs_get_str(h, "result", s_result, &n) == ESP_OK)
        for (int i = 0; i < 12; i++) {             /* 12 s: time to open the console */
            printf("--- last run ---\n%s--- end ---\n", s_result);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    nvs_close(h);
    s_result[0] = 0;
}

static void keep(void)
{
    nvs_handle_t h;
    if (nvs_open("probe", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "result", s_result);
        nvs_commit(h);
        nvs_close(h);
    }
}

void app_main(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    led(0);
    show_last();
    s_len = 0;
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    pick_channel();
    announce(0);
    phase_listen();
    announce(1);
    phase_windows(1, 1);
    announce(2);
    esp_wifi_start();
    phase_windows(2, 0);
    announce(3);
    phase_floor();
    announce(4);
    phase_ticks();
    note("done\n");
    keep();
    for (;;) {
        led(1);
        vTaskDelay(pdMS_TO_TICKS(100));
        led(0);
        vTaskDelay(pdMS_TO_TICKS(1900));           /* done: one blink every 2 s */
    }
}
