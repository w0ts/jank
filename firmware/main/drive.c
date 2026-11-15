/* SPDX-License-Identifier: GPL-3.0-only */
/* The JANK drive: a made-up FAT16 volume (core/jank_vdrive.h) served straight from TinyUSB's mass storage callbacks. It
 * holds jank.html (the control page, from the image) and UPDATE.TXT (the last update's note); nothing of it is stored.
 *
 * Updating: the user copies a jank.bin (the app image, no conversion) onto the drive. Its first sector gives it away (an
 * ESP32-S3 image header); from there each sector the computer writes goes straight into the spare app slot (a 4 KB page
 * erased before its first write), so the file is never stored anywhere else: that is what lets a 4 MB chip (the FH4R2:
 * two 1.6 MB slots, no room for a drive that could hold the file) take an update by copy. Once every byte of the image is
 * in (its length comes from its own headers), esp_ota_set_boot_partition checks it (segments, SHA-256: esp_image_verify)
 * and picks it for the next start; the stick restarts when the computer has been quiet for QUIET_MS (it is still writing
 * the file's directory entry and FAT just after the data), or at an eject. A refusal (not an image that fits, a running
 * image not confirmed yet, an update over MIDI going on, a failed check, a copy cut short and ejected) is a restart too,
 * with the reason in UPDATE.TXT. Rollback (ota.c) is unchanged: the new image must confirm itself.
 *
 * TinyUSB's own MSC glue (esp_tinyusb's tinyusb_msc.c) is not used: scripts/patch_tinyusb.sh makes its TinyUSB callbacks
 * weak (third_party/esp_tinyusb-msc-weak.patch) and these take their place; its storage is never installed, so its mount
 * hooks (tud_mount_cb, tud_umount_cb) find no storage and do nothing. Flash writes run in TinyUSB's task: USB waits out a
 * page erase (tens of ms), as it did with the FAT drive's wear levelling. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_ota.h"
#include "jank_vdrive.h"
#include "nvs.h"
#include "tusb.h"

#define NOTE_MAX 160
#define NOTE_NS "jank_upd"                /* the install's note, kept in NVS across the restart (and a rollback) */
#define QUIET_MS 1500u                    /* the computer is done with the file this long after its last write ... */
#define FINISH_MAX_MS 10000u              /* ... or, writing on and on, this long after the image was whole */
#define POLL_MS 100u
_Static_assert(CONFIG_TINYUSB_MSC_BUFSIZE % JV_SECTOR == 0, "TinyUSB hands over whole sectors (Kconfig default 512 on the S3)");

extern const uint8_t page_start[] asm("_binary_jank_html_start");
extern const uint8_t page_end[] asm("_binary_jank_html_end");

static const char *TAG = "drive";
static struct jv s_d;
static char s_note[NOTE_MAX];             /* UPDATE.TXT, this start's */
static const esp_partition_t *s_slot;     /* the spare app slot */
static TaskHandle_t s_task;
static volatile uint32_t s_last_write_ms;
static volatile uint8_t s_eject;          /* the computer ejected the drive */
static volatile uint8_t s_ready;
static const char *volatile s_refused;    /* why an image was not taken (set in TinyUSB's task) */
static esp_err_t s_flash_err;
static uint8_t *s_page;                   /* 4 KB, only for a sector of the image written twice */

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

/* the note for UPDATE.TXT at the next start: the slot that should run then, and what to say. In NVS, not RTC memory:
 * another image's start (one that crashes and is rolled back) may reuse RTC memory, NVS it leaves alone */
void drive_note(const char *slot, const char *text)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NOTE_NS, NVS_READWRITE, &h);
    if (e == ESP_OK) {
        e = nvs_set_str(h, "slot", slot);
        if (e == ESP_OK)
            e = nvs_set_str(h, "text", text);
        if (e == ESP_OK)
            e = nvs_commit(h);
        nvs_close(h);
    }
    if (e != ESP_OK)
        ESP_LOGE(TAG, "note: %s", esp_err_to_name(e));
}

void drive_note_forget(void)
{
    nvs_handle_t h;
    if (nvs_open(NOTE_NS, NVS_READWRITE, &h) != ESP_OK)
        return;
    if (nvs_erase_all(h) == ESP_OK)
        nvs_commit(h);
    nvs_close(h);
}

