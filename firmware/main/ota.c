/* SPDX-License-Identifier: GPL-3.0-only */
/* Firmware updates over the control line (core/jank_ota.h has the protocol), and the rollback's confirmation.
 * Everything here runs in the control task (ctl.c), never in TinyUSB's: a flash sector erase takes tens of milliseconds. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_ota.h"

#define VALID_POLL_MS 200
#define RESTART_DELAY_MS 100

static const char *TAG = "ota";
static struct jo s_jo;
static struct jo_cmd s_cmd;
static esp_ota_handle_t s_handle;
static const esp_partition_t *s_slot;
static char s_reply[48];

static uint32_t now_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

static void say(enum jo_err e, uint32_t offset)
{
    size_t n = jo_reply(e, offset, s_reply, sizeof s_reply);
    if (n)
        usb_serial_write(s_reply, n);
}

static void drop_flash(void)
{
    if (s_handle)
        esp_ota_abort(s_handle);
    s_handle = 0;
    s_slot = NULL;
    jo_abort(&s_jo);
}

static void do_begin(const struct jo_cmd *c)
{
    enum jo_err e;
    esp_err_t r;
    int n;
    const esp_partition_t *slot = esp_ota_get_next_update_partition(NULL);
    if (!slot) {
        say(JO_E_STATE, 0);
        return;
    }
    e = jo_begin(&s_jo, c->size, c->crc, slot->size, now_ms());
    if (e != JO_OK) {
        say(e, s_jo.offset);
        return;
    }
    r = esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &s_handle);    /* sectors are erased as the writes reach them */
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "begin: %s", esp_err_to_name(r));
        drop_flash();
        say(JO_E_STATE, 0);
        return;
    }
    s_slot = slot;
    n = snprintf(s_reply, sizeof s_reply, "ok 0 %u\n", (unsigned)JO_CHUNK_MAX);
    usb_serial_write(s_reply, (size_t)n);
    ESP_LOGI(TAG, "update of %lu bytes into %s", (unsigned long)c->size, slot->label);
}

static void do_data(const struct jo_cmd *c)
{
    enum jo_err e = jo_data(&s_jo, c, now_ms());
    if (e == JO_OK) {
        esp_err_t r = esp_ota_write(s_handle, c->data, c->n);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "write at %lu: %s", (unsigned long)c->offset, esp_err_to_name(r));
            drop_flash();
            say(JO_E_STATE, 0);
            return;
        }
    }
    say(e, s_jo.offset);
}

/* size and CRC as the sender said, then the image as the loader sees it (header, segments, SHA-256), then the boot choice */
static void do_end(void)
{
    enum jo_err e = jo_end(&s_jo);
    esp_err_t r = ESP_OK;
    if (e == JO_E_SHORT) {
        say(e, s_jo.offset);
        return;
    }
    if (e == JO_OK) {
        r = esp_ota_end(s_handle);
        s_handle = 0;
        if (r == ESP_OK)
            r = esp_ota_set_boot_partition(s_slot);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "end: %s", esp_err_to_name(r));
            e = JO_E_HEADER;
        }
    }
    if (e != JO_OK) {
        drop_flash();
        say(e, 0);
        return;
    }
    say(JO_OK, s_jo.offset);
    ESP_LOGI(TAG, "update accepted, restarting");
    drive_note(s_slot->label, "jank.bin installed over MIDI, restarted");
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));      /* the answer goes out first */
    esp_restart();
}

bool ota_busy(void) { return s_jo.active != 0; }

bool ota_line(const char *line)
{
    switch (jo_parse(line, &s_cmd)) {
    case JO_NOT_OTA:
        return false;
    case JO_BEGIN:
        do_begin(&s_cmd);
        break;
    case JO_DATA:
        do_data(&s_cmd);
        break;
    case JO_END:
        do_end();
        break;
    case JO_ABORT:
        drop_flash();
        say(JO_OK, 0);
        break;
    default:
        say(JO_E_ARG, s_jo.offset);
        break;
    }
    return true;
}

void ota_idle(void)
{
    if (jo_idle(&s_jo, now_ms()) == JO_E_IDLE) {
        ESP_LOGW(TAG, "sender silent, update dropped");
        drop_flash();
    }
}

void ota_mark_valid(void)
{
    esp_err_t r = esp_ota_mark_app_valid_cancel_rollback();
    if (r != ESP_OK)
        ESP_LOGE(TAG, "mark valid: %s", esp_err_to_name(r));
    else
        ESP_LOGI(TAG, "this image is confirmed");
    drive_note_forget();
}

/* A new image starts "pending verify": the next reset of any kind, a crash included, puts the old image back. It is
 * confirmed once it has shown that the computer sees it (USB configured by the host) and the control task has gone round
 * once after that, which is the path the next update needs. A start as USB host (no computer) confirms in usb.c before
 * its restart. */
static void valid_task(void *arg)
{
    uint32_t beats = 0;
    int seen = 0;
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(VALID_POLL_MS));
        if (!usb_up())
            continue;
        if (!seen) {
            seen = 1;
            beats = ctl_beats();
        } else if (ctl_beats() != beats) {
            ota_mark_valid();
            vTaskDelete(NULL);
        }
    }
}

void ota_validate_start(void)
{
    esp_ota_img_states_t st;
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (!run || esp_ota_get_state_partition(run, &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY)
        return;
    ESP_LOGW(TAG, "new image, not confirmed yet");
    if (xTaskCreate(valid_task, "ota_valid", 2048, NULL, 1, NULL) != pdPASS)
        ESP_LOGE(TAG, "no task to confirm the image: the next reset rolls it back");
}
