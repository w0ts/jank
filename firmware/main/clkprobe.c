/* SPDX-License-Identifier: GPL-3.0-only */
/* A test build's probe of the MIDI clock at the pin (JANK_CLOCK_PROBE=1 scripts/build.sh xiao; never in a shipped build).
 *
 * The MCPWM capture unit timestamps, at 80 MHz, the falling edge on the MIDI TX pad (the start bit, read back through the
 * pad's input path while the UART drives it) and a software capture made in the clock interrupt just before it fills the
 * UART FIFO. With the timer's due time and the moment the interrupt read the counter, each byte gives:
 *   isr  = the interrupt's start - due (1 us steps: the 1 MHz timer)
 *   uart = the start bit on the pad - the FIFO write (12.5 ns steps)
 *   pin  = isr + uart: the start bit against the session's grid as the stick holds it
 * The capture interrupt runs on core 0, away from the clock's core 1. Commands: `probe` (report), `probe reset`. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "board.h"
#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/mcpwm_periph.h"
#include "hal/mcpwm_ll.h"

#ifdef JANK_CLOCK_PROBE
#define SOFT_CHANNEL 1                         /* group 0, made second (the edge channel is 0) */
#define HIST 8                                 /* pin error buckets: <2, <4, <8, <16, <32, <64, <128, >=128 us */

struct stat {
    int64_t n, sum, sumsq, min, max;           /* ns */
};

static const char *TAG = "probe";
static mcpwm_cap_channel_handle_t s_edge, s_soft;
static uint32_t s_res;                         /* capture ticks a second */
static volatile int64_t s_due, s_isr;          /* us, the timer's */
static volatile uint32_t s_want, s_have, s_soft_cap, s_edge_cap;
static struct stat s_isr_st, s_uart_st, s_pin_st;
static uint32_t s_hist[HIST], s_missed;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static void add(struct stat *s, int64_t v)
{
    if (!s->n || v < s->min)
        s->min = v;
    if (!s->n || v > s->max)
        s->max = v;
    s->n++;
    s->sum += v;
    s->sumsq += v / 10 * (v / 10);             /* in 10 ns units: no overflow over hours */
}

static void settle(void)
{
    int64_t isr_ns = (s_isr - s_due) * 1000;
    int64_t uart_ns = (int64_t)(uint32_t)(s_edge_cap - s_soft_cap) * 1000000000 / s_res;
    int64_t pin = isr_ns + uart_ns, us = pin / 1000;
    int b = 0;
    while (b < HIST - 1 && us >= (2 << b))
        b++;
    portENTER_CRITICAL_ISR(&s_mux);
    add(&s_isr_st, isr_ns);
    add(&s_uart_st, uart_ns);
    add(&s_pin_st, pin);
    s_hist[b]++;
    portEXIT_CRITICAL_ISR(&s_mux);
    s_want = s_have = 0;
}

static bool on_soft(mcpwm_cap_channel_handle_t c, const mcpwm_capture_event_data_t *e, void *ctx)
{
    (void)c;
    (void)ctx;
    if (s_want && !(s_have & 1u)) {
        s_soft_cap = e->cap_value;
        s_have |= 1u;
        if (s_have == 3u)
            settle();
    }
    return false;
}

static bool on_edge(mcpwm_cap_channel_handle_t c, const mcpwm_capture_event_data_t *e, void *ctx)
{
    (void)c;
    (void)ctx;
    if (s_want && !(s_have & 2u)) {
        s_edge_cap = e->cap_value;
        s_have |= 2u;
        if (s_have == 3u)
            settle();
    }
    return false;
}

/* the clock interrupt, just before the FIFO write. A byte still unpaired is counted as missed (a lost edge) */
void IRAM_ATTR probe_mark(int64_t due, int64_t isr_now)
{
    if (s_want)
        s_missed++;
    s_have = 0;
    s_due = due;
    s_isr = isr_now;
    s_want = 1;
    mcpwm_ll_trigger_soft_capture(MCPWM_LL_GET_HW(0), SOFT_CHANNEL);   /* a register write, in IRAM: the driver's call
                                                                        * is flash code, which crashed this flash-safe
                                                                        * interrupt while core 0 wrote flash */
}