/* the note's text for this start in out ("" none) -> 1 forget it now (jo_note_at_start) */
static int note_for_start(char *out, size_t cap)
{
    char slot[17] = "", text[NOTE_MAX] = "";
    size_t ns = sizeof slot, nt = sizeof text;
    nvs_handle_t h;
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st = ESP_OTA_IMG_VALID;
    out[0] = 0;
    if (nvs_open(NOTE_NS, NVS_READONLY, &h) != ESP_OK)
        return 0;
    int got = nvs_get_str(h, "slot", slot, &ns) == ESP_OK && nvs_get_str(h, "text", text, &nt) == ESP_OK;
    nvs_close(h);
    if (!got || !run)
        return 0;
    esp_ota_get_state_partition(run, &st);
    return jo_note_at_start(slot, run->label, st == ESP_OTA_IMG_PENDING_VERIFY, text, out, cap);
}

static void *alloc_sector(void *ctx)
{
    (void)ctx;
    void *p = heap_caps_malloc(JV_SECTOR, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_malloc(JV_SECTOR, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static int read_slot(void *ctx, uint32_t offset, uint8_t *out, uint32_t n)
{
    (void)ctx;
    return esp_partition_read(s_slot, offset, out, n) == ESP_OK ? 0 : -1;
}

/* The spare slot has one writer at a time: this drive or an update over MIDI (ota.c). Taken under a lock, so that the two
 * cannot both pass their check; the drive keeps it from an image's first sector until the restart (or its failure). */
static portMUX_TYPE s_slot_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_slot_user;               /* enum slot_user */

bool slot_take(enum slot_user who)
{
    bool ok;
    portENTER_CRITICAL(&s_slot_mux);
    ok = s_slot_user == SLOT_FREE || s_slot_user == who;
    if (ok)
        s_slot_user = (uint8_t)who;
    portEXIT_CRITICAL(&s_slot_mux);
    return ok;
}

void slot_give(enum slot_user who)
{
    portENTER_CRITICAL(&s_slot_mux);
    if (s_slot_user == who)
        s_slot_user = SLOT_FREE;
    portEXIT_CRITICAL(&s_slot_mux);
}

/* the running image is a new one not confirmed yet: any restart now puts the old one back (the bootloader marks a
 * PENDING_VERIFY slot aborted at boot: bootloader_support/src/bootloader_utility.c) */
static bool running_unconfirmed(void)
{
    esp_ota_img_states_t st;
    const esp_partition_t *run = esp_ota_get_running_partition();
    return run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
}

/* why an image may not start now, or NULL with the slot taken (esp_ota_begin's own checks, which this file does not call) */
static const char *start_refused(void)
{
    if (!s_slot)
        return "no spare app slot";
    if (running_unconfirmed())
        return "the running firmware is not confirmed yet (it confirms itself once a computer has talked to it for 20 s)";
    if (!slot_take(SLOT_DRIVE))
        return "an update over MIDI is running";
    return NULL;
}

static esp_err_t flash_op(const struct jv_op *op, const uint8_t *data)
{
    esp_err_t e = ESP_OK;
    if (op->erase_page >= 0)
        e = esp_partition_erase_range(s_slot, (size_t)op->erase_page * JV_PAGE, JV_PAGE);
    return e == ESP_OK ? esp_partition_write(s_slot, op->offset, data, JV_SECTOR) : e;
}

/* 1 when the slot holds other bytes than data at op's sector, 0 the same, -1 unreadable */
static int slot_differs(const struct jv_op *op, const uint8_t *data)
{
    uint8_t old[64];
    for (uint32_t i = 0; i < JV_SECTOR; i += sizeof old) {
        if (esp_partition_read(s_slot, op->offset + i, old, sizeof old) != ESP_OK)
            return -1;
        if (memcmp(old, data + i, sizeof old))
            return 1;
    }
    return 0;
}

/* a sector of the image written again with other bytes: the page read, the sector put in, the page erased and written back */
static esp_err_t flash_rewrite(const struct jv_op *op, const uint8_t *data)
{
    uint32_t base = op->offset / JV_PAGE * JV_PAGE;
    esp_err_t e;
    if (!s_page && !(s_page = heap_caps_malloc(JV_PAGE, MALLOC_CAP_8BIT)))
        return ESP_ERR_NO_MEM;
    e = esp_partition_read(s_slot, base, s_page, JV_PAGE);
    if (e == ESP_OK) {
        memcpy(s_page + (op->offset - base), data, JV_SECTOR);
        e = esp_partition_erase_range(s_slot, base, JV_PAGE);
    }
    return e == ESP_OK ? esp_partition_write(s_slot, base, s_page, JV_PAGE) : e;
}

/* an image's first sector (or a new one over a half-finished copy): refused images are only noted, never a restart */
static esp_err_t start(uint32_t lba, const uint8_t *data, struct jv_op *op)
{
    const uint8_t *early;
    const char *why = start_refused();
    esp_err_t e;
    if (why) {
        s_refused = why;
        jv_fail(&s_d);
        ESP_LOGW(TAG, "jank.bin not taken: %s", why);
        return ESP_OK;
    }
    s_refused = NULL;
    ESP_LOGI(TAG, "an image starts at sector %lu", (unsigned long)lba);
    e = flash_op(op, data);
    if (e == ESP_OK)
        jv_written(&s_d, op, read_slot, NULL);
    while (e == ESP_OK && jv_take_early(&s_d, &early, op))
        if ((e = flash_op(op, early)) == ESP_OK)
            jv_written(&s_d, op, read_slot, NULL);
    return e;
}

/* one sector the computer wrote (TinyUSB's task) */
static void write_sector(uint32_t lba, const uint8_t *data)
{
    struct jv_op op;
    const uint8_t prior = s_d.phase;
    enum jv_kind k = jv_write(&s_d, lba, data, &op);
    esp_err_t e = ESP_OK;
    if (k == JV_W_REWRITE) {
        int differs = slot_differs(&op, data);
        k = differs < 0 ? JV_W_IGNORED : jv_rewrite(&s_d, lba, data, differs, &op);
        if (differs < 0)
            e = ESP_FAIL;
    }
    if (k == JV_W_START)
        e = start(lba, data, &op);
    else if (k == JV_W_IMAGE || k == JV_W_REWRITE) {
        e = k == JV_W_IMAGE ? flash_op(&op, data) : flash_rewrite(&op, data);
        if (e == ESP_OK)
            jv_written(&s_d, &op, read_slot, NULL);
    }
    if (e != ESP_OK) {
        s_flash_err = e;
        s_refused = "flash write failed";
        jv_fail(&s_d);
    } else if (s_d.phase == JV_FAILED && prior == JV_STREAM && !s_refused) {
        s_refused = "not an ESP32-S3 app image that fits the app slot";
    }
    if (s_d.phase == JV_FAILED && prior != JV_FAILED && s_slot_user == SLOT_DRIVE) {
        slot_give(SLOT_DRIVE);            /* the slot holds nothing chosen: free for another copy or an update over MIDI */
        if (s_task)
            xTaskNotifyGive(s_task);      /* a restart to show why, if one is safe */
    }
    if (s_d.phase == JV_DONE && prior != JV_DONE && s_task)
        xTaskNotifyGive(s_task);
}

/* --- TinyUSB's mass storage callbacks (msc_device.h); they replace esp_tinyusb's, made weak by the patch --- */

uint8_t tud_msc_get_maxlun_cb(void) { return 1; }

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;
    memcpy(vendor_id, "jank", 4);
    memcpy(product_id, "JANK drive", 10);
    memcpy(product_rev, "2.0", 3);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    if (s_ready)
        return true;
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);   /* medium not present, until drive_start is through */
    return false;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;
    *block_count = JV_SECTORS;
    *block_size = JV_SECTOR;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void)lun;
    (void)power_condition;
    if (load_eject && !start) {
        s_eject = 1;
        if (s_task)
            xTaskNotifyGive(s_task);
    }
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    static uint8_t sec[JV_SECTOR];
    uint8_t *out = buffer;
    uint32_t done = 0;
    while (done < bufsize) {
        uint32_t at = 0, n;
        int r = jv_read(&s_d, lba, sec, &at);
        if (r < 0 || (r == 1 && esp_partition_read(s_slot, at, sec, JV_SECTOR) != ESP_OK)) {
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0x00);   /* logical block address out of range */
            return -1;
        }
        n = JV_SECTOR - offset < bufsize - done ? JV_SECTOR - offset : bufsize - done;
        memcpy(out + done, sec + offset, n);
        done += n;
        offset = 0;
        lba++;
    }
    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    if (offset || bufsize % JV_SECTOR || lba + bufsize / JV_SECTOR > JV_SECTORS) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0x00);
        return -1;
    }
    for (uint32_t i = 0; i < bufsize / JV_SECTOR; i++)
        write_sector(lba + i, buffer + i * JV_SECTOR);
    s_last_write_ms = now_ms();
    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    (void)buffer;
    (void)bufsize;
    switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        return 0;
    case 0x35:                            /* SYNCHRONIZE CACHE (10): nothing is cached, every write is done when answered */
        return 0;
    default:
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);   /* invalid command operation code */
        return -1;
    }
}

