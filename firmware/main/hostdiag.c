/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode's probe (core/jank_hostdiag.h): the host clients fill g_hostdiag as things happen; this task saves it to flash
 * when it has changed, and `diag` prints what was saved, so what a hosting unit saw can be read once the computer is plugged in
 * again. Only a host start saves (hostdiag_start is called by usb_midi_host_start): a boot as a device keeps the last record
 * for `diag`. A flash write stalls the CPU for a moment, so none is made while the session or the clock plays, and no two
 * closer than MIN_GAP_MS. The record is read while it is written to (no lock: a torn read is a diagnostic's worst case). */
#include <string.h>
#include <stdio.h>
#include "app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "jank_hostdiag.h"
#include "usb_host_mode.h"

#define NS "hostdiag"
#define KEY "d"
#define TASK_STACK 3072
#define TASK_PRIO 1
#define EVERY_MS 5000
#define MIN_GAP_MS 10000

static const char *TAG = "hostdiag";

struct jhd g_hostdiag;

static esp_err_t save(const struct jhd *d)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READWRITE, &h);
    if (e != ESP_OK)
        return e;
    e = nvs_set_blob(h, KEY, d, sizeof *d);
    if (e == ESP_OK)
        e = nvs_commit(h);
    nvs_close(h);
    return e;
}

static void save_task(void *arg)
{
    static struct jhd snap, saved;     /* not on the stack */
    TickType_t last = 0;
    int have_saved = 0;
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(EVERY_MS));
        memcpy(&snap, &g_hostdiag, sizeof snap);
        if (have_saved && !memcmp(&snap, &saved, sizeof snap))
            continue;
        if (bridge_is_playing())
            continue;
        if (have_saved && xTaskGetTickCount() - last < pdMS_TO_TICKS(MIN_GAP_MS))
            continue;
        if (save(&snap) == ESP_OK) {
            saved = snap;
            have_saved = 1;
            last = xTaskGetTickCount();
        } else {
            ESP_LOGW(TAG, "record not saved");
        }
    }
}

void hostdiag_start(void)
{
    jhd_init(&g_hostdiag);
    g_hostdiag.stack = JHD_STACK_HOST;
    if (xTaskCreate(save_task, "hostdiag", TASK_STACK, NULL, TASK_PRIO, NULL) != pdPASS)
        ESP_LOGW(TAG, "no task: the record is not saved");
}

/* the saved record as text (the control task's, one at a time) */
size_t hostdiag_text(char *out, size_t cap)
{
    static struct jhd d;
    size_t n = sizeof d;
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READONLY, &h);
    if (e == ESP_OK) {
        e = nvs_get_blob(h, KEY, &d, &n);
        nvs_close(h);
    }
    if (e != ESP_OK || !jhd_valid(&d, n)) {
        int w = snprintf(out, cap, "hostdiag=none\n");
        return w > 0 && (size_t)w < cap ? (size_t)w : 0;
    }
    return jhd_format(&d, out, cap);
}
