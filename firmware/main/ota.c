/* SPDX-License-Identifier: GPL-3.0-only */
/* Firmware updates over the control line (core/jank_ota.h has the protocol), and the rollback's confirmation.
 * Everything here runs in the control task (ctl.c), never in TinyUSB's: a flash sector erase takes tens of milliseconds. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_ota.h"
#include "jank_wota.h"

#define VALID_POLL_MS 200
#define VALID_MIN_UP_US 20000000LL     /* and 20 s alive: a crash once the clock or the radio gets going is rolled back too */
#define VALID_DEADLINE_US 120000000LL  /* not confirmed by then: restart, and the bootloader puts the old image back */
#define RESTART_DELAY_MS 100

static const char *TAG = "ota";
static struct jo s_jo;
static struct jo_cmd s_cmd;
static esp_ota_handle_t s_handle;
static const esp_partition_t *s_slot;
static const esp_partition_t *s_wpart;   /* a weights transfer (ota begin ... beatw): the beatw partition, written in place */
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
    if (s_wpart) {                     /* a weights transfer holds no app slot */
        s_wpart = NULL;
        jo_abort(&s_jo);
        return;
    }
    if (s_handle)
        esp_ota_abort(s_handle);
    s_handle = 0;
    s_slot = NULL;
    jo_abort(&s_jo);
    slot_give(SLOT_MIDI);
}

/* ---- the transfer's engine, shared by the control line (do_* below) and the Wi-Fi update (ota_net_*, wota.c): each core returns the
 * answer and leaves in s_off the offset that answer carries (where the sender goes on from; 0 after a failure that ended the transfer) ---- */
static uint32_t s_off;
static uint8_t s_net_weights;          /* the transfer that just ended was the beat weights (the restart note says so) */

static enum jo_err fail(enum jo_err e, uint32_t off)
{
    s_off = off;
    return e;
}

/* the beat network's weights (core/beat/beat_weights.h) into the beatw partition. Erased first (128 KB, about a second), then written chunk by
 * chunk; no app slot, no boot change. A device whose partition table has no beatw (an image from before 2026-11-22) answers err state: it
 * needs one full flash. */
static enum jo_err begin_beatw_core(uint32_t size, uint32_t crc)
{
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "beatw");
    enum jo_err e;
    if (!p)
        return fail(JO_E_STATE, 0);
    e = jo_begin(&s_jo, size, crc, p->size, now_ms());
    if (e != JO_OK)
        return fail(e, s_jo.offset);
    s_jo.target = JO_TARGET_BEATW;
    if (esp_partition_erase_range(p, 0, p->size) != ESP_OK) {
        ESP_LOGE(TAG, "beatw: erase failed");
        jo_abort(&s_jo);
        return fail(JO_E_STATE, 0);
    }
    s_wpart = p;
    ESP_LOGI(TAG, "beat weights: %lu bytes into %s", (unsigned long)size, p->label);
    return fail(JO_OK, 0);
}

static enum jo_err begin_core(uint32_t size, uint32_t crc, uint8_t target)
{
    enum jo_err e;
    esp_err_t r;
    const esp_partition_t *slot;
    if (target == JO_TARGET_BEATW)
        return begin_beatw_core(size, crc);
    slot = esp_ota_get_next_update_partition(NULL);
    if (!slot)
        return fail(JO_E_STATE, 0);
    /* no "stop first" (the user, 2026-11-15: "drop the guard. just let me flash"): an update while playing is allowed;
     * its flash erases stall USB, Wi-Fi and the tracker for a moment, so the clock may hiccup during the transfer */
    if (!slot_take(SLOT_MIDI))             /* a jank.bin from the drive has the slot (streaming, or installed) */
        return fail(JO_E_BUSY, 0);
    e = jo_begin(&s_jo, size, crc, slot->size, now_ms());
    if (e != JO_OK) {
        if (!s_jo.active)                  /* (JO_E_BUSY: the transfer running keeps the slot) */
            slot_give(SLOT_MIDI);
        return fail(e, s_jo.offset);
    }
    r = esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &s_handle);    /* sectors are erased as the writes reach them */
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "begin: %s", esp_err_to_name(r));
        drop_flash();
        return fail(JO_E_STATE, 0);
    }
    s_slot = slot;
    ESP_LOGI(TAG, "update of %lu bytes into %s", (unsigned long)size, slot->label);
    return fail(JO_OK, 0);
}

