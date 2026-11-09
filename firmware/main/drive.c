/* SPDX-License-Identifier: GPL-3.0-only */
/* The JANK drive: a real FAT volume the computer can write to, wiped at every start and given jank.html again.
 *
 * Updating: the user copies a jank.bin (the app image, no conversion) onto the drive and ejects it. Eject is SCSI START STOP
 * UNIT with LoEj; esp_tinyusb answers it by taking the file system away from the host and mounting it for the app
 * (msc_storage_mount_to_app, event TINYUSB_MSC_EVENT_MOUNT_COMPLETE with the app as owner). Only then does this file read
 * the volume: the host has flushed its cache and no longer touches the sectors, so the file is whole. The image is checked
 * (header, chip, size, then esp_ota_end's SHA-256 over the segments), written to the other app slot and chosen for the next
 * start; the restart wipes the drive. Any failure is also a restart, with a note UPDATE.TXT on the clean drive.
 * The volume is never changed while the host has it mounted: macOS and Windows keep FAT and directory sectors in a cache
 * and write their copy back, so a change made behind their back is lost or, worse, corrupts the volume. The wipe and the
 * page are therefore written at the start only, before USB is up, and after an update (a restart). */
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include "app.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_ota.h"
#include "tinyusb_msc.h"
#include "wear_levelling.h"

#define MOUNT CONFIG_TINYUSB_MSC_MOUNT_PATH
#define PATH_MAX_LEN 128
#define WIPE_DEPTH 4
#define COPY_BLOCK 4096u
#define NOTE_MAX 80
#define NOTE_MAGIC 0x4A4E4F54u
#define ERASED_MAGIC 0x4A455241u

extern const char page_start[] asm("_binary_jank_html_start");
extern const char page_end[] asm("_binary_jank_html_end");

static const char *TAG = "drive";
static tinyusb_msc_storage_handle_t s_storage;
static TaskHandle_t s_task;
static volatile int s_ready;                 /* the start is over: an app mount from now on is an eject (or an unplug) */
static RTC_NOINIT_ATTR uint32_t s_note_magic;
static RTC_NOINIT_ATTR char s_note[NOTE_MAX];
static RTC_NOINIT_ATTR uint32_t s_erased;
static uint8_t s_block[COPY_BLOCK];

static void note(const char *fmt, const char *arg)
{
    snprintf(s_note, sizeof s_note, fmt, arg);
    s_note_magic = NOTE_MAGIC;
}

/* removes everything below dir (not dir itself): the macOS and Windows litter as well as the user's files */
static esp_err_t wipe(const char *dir, int depth)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    esp_err_t r = ESP_OK;
    if (!d)
        return ESP_FAIL;
    while ((e = readdir(d)) != NULL && r == ESP_OK) {
        char path[PATH_MAX_LEN];
        struct stat st;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path || stat(path, &st))
            r = ESP_FAIL;
        else if (!S_ISDIR(st.st_mode))
            r = unlink(path) ? ESP_FAIL : ESP_OK;
        else
            r = depth < WIPE_DEPTH && wipe(path, depth + 1) == ESP_OK && !rmdir(path) ? ESP_OK : ESP_FAIL;
    }
    closedir(d);
    return r;
}

static esp_err_t write_file(const char *path, const void *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    esp_err_t r = ESP_OK;
    if (!f)
        return ESP_FAIL;
    if (fwrite(data, 1, n, f) != n)
        r = ESP_FAIL;
    if (fclose(f))
        r = ESP_FAIL;
    return r;
}

static esp_err_t restore(void)
{
    esp_err_t r = write_file(MOUNT "/jank.html", page_start, (size_t)(page_end - page_start - 1));   /* a text blob ends in a NUL */
    if (r == ESP_OK && s_note_magic == NOTE_MAGIC) {
        s_note[NOTE_MAX - 1] = 0;
        r = write_file(MOUNT "/UPDATE.TXT", s_note, strlen(s_note));
    }
    s_note_magic = 0;
    return r;
}

/* the largest *.bin in the root: the update (a copy of the app image) */
static int find_image(char *path, size_t cap, size_t *size)
{
    DIR *d = opendir(MOUNT);
    struct dirent *e;
    int found = 0;
    *size = 0;
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        char p[PATH_MAX_LEN];
        struct stat st;
        if (n < 5 || strcasecmp(e->d_name + n - 4, ".bin"))
            continue;
        if (snprintf(p, sizeof p, "%s/%s", MOUNT, e->d_name) >= (int)sizeof p || stat(p, &st) || st.st_size <= 0)
            continue;
        if ((size_t)st.st_size > *size) {
            *size = (size_t)st.st_size;
            snprintf(path, cap, "%s", p);
            found = 1;
        }
    }
    closedir(d);
    return found;
}

