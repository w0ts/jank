/* SPDX-License-Identifier: GPL-3.0-only */
/* The control protocol's task: lines from the USB MIDI port's SysEx (and BLE MIDI's, CONFIG_JANK_BLE_MIDI) to jk_ctl_line,
 * and each answer back to where its line came from. Each source has its own line buffer, so lines from two at once never
 * mix; the input is one message buffer of chunks tagged with their source. */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "freertos/task.h"

#define CTL_LINE_MAX 256u                /* an update chunk is a line of about 190 (core/jank_ota.h) */
#define CTL_CHUNK_MAX 64u                /* text a chunk carries (usb.c reads 64 bytes of MIDI at a time) */
#define CTL_IN_SIZE 1280u                /* room for a few update lines with each chunk's 5 bytes of header */
#define CTL_TICK_MS 1000
#define OUT_MAX 1400u

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
static enum ctl_src s_src;             /* where the line being run came from: its answer goes there */

void ctl_line_rx(enum ctl_src src, const uint8_t *data, size_t n)
{
    uint8_t chunk[1u + CTL_CHUNK_MAX];
    if (!s_in || src >= CTL_SRC_COUNT)
        return;
    while (n) {
        size_t k = n < CTL_CHUNK_MAX ? n : CTL_CHUNK_MAX;
        chunk[0] = (uint8_t)src;
        memcpy(chunk + 1, data, k);
        xMessageBufferSend(s_in, chunk, 1u + k, 0);
        data += k;
        n -= k;
    }
}

void ctl_serial_rx(const uint8_t *data, size_t n) { ctl_line_rx(CTL_SRC_USB, data, n); }

void ctl_reply(const char *text, size_t n)
{
#if CONFIG_JANK_BLE_MIDI
    if (s_src == CTL_SRC_BLE) {
        ble_midi_write(text, n);
        return;
    }
#endif
    usb_serial_write(text, n);
}

static void reply_out(void) { ctl_reply(s_out, strlen(s_out)); }

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
    bridge_diag(s_out + n, sizeof s_out - n);
    n = strlen(s_out);
    drive_state(s_out + n, sizeof s_out - n);
#if CONFIG_JANK_BLE_MIDI
    n = strlen(s_out);
    ble_midi_diag(s_out + n, sizeof s_out - n);
#endif
    strlcat(s_out, "ok\n", sizeof s_out);
    reply_out();
}

/* Lines a phone may not send (docs/ble.md): an update (a dropped link would leave a slot half written), download mode
 * (only USB can use it), and the Wi-Fi password (the BLE link is not encrypted: no pairing, by the ruling's design, so it
 * would cross the air in clear text). -> the refusal, or NULL */
static const char *usb_only(const char *line)
{
    while (*line == ' ')
        line++;
    if (!strncmp(line, "ota", 3))
        return "err ota over USB only\n";
    if (!strncmp(line, "bootloader", 10))
        return "err bootloader over USB only\n";
    if (!strncmp(line, "set pass", 8) && (!line[8] || line[8] == ' '))
        return "err the Wi-Fi password over USB only\n";
    return NULL;
}

#if CONFIG_JANK_BLE_MIDI
/* `ble pair` and `ble close`: answered before they act, so that a phone asking to close still hears ok */
static void run_ble(enum jk_ctl_action a)
{
    if (a == JK_CTL_BLE_PAIR) {
        strcpy(s_out, ble_midi_pair() ? "err ble is off\n" : "ok\n");
        reply_out();
        return;
    }
    strcpy(s_out, "ok\n");
    reply_out();
    vTaskDelay(pdMS_TO_TICKS(200));
    ble_midi_close();
}
#endif

static void run_line(const char *line)
{
    struct jk_cfg cfg;
    enum jk_ctl_action a;
    const char *refused = s_src == CTL_SRC_USB ? NULL : usb_only(line);
    if (refused) {
        ctl_reply(refused, strlen(refused));
        return;
    }
    if (ota_line(line))
        return;
    if (!strcmp(line, "diag")) {
        run_diag();
        return;
    }
    if (!strcmp(line, "drive")) {
        drive_state(s_out, sizeof s_out);
        strlcat(s_out, "ok\n", sizeof s_out);
        reply_out();
        return;
    }
#ifdef JANK_CLOCK_PROBE
    if (!strncmp(line, "probe", 5) && (!line[5] || !strcmp(line + 5, " reset"))) {
        ctl_reply(s_out, probe_report(s_out, sizeof s_out, line[5] != 0));
        return;
    }
#endif
    app_cfg_get(&cfg);
    bridge_snapshot(&s_snap);
    a = jk_ctl_line(line, &cfg, &s_snap, bridge_ms(), s_out, sizeof s_out);
    if (a == JK_CTL_CFG_CHANGED)
        app_cfg_set(&cfg);
    if (a == JK_CTL_SCAN) {
        run_scan();
        return;
    }
    if (a == JK_CTL_BLE_PAIR || a == JK_CTL_BLE_CLOSE) {
#if CONFIG_JANK_BLE_MIDI
        run_ble(a);
#else
        ctl_reply("err no bluetooth in this build\n", 31);
#endif
        return;
    }
    if (a == JK_CTL_TEMPO || a == JK_CTL_PLAY || a == JK_CTL_STOP) {
        int32_t mpb = 0;
        int r = a == JK_CTL_TEMPO ? (jk_ctl_tempo(line, &mpb) ? -1 : bridge_tempo(mpb)) : bridge_play(a == JK_CTL_PLAY);
        strcpy(s_out, r == -2 ? "err audio mode\n" : r ? "err no session\n" : "ok\n");
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

static void take(struct line_in *in, enum ctl_src src, uint8_t c)
{
    if (c == '\n' || c == '\r') {
        in->line[in->n] = 0;
        s_src = src;
        if (in->too_long)
            ctl_reply("err line too long\n", 18);
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
    static struct line_in lines[CTL_SRC_COUNT];
    uint8_t chunk[1u + CTL_CHUNK_MAX];
    (void)arg;
    for (;;) {
        size_t n;
        ++s_beats;
        n = xMessageBufferReceive(s_in, chunk, sizeof chunk, pdMS_TO_TICKS(CTL_TICK_MS));
        if (!n) {
            ota_idle();
            continue;
        }
        if (chunk[0] >= CTL_SRC_COUNT)
            continue;
        for (size_t i = 1; i < n; ++i)
            take(&lines[chunk[0]], (enum ctl_src)chunk[0], chunk[i]);
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
