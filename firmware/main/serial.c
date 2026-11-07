/* SPDX-License-Identifier: GPL-3.0-only */
/* A board with no USB device: the control protocol on UART0, at its USB-serial chip, and a log line a second for the
 * first air tests. The line starts with '#', so the control page reads past it. */
#include <string.h>
#include "app.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_log.h"

#define CTL_UART UART_NUM_0
#define BAUD 115200
#define RX_BYTES 512
#define TX_BYTES 2048
#define LOG_EVERY_MS 1000u

static const char *TAG = "serial";
static struct jk_snapshot s_snap;

void usb_serial_write(const char *text, size_t n)
{
    if (uart_write_bytes(CTL_UART, text, n) < 0)
        ESP_LOGE(TAG, "write refused");
}

static void log_line(void)
{
    static char line[320];
    uint32_t heard[JK_GROUP_MAX + 1u], ok, fail;
    size_t n;
    bridge_snapshot(&s_snap);
    bridge_group_frames(heard);
    radio_unicast_results(&ok, &fail);
    n = jk_log_line(line, sizeof line, &s_snap, heard, ok, fail, bridge_ms());
    if (n)
        usb_serial_write(line, n);
    else
        ESP_LOGE(TAG, "log line too long");
}

static void serial_task(void *arg)
{
    uint8_t buf[64];
    uint32_t last = 0;
    (void)arg;
    for (;;) {
        int n = uart_read_bytes(CTL_UART, buf, sizeof buf, pdMS_TO_TICKS(50));
        if (n > 0)
            ctl_serial_rx(buf, (size_t)n);
        if (bridge_ms() - last >= LOG_EVERY_MS) {
            last = bridge_ms();
            log_line();
        }
    }
}

esp_err_t serial_start(void)
{
    uart_config_t c = {.baud_rate = BAUD, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    esp_err_t e = uart_driver_install(CTL_UART, RX_BYTES, TX_BYTES, 0, NULL, 0);
    if (e == ESP_OK)
        e = uart_param_config(CTL_UART, &c);
    if (e != ESP_OK)
        return e;
    return xTaskCreatePinnedToCore(serial_task, "serial", 4096, NULL, 4, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
