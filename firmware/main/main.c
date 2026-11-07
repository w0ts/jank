/* SPDX-License-Identifier: GPL-3.0-only */
/* jank: start-up, and the settings' one owner. */
#include <string.h>
#include "app.h"
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
    BOARD_FORCE_DOWNLOAD();
    esp_restart();
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

void app_main(void)
{
    uint8_t efuse[6], mac[6], peer_mac[6];
    s_cfg_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(s_cfg_lock ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(nvs_up());
    store_load(&s_cfg);
#if BOARD_BUTTON_GPIO < 0
    if (reset_button_double()) {        /* RST twice: the board's only button, as the long press on the others */
        s_cfg.net = s_cfg.net == JK_NET_WIFI || !s_cfg.ssid[0] ? JK_NET_JAM : JK_NET_WIFI;
        ESP_ERROR_CHECK(store_save(&s_cfg));
    }
#endif
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(efuse));
    jk_mac_from_efuse(efuse, mac);
    memcpy(peer_mac, mac, 6);
    peer_mac[0] ^= 0x04u;                /* (stays unicast, locally administered) */
    ESP_ERROR_CHECK(ctl_start());
    ESP_ERROR_CHECK(bridge_start(&s_cfg, mac, peer_mac));
    ESP_ERROR_CHECK(ui_start());
#if BOARD_BUTTON_GPIO < 0
    ESP_ERROR_CHECK(reset_button_arm());
#endif
}