static enum jo_err data_core(uint32_t offset, uint32_t crc, const uint8_t *data, size_t n, size_t max_n)
{
    const uint32_t at = s_jo.offset;
    enum jo_err e = jo_data_raw(&s_jo, offset, crc, data, n, max_n, now_ms());
    if (e == JO_OK) {
        esp_err_t r = s_wpart ? esp_partition_write(s_wpart, at, data, n) : esp_ota_write(s_handle, data, n);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "write at %lu: %s", (unsigned long)offset, esp_err_to_name(r));
            drop_flash();
            return fail(JO_E_STATE, 0);
        }
    }
    return fail(e, s_jo.offset);
}

/* size and CRC as the sender said, then the image as the loader sees it (header, segments, SHA-256), then the boot choice. JO_OK: the caller
 * answers, then restarts (restart_after_update) */
static enum jo_err end_core(void)
{
    enum jo_err e = jo_end(&s_jo);
    esp_err_t r = ESP_OK;
    s_net_weights = 0;
    if (e == JO_E_SHORT)
        return fail(e, s_jo.offset);
    if (e == JO_OK && s_wpart) {      /* the weights: written in place; the restart binds them (beat.c weights_bind) */
        s_wpart = NULL;
        s_net_weights = 1;
        return fail(JO_OK, s_jo.offset);
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
        return fail(e, 0);
    }
    return fail(JO_OK, s_jo.offset);
}

/* after the answer: the note for UPDATE.TXT, a moment for the answer to leave, the restart */
static void restart_after_update(const char *how)
{
    char note[64];
    if (s_net_weights) {
        ESP_LOGI(TAG, "beat weights written, restarting");
        snprintf(note, sizeof note, "beat weights written %s, restarted", how);
        drive_note(esp_ota_get_running_partition()->label, note);   /* (the app slot stays) */
    } else {
        ESP_LOGI(TAG, "update accepted, restarting");
        snprintf(note, sizeof note, "jank.bin installed %s, restarted", how);
        drive_note(s_slot->label, note);
    }
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));      /* the answer goes out first */
    esp_restart();
}

/* ---- the control line's `ota` lines ---- */
static void do_begin(const struct jo_cmd *c)
{
    enum jo_err e = begin_core(c->size, c->crc, c->target);
    int n;
    if (e != JO_OK) {
        say(e, s_off);
        return;
    }
    n = snprintf(s_reply, sizeof s_reply, "ok 0 %u\n", (unsigned)JO_CHUNK_MAX);
    usb_serial_write(s_reply, (size_t)n);
}

static void do_data(const struct jo_cmd *c)
{
    enum jo_err e = data_core(c->offset, c->crc, c->data, c->n, JO_CHUNK_MAX);
    say(e, s_off);
}

static void do_end(void)
{
    enum jo_err e = end_core();
    say(e, s_off);
    if (e == JO_OK)
        restart_after_update("over MIDI");
}

/* ---- the Wi-Fi update (wota.c): the same engine, called from the control task with a signed request's fields; ota_net_offset() is the
 * offset the answer carries ---- */
enum jo_err ota_net_begin(uint32_t size, uint32_t crc, uint8_t target) { return begin_core(size, crc, target); }

enum jo_err ota_net_data(uint32_t offset, uint32_t crc, const uint8_t *data, size_t n) { return data_core(offset, crc, data, n, JWO_DATA_MAX); }