static esp_err_t up(void)
{
    mcpwm_cap_timer_handle_t t;
    mcpwm_capture_timer_config_t tc = {.group_id = 0, .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT};
    mcpwm_capture_channel_config_t edge = {.gpio_num = -1, .prescale = 1, .flags.neg_edge = 1};
    mcpwm_capture_channel_config_t soft = {.gpio_num = -1, .prescale = 1, .flags.pos_edge = 1};
    mcpwm_capture_event_callbacks_t ce = {.on_cap = on_edge}, cs = {.on_cap = on_soft};
    ESP_RETURN_ON_ERROR(mcpwm_new_capture_timer(&tc, &t), TAG, "timer");
    ESP_RETURN_ON_ERROR(mcpwm_new_capture_channel(t, &edge, &s_edge), TAG, "edge channel");
    ESP_RETURN_ON_ERROR(mcpwm_new_capture_channel(t, &soft, &s_soft), TAG, "soft channel");
    ESP_RETURN_ON_ERROR(mcpwm_capture_channel_register_event_callbacks(s_edge, &ce, NULL), TAG, "edge cb");
    ESP_RETURN_ON_ERROR(mcpwm_capture_channel_register_event_callbacks(s_soft, &cs, NULL), TAG, "soft cb");
    ESP_RETURN_ON_ERROR(mcpwm_capture_channel_enable(s_edge), TAG, "edge on");
    ESP_RETURN_ON_ERROR(mcpwm_capture_channel_enable(s_soft), TAG, "soft on");
    ESP_RETURN_ON_ERROR(mcpwm_capture_timer_get_resolution(t, &s_res), TAG, "res");
    /* the pad's input path into the edge channel, leaving the pad's function (the UART's TX) as it is */
    ESP_RETURN_ON_ERROR(gpio_input_enable(BOARD_MIDI_TX_GPIO), TAG, "input");
    /* the driver hands out group 0's channels from 0: the edge channel, made first, is channel 0 (the report's n shows it) */
    esp_rom_gpio_connect_in_signal(BOARD_MIDI_TX_GPIO, mcpwm_periph_signals.groups[0].captures[0].cap_sig, false);
    ESP_RETURN_ON_ERROR(mcpwm_capture_timer_enable(t), TAG, "timer on");
    ESP_RETURN_ON_ERROR(mcpwm_capture_timer_start(t), TAG, "timer start");
    ESP_LOGW(TAG, "clock probe on GPIO%d, %lu Hz", BOARD_MIDI_TX_GPIO, (unsigned long)s_res);
    return ESP_OK;
}

static void up_on_core0(void *arg)
{
    esp_err_t e = up();
    if (e != ESP_OK)
        ESP_LOGE(TAG, "%s", esp_err_to_name(e));
    xTaskNotify((TaskHandle_t)arg, (uint32_t)e, eSetValueWithOverwrite);
    vTaskDelete(NULL);
}

esp_err_t probe_start(void)
{
    uint32_t r = ESP_FAIL;
    if (xTaskCreatePinnedToCore(up_on_core0, "probe_init", 3072, xTaskGetCurrentTaskHandle(), 5, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    xTaskNotifyWait(0, 0, &r, portMAX_DELAY);
    return (esp_err_t)r;
}

static int put_stat(char *out, size_t cap, const char *name, const struct stat *s)
{
    int64_t mean = s->n ? s->sum / s->n : 0;
    int64_t m10 = mean / 10, var = s->n ? s->sumsq / s->n - m10 * m10 : 0;
    int64_t sd = 0;
    while ((sd + 1) * (sd + 1) <= var)         /* integer sqrt, in 10 ns */
        sd++;
    return snprintf(out, cap, "%s_ns=n %lld min %lld mean %lld sd %lld max %lld\n", name, (long long)s->n, (long long)s->min,
                    (long long)mean, (long long)(sd * 10), (long long)s->max);
}

size_t probe_report(char *out, size_t cap, int reset)
{
    struct stat a, b, c;
    uint32_t h[HIST], missed;
    size_t n = 0;
    portENTER_CRITICAL(&s_mux);
    a = s_isr_st;
    b = s_uart_st;
    c = s_pin_st;
    memcpy(h, s_hist, sizeof h);
    missed = s_missed;
    if (reset) {
        memset(&s_isr_st, 0, sizeof s_isr_st);
        memset(&s_uart_st, 0, sizeof s_uart_st);
        memset(&s_pin_st, 0, sizeof s_pin_st);
        memset(s_hist, 0, sizeof s_hist);
        s_missed = 0;
    }
    portEXIT_CRITICAL(&s_mux);
    n += put_stat(out + n, cap - n, "isr", &a);
    n += put_stat(out + n, cap - n, "uart", &b);
    n += put_stat(out + n, cap - n, "pin", &c);
    n += snprintf(out + n, cap - n, "pin_hist_us=<2:%lu <4:%lu <8:%lu <16:%lu <32:%lu <64:%lu <128:%lu more:%lu\nmissed=%lu\nok\n",
                  (unsigned long)h[0], (unsigned long)h[1], (unsigned long)h[2], (unsigned long)h[3], (unsigned long)h[4],
                  (unsigned long)h[5], (unsigned long)h[6], (unsigned long)h[7], (unsigned long)missed);
    return n;
}
#endif
