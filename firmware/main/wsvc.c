/* SPDX-License-Identifier: GPL-3.0-only */
/* The Wi-Fi diagnostics service (core/jank_wsvc.h) between its two tasks: the bridge task sees the frames from the network and
 * sends the answers; the control task runs the control lines. Two small queues carry a request one way and the answer's chunks
 * back, so neither task waits for the other and no control-line buffer is shared. Only in MODE WIFI (the unit has an address
 * on a network then): wsvc_start is called by bridge.c in that mode alone, and nothing is allocated otherwise. */
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "jank_wsvc.h"

#define REQUESTS 2u                    /* a third request while two wait is dropped: the client asks again */
#define REPLIES 12u                    /* the longest control answer is 11 chunks */
#define PUT_WAIT_MS 50                 /* the control task waits this long for room for one chunk, then gives the answer up */

static const char *TAG = "wsvc";

struct reply {
    uint8_t mac[6];
    uint8_t index_last;                /* the chunk index, bit 7 the last */
    uint8_t n;
    uint16_t port;
    uint32_t ip;
    char text[JW_CHUNK];
};

static QueueHandle_t s_req, s_rep;

esp_err_t wsvc_start(void)
{
    QueueHandle_t req, rep;
    if (s_req)
        return ESP_OK;
    req = xQueueCreate(REQUESTS, sizeof(struct jw_req));
    rep = xQueueCreate(REPLIES, sizeof(struct reply));
    if (!req || !rep) {
        if (req)
            vQueueDelete(req);
        if (rep)
            vQueueDelete(rep);
        ESP_LOGW(TAG, "no memory: no Wi-Fi diagnostics");
        return ESP_ERR_NO_MEM;
    }
    s_rep = rep;                       /* both made: only now does another task see them (s_req last: the control task tests it) */
    s_req = req;
    return ESP_OK;
}

int wsvc_input(const uint8_t mac[6], uint32_t ip, uint32_t mask, const uint8_t *eth, size_t len, uint8_t *out, size_t out_cap, size_t *out_len)
{
    struct jw_req rq;
    *out_len = 0;
    if (!s_req)
        return WSVC_NOT_OURS;
    switch (jw_input(mac, ip, mask, eth, len, &rq, out, out_cap, out_len)) {
    case JW_ARP_REPLIED: return WSVC_SEND_ARP;
    case JW_REQUEST:
        if (xQueueSend(s_req, &rq, 0) == pdTRUE)   /* a full queue drops it: the client asks again */
            ctl_wake();                            /* the control task sleeps up to a second between passes */
        return WSVC_CONSUMED;
    default: return WSVC_NOT_OURS;
    }
}

size_t wsvc_next_frame(const uint8_t mac[6], uint32_t ip, uint8_t *out, size_t cap)
{
    struct reply r;
    struct jw_req to;
    if (!s_rep || xQueueReceive(s_rep, &r, 0) != pdTRUE)
        return 0;
    if (!ip)                           /* the lease is gone: a frame from 0.0.0.0 would be wrong, the chunk is dropped */
        return 0;
    memcpy(to.src_mac, r.mac, 6);
    to.src_ip = r.ip;
    to.src_port = r.port;
    to.line[0] = 0;
    return jw_reply_frame(mac, ip, &to, r.index_last & 0x7Fu, (r.index_last & 0x80u) != 0, r.text, r.n, out, cap);
}

int wsvc_take_request(struct jw_req *rq) { return s_req && xQueueReceive(s_req, rq, 0) == pdTRUE; }

int wsvc_put_reply(const struct jw_req *to, unsigned index, int last, const char *text, size_t n)
{
    struct reply r;
    if (!s_rep || n > JW_CHUNK || index > JW_CHUNKS_MAX)
        return 0;
    memcpy(r.mac, to->src_mac, 6);
    r.ip = to->src_ip;
    r.port = to->src_port;
    r.index_last = (uint8_t)(index | (last ? 0x80u : 0u));
    r.n = (uint8_t)n;
    if (n)
        memcpy(r.text, text, n);
    return xQueueSend(s_rep, &r, pdMS_TO_TICKS(PUT_WAIT_MS)) == pdTRUE;
}
