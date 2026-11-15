/* SPDX-License-Identifier: GPL-3.0-only */
/* A BLE MIDI peripheral for a phone (CONFIG_JANK_BLE_MIDI, docs/ble.md): the standard BLE-MIDI service and its one
 * characteristic, carrying the control line's SysEx (core/jank_sysex.h) both ways in BLE-MIDI packets (core/jank_blemidi.h).
 * Lines from the phone go to the same handler as USB MIDI's (ctl.c, CTL_SRC_BLE) and their answers come back here. While a
 * phone listens, the HOME line (core/jank_phone.h) goes to it a few times a second. Nothing else: no beat reports, no MIDI
 * clock (connection events 30..60 ms apart, some given up to Wi-Fi, cannot carry a 24 PPQN clock).
 *
 * The controller is ON only while a pair window is open or a phone is connected (core/jank_phone.h, the user's ruling):
 * this file's task brings the whole stack up (nimble_port_init: controller and host) when the window opens and takes it
 * down again (nimble_port_stop, nimble_port_deinit: the controller disabled and its memory freed) when the window closes
 * with no phone, or the phone leaves and does not come back. With the controller off, MODE JANK has the radio alone.
 *
 * No bonding: a phone gets on only during a window opened on the device itself, and nothing is remembered; a remembered
 * phone would need the controller on (advertising or listening) all the time, which is what the ruling forbids. If a
 * phone asks to pair, it gets "just works" encryption for that connection, no keys kept. One connection.
 * NimBLE's host task and the controller run on core 0 (core 1 is the clock's alone, midi.c). */
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
#include "jank_phone.h"
#include "jank_sysex.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define PKT_MAX 256u                   /* CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU at most, less 3 */
#define RX_TEXT_MAX 64u
#define NOTIFY_TRIES 40                /* a line's answer waits up to 40 x 5 ms for NimBLE's buffers */
#define NOTIFY_WAIT_MS 5
#define WRITE_MAX_US 1000000           /* one answer waits for buffers 1 s in all at most: the control task is not held longer */
#define TASK_EVERY_MS 100u             /* the task looks at the window and the HOME line this often */
#define TASK_STACK 4096
#define TASK_PRIO 2                    /* below the control task (4): the HOME line is the least urgent thing here */
#define DOWN_WAIT_MS 300u              /* a phone told to go: this long for its link to end before the stack stops */
/* Wi-Fi first (Link, the jank radio): BLE gets what is left. BLE-MIDI asks for 15 ms or less, for playing notes; the
 * control line is a few lines a second, so the peripheral asks for 30..60 ms (x 1.25 ms), and a peripheral latency of 4: it
 * may skip up to 4 connection events in a row when it has nothing to send, which gives Wi-Fi those slots too; a line from
 * the phone then waits up to (4 + 1) x 60 = 300 ms. Apple's rules for a request (Accessory Design Guidelines for Apple
 * Devices, Bluetooth LE, "Connection Parameters", as stage 1 quoted them; not re-read here): interval min >= 15 ms, max >=
 * min + 15 ms, latency <= 30, max x (latency + 1) <= 2 s, supervision timeout 2..6 s and > max x (latency + 1) x 3:
 * here 300 ms, and 900 ms < 4 s. The phone decides: it may keep its own interval. */
#define CONN_ITVL_MIN 24
#define CONN_ITVL_MAX 48
#define CONN_LATENCY 4
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
static volatile uint8_t s_synced;      /* the host and controller agree: GAP calls work */
static volatile uint8_t s_advertising;
static uint8_t s_stack_on;             /* the task's alone */
static uint8_t s_own_addr;
static char s_name[12];
static SemaphoreHandle_t s_tx_lock;    /* one sender at a time: a split message must not interleave */
static SemaphoreHandle_t s_ph_lock;    /* s_ph: the task, the GAP events, the control and button tasks */
static struct jph s_ph;
static TaskHandle_t s_task;
static struct jbm_in s_rx_ble;         /* the host task's alone */
static struct jx_in s_rx_jx;
static uint8_t s_rx_text[RX_TEXT_MAX];
static size_t s_rx_n;
static size_t s_heap_used;             /* internal heap the last start took, for `diag` */
static uint32_t s_ups, s_up_fails;     /* stack starts, failed ones */
static uint32_t s_home_sent;
static volatile uint32_t s_connects;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void ph_lock(void) { xSemaphoreTake(s_ph_lock, portMAX_DELAY); }
static void ph_unlock(void) { xSemaphoreGive(s_ph_lock); }

