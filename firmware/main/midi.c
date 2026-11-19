/* SPDX-License-Identifier: GPL-3.0-only */
/* MIDI out: a 1 MHz hardware timer that doubles as Link's clock, and UART1 at 31250 baud, plus USB MIDI on the S3.
 * The timer's alarm interrupt runs jc_step (multiply and shift only, in IRAM) and writes the bytes straight into the UART's
 * FIFO: no task is between the alarm and the wire. It is pinned to core 1, level 3, and core 1 runs nothing else: every task
 * (ours, TinyUSB's, the USB interrupt, FreeRTOS's timers, lwIP) is on core 0, because a critical section on core 1 masks
 * this level-3 interrupt (measured 2026-11-09: late ticks up to 37 us while TinyUSB's task shared core 1). */
#include <string.h>
#include "app.h"
#include "board.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/uart.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/uart_ll.h"
#ifdef BOARD_BENCH_CLK_GPIO
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#endif
#include "soc/uart_periph.h"

#define MIDI_UART UART_NUM_1
#define MIDI_BAUD 31250
#define ALARM_MARGIN_US 10            /* an alarm has to be ahead of the counter or it never fires */
/* the alarm goes this early: from the alarm to the start bit on the pad takes 2.3 us (interrupt entry ~2.0 us, the UART
 * 0.3 us; clkprobe.c, measured 2026-11-09 on the XIAO, 160 MHz, sd 0.18 us): the start bit then lands on the tick.
 * The beat builds run at 160 MHz again (sdkconfig.beat, 2026-11-18), the clock this lead was measured at. */
#define CLK_LEAD_US 2

static const char *TAG = "midi";
static gptimer_handle_t s_timer;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static DRAM_ATTR struct jc_map s_map;
static DRAM_ATTR struct jc_ticker s_tick;
static uint8_t s_running;
static DRAM_ATTR volatile uint32_t s_late_max_us;   /* the alarm's worst lateness since boot (ABOUT screen): one compare and store per tick */
#ifdef BOARD_BENCH_CLK_GPIO
static DRAM_ATTR uint8_t s_bench_clk;   /* the CLK pin's level: it changes on every F8 */
static DRAM_ATTR uint8_t s_bench_rst;   /* the RST pin is high: low again at the next clock */

/* the bench pins (board.h), in the clock's interrupt: register writes only (gpio_ll, inline), the state in DRAM */
static void IRAM_ATTR bench_pins(const struct jc_fire *f)
{
    for (unsigned i = 0; i < f->n; i++) {
        if (f->bytes[i] == JC_START) {
            gpio_ll_set_level(&GPIO, BOARD_BENCH_RST_GPIO, 1);
            s_bench_rst = 2;            /* (this tick's own F8 follows: low at the one after) */
        } else if (f->bytes[i] == JC_CLOCK) {
            s_bench_clk ^= 1u;
            gpio_ll_set_level(&GPIO, BOARD_BENCH_CLK_GPIO, s_bench_clk);
            if (s_bench_rst && --s_bench_rst == 0)
                gpio_ll_set_level(&GPIO, BOARD_BENCH_RST_GPIO, 0);
        }
    }
}

static esp_err_t bench_pins_up(void)
{
    gpio_config_t c = {.pin_bit_mask = (1ull << BOARD_BENCH_CLK_GPIO) | (1ull << BOARD_BENCH_RST_GPIO), .mode = GPIO_MODE_OUTPUT,
                       .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE};
    esp_err_t e = gpio_config(&c);
    if (e == ESP_OK)
        e = gpio_set_level(BOARD_BENCH_CLK_GPIO, 0);
    if (e == ESP_OK)
        e = gpio_set_level(BOARD_BENCH_RST_GPIO, 0);
    return e;
}
#endif

/* core 1's load: the cycles spent in on_alarm, summed (one writer, the interrupt; read from core 0 as a running count) */
static volatile uint32_t s_busy_cyc;
uint32_t midi_busy_cycles(void) { return s_busy_cyc; }

static void IRAM_ATTR arm(gptimer_handle_t timer, int64_t at, int64_t now)
{
    gptimer_alarm_config_t a = {.alarm_count = (uint64_t)(at > now + ALARM_MARGIN_US ? at : now + ALARM_MARGIN_US)};
    gptimer_set_alarm_action(timer, &a);
}