enum jo_err ota_net_end(void) { return end_core(); }

void ota_net_restart(void) { restart_after_update("over Wi-Fi"); }

void ota_net_abort(void)
{
    drop_flash();
    s_off = 0;
}

uint32_t ota_net_offset(void) { return s_off; }

bool ota_busy(void) { return s_jo.active != 0; }

bool ota_line(const char *line)
{
    if (!strncmp(line, "otakey", 6) && (line[6] == 0 || line[6] == ' ' || line[6] == '?'))
        return wota_key_line(line);    /* the Wi-Fi update's key (wota.c): only this control line sets it, never the network. One word, not "ota key":
                                          scripts/ctl.py leaves lines that begin with the word ota to scripts/ota.py */
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

static volatile uint8_t s_confirmed;   /* this image has been confirmed (by the computer over USB, or by a signed request over Wi-Fi): valid_task has nothing left to do */

void ota_mark_valid(void)
{
    esp_err_t r = esp_ota_mark_app_valid_cancel_rollback();
    if (r != ESP_OK)
        ESP_LOGE(TAG, "mark valid: %s", esp_err_to_name(r));
    else
        ESP_LOGI(TAG, "this image is confirmed");
    s_confirmed = 1;
    drive_note_forget();
}

/* The Wi-Fi update's confirm (a signed request from the sender, after the unit came back from its restart): the image on probation is
 * confirmed once it has run VALID_MIN_UP_US. 0 confirmed, else JWO_S_NOT_PENDING / JWO_S_TOO_EARLY. In host mode there is no computer to speak
 * on the control line, so this is the only way a new image there stays; a signed request proves a sender who holds the key can reach it. */
unsigned ota_net_confirm(void)
{
    esp_ota_img_states_t st;
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (!run || esp_ota_get_state_partition(run, &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY)
        return JWO_S_NOT_PENDING;
    if (esp_timer_get_time() < VALID_MIN_UP_US)
        return JWO_S_TOO_EARLY;
    ota_mark_valid();
    return 0;
}

/* A new image starts "pending verify": the next reset of any kind, a crash included, puts the old image back. It is
 * confirmed only once the computer has configured the USB device AND has spoken to it on the MIDI control line (a SysEx
 * line from scripts/ota.py's `get` after the update, or from a page), the control task has gone round once after that,
 * which is the path the next update needs, and it has run 20 s (measured 2026-11-09: a probe build that crashed when its
 * MIDI clock started, a few seconds in, had confirmed at USB-up and looped instead of rolling back). Not confirmed within
 * VALID_DEADLINE_US: it restarts, and the bootloader starts the old image. A start as USB host (no computer) does not
 * confirm a pending image either (usb.c): its restart rolls it back. */
static void valid_task(void *arg)
{
    uint32_t beats = 0;
    int seen = 0, ready = 0;
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(VALID_POLL_MS));
        if (s_confirmed)               /* confirmed over Wi-Fi meanwhile */
            vTaskDelete(NULL);
        if (ready) {                   /* the confirm (an otadata write) is made at once, playing or not */
            ota_mark_valid();
            vTaskDelete(NULL);
        }
        if (esp_timer_get_time() >= VALID_DEADLINE_US) {
            ESP_LOGE(TAG, "no computer spoke on the MIDI control line within %d s: restarting into the old image", (int)(VALID_DEADLINE_US / 1000000));
            vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
            esp_restart();
        }
        if (!usb_up() || !usb_host_spoke())
            continue;
        if (!seen) {
            seen = 1;
            beats = ctl_beats();
        } else if (ctl_beats() != beats && esp_timer_get_time() >= VALID_MIN_UP_US) {
            ready = 1;
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
    if (xTaskCreatePinnedToCore(valid_task, "ota_valid", 4096, NULL, 1, NULL, 0) != pdPASS)
        ESP_LOGE(TAG, "no task to confirm the image: the next reset rolls it back");
}
