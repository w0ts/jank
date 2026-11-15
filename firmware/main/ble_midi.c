/* SPDX-License-Identifier: GPL-3.0-only */
/* A BLE MIDI peripheral (CONFIG_JANK_BLE_MIDI): the standard BLE-MIDI service and its one characteristic, carrying the
 * control line's SysEx (core/jank_sysex.h) both ways, in BLE-MIDI packets (core/jank_blemidi.h). Lines from the phone go
 * to the same handler as USB MIDI's (ctl.c, CTL_SRC_BLE) and their answers come back here; the beat reports go to both.
 * No MIDI clock over BLE: connection events every 30..60 ms, some given up to Wi-Fi, cannot carry a 24 PPQN clock.
 * One connection. NimBLE's host task and the controller run on core 0 (core 1 is the clock's alone, midi.c). */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_coexist.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "jank_blemidi.h"
#include "jank_sysex.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define PKT_MAX 256u                   /* CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU at most, less 3 */
#define RX_TEXT_MAX 64u
#define NOTIFY_TRIES 40                /* a line's answer waits up to 40 x 5 ms for NimBLE's buffers */
#define NOTIFY_WAIT_MS 5
/* Wi-Fi first (Link, the jank radio): BLE gets what is left. BLE-MIDI asks for 15 ms or less, for playing notes; the
 * control line is a few lines a second, so the peripheral asks for 30..60 ms (x 1.25 ms), which leaves Wi-Fi more air.
 * The phone decides: it may keep its own interval. Apple's rule for a request: min >= 15 ms, max >= min + 15 ms. */
#define CONN_ITVL_MIN 24
#define CONN_ITVL_MAX 48
#define CONN_TIMEOUT 400               /* x 10 ms: rides out a flash erase or a long Wi-Fi slice */
/* advertising every 152.5 ms (x 0.625 ms), one of Apple's recommended intervals: found within a second or two */
#define ADV_ITVL 244

void ble_store_config_init(void);

static const char *TAG = "ble_midi";

/* 03B80E5A-EDE8-4B33-A751-6CE34EC4C700 and 7772E5DB-3868-4112-A1A9-F2669D106BF3, least significant byte first */
static const ble_uuid128_t SVC_UUID = BLE_UUID128_INIT(0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7, 0x33, 0x4B, 0xE8, 0xED,
                                                       0x5A, 0x0E, 0xB8, 0x03);
static const ble_uuid128_t CHR_UUID = BLE_UUID128_INIT(0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1, 0x12, 0x41, 0x68, 0x38,
                                                       0xDB, 0xE5, 0x72, 0x77);

static uint16_t s_val_handle;
static volatile uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static volatile uint16_t s_mtu = BLE_ATT_MTU_DFLT;
static volatile uint8_t s_notify;      /* the phone subscribed */
static uint8_t s_own_addr;
static char s_name[12];
static SemaphoreHandle_t s_tx_lock;    /* one SysEx's packets at a time: a split message must not interleave */
static struct jbm_in s_rx_ble;         /* the host task's alone */
static struct jx_in s_rx_jx;
static uint8_t s_rx_text[RX_TEXT_MAX];
static size_t s_rx_n;
static size_t s_heap_used;             /* internal heap taken by the start, for `diag` */

static void advertise(void);

static void rx_flush(void)
{
    if (s_rx_n)
        ctl_line_rx(CTL_SRC_BLE, s_rx_text, s_rx_n);
    s_rx_n = 0;
}

static void rx_byte(void *ctx, uint8_t b)
{
    int c = jx_feed(&s_rx_jx, b);
    (void)ctx;
    if (c < 0)
        return;
    s_rx_text[s_rx_n++] = (uint8_t)c;
    if (s_rx_n == sizeof s_rx_text)
        rx_flush();
}

static int on_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t pkt[PKT_MAX];
    uint16_t len = 0;
    (void)conn;
    (void)attr;
    (void)arg;
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:  /* BLE-MIDI: a read answers with no payload */
        return 0;
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (OS_MBUF_PKTLEN(ctxt->om) > sizeof pkt)
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if (ble_hs_mbuf_to_flat(ctxt->om, pkt, sizeof pkt, &len) != 0)
            return BLE_ATT_ERR_UNLIKELY;
        if (jbm_decode(&s_rx_ble, pkt, len, rx_byte, NULL) != 0)
            ESP_LOGW(TAG, "bad packet of %u bytes", (unsigned)len);
        rx_flush();
        return 0;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static const struct ble_gatt_svc_def SERVICES[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &CHR_UUID.u,
                .access_cb = on_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_val_handle,
            },
            {0},
        },
    },
    {0},
};

static void on_connect(uint16_t conn)
{
    const struct ble_gap_upd_params p = {.itvl_min = CONN_ITVL_MIN, .itvl_max = CONN_ITVL_MAX, .latency = 0,
                                         .supervision_timeout = CONN_TIMEOUT};
    int rc;
    jbm_init(&s_rx_ble);
    jx_init(&s_rx_jx);
    s_rx_n = 0;
    s_mtu = BLE_ATT_MTU_DFLT;
    s_notify = 0;
    s_conn = conn;
    rc = ble_gap_update_params(conn, &p);
    if (rc != 0)
        ESP_LOGW(TAG, "connection parameters: %d", rc);
}

static int on_gap(struct ble_gap_event *ev, void *arg)
{
    (void)arg;
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status == 0)
            on_connect(ev->connect.conn_handle);
        else
            advertise();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        s_notify = 0;
        advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (ev->subscribe.attr_handle == s_val_handle)
            s_notify = ev->subscribe.cur_notify;
        break;
    case BLE_GAP_EVENT_MTU:
        s_mtu = ev->mtu.value;
        break;
    default:
        break;
    }
    return 0;
}