static void wake(void)
{
    if (s_task)
        xTaskNotifyGive(s_task);
}

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
    const struct ble_gap_upd_params p = {.itvl_min = CONN_ITVL_MIN, .itvl_max = CONN_ITVL_MAX, .latency = CONN_LATENCY,
                                         .supervision_timeout = CONN_TIMEOUT};
    int rc;
    jbm_init(&s_rx_ble);
    jx_init(&s_rx_jx);
    s_rx_n = 0;
    s_mtu = BLE_ATT_MTU_DFLT;
    s_notify = 0;
    s_conn = conn;
    s_connects++;
    s_advertising = 0;                 /* a connection ends undirected advertising */
    ph_lock();
    jph_connected(&s_ph, now_ms());
    ph_unlock();
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
            s_advertising = 0;         /* the task starts it again while the window lasts */
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        s_notify = 0;
        ph_lock();
        jph_disconnected(&s_ph, now_ms());
        ph_unlock();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        s_advertising = 0;
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
    wake();
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
        rc = ble_gap_adv_start(s_own_addr, NULL, BLE_HS_FOREVER, &p, on_gap, NULL);   /* the task stops it */
    if (rc == 0)
        s_advertising = 1;
    else
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
    s_synced = 1;
    wake();                            /* the task starts advertising */
}

static void on_reset(int reason)
{
    s_synced = 0;
    s_advertising = 0;
    ESP_LOGW(TAG, "host reset: %d", reason);
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();                 /* returns on nimble_port_stop() */
    nimble_port_freertos_deinit();
}

/* controller and host up; advertising follows on_sync. -> ESP_OK, or the stack is down again */
static esp_err_t stack_up(void)
{
    const size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    esp_err_t e = nimble_port_init();  /* the controller too (esp_bt_controller_init, _enable); its interrupt to this core: 0 */
    int rc;
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
    /* no bonding (the file's header): "just works" for one connection if the phone asks, no keys handed out or kept */
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_our_key_dist = 0;
    ble_hs_cfg.sm_their_key_dist = 0;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    rc = ble_gatts_count_cfg(SERVICES);
    if (rc == 0)
        rc = ble_gatts_add_svcs(SERVICES);
    if (rc == 0)
        rc = ble_svc_gap_device_name_set(s_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT: %d", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }
    ble_store_config_init();           /* RAM only (no CONFIG_BT_NIMBLE_NVS_PERSIST): the CCCD of this connection */
    s_conn = BLE_HS_CONN_HANDLE_NONE;
    s_notify = 0;
    s_synced = 0;
    s_advertising = 0;
    nimble_port_freertos_init(host_task);
    s_heap_used = before - heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    return ESP_OK;
}

/* the phone told to go, advertising stopped, host and controller down: the controller's memory comes back */
static void stack_down(void)
{
    uint16_t conn = s_conn;
    if (s_synced) {
        if (conn != BLE_HS_CONN_HANDLE_NONE && ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM) == 0)
            for (uint32_t t = 0; t < DOWN_WAIT_MS && s_conn != BLE_HS_CONN_HANDLE_NONE; t += 10u)
                vTaskDelay(pdMS_TO_TICKS(10));
        if (s_advertising)
            ble_gap_adv_stop();
    }
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);   /* no notification half sent */
    s_synced = 0;
    if (nimble_port_stop() != 0)
        ESP_LOGE(TAG, "host stop failed");
    if (nimble_port_deinit() != ESP_OK)
        ESP_LOGE(TAG, "deinit failed");
    s_conn = BLE_HS_CONN_HANDLE_NONE;
    s_notify = 0;
    s_advertising = 0;
    xSemaphoreGive(s_tx_lock);
}

