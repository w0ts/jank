/* SPDX-License-Identifier: GPL-3.0-only */
/* Settings in NVS: one blob with a version. Anything unreadable falls back to the defaults. */
#include <stddef.h>
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "jank_conf.h"
#include "jank_usbrole.h"
#include "nvs.h"

#define NS "jank"
#define KEY "cfg1"
#define KEY_USB "usb1"                 /* USB DEVICE / HOST, AUDIO > SOURCE, AUTO BAR, USB MIDI CLOCK, LOW CONFIDENCE, THRESHOLD,
                                        * RATE, BEAT FLASH: eight bytes, apart from the release's blob (an older, shorter one keeps the rest's
                                        * defaults) */
#define KEY_LAT "lat1"                 /* beat_latency per audio source, the auto bases (core/jank_lat.h jla_pack) */
#define KEY_TRIM "latt"                /* the AUDIO SHIFTs on top (jla_pack_trim, M2h): none stored (an older firmware's): 0 */
/* what is stored is the struct up to the beat-follow bench settings: a stick flashed with this build keeps its group, name
 * and Wi-Fi, and one flashed back to the release reads the same blob (no setting of the bench build is saved) */
#define STORED_SIZE JK_CFG_STORED
_Static_assert(STORED_SIZE == 130, "the release's blob was 130 bytes");
static const char *TAG = "store";

static int valid(const struct jk_cfg *c)
{
    return jk_channel(c->group) && c->filter <= JK_FILTER_ALL && c->transport <= JK_TRANSPORT_AUTO && c->quantum >= 1u && c->quantum <= JK_QUANTUM_MAX
           && c->offset_10us >= -JK_OFFSET_MAX_10US && c->offset_10us <= JK_OFFSET_MAX_10US && memchr(c->name, 0, sizeof c->name);
}

static void load_usb(nvs_handle_t h, struct jk_cfg *c)
{
    uint8_t u[8];                      /* usb, source, autobar (since M2c), usbmidi (since M2d), lowconf, threshold (since M2e),
                                        * rate (since M2i), beat_flash (since the lcd2): a shorter blob keeps the defaults */
    size_t n = sizeof u;
    esp_err_t e = nvs_get_blob(h, KEY_USB, u, &n);
    if (e == ESP_OK && n >= 2u && u[0] <= JUR_USB_HOST && u[1] <= JUR_SRC_LINK && (n < 3u || u[2] <= JK_AUTOBAR_ALWAYS) && (n < 4u || u[3] <= 1u)
        && (n < 5u || u[4] <= JCF_STOP) && (n < 6u || u[5] <= 100u) && (n < 7u || u[6] <= 1u) && (n < 8u || u[7] <= 1u)) {
        c->usb = u[0];
        c->source = u[1];
        if (n >= 3u)
            c->autobar = u[2];
        if (n >= 4u)
            c->usbmidi = u[3];
        if (n >= 6u) {
            c->lowconf = u[4];
            c->threshold = u[5];
        }
        if (n >= 7u)
            c->rate = u[6];
        if (n >= 8u)
            c->beat_flash = u[7];
    } else if (e != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "stored USB settings ignored: %s, %u bytes", esp_err_to_name(e), (unsigned)n);
    }
}

/* beat_latency per audio source (core/jank_lat.h); the live value is the computer's until usbrole.c sees another source */
static void load_lat(nvs_handle_t h, struct jk_cfg *c)
{
    uint8_t b[JLA_PACKED];
    size_t n = sizeof b;
    esp_err_t e = nvs_get_blob(h, KEY_LAT, b, &n);
    if (e == ESP_OK && jla_unpack(&c->lat, b, n) != 0)
        ESP_LOGW(TAG, "stored latencies ignored: %u bytes", (unsigned)n);
    else if (e != ESP_OK && e != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "stored latencies ignored: %s", esp_err_to_name(e));
    {
        uint8_t t[JLA_TRIM_PACKED];
        n = sizeof t;
        e = nvs_get_blob(h, KEY_TRIM, t, &n);
        if (e == ESP_OK && jla_unpack_trim(&c->lat, t, n) != 0)
            ESP_LOGW(TAG, "stored audio shifts ignored: %u bytes", (unsigned)n);
        else if (e != ESP_OK && e != ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGW(TAG, "stored audio shifts ignored: %s", esp_err_to_name(e));
    }
    jk_lat_select(c, &c->lat_key);
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
    if (e == ESP_OK && n == STORED_SIZE && valid(&r)) {
        memcpy(c, &r, STORED_SIZE);           /* the bench settings keep their defaults */
        if (c->ble > JK_BLE_AUTO)             /* the pad byte of an older build: anything but 0 there is not a setting */
            c->ble = JK_BLE_PAIR;
    } else if (e != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "stored settings ignored: %s, %u bytes", esp_err_to_name(e), (unsigned)n);
    load_usb(h, c);
    load_lat(h, c);
    nvs_close(h);
}

esp_err_t store_save(const struct jk_cfg *c)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READWRITE, &h);
    if (e != ESP_OK)
        return e;
    e = nvs_set_blob(h, KEY, c, STORED_SIZE);
    if (e == ESP_OK) {
        const uint8_t u[8] = {c->usb, c->source, c->autobar, c->usbmidi, c->lowconf, c->threshold, c->rate, c->beat_flash};
        e = nvs_set_blob(h, KEY_USB, u, sizeof u);
    }
    if (e == ESP_OK) {
        uint8_t b[JLA_PACKED];
        e = nvs_set_blob(h, KEY_LAT, b, jla_pack(&c->lat, b, sizeof b));
    }
    if (e == ESP_OK) {
        uint8_t t[JLA_TRIM_PACKED];
        e = nvs_set_blob(h, KEY_TRIM, t, jla_pack_trim(&c->lat, t, sizeof t));
    }
    if (e == ESP_OK)
        e = nvs_commit(h);
    nvs_close(h);
    return e;
}
