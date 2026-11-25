/* SPDX-License-Identifier: GPL-3.0-only */
/* jank: start-up, and the settings' one owner. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "board.h"
#include "jank_panic.h"

static const char *TAG = "jank";

/* app_main's checks: a failure aborts with "main:<line> <error>", which the crash record keeps (panic_info.c, `diag`): the
 * chip has no console, so this is how a start that fails names its step */
static char s_why[JK_PANIC_MSG];
#define MUST(x) must((x), __LINE__)
static void must(esp_err_t e, int line)
{
    if (e == ESP_OK)
        return;
    snprintf(s_why, sizeof s_why, "main:%d %s", line, esp_err_to_name(e));
    esp_system_abort(s_why);
}
static SemaphoreHandle_t s_cfg_lock;
static SemaphoreHandle_t s_set_lock;   /* app_cfg_set and app_cfg_flush: one save at a time, in order */
static struct jk_cfg s_cfg;
static struct jk_nets s_nets;          /* the saved Wi-Fi networks (s_cfg_lock) */
static struct jk_nets s_nets_work;     /* a copy to change or save (s_set_lock) */
static uint8_t s_save_pending;         /* a change made while playing, not saved yet (s_set_lock) */

void app_cfg_get(struct jk_cfg *c)
{
    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    *c = s_cfg;
    xSemaphoreGive(s_cfg_lock);
}

void app_nets_get(struct jk_nets *s)
{
    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    *s = s_nets;
    xSemaphoreGive(s_cfg_lock);
}

static void nets_publish(const struct jk_nets *s)
{
    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    s_nets = *s;
    xSemaphoreGive(s_cfg_lock);
}

static void save_nets(void)            /* (s_set_lock) */
{
    esp_err_t e;
    app_nets_get(&s_nets_work);
    e = store_save_nets(&s_nets_work);
    if (e != ESP_OK)
        ESP_LOGE(TAG, "saved networks not saved: %s", esp_err_to_name(e));
}

/* the settings, and the saved networks with them (the network of ssid / pass is in the list by then: app_cfg_set) */
static void save(const struct jk_cfg *c)
{
    esp_err_t e = store_save(c);
    if (e != ESP_OK)
        ESP_LOGE(TAG, "settings not saved: %s", esp_err_to_name(e));
    save_nets();
}

/* Saved FIRST, then published: a change of USB DEVICE / HOST makes usbrole.c restart as soon as it sees it, and the
 * setting has to be in flash by then. While the session or the MIDI clock plays, the save waits until it stops
 * (app_cfg_flush), unless a restart follows (jk_save_on_change). s_set_lock keeps two setters' saves in order; s_cfg_lock
 * stays short, so app_cfg_get never waits for flash. */
static void cfg_set_locked(const struct jk_cfg *c)   /* (s_set_lock) */
{
    struct jk_cfg old;
    enum jk_save d;
    app_cfg_get(&old);
    app_nets_get(&s_nets_work);
    if (jk_nets_take_cfg(&s_nets_work, c))       /* `set ssid` / `set pass`: that network is saved (or its new password) */
        nets_publish(&s_nets_work);
    d = jk_save_on_change(&s_save_pending, jk_cfg_stored_differs(&old, c), bridge_is_playing(), c->usb != old.usb);
    if (d == JK_SAVE_NOW)
        save(c);
    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    s_cfg = *c;
    xSemaphoreGive(s_cfg_lock);
}

void app_cfg_set(const struct jk_cfg *c)
{
    xSemaphoreTake(s_set_lock, portMAX_DELAY);
    cfg_set_locked(c);
    xSemaphoreGive(s_set_lock);
    bridge_apply(c);
    usb_midi_clock(c->usbmidi);
}

int app_nets_forget(const char *ssid)
{
    struct jk_cfg c;
    int changed;
    xSemaphoreTake(s_set_lock, portMAX_DELAY);
    app_nets_get(&s_nets_work);
    if (jk_nets_forget(&s_nets_work, ssid)) {
        xSemaphoreGive(s_set_lock);
        return -1;
    }
    nets_publish(&s_nets_work);
    app_cfg_get(&c);
    changed = jk_cfg_after_forget(&c, &s_nets_work);
    if (changed)
        cfg_set_locked(&c);            /* ssid / pass were the forgotten one's: the first saved one now, saved with the list */
    else if (bridge_is_playing())
        s_save_pending = 1;            /* no flash write while playing: app_cfg_flush saves the settings and the list once stopped */
    else
        save_nets();
    xSemaphoreGive(s_set_lock);
    if (changed) {
        bridge_apply(&c);
        usb_midi_clock(c.usbmidi);
    }
    return 0;
}

