/* SPDX-License-Identifier: GPL-3.0-only */
/* The control protocol's task: lines from the USB MIDI port's SysEx to jk_ctl_line, and each answer back on the port. The
 * input is one message buffer of chunks of the line's text. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "jank_fold.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "freertos/task.h"
#include "uiscreen.h"
#include "usb_host_mode.h"

#define CTL_LINE_MAX 256u                /* an update chunk is a line of about 190 (core/jank_ota.h) */
#define CTL_CHUNK_MAX 64u                /* text a chunk carries (usb.c reads 64 bytes of MIDI at a time) */
#define CTL_IN_SIZE 1280u                /* room for a few update lines with each chunk's 4 bytes of header */
#define CTL_TICK_MS 1000
#define OUT_MAX JK_CTL_OUT_MAX

void app_bootloader(void);
void app_reboot(void);

struct line_in {
    char line[CTL_LINE_MAX];
    size_t n;
    int too_long;
};

static MessageBufferHandle_t s_in;
static volatile uint32_t s_beats;      /* passes of the task: it is alive and taking lines */
static struct jk_snapshot s_snap;
static char s_out[OUT_MAX];

void ctl_serial_rx(const uint8_t *data, size_t n)
{
    if (!s_in)
        return;
    while (n) {
        size_t k = n < CTL_CHUNK_MAX ? n : CTL_CHUNK_MAX;
        xMessageBufferSend(s_in, data, k, 0);
        data += k;
        n -= k;
    }
}

/* Where an answer goes: the USB control line, or, while a Wi-Fi request runs (net_serve), a collector. One task runs every line. */
static void (*s_sink)(const char *b, size_t n);

static void ctl_write(const char *b, size_t n)
{
    if (s_sink)
        s_sink(b, n);
    else
        usb_serial_write(b, n);
}

static void reply_out(void) { ctl_write(s_out, strlen(s_out)); }

static void run_scan(void)
{
    static struct js_table table;
    esp_err_t e = ui_scan_table(&table);
    if (e != ESP_OK)
        snprintf(s_out, sizeof s_out, "err %s\n", esp_err_to_name(e));
    else if (!js_table_report(&table, s_out, sizeof s_out))
        strcpy(s_out, "err scan answer too long\n");
    reply_out();
}

static void run_diag(void)
{
    size_t n;
    app_diag(s_out, sizeof s_out);
    n = strlen(s_out);
    n += panic_info_text(s_out + n, sizeof s_out - n);
    bridge_diag(s_out + n, sizeof s_out - n);
    n = strlen(s_out);
    drive_state(s_out + n, sizeof s_out - n);
    n = strlen(s_out);
    if (sizeof s_out - n > 8u)         /* what the last USB host session saw (hostdiag.c); the "ok" below keeps its room */
        hostdiag_text(s_out + n, sizeof s_out - n - 4u);
    strlcat(s_out, "ok\n", sizeof s_out);
    reply_out();
}

/* `diag intr`: which interrupts each core has (esp_intr_dump, cut to those in use). Core 1 should show the
 * MIDI clock's gptimer alone (and the CPU-internal ones the dump leaves out): anything else there delays the clock. */
static void run_diag_intr(void)
{
    const size_t cap = 8192u;
    char *dump = malloc(cap);
    FILE *f = dump ? fmemopen(dump, cap, "w") : NULL;
    if (!f) {
        free(dump);
        strcpy(s_out, "err no memory for the dump\n");
        reply_out();
        return;
    }
    esp_intr_dump(f);
    fclose(f);
    dump[cap - 1u] = 0;
    if (!jk_intr_summary(dump, s_out, sizeof s_out))
        strcpy(s_out, "err interrupt list too long\n");
    free(dump);
    reply_out();
}

/* `nudge beat <b>` (core/jank_core.h jk_ctl_nudge): the beat task applies it (beat_nudge); the answer is the user's total
 * and the downbeat estimator's view before it */
static void run_nudge(const char *line)
{
    static const char *const why[] = {"err link mode\n", "err no beat yet\n", "err nudge beat at its limit, -3 to 3 beats in all\n", "err busy\n"};
    int halves = 0, rc;
    int8_t total = 0;
    char b[8], view[96];
    rc = jk_ctl_nudge(line, &halves) ? BEAT_NUDGE_LIMIT : beat_nudge(halves, &total, view, sizeof view);
    if (rc == BEAT_NUDGE_OK)
        snprintf(s_out, sizeof s_out, "nudge_beats=%s\n%sok\n", jk_half_beats(total, b), view);
    else
        strlcpy(s_out, why[-rc - 1 < 4 ? -rc - 1 : 3], sizeof s_out);
}