/* --- the install --- */

/* The note for UPDATE.TXT, then the restart once the computer is quiet. A restart that would cost something is not made:
 * with the running image not confirmed yet (the bootloader would roll it back) or an update over MIDI running (it would
 * be cut off), a refusal is only noted (`drive` on the control line says why) and the stick runs on. installed: the new
 * image is the boot choice, the restart is the point. */
static void finish(const char *text, const char *slot, bool installed)
{
    uint32_t done_at = now_ms();
    ESP_LOGW(TAG, "%s", text);
    if (!installed && (running_unconfirmed() || !slot_take(SLOT_DRIVE))) {
        ESP_LOGW(TAG, "no restart now: it would roll back the new firmware or cut an update over MIDI");
        return;
    }
    drive_note(slot, text);
    while (!s_eject && now_ms() - s_last_write_ms < QUIET_MS && now_ms() - done_at < FINISH_MAX_MS)
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    esp_restart();                        /* the next start shows the note in UPDATE.TXT */
}

static void drive_task(void *arg)
{
    char text[NOTE_MAX];
    const esp_partition_t *run = esp_ota_get_running_partition();
    const char *run_label = run ? run->label : "";
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (s_d.phase == JV_DONE) {
            esp_err_t e = esp_ota_set_boot_partition(s_slot);   /* esp_image_verify first: segments, checksum, SHA-256 */
            if (e == ESP_OK)
                finish("jank.bin installed from the drive, restarted", s_slot->label, true);
            snprintf(text, sizeof text, "jank.bin not installed: image check failed (%s)", esp_err_to_name(e));
            finish(text, run_label, false);   /* the slot is still the drive's: the restart is made */
        } else if (s_d.phase == JV_FAILED && s_slot_user != SLOT_DRIVE) {
            snprintf(text, sizeof text, "jank.bin not installed: %s", s_refused ? s_refused : "refused");
            finish(text, run_label, false);
        } else if (s_eject && s_d.phase == JV_STREAM) {
            snprintf(text, sizeof text, "jank.bin not installed: the copy was not whole (%lu of its sectors came)", (unsigned long)s_d.sectors_in);
            finish(text, run_label, false);
        }
        s_eject = 0;                      /* an eject with no update on the drive: nothing to do */
    }
}