/* the deferred save, once stopped (the control task, every pass); force: a restart follows, save whatever plays */
void app_cfg_flush(int force)
{
    struct jk_cfg c;
    if (!s_save_pending)               /* (a byte: read without the lock, taken below before it counts) */
        return;
    xSemaphoreTake(s_set_lock, portMAX_DELAY);
    if (jk_save_due(&s_save_pending, force ? 0 : bridge_is_playing())) {
        app_cfg_get(&c);
        save(&c);
    }
    xSemaphoreGive(s_set_lock);
}

/* every esp_restart (the reboot command, MODE switch, USB role, updates, rollback): a deferred save first */
static void flush_at_restart(void) { app_cfg_flush(1); }

void app_reboot(void) { esp_restart(); }

void app_bootloader(void)
{
    usb_to_rom_download();
}

static esp_err_t nvs_up(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erased: %s", esp_err_to_name(e));
        e = nvs_flash_erase();
        if (e == ESP_OK)
            e = nvs_flash_init();
    }
    return e;
}

/* restarts since power-on and why the last one happened, for `diag` (RTC memory: survives a restart, not power-off) */
#define BOOTS_MAGIC 0x4A424F54u
static RTC_NOINIT_ATTR uint32_t s_boots_magic, s_boots;
static uint32_t s_boots_now;
static esp_reset_reason_t s_reason;

static void count_boot(void)
{
    s_reason = esp_reset_reason();
    if (s_boots_magic != BOOTS_MAGIC || s_reason == ESP_RST_POWERON) {
        s_boots_magic = BOOTS_MAGIC;
        s_boots = 0;
    }
    s_boots_now = ++s_boots;
}

void app_diag(char *out, size_t cap)
{
    static const char *const why[] = {"unknown", "poweron", "ext", "sw", "panic", "int_wdt", "task_wdt", "wdt", "deepsleep",
                                      "brownout", "sdio", "usb", "jtag", "efuse", "pwr_glitch", "cpu_lockup"};
    unsigned r = (unsigned)s_reason;
    snprintf(out, cap, "boots=%lu\nreset=%s\nsave_pending=%u\n", (unsigned long)s_boots_now, r < sizeof why / sizeof why[0] ? why[r] : "?",
             (unsigned)s_save_pending);
}

void app_main(void)
{
    uint8_t efuse[6], mac[6], peer_mac[6];
    board_init();                        /* which board, before any pin is driven (board.c) */
    power_early_init();
    count_boot();
    panic_info_take();
    s_cfg_lock = xSemaphoreCreateMutex();
    s_set_lock = xSemaphoreCreateMutex();
    MUST(s_cfg_lock && s_set_lock ? ESP_OK : ESP_ERR_NO_MEM);
    MUST(esp_register_shutdown_handler(flush_at_restart));
    MUST(nvs_up());
    store_load(&s_cfg);
    store_load_nets(&s_nets);
    if (jk_nets_take_cfg(&s_nets, &s_cfg))       /* a firmware without the list ran before: its network is the first saved one */
        if (store_save_nets(&s_nets) != ESP_OK)
            ESP_LOGE(TAG, "saved networks not saved");   /* (kept in memory; saved with the next settings change) */
    usb_midi_clock(s_cfg.usbmidi);
    MUST(esp_efuse_mac_get_default(efuse));
    jk_mac_from_efuse(efuse, mac);
    memcpy(peer_mac, mac, 6);
    peer_mac[0] ^= 0x04u;                /* (stays unicast, locally administered) */
    MUST(ctl_start());
    MUST(ui_screen_early());  /* the screen's panel on while the radio is still off (the 2.42"'s rule) */
    MUST(bridge_start(&s_cfg, mac, peer_mac));
    MUST(ui_start());
    ota_validate_start();
}
