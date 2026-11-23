/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host mode's probe (core/jank_hostdiag.h): the host clients fill g_hostdiag as things happen; this task saves it to flash
 * when it has changed, and `diag` prints what was saved, so what a hosting unit saw can be read once the computer is plugged in
 * again (or, while hosting, live over Wi-Fi). Only a host start saves (hostdiag_start is called by usb_midi_host_start): a boot as
 * a device keeps the last record for `diag`. A flash write stalls the CPU for a moment, so none is made while the session or the
 * clock plays, and no two closer than MIN_GAP_MS. The record is read while it is written to (no lock: a torn read is a
 * diagnostic's worst case).
 *
 * A failed host start is kept apart (key "hostfail"): the unit restarts as a device after it (usbrole.c), and the reason has to
 * outlive that restart and the power cycle after it. A later successful host start clears it. */
#include <string.h>
#include <stdio.h>
#include "app.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "jank_hostdiag.h"
#include "usb_host_mode.h"

#define NS "hostdiag"
#define KEY "d"
#define KEY_FAIL "hostfail"
#define FAIL_BYTES 5u                  /* the step, then the esp_err_t, 4 bytes */
#define TASK_STACK 3072
#define TASK_PRIO 1
#define EVERY_MS 5000
#define MIN_GAP_MS 10000

static const char *TAG = "hostdiag";

struct jhd g_hostdiag;

/* the memory numbers: this is what tells whether the host start's allocations (task stacks, USB buffers) fit the internal heap */
static void sample_mem(struct jhd *d)
{
    d->heap_kb = (uint16_t)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024u);
    d->heap_min_kb = (uint16_t)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024u);
    d->client_stack_free = usb_midi_host_stack_free();
}

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
        uint16_t heap_now;
        vTaskDelay(pdMS_TO_TICKS(EVERY_MS));
        sample_mem(&g_hostdiag);
        memcpy(&snap, &g_hostdiag, sizeof snap);
        heap_now = snap.heap_kb;
        snap.heap_kb = saved.heap_kb;  /* the free heap moves all the time: it alone is not a change worth a flash write */
        if (have_saved && !memcmp(&snap, &saved, sizeof snap))
            continue;
        snap.heap_kb = heap_now;
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
    sample_mem(&g_hostdiag);
}

/* The saver task is made once the start-up is through (usb.c usb_host_boot_done): its 3 KB stack is not worth a failed start in a heap this
 * small, and without it the live record still reads over Wi-Fi. A failure to make it is only logged. */
void hostdiag_start_saver(void)
{
    if (xTaskCreate(save_task, "hostdiag", TASK_STACK, NULL, TASK_PRIO, NULL) != pdPASS)
        ESP_LOGW(TAG, "no task: the record is not saved");
}

/* The internal heap at each stage of a host start, in RTC memory: it survives the panic or restart a failed start ends in, so `diag` can
 * say, after the restart as a device, how much was left when it failed. Written in host mode only; a power cycle clears it. */
#define TRACE_MAGIC 0x4A485443u
static RTC_NOINIT_ATTR uint32_t s_trace_magic, s_trace_n;
static RTC_NOINIT_ATTR struct jhd_mark s_trace[JHD_TRACE_MAX];

void hostdiag_trace_reset(void)
{
    s_trace_magic = TRACE_MAGIC;
    s_trace_n = 0;
}

void hostdiag_mark(uint8_t step)
{
    if (!usb_is_host() || s_trace_magic != TRACE_MAGIC || s_trace_n >= JHD_TRACE_MAX)
        return;
    s_trace[s_trace_n].step = step;
    s_trace[s_trace_n].free_b = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s_trace[s_trace_n].largest_b = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    s_trace_n++;
}

static size_t trace_text(char *out, size_t cap)
{
    if (s_trace_magic != TRACE_MAGIC)
        return 0;
    return jhd_format_trace(s_trace, s_trace_n < JHD_TRACE_MAX ? (unsigned)s_trace_n : JHD_TRACE_MAX, out, cap);
}

/* A host start failed at `step` with `e`: kept in flash at once (the unit restarts right after, and the reason must outlive it) */
void hostdiag_fail(uint8_t step, esp_err_t e)
{
    uint8_t b[FAIL_BYTES];
    int32_t err = (int32_t)e;
    nvs_handle_t h;
    b[0] = step;
    memcpy(b + 1, &err, sizeof err);
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK)
        return;
    if (nvs_set_blob(h, KEY_FAIL, b, sizeof b) == ESP_OK)
        (void)nvs_commit(h);
    nvs_close(h);
}

/* a host start worked: an older failure is no longer news (a flash write only if there is one to clear) */
void hostdiag_clear_failure(void)
{
    uint8_t b[FAIL_BYTES];
    size_t n = sizeof b;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK)
        return;
    if (nvs_get_blob(h, KEY_FAIL, b, &n) == ESP_OK && nvs_erase_key(h, KEY_FAIL) == ESP_OK)
        (void)nvs_commit(h);
    nvs_close(h);
}

/* the kept failure as a line, when there is one */
static size_t failure_text(char *out, size_t cap)
{
    uint8_t b[FAIL_BYTES];
    size_t n = sizeof b;
    int32_t err;
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READONLY, &h);
    if (e != ESP_OK)
        return 0;
    e = nvs_get_blob(h, KEY_FAIL, b, &n);
    nvs_close(h);
    if (e != ESP_OK || n != sizeof b)
        return 0;
    memcpy(&err, b + 1, sizeof err);
    return jhd_format_failure(b[0], err, out, cap);
}

/* The record as text, for `diag` (the control task's, one at a time): while the host stack runs, the live one (a Wi-Fi request
 * sees it as it is now); otherwise what the last host session saved. Then, if a host start ever failed, why. */
size_t hostdiag_text(char *out, size_t cap)
{
    static struct jhd d;
    size_t n = sizeof d, len;
    nvs_handle_t h;
    esp_err_t e;
    if (!cap)
        return 0;
    if (usb_is_host()) {
        sample_mem(&g_hostdiag);
        len = jhd_format(&g_hostdiag, out, cap);
    } else {
        e = nvs_open(NS, NVS_READONLY, &h);
        if (e == ESP_OK) {
            e = nvs_get_blob(h, KEY, &d, &n);
            nvs_close(h);
        }
        if (e != ESP_OK || !jhd_valid(&d, n)) {
            int w = snprintf(out, cap, "hostdiag=none\n");
            len = w > 0 && (size_t)w < cap ? (size_t)w : 0;
        } else {
            len = jhd_format(&d, out, cap);
        }
    }
    len += trace_text(out + len, cap - len);
    return len + failure_text(out + len, cap - len);
}