static void advertise(void)
{
    struct ble_hs_adv_fields adv = {0}, rsp = {0};
    struct ble_gap_adv_params p = {.conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = BLE_GAP_DISC_MODE_GEN, .itvl_min = ADV_ITVL,
                                   .itvl_max = ADV_ITVL};
    int rc;
    /* the service's UUID in the advertisement (what BLE-MIDI centrals filter on), the name in the scan response */
    adv.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv.uuids128 = &SVC_UUID;
    adv.num_uuids128 = 1;
    adv.uuids128_is_complete = 1;
    rsp.name = (const uint8_t *)s_name;
    rsp.name_len = (uint8_t)strlen(s_name);
    rsp.name_is_complete = 1;
    rc = ble_gap_adv_set_fields(&adv);
    if (rc == 0)
        rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc == 0)
        rc = ble_gap_adv_start(s_own_addr, NULL, BLE_HS_FOREVER, &p, on_gap, NULL);
    if (rc != 0)
        ESP_LOGE(TAG, "advertising: %d", rc);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0)
        rc = ble_hs_id_infer_auto(0, &s_own_addr);
    if (rc != 0) {
        ESP_LOGE(TAG, "address: %d", rc);
        return;
    }
    advertise();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "host reset: %d", reason);
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();                 /* returns on nimble_port_stop() */
    nimble_port_freertos_deinit();
}

/* one whole SysEx in as many notifications as it takes; tries 0: drop it when NimBLE has no buffer free */
static int send_sysex(const uint8_t *msg, size_t n, int tries)
{
    uint8_t pkt[PKT_MAX];
    const uint16_t conn = s_conn;
    const size_t mtu = s_mtu;
    const size_t cap = mtu - 3u < sizeof pkt ? mtu - 3u : sizeof pkt;
    const uint16_t ms = (uint16_t)((esp_timer_get_time() / 1000) & 0x1FFF);
    size_t at = 0, len;
    while ((len = jbm_sysex_packet(msg, n, &at, ms, pkt, cap)) != 0) {
        int rc;
        int left = tries;
        for (;;) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(pkt, (uint16_t)len);
            rc = om ? ble_gatts_notify_custom(conn, s_val_handle, om) : BLE_HS_ENOMEM;   /* om is consumed either way */
            if (rc != BLE_HS_ENOMEM || left-- <= 0)
                break;
            vTaskDelay(pdMS_TO_TICKS(NOTIFY_WAIT_MS));
        }
        if (rc != 0)
            return rc;             /* the rest is dropped: the reader ends the cut SysEx at the next F0 */
    }
    return 0;
}

static int connected(void) { return s_conn != BLE_HS_CONN_HANDLE_NONE && s_notify; }

void ble_midi_write(const char *text, size_t n)
{
    uint8_t frame[JX_FRAME_MAX];
    if (!connected() || !s_tx_lock)
        return;
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    while (n) {
        size_t k = n < JX_TEXT_MAX ? n : JX_TEXT_MAX;
        if (send_sysex(frame, jx_frame(text, k, frame), NOTIFY_TRIES) != 0)
            break;
        text += k;
        n -= k;
    }
    xSemaphoreGive(s_tx_lock);
}

void ble_midi_push(const char *text, size_t n)
{
    uint8_t frame[JX_PUSH_FRAME_MAX];
    size_t w;
    if (!connected() || !s_tx_lock)
        return;
    w = jx_push_frame(JX_PUSH_BEAT, text, n, frame);
    if (!w || xSemaphoreTake(s_tx_lock, 0) != pdTRUE)    /* never waits: the beat task is the tracker's */
        return;
    send_sysex(frame, w, 0);
    xSemaphoreGive(s_tx_lock);
}

void ble_midi_diag(char *out, size_t cap)
{
    snprintf(out, cap, "ble=%s mtu=%u heap_taken=%u\n", s_conn == BLE_HS_CONN_HANDLE_NONE ? "advertising" : s_notify ? "subscribed" : "connected",
             (unsigned)s_mtu, (unsigned)s_heap_used);
}

esp_err_t ble_midi_start(const uint8_t mac[6])
{
    const size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    esp_err_t e;
    int rc;
    s_tx_lock = xSemaphoreCreateMutex();
    if (!s_tx_lock)
        return ESP_ERR_NO_MEM;
    snprintf(s_name, sizeof s_name, "jank %02X%02X", mac[4], mac[5]);
    e = nimble_port_init();            /* the controller too; its interrupt goes to this core (app_main: core 0) */
    if (e != ESP_OK)
        return e;
    /* Wi-Fi first. esp_coexist.h marks this deprecated in favour of status bits, which exist only for BLE Mesh and A2DP;
     * what it changes in the S3's closed coexistence library is not documented: only a hardware test shows it */
    e = esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
    if (e != ESP_OK)
        ESP_LOGW(TAG, "coexistence preference: %s", esp_err_to_name(e));
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    /* "just works" if the phone asks to pair (iOS may); no bonds kept across restarts unless BT_NIMBLE_NVS_PERSIST */
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    rc = ble_gatts_count_cfg(SERVICES);
    if (rc == 0)
        rc = ble_gatts_add_svcs(SERVICES);
    if (rc == 0)
        rc = ble_svc_gap_device_name_set(s_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT: %d", rc);
        return ESP_FAIL;
    }
    ble_store_config_init();
    nimble_port_freertos_init(host_task);
    s_heap_used = before - heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    return ESP_OK;
}