void drive_state(char *out, size_t cap)
{
    static const char *const phase[] = {"idle", "stream", "done", "failed"};
    snprintf(out, cap, "drive=%s\ndrive_image=%s %lu/%lu bytes, %lu sectors in\ndrive_cache=%lu dropped=%lu%s%s\n", s_ready ? "ok" : "not started",
             phase[s_d.phase & 3u], (unsigned long)(s_d.frontier * JV_SECTOR), (unsigned long)jv_image_len(&s_d), (unsigned long)s_d.sectors_in,
             (unsigned long)s_d.ncache, (unsigned long)s_d.cache_dropped, s_refused ? " refused: " : "", s_refused ? s_refused : "");
}

esp_err_t drive_start(void)
{
    uint8_t mac[6] = {0};
    int forget = note_for_start(s_note, sizeof s_note);
    if (s_note[0])
        strlcat(s_note, "\r\n", sizeof s_note);
    if (forget)
        drive_note_forget();
    s_slot = esp_ota_get_next_update_partition(NULL);
    esp_efuse_mac_get_default(mac);
    /* page_end - 1: a text blob ends in a NUL (target_add_binary_data TEXT) */
    jv_init(&s_d, page_start, (uint32_t)(page_end - page_start - 1), s_note, (uint32_t)mac[2] << 24 | (uint32_t)mac[3] << 16 | (uint32_t)mac[4] << 8 | mac[5],
            s_slot ? (uint32_t)s_slot->size : 0u, alloc_sector, NULL);
    /* the image check (esp_image_verify, SHA-256) and the NVS note run on this stack, as ota.c's do_end did on the control task's */
    if (xTaskCreatePinnedToCore(drive_task, "drive", 4096, NULL, 3, &s_task, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    s_ready = 1;
    return s_slot ? ESP_OK : ESP_ERR_NOT_FOUND;
}
