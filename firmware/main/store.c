/* SPDX-License-Identifier: GPL-3.0-only */
/* Settings in NVS: one blob with a version. Anything unreadable falls back to the defaults. */
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "nvs.h"

#define NS "jank"
#define KEY "cfg1"
static const char *TAG = "store";

static int valid(const struct jk_cfg *c)
{
    return jk_channel(c->group) && c->filter <= JK_FILTER_ALL && c->transport <= JK_TRANSPORT_AUTO && c->quantum >= 1u && c->quantum <= JK_QUANTUM_MAX
           && c->offset_10us >= -JK_OFFSET_MAX_10US && c->offset_10us <= JK_OFFSET_MAX_10US && memchr(c->name, 0, sizeof c->name);
}

void store_load(struct jk_cfg *c)
{
    struct jk_cfg r;
    size_t n = sizeof r;
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
    if (e == ESP_OK && n == sizeof r && valid(&r))
        *c = r;
    else if (e != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "stored settings ignored: %s, %u bytes", esp_err_to_name(e), (unsigned)n);
}

esp_err_t store_save(const struct jk_cfg *c)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READWRITE, &h);
    if (e != ESP_OK)
        return e;
    e = nvs_set_blob(h, KEY, c, sizeof *c);
    if (e == ESP_OK)
        e = nvs_commit(h);
    nvs_close(h);
    return e;
}