static const char *copy_image(FILE *f, size_t size, esp_ota_handle_t h)
{
    for (size_t at = 0; at < size;) {
        size_t n = fread(s_block, 1, COPY_BLOCK, f);
        if (!n)
            return "file unreadable";
        if (!at && !jo_header_ok(s_block, n))
            return "not an ESP32-S3 app image";
        if (esp_ota_write(h, s_block, n) != ESP_OK)
            return "flash write failed";
        at += n;
    }
    return NULL;
}

/* the file -> the other slot, and the boot choice; NULL when done, else why not */
static const char *install(const char *path, size_t size)
{
    const esp_partition_t *slot = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t h;
    const char *why;
    FILE *f;
    if (!slot || size > slot->size)
        return "bigger than the app slot";
    if (ota_busy())
        return "an update over MIDI is running";
    f = fopen(path, "rb");
    if (!f)
        return "file unreadable";
    if (esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &h) != ESP_OK) {
        fclose(f);
        return "no slot to write";
    }
    why = copy_image(f, size, h);
    fclose(f);
    if (why) {
        esp_ota_abort(h);
        return why;
    }
    if (esp_ota_end(h) != ESP_OK)
        return "image check failed (size, segments or SHA-256)";
    return esp_ota_set_boot_partition(slot) == ESP_OK ? NULL : "boot choice failed";
}

static void install_task(void *arg)
{
    char path[PATH_MAX_LEN], text[NOTE_MAX];
    size_t size;
    (void)arg;
    for (;;) {
        const char *why;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!find_image(path, sizeof path, &size))
            continue;                     /* an eject or an unplug with no update on the drive */
        ESP_LOGI(TAG, "installing %s, %u bytes", path, (unsigned)size);
        why = install(path, size);
        snprintf(text, sizeof text, "%s", why ? why : "installed, restarted");
        note(why ? "jank.bin not installed: %s" : "%s", text);
        ESP_LOGW(TAG, "%s", s_note);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();                    /* the start wipes the drive and writes the note */
    }
}

/* TinyUSB's task: only wakes the install task */
static void on_event(tinyusb_msc_storage_handle_t h, tinyusb_msc_event_t *ev, void *arg)
{
    (void)h;
    (void)arg;
    if (s_ready && s_task && ev->id == TINYUSB_MSC_EVENT_MOUNT_COMPLETE && ev->mount_point == TINYUSB_MSC_STORAGE_MOUNT_APP)
        xTaskNotifyGive(s_task);
}

/* a volume that will not mount or wipe: erase it once and restart, the next start formats it afresh */
static esp_err_t rescue(const esp_partition_t *p, esp_err_t why)
{
    if (s_erased == ERASED_MAGIC)
        return why;
    s_erased = ERASED_MAGIC;
    ESP_LOGE(TAG, "drive unusable (%s): erased, restarting", esp_err_to_name(why));
    if (esp_partition_erase_range(p, 0, p->size) == ESP_OK)
        esp_restart();
    return why;
}

static esp_err_t open_volume(const esp_partition_t *p)
{
    wl_handle_t wl;
    tinyusb_msc_driver_config_t drv = {.callback = on_event};
    tinyusb_msc_storage_config_t c = {.fat_fs = {.config = {.max_files = 4}}, .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP};
    ESP_RETURN_ON_ERROR(wl_mount(p, &wl), TAG, "wear levelling");
    c.medium.wl_handle = wl;
    ESP_RETURN_ON_ERROR(tinyusb_msc_install_driver(&drv), TAG, "msc driver");
    return tinyusb_msc_new_storage_spiflash(&c, &s_storage);     /* mounted for the app, formatted if it holds no file system */
}

esp_err_t drive_start(void)
{
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "storage");
    esp_err_t e;
    ESP_RETURN_ON_FALSE(p, ESP_ERR_NOT_FOUND, TAG, "no storage partition");
    e = open_volume(p);
    if (e == ESP_OK)
        e = wipe(MOUNT, 0);
    if (e == ESP_OK)
        e = restore();
    if (e != ESP_OK)
        return rescue(p, e);
    s_erased = 0;
    ESP_RETURN_ON_FALSE(xTaskCreate(install_task, "drive", 6144, NULL, 3, &s_task) == pdPASS, ESP_ERR_NO_MEM, TAG, "task");
    s_ready = 1;
    return tinyusb_msc_set_storage_mount_point(s_storage, TINYUSB_MSC_STORAGE_MOUNT_USB);
}
