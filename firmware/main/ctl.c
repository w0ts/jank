/* SPDX-License-Identifier: GPL-3.0-only */
/* The control protocol's task: lines from the USB MIDI port's SysEx to jk_ctl_line and back. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

#define CTL_LINE_MAX 256u                /* an update chunk is a line of about 190 (core/jank_ota.h) */
#define CTL_TICK_MS 1000
#define OUT_MAX 1400u

void app_bootloader(void);
void app_reboot(void);

static StreamBufferHandle_t s_in;
static volatile uint32_t s_beats;      /* passes of the task: it is alive and taking lines */
static struct jk_snapshot s_snap;
static char s_out[OUT_MAX];

void ctl_serial_rx(const uint8_t *data, size_t n)
{
    if (s_in)
        xStreamBufferSend(s_in, data, n, 0);
}

static void run_scan(void)
{
    static struct js_table table;
    esp_err_t e = ui_scan_table(&table);
    if (e != ESP_OK)
        snprintf(s_out, sizeof s_out, "err %s\n", esp_err_to_name(e));
    else if (!js_table_report(&table, s_out, sizeof s_out))
        strcpy(s_out, "err scan answer too long\n");
    usb_serial_write(s_out, strlen(s_out));
}

static void run_line(const char *line)
{
    struct jk_cfg cfg;
    enum jk_ctl_action a;
    if (ota_line(line))
        return;
    app_cfg_get(&cfg);
    bridge_snapshot(&s_snap);
    a = jk_ctl_line(line, &cfg, &s_snap, bridge_ms(), s_out, sizeof s_out);
    if (a == JK_CTL_CFG_CHANGED)
        app_cfg_set(&cfg);
    if (a == JK_CTL_SCAN) {
        run_scan();
        return;
    }
    if (a == JK_CTL_TEMPO || a == JK_CTL_PLAY || a == JK_CTL_STOP) {
        int32_t mpb = 0;
        int r = a == JK_CTL_TEMPO ? (jk_ctl_tempo(line, &mpb) ? -1 : bridge_tempo(mpb)) : bridge_play(a == JK_CTL_PLAY);
        strcpy(s_out, r ? "err no session\n" : "ok\n");
    }
    usb_serial_write(s_out, strlen(s_out));
    if (a == JK_CTL_REBOOT || a == JK_CTL_BOOTLOADER) {
        vTaskDelay(pdMS_TO_TICKS(100));        /* the answer goes out first */
        if (a == JK_CTL_REBOOT)
            app_reboot();
        else
            app_bootloader();
    }
}

static void ctl_task(void *arg)
{
    char line[CTL_LINE_MAX];
    size_t n = 0;
    int too_long = 0;
    (void)arg;
    for (;;) {
        uint8_t c;
        ++s_beats;
        if (!xStreamBufferReceive(s_in, &c, 1, pdMS_TO_TICKS(CTL_TICK_MS))) {
            ota_idle();
            continue;
        }
        if (c == '\n' || c == '\r') {
            line[n] = 0;
            if (too_long)
                usb_serial_write("err line too long\n", 18);
            else
                run_line(line);
            n = 0;
            too_long = 0;
        } else if (n + 1u < sizeof line) {
            line[n++] = (char)c;
        } else {
            too_long = 1;
        }
    }
}

uint32_t ctl_beats(void) { return s_beats; }

esp_err_t ctl_start(void)
{
    s_in = xStreamBufferCreate(1024, 1);        /* room for a few update lines: the sender waits for answers, but a flash write stalls this task */
    if (!s_in)
        return ESP_ERR_NO_MEM;
    return xTaskCreatePinnedToCore(ctl_task, "ctl", 6144, NULL, 4, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