static bool IRAM_ATTR on_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *ev, void *ctx)
{
    struct jc_fire f;
    const uint32_t c_in = esp_cpu_get_cycle_count();
    int64_t now = (int64_t)ev->count_value;
    bool woken = false;
    (void)ctx;
    const uint64_t late = (uint64_t)now - ev->alarm_value;     /* (an early count wraps to a huge value: ignored) */
    if (late > s_late_max_us && late < 1000000u)
        s_late_max_us = (uint32_t)late;
    portENTER_CRITICAL_ISR(&s_mux);
    jc_step(&s_tick, &s_map, now, &f);
    portEXIT_CRITICAL_ISR(&s_mux);
#ifdef JANK_CLOCK_PROBE
    if (f.n)
        probe_mark((int64_t)ev->alarm_value + CLK_LEAD_US, now);
#endif
    uart_ll_write_txfifo(UART_LL_GET_HW(MIDI_UART), f.bytes, f.n);
#ifdef BOARD_BENCH_CLK_GPIO
    bench_pins(&f);                      /* right after the UART starts the byte: the edge sits with the start bit */
#endif
    woken = usb_midi_isr(f.bytes, f.n);
    arm(timer, f.next - CLK_LEAD_US, now);
    s_busy_cyc += esp_cpu_get_cycle_count() - c_in;     /* core 1's load: this interrupt's cycles (two counter reads and an add) */
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
 * (pad to its GPIO function, then the signal. On the ESP32 / S2 the ROM patch enables the output after the signal is in
 * place; on the S3 the ROM routine enables it first, so the pad is driven low for an instant: the MIDI FET stays off,
 * harmless. Review mini3-6A N8, ESP-IDF v5.5 esp_rom_gpio.c.)
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
#ifdef BOARD_MIDI_TX_INVERT
    /* the loop is sunk by an n-FET whose gate is the TX line: idle must be LOW. Inverted before the pin is routed, so
       the line never pulses high once at start (review mini3-3A W1); the MORE pins carry the same inverted signal */
    if (e == ESP_OK)
        e = uart_set_line_inverse(MIDI_UART, UART_SIGNAL_TXD_INV);
#endif
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
#ifdef BOARD_BENCH_CLK_GPIO
    if (e == ESP_OK)
        e = bench_pins_up();             /* before the timer: its interrupt drives them from the first tick */
#endif
    if (e != ESP_OK)
        return e;
#ifdef JANK_CLOCK_PROBE
    if ((e = probe_start()) != ESP_OK)
        ESP_LOGE(TAG, "probe: %s", esp_err_to_name(e));
#endif
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

/* the same clock from an IRAM interrupt that runs with the cache off (the LINE IN's ADC frames, linein.c):
 * gptimer_get_raw_count is in IRAM with CONFIG_GPTIMER_CTRL_FUNC_IN_IRAM (sdkconfig.defaults; esp_driver_gptimer
 * linker.lf), the timer object in internal RAM with CONFIG_GPTIMER_OBJ_CACHE_SAFE. Its spinlock is the timer's own, held
 * for one counter capture, as the USB audio interrupt's midi_now_us already does on core 0. */
int64_t IRAM_ATTR midi_now_us_isr(void)
{
    uint64_t c = 0;
    if (gptimer_get_raw_count(s_timer, &c) != ESP_OK)
        return -1;
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
        arm(s_timer, s_tick.t_due - CLK_LEAD_US, now);
    }
}

int midi_start_pending(void)
{
    int r;
    portENTER_CRITICAL(&s_mux);
    r = s_tick.want_start || s_tick.resync;
    portEXIT_CRITICAL(&s_mux);
    return r;
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

int midi_is_playing(void)
{
    int r;
    portENTER_CRITICAL(&s_mux);
    r = s_tick.playing || s_tick.want_start || s_tick.resync;
    portEXIT_CRITICAL(&s_mux);
    return r;
}

uint32_t midi_late_max_us(void) { return s_late_max_us; }

void midi_stats(uint32_t *ticks, uint32_t *resyncs)
{
    portENTER_CRITICAL(&s_mux);
    *ticks = s_tick.sent;
    *resyncs = s_tick.resyncs;
    portEXIT_CRITICAL(&s_mux);
}