/* one packet as one notification; tries 0: dropped when NimBLE has no buffer free */
static int notify(const uint8_t *pkt, size_t len, int tries, int64_t until_us)
{
    for (;;) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(pkt, (uint16_t)len);
        int rc = om ? ble_gatts_notify_custom(s_conn, s_val_handle, om) : BLE_HS_ENOMEM;   /* om is consumed either way */
        if (rc != BLE_HS_ENOMEM || tries-- <= 0 || esp_timer_get_time() >= until_us)
            return rc;
        vTaskDelay(pdMS_TO_TICKS(NOTIFY_WAIT_MS));
    }
}

/* SysEx messages out in as few packets as they fit (core/jank_blemidi.h jbm_pack; tests/test_blemidi.c send_all is the same
 * rule): whole messages packed while they fit, one too big for an empty packet split on its own. s_tx_lock held. */
struct tx {
    uint8_t pkt[PKT_MAX];
    size_t len, cap;
    uint16_t ms;
    int tries, rc;
    int64_t until_us;
};

static void tx_begin(struct tx *t, int tries)
{
    const size_t mtu = s_mtu;
    t->cap = mtu - 3u < sizeof t->pkt ? mtu - 3u : sizeof t->pkt;
    t->len = 0;
    t->ms = (uint16_t)((esp_timer_get_time() / 1000) & 0x1FFF);
    t->tries = tries;
    t->until_us = esp_timer_get_time() + WRITE_MAX_US;
    t->rc = 0;
}

static void tx_flush(struct tx *t)
{
    if (t->len && !t->rc)
        t->rc = notify(t->pkt, t->len, t->tries, t->until_us);
    t->len = 0;
}

static void tx_msg(struct tx *t, const uint8_t *msg, size_t n)
{
    size_t at = 0, l;
    if (t->rc || jbm_pack(msg, n, t->ms, t->pkt, &t->len, t->cap))
        return;
    tx_flush(t);
    if (jbm_pack(msg, n, t->ms, t->pkt, &t->len, t->cap))
        return;
    while (!t->rc && (l = jbm_sysex_packet(msg, n, &at, t->ms, t->pkt, t->cap)) != 0)
        t->rc = notify(t->pkt, l, t->tries, t->until_us);   /* a cut message: the reader ends it at the next F0 */
}

static int listening(void) { return s_synced && s_conn != BLE_HS_CONN_HANDLE_NONE && s_notify; }

void ble_midi_write(const char *text, size_t n)
{
    static struct tx t;
    uint8_t frame[JX_FRAME_MAX];
    if (!s_tx_lock || !listening())
        return;
    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    if (listening()) {
        tx_begin(&t, NOTIFY_TRIES);
        while (n && !t.rc) {
            size_t k = n < JX_TEXT_MAX ? n : JX_TEXT_MAX;
            tx_msg(&t, frame, jx_frame(text, k, frame));
            text += k;
            n -= k;
        }
        tx_flush(&t);
    }
    xSemaphoreGive(s_tx_lock);
}

/* the HOME line to a listening phone, when jph_home_due says so; never waits for a buffer */
static void home_push(void)
{
    static struct jk_snapshot snap;
    static struct jph_rate rate;
    static struct tx t;
    static uint32_t rate_for;
    struct jk_cfg cfg;
    char line[JX_PUSH_TEXT_MAX + 1];
    uint8_t frame[JX_PUSH_FRAME_MAX];
    size_t n, w;
    if (!listening())
        return;
    if (rate_for != s_connects) {      /* a new connection gets the line at once */
        rate_for = s_connects;
        jph_rate_init(&rate);
    }
    app_cfg_get(&cfg);
    bridge_snapshot(&snap);
    n = jph_home_line(&cfg, &snap, bridge_ms(), line, sizeof line);
    if (!n || !jph_home_due(&rate, line, n, now_ms()))
        return;
    w = jx_push_frame(JX_PUSH_HOME, line, n, frame);
    if (!w || xSemaphoreTake(s_tx_lock, 0) != pdTRUE)
        return;
    if (listening()) {
        tx_begin(&t, 0);
        tx_msg(&t, frame, w);
        tx_flush(&t);
        s_home_sent += t.rc == 0;
    }
    xSemaphoreGive(s_tx_lock);
}

