/* SPDX-License-Identifier: GPL-3.0-only */
/* MIDI out: a 1 MHz hardware timer that doubles as Link's clock, and UART1 at 31250 baud, plus USB MIDI on the S3.
 * The timer's alarm interrupt runs jc_step (multiply and shift only, in IRAM) and writes the bytes straight into the UART's
 * FIFO: no task is between the alarm and the wire. It is pinned to core 1, level 3, away from the Wi-Fi task on core 0. */
#include <string.h>
#include "app.h"
#include "board.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/uart.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/uart_ll.h"
#include "soc/uart_periph.h"

#define MIDI_UART UART_NUM_1
#define MIDI_BAUD 31250
#define ALARM_MARGIN_US 10            /* an alarm has to be ahead of the counter or it never fires */

static const char *TAG = "midi";
static gptimer_handle_t s_timer;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static DRAM_ATTR struct jc_map s_map;
static DRAM_ATTR struct jc_ticker s_tick;
static uint8_t s_running;

static void IRAM_ATTR arm(gptimer_handle_t timer, int64_t at, int64_t now)
{
    gptimer_alarm_config_t a = {.alarm_count = (uint64_t)(at > now + ALARM_MARGIN_US ? at : now + ALARM_MARGIN_US)};
    gptimer_set_alarm_action(timer, &a);
}

static bool IRAM_ATTR on_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *ev, void *ctx)
{
    struct jc_fire f;
    int64_t now = (int64_t)ev->count_value;
    bool woken = false;
    (void)ctx;
    portENTER_CRITICAL_ISR(&s_mux);
    jc_step(&s_tick, &s_map, now, &f);
    portEXIT_CRITICAL_ISR(&s_mux);
    uart_ll_write_txfifo(UART_LL_GET_HW(MIDI_UART), f.bytes, f.n);
#if CONFIG_JANK_USB_NET
    woken = usb_midi_isr(f.bytes, f.n);
#endif
    arm(timer, f.next, now);
    return woken;
}

#ifdef BOARD_MIDI_TX_DRIVE
static esp_err_t tx_drive(int pin)
{
    return gpio_set_drive_capability(pin, BOARD_MIDI_TX_DRIVE);
}
#endif

#ifdef BOARD_MIDI_TX_MORE_GPIOS
/* More MIDI outs: the same UART TX signal on more pads, through the GPIO matrix, exactly as uart_set_pin routes the first
 * (pad to its GPIO function, then the signal; the ROM patch enables the output after the signal is in place, no glitch).
 * The hardware copies each bit to every pad at once: nothing in the clock path changes. */
static esp_err_t tx_more_up(void)
{
    static const int pins[] = BOARD_MIDI_TX_MORE_GPIOS;
    esp_err_t e = ESP_OK;
    for (size_t i = 0; i < sizeof pins / sizeof pins[0] && e == ESP_OK; i++) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(pins[i]) || pins[i] == BOARD_MIDI_TX_GPIO)
            return ESP_ERR_INVALID_ARG;
        esp_rom_gpio_pad_select_gpio((uint32_t)pins[i]);
#ifdef BOARD_MIDI_TX_DRIVE
        e = tx_drive(pins[i]);
#endif
        if (e == ESP_OK)
            esp_rom_gpio_connect_out_signal((uint32_t)pins[i], UART_PERIPH_SIGNAL(MIDI_UART, SOC_UART_TX_PIN_IDX), false, false);
    }
    return e;
}
#endif