static void run_line(const char *line)
{
    struct jk_cfg cfg;
    enum jk_ctl_action a;
    if (ota_line(line))
        return;
    if (!strcmp(line, "diag")) {
        run_diag();
        return;
    }
    if (!strcmp(line, "diag intr")) {
        run_diag_intr();
        return;
    }
    if (!strcmp(line, "drive")) {
        drive_state(s_out, sizeof s_out);
        strlcat(s_out, "ok\n", sizeof s_out);
        reply_out();
        return;
    }
    if (!strcmp(line, "line")) {
        linein_report(s_out, sizeof s_out);
        reply_out();
        return;
    }
#ifdef JANK_CLOCK_PROBE
    if (!strncmp(line, "probe", 5) && (!line[5] || !strcmp(line + 5, " reset"))) {
        usb_serial_write(s_out, probe_report(s_out, sizeof s_out, line[5] != 0));
        return;
    }
#endif
    app_cfg_get(&cfg);
    bridge_snapshot(&s_snap);
    usbrole_snapshot(&s_snap.usb);
    uis_sys(&s_snap.sys);
    s_snap.lead = (uint8_t)uis_lead_get();
    s_snap.sleep = (uint8_t)uis_sleep_get();
    s_snap.bright = (uint8_t)uis_bright_get();
    a = jk_ctl_line(line, &cfg, &s_snap, bridge_ms(), s_out, sizeof s_out);
    if (a == JK_CTL_CFG_CHANGED)
        app_cfg_set(&cfg);
    if (a == JK_CTL_LEAD) {
        int on;
        if (!jk_ctl_lead(line, &on))
            uis_lead_set(on);
    }
    if (a == JK_CTL_DISPLAY) {
        int br, v;
        if (!jk_ctl_display(line, &br, &v)) {
            if (br)
                uis_bright_set(v);
            else
                uis_sleep_set(v);
        }
    }
    if (a == JK_CTL_SCAN) {
        run_scan();
        return;
    }
    if (a == JK_CTL_FOLD) {
        int dir = 0, exp = jk_ctl_fold(line, &dir) ? -99 : beat_fold(dir);
        if (exp == -99)
            strcpy(s_out, "err link mode\n");
        else
            snprintf(s_out, sizeof s_out, "fold=%s\nok\n", jfo_name(exp));
    }
    if (a == JK_CTL_NUDGE)
        run_nudge(line);
    if (a == JK_CTL_TEMPO || a == JK_CTL_PLAY || a == JK_CTL_STOP) {
        int32_t mpb = 0;
        int r = a == JK_CTL_TEMPO ? (jk_ctl_tempo(line, &mpb) ? -1 : bridge_tempo(mpb)) : bridge_play(a == JK_CTL_PLAY);
        strcpy(s_out, r ? "err no session\n" : "ok\n");   /* (AUDIO mode: play / stop are the hold, always ok) */
    }
    reply_out();
    if (a == JK_CTL_REBOOT || a == JK_CTL_BOOTLOADER) {
        vTaskDelay(pdMS_TO_TICKS(100));        /* the answer goes out first */
        if (a == JK_CTL_REBOOT)
            app_reboot();
        else
            app_bootloader();
    }
}

/* The Wi-Fi diagnostics (core/jank_wsvc.h): a request the bridge task queued runs here, in the control task, only if it is one of
 * the read-only lines; its answer is collected and goes back in chunks for the bridge task to send. Nothing here changes a setting. */
#define NET_REPLY_MAX (JK_CTL_OUT_MAX + 64u)
static char *s_net_buf;                /* allocated at the first request: nothing is held in a unit that never has one */
static size_t s_net_n;

static void net_collect(const char *b, size_t n)
{
    size_t room = NET_REPLY_MAX - s_net_n;
    if (n > room)
        n = room;
    memcpy(s_net_buf + s_net_n, b, n);
    s_net_n += n;
}

static void net_serve(void)
{
    struct jw_req rq;
    static const char refused[] = "err not allowed over wifi\n";
    size_t off = 0;
    unsigned total;
    if (!wsvc_take_request(&rq))
        return;
    if (!s_net_buf)
        s_net_buf = malloc(NET_REPLY_MAX);
    if (!s_net_buf)
        return;
    s_net_n = 0;
    s_sink = net_collect;
    if (jw_allowed(rq.line))
        run_line(rq.line);
    else
        net_collect(refused, sizeof refused - 1u);
    s_sink = NULL;
    total = jw_chunks(s_net_n);
    if (total > JW_CHUNKS_MAX + 1u)
        total = JW_CHUNKS_MAX + 1u;
    for (unsigned i = 0; i < total; i++) {
        size_t n = s_net_n - off > JW_CHUNK ? JW_CHUNK : s_net_n - off;
        if (!wsvc_put_reply(&rq, i, i + 1u == total, s_net_buf + off, n))
            return;                    /* the queue stayed full: this answer is given up, the client asks again */
        off += n;
    }
}

static void take(struct line_in *in, uint8_t c)
{
    if (c == '\n' || c == '\r') {
        in->line[in->n] = 0;
        if (in->too_long)
            usb_serial_write("err line too long\n", 18);
        else
            run_line(in->line);
        in->n = 0;
        in->too_long = 0;
    } else if (in->n + 1u < sizeof in->line) {
        in->line[in->n++] = (char)c;
    } else {
        in->too_long = 1;
    }
}

static void ctl_task(void *arg)
{
    static struct line_in in;
    uint8_t chunk[CTL_CHUNK_MAX];
    (void)arg;
    for (;;) {
        size_t n;
        ++s_beats;
        app_cfg_flush(0);                      /* a change made while playing: saved once stopped */
        net_serve();                           /* a Wi-Fi request (read-only lines), within a tick */
        n = xMessageBufferReceive(s_in, chunk, sizeof chunk, pdMS_TO_TICKS(CTL_TICK_MS));
        if (!n) {
            ota_idle();
            continue;
        }
        for (size_t i = 0; i < n; ++i)
            take(&in, chunk[i]);
    }
}

uint32_t ctl_beats(void) { return s_beats; }

esp_err_t ctl_start(void)
{
    s_in = xMessageBufferCreate(CTL_IN_SIZE);  /* the sender waits for answers, but a flash write stalls this task */
    if (!s_in)
        return ESP_ERR_NO_MEM;
    return xTaskCreatePinnedToCore(ctl_task, "ctl", 6144, NULL, 4, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
