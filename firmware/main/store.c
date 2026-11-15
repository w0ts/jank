/* SPDX-License-Identifier: GPL-3.0-only */
/* Settings in NVS: one blob with a version. Anything unreadable falls back to the defaults. */
#include <stddef.h>
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "nvs.h"

#define NS "jank"
#define KEY "cfg1"
/* what is stored is the struct up to the beat-follow bench settings: a stick flashed with this build keeps its group, name
 * and Wi-Fi, and one flashed back to the release reads the same blob (no setting of the bench build is saved) */
#define STORED_SIZE offsetof(struct jk_cfg, beat)
_Static_assert(STORED_SIZE == 130, "the release's blob was 130 bytes");
static const char *TAG = "store";

static int valid(const struct jk_cfg *c)
{
    return jk_channel(c->group) && c->filter <= JK_FILTER_ALL && c->transport <= JK_TRANSPORT_AUTO && c->quantum >= 1u && c->quantum <= JK_QUANTUM_MAX
           && c->offset_10us >= -JK_OFFSET_MAX_10US && c->offset_10us <= JK_OFFSET_MAX_10US && memchr(c->name, 0, sizeof c->name);
}

void store_load(struct jk_cfg *c)
{
    struct jk_cfg r;
    size_t n = STORED_SIZE;
    nvs_handle_t h;
    jk_cfg_defaults(c);
    esp_err_t e = nvs_open(NS, NVS_READONLY, &h);
    if (e != ESP_OK) {
        if (e != ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGW(TAG, "open: %s", esp_err_to_name(e));
        return;
    }
    e = nvs_get_blob(h, KEY, &r, &n);
    nvs_close(h);
    if (e == ESP_OK && n == STORED_SIZE && valid(&r)) {
        memcpy(c, &r, STORED_SIZE);           /* the bench settings keep their defaults */
        if (c->ble > JK_BLE_AUTO)             /* the pad byte of an older build: anything but 0 there is not a setting */
            c->ble = JK_BLE_PAIR;
    } else if (e != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "stored settings ignored: %s, %u bytes", esp_err_to_name(e), (unsigned)n);
}

esp_err_t store_save(const struct jk_cfg *c)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READWRITE, &h);
    if (e != ESP_OK)
        return e;
    e = nvs_set_blob(h, KEY, c, STORED_SIZE);
    if (e == ESP_OK)
        e = nvs_commit(h);
    nvs_close(h);
    return e;
}
