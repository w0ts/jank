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

static const char *TAG = "jank";
static SemaphoreHandle_t s_cfg_lock;
static struct jk_cfg s_cfg;

void app_cfg_get(struct jk_cfg *c)
{
    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    *c = s_cfg;
    xSemaphoreGive(s_cfg_lock);
}

void app_cfg_set(const struct jk_cfg *c)
{
    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    s_cfg = *c;
    xSemaphoreGive(s_cfg_lock);
    bridge_apply(c);
    esp_err_t e = store_save(c);
    if (e != ESP_OK)
        ESP_LOGE(TAG, "settings not saved: %s", esp_err_to_name(e));
}

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
    snprintf(out, cap, "boots=%lu\nreset=%s\n", (unsigned long)s_boots_now, r < sizeof why / sizeof why[0] ? why[r] : "?");
}

void app_main(void)
{
    uint8_t efuse[6], mac[6], peer_mac[6];
    count_boot();
    s_cfg_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(s_cfg_lock ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(nvs_up());
    store_load(&s_cfg);
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(efuse));
    jk_mac_from_efuse(efuse, mac);
    memcpy(peer_mac, mac, 6);
    peer_mac[0] ^= 0x04u;                /* (stays unicast, locally administered) */
    ESP_ERROR_CHECK(ctl_start());
    ESP_ERROR_CHECK(bridge_start(&s_cfg, mac, peer_mac));
    ESP_ERROR_CHECK(ui_start());
    ota_validate_start();
}