/* the window and the stack: on when jank_phone says so, off otherwise; advertising while a window is open */
static void step(void)
{
    int on, adv;
    ph_lock();
    jph_step(&s_ph, now_ms());
    on = jph_radio_on(&s_ph);
    adv = jph_advertising(&s_ph);
    ph_unlock();
    if (on && !s_stack_on) {
        esp_err_t e = stack_up();
        s_ups++;
        if (e == ESP_OK) {
            s_stack_on = 1;
            ESP_LOGI(TAG, "on: %u bytes of internal heap", (unsigned)s_heap_used);
        } else {
            s_up_fails++;
            ESP_LOGE(TAG, "no Bluetooth: %s", esp_err_to_name(e));
            ph_lock();
            jph_close(&s_ph);          /* no retry loop: the next window tries again */
            ph_unlock();
        }
        return;
    }
    if (!on && s_stack_on) {
        stack_down();
        s_stack_on = 0;
        ESP_LOGI(TAG, "off");
        return;
    }
    if (!s_stack_on || !s_synced)
        return;
    if (adv && !s_advertising && s_conn == BLE_HS_CONN_HANDLE_NONE)
        advertise();
    else if (!adv && s_advertising) {
        ble_gap_adv_stop();            /* BLE_HS_EALREADY when it ended by itself: stopped either way */
        s_advertising = 0;
    }
}

static void phone_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TASK_EVERY_MS));
        step();
        home_push();
    }
}

int ble_midi_pair(void)
{
    int r;
    if (!s_ph_lock)
        return -1;
    ph_lock();
    r = jph_pair(&s_ph, now_ms());
    ph_unlock();
    wake();
    return r;
}

void ble_midi_close(void)
{
    if (!s_ph_lock)
        return;
    ph_lock();
    jph_close(&s_ph);
    ph_unlock();
    wake();
}

void ble_midi_setting(uint32_t mode)
{
    if (!s_ph_lock)
        return;
    ph_lock();
    jph_set_mode(&s_ph, mode);
    ph_unlock();
    wake();
}

void ble_midi_state(uint8_t *state, uint32_t *left_s)
{
    if (!s_ph_lock) {
        *state = JPH_OFF;
        *left_s = 0;
        return;
    }
    ph_lock();
    *state = s_ph.state;
    *left_s = jph_left_s(&s_ph, now_ms());
    ph_unlock();
}

int ble_midi_advertising(void)
{
    uint8_t st;
    uint32_t left;
    ble_midi_state(&st, &left);
    return st == JPH_WINDOW;
}

void ble_midi_diag(char *out, size_t cap)
{
    uint8_t st;
    uint32_t left;
    ble_midi_state(&st, &left);
    snprintf(out, cap, "ble=%s link=%s mtu=%u heap_taken=%u ups=%lu up_fails=%lu home_sent=%lu\n",
             st == JPH_CONNECTED ? "connected" : st == JPH_WINDOW ? "window" : "off",
             !s_stack_on ? "down" : s_conn == BLE_HS_CONN_HANDLE_NONE ? (s_advertising ? "advertising" : "idle") : s_notify ? "subscribed" : "connected",
             (unsigned)s_mtu, (unsigned)s_heap_used, (unsigned long)s_ups, (unsigned long)s_up_fails, (unsigned long)s_home_sent);
}

esp_err_t ble_midi_start(const uint8_t mac[6], uint32_t mode)
{
    s_tx_lock = xSemaphoreCreateMutex();
    s_ph_lock = xSemaphoreCreateMutex();
    if (!s_tx_lock || !s_ph_lock)
        return ESP_ERR_NO_MEM;
    snprintf(s_name, sizeof s_name, "jank %02X%02X", mac[4], mac[5]);
    jph_init(&s_ph, mode, now_ms());   /* auto: the start's window */
    return xTaskCreatePinnedToCore(phone_task, "ble", TASK_STACK, NULL, TASK_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
