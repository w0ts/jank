/* SPDX-License-Identifier: GPL-3.0-only */
/* The signed Wi-Fi update (core/jank_wota.h) in the firmware: the key in flash, the USB control line that sets it, and the control task's handling
 * of the frames the bridge task queued (wsvc.c). Everything that touches flash runs in the control task, as the USB update does (ota.c). The key
 * is 32 bytes in NVS ("wota"/"key"): it is written only by the `otakey` control line, never read out, never logged. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_ota.h"
#include "jank_wota.h"
#include "nvs.h"

#define NS "wota"
#define KEY "key"

static const char *TAG = "wota";
static struct jwo s_s;                 /* the control task's alone */
static uint8_t s_loaded;

static uint32_t now_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

static void load_key(void)
{
    uint8_t k[JWO_KEY];
    size_t n = sizeof k;
    nvs_handle_t h;
    s_loaded = 1;
    jwo_init(&s_s);
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK)
        return;
    if (nvs_get_blob(h, KEY, k, &n) == ESP_OK && n == sizeof k)
        jwo_set_key(&s_s, k);
    nvs_close(h);
    memset(k, 0, sizeof k);
}

static esp_err_t save_key(const uint8_t *k)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READWRITE, &h);
    if (e != ESP_OK)
        return e;
    e = k ? nvs_set_blob(h, KEY, k, JWO_KEY) : nvs_erase_key(h, KEY);
    if (e == ESP_ERR_NVS_NOT_FOUND && !k)
        e = ESP_OK;                    /* nothing to erase */
    if (e == ESP_OK)
        e = nvs_commit(h);
    nvs_close(h);
    return e;
}

void wota_start(void)
{
    if (!s_loaded)
        load_key();
    wsvc_binary_enable(s_s.key_set);
}

static int hex_key(const char *hex, uint8_t *out)
{
    for (unsigned i = 0; i < JWO_KEY; i++) {
        unsigned v = 0;
        for (unsigned k = 0; k < 2; k++) {
            char c = hex[2 * i + k];
            unsigned d;
            if (c >= '0' && c <= '9')
                d = (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f')
                d = (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                d = (unsigned)(c - 'A' + 10);
            else
                return 0;
            v = v << 4 | d;
        }
        out[i] = (uint8_t)v;
    }
    return hex[2 * JWO_KEY] == 0;      /* exactly 64 characters */
}

/* the control line: `otakey <64 hex>` sets the key, `otakey clear` removes it, `otakey` / `otakey?` says whether there is one (never the key) */
bool wota_key_line(const char *line)
{
    const char *arg = line + 6;        /* after "otakey" */
    uint8_t k[JWO_KEY];
    static const char ok[] = "ok\n", bad[] = "err arg\n", store[] = "err store\n";
    char msg[24];
    int n;
    if (!s_loaded)
        load_key();
    while (*arg == ' ')
        arg++;
    if (*arg == 0 || *arg == '?') {
        n = snprintf(msg, sizeof msg, "wota_key=%s\n", s_s.key_set ? "set" : "none");
        usb_serial_write(msg, (size_t)n);
        return true;
    }
    if (!strcmp(arg, "clear")) {
        if (save_key(NULL) != ESP_OK) {
            usb_serial_write(store, sizeof store - 1u);
            return true;
        }
        jwo_clear_key(&s_s);
        wsvc_binary_enable(0);
        ESP_LOGI(TAG, "update key removed");
        usb_serial_write(ok, sizeof ok - 1u);
        return true;
    }
    if (!hex_key(arg, k)) {
        usb_serial_write(bad, sizeof bad - 1u);
        return true;
    }
    if (save_key(k) != ESP_OK) {
        memset(k, 0, sizeof k);
        usb_serial_write(store, sizeof store - 1u);
        return true;
    }
    jwo_set_key(&s_s, k);
    memset(k, 0, sizeof k);
    wsvc_binary_enable(1);
    ESP_LOGI(TAG, "update key set");
    usb_serial_write(ok, sizeof ok - 1u);
    return true;
}

/* a signed request's work: the engine's answer and the offset it carries (ota.c) */
static void run(const struct jwo_req *q, uint8_t *status, uint32_t *offset, uint16_t *chunk, int *restart)
{
    enum jo_err e = JO_OK;
    *chunk = 0;
    *restart = 0;
    switch (q->op) {
    case JWO_OP_BEGIN:
        e = ota_net_begin(q->size, q->crc, q->target);
        *chunk = e == JO_OK ? (uint16_t)JWO_DATA_MAX : 0;
        break;
    case JWO_OP_DATA:
        e = ota_net_data(q->offset, q->crc, q->data, q->n);
        break;
    case JWO_OP_END:
        e = ota_net_end();
        *restart = e == JO_OK;
        break;
    case JWO_OP_ABORT:
        ota_net_abort();
        break;
    case JWO_OP_CONFIRM:
        *status = (uint8_t)ota_net_confirm();
        *offset = 0;
        return;
    default:
        *status = JWO_S_BAD_OP;
        *offset = 0;
        return;
    }
    *status = (uint8_t)e;
    *offset = ota_net_offset();
}

/* the frame the bridge task queued, if any (the control task, once a pass) */
void wota_serve(void)
{
    struct jw_req rq;
    const uint8_t *f;
    size_t n, olen = 0;
    uint8_t rnd[JWO_NONCE], out[JWO_REPLY_LEN > JWO_HELLO_R_LEN ? JWO_REPLY_LEN : JWO_HELLO_R_LEN];
    struct jwo_req q;
    if (!wsvc_take_binary(&rq, &f, &n))
        return;
    if (!s_loaded)
        load_key();
    esp_fill_random(rnd, sizeof rnd);
    switch (jwo_input(&s_s, f, n, now_ms(), rnd, &q, out, sizeof out, &olen)) {
    case JWO_HELLO:
        wsvc_put_raw(&rq, out, olen);
        break;
    case JWO_CMD: {
        uint8_t status = 0;
        uint32_t offset = 0;
        uint16_t chunk = 0;
        int restart = 0;
        run(&q, &status, &offset, &chunk, &restart);
        olen = jwo_reply(&s_s, q.counter, status, offset, chunk, out, sizeof out);
        if (olen)
            wsvc_put_raw(&rq, out, olen);
        wsvc_release_binary();             /* (the frame, which q.data pointed into, is done with) */
        if (restart)
            ota_net_restart();             /* the answer is queued; the restart leaves it a moment to go out */
        return;
    }
    default:
        break;
    }
    wsvc_release_binary();
}