static esp_err_t uart_up(void)
{
    uart_config_t c = {.baud_rate = MIDI_BAUD, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    esp_err_t e = uart_param_config(MIDI_UART, &c);
    if (e == ESP_OK)
        e = uart_set_pin(MIDI_UART, BOARD_MIDI_TX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
#ifdef BOARD_MIDI_TX_DRIVE
    if (e == ESP_OK)
        e = tx_drive(BOARD_MIDI_TX_GPIO);
#endif
#ifdef BOARD_MIDI_TX_MORE_GPIOS
    if (e == ESP_OK && (e = tx_more_up()) != ESP_OK)
        ESP_LOGE(TAG, "more MIDI outs: %s", esp_err_to_name(e));
#endif
    return e;
}

static esp_err_t source_pin_up(void)
{
#if BOARD_MIDI_SOURCE_GPIO >= 0
    esp_err_t e = gpio_set_direction(BOARD_MIDI_SOURCE_GPIO, GPIO_MODE_OUTPUT);
    if (e == ESP_OK)
        e = gpio_set_drive_capability(BOARD_MIDI_SOURCE_GPIO, GPIO_DRIVE_CAP_3);
    if (e == ESP_OK)
        e = gpio_set_level(BOARD_MIDI_SOURCE_GPIO, 1);
    return e;
#else
    return ESP_OK;
#endif
}

static esp_err_t timer_up(void)
{
    gptimer_config_t c = {.clk_src = GPTIMER_CLK_SRC_DEFAULT, .direction = GPTIMER_COUNT_UP, .resolution_hz = 1000000, .intr_priority = 3};
    gptimer_event_callbacks_t cb = {.on_alarm = on_alarm};
    esp_err_t e = gptimer_new_timer(&c, &s_timer);
    if (e == ESP_OK)
        e = gptimer_register_event_callbacks(s_timer, &cb, NULL);
    if (e == ESP_OK)
        e = gptimer_enable(s_timer);
    if (e == ESP_OK)
        e = gptimer_start(s_timer);
    return e;
}

/* the interrupt belongs to the core that allocates it */
static void start_on_core1(void *arg)
{
    TaskHandle_t parent = arg;
    esp_err_t e = timer_up();
    if (e != ESP_OK)
        ESP_LOGE(TAG, "timer: %s", esp_err_to_name(e));
    xTaskNotify(parent, (uint32_t)e, eSetValueWithOverwrite);
    vTaskDelete(NULL);
}

esp_err_t midi_start(void)
{
    uint32_t r = ESP_FAIL;
    esp_err_t e = uart_up();
    if (e == ESP_OK)
        e = source_pin_up();
    if (e != ESP_OK)
        return e;
    jc_ticker_init(&s_tick, 4);
    if (xTaskCreatePinnedToCore(start_on_core1, "midi_init", 3072, xTaskGetCurrentTaskHandle(), 5, NULL, 1) != pdPASS)
        return ESP_ERR_NO_MEM;
    xTaskNotifyWait(0, 0, &r, portMAX_DELAY);
    return (esp_err_t)r;
}

int64_t midi_now_us(void)
{
    uint64_t c = 0;
    ESP_ERROR_CHECK(gptimer_get_raw_count(s_timer, &c));
    return (int64_t)c;
}

void midi_set_quantum(uint32_t quantum)
{
    portENTER_CRITICAL(&s_mux);
    s_tick.bar_ticks = quantum * JC_PPQN;
    portEXIT_CRITICAL(&s_mux);
}

void midi_publish(const struct jc_map *m)
{
    int64_t now = midi_now_us();
    int begin = 0;
    portENTER_CRITICAL(&s_mux);
    s_map = *m;
    if (!s_running) {
        begin = jc_begin(&s_tick, &s_map, now + 1000) == 0;
        s_running = (uint8_t)begin;
    }
    portEXIT_CRITICAL(&s_mux);
    if (begin) {
        arm(s_timer, s_tick.t_due, now);
    }
}

void midi_want(int play)
{
    portENTER_CRITICAL(&s_mux);
    jc_want(&s_tick, play);
    portEXIT_CRITICAL(&s_mux);
}

void midi_resync(void)
{
    portENTER_CRITICAL(&s_mux);
    jc_resync(&s_tick);
    portEXIT_CRITICAL(&s_mux);
}

void midi_stats(uint32_t *ticks, uint32_t *resyncs)
{
    portENTER_CRITICAL(&s_mux);
    *ticks = s_tick.sent;
    *resyncs = s_tick.resyncs;
    portEXIT_CRITICAL(&s_mux);
}
