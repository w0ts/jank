/* SPDX-License-Identifier: GPL-3.0-only */
/* USB: a CDC-NCM network adapter, a USB MIDI port, a drive and (bench build) a UAC2 speaker for the beat tracker. The MIDI
 * port carries the clock to the computer and the control line in SysEx (core/jank_sysex.h); the drive holds the control
 * page (drive.c), so there is no link to remember.
 * No CDC-ACM serial: the S3 has 5 IN endpoints, EP0 included (tinyusb dwc2_esp32.h ep_in_count). NCM takes 2, MIDI 1 and
 * the drive 1, which is all of them; more fails when the host configures the device. The speaker (usb_audio.h) is an OUT
 * endpoint (EP5 of 7), adaptive, with no feedback endpoint for that reason.
 * Device or host is decided at each start and kept (usbrole.c, core/jank_usbrole.h): the setting USB DEVICE / HOST, and on
 * a board with the CC chip what is on the cable. Host mode is usb_hostlib.c with two clients, usb_midi_host.c (the clock
 * out) and usb_uac_host.c (a device's audio into the tracker). Another role later is a restart into it (usbrole.c): the
 * device and host stacks are never both installed. On the XIAO, HOST means what this file did alone before: a device
 * first, and when no computer configured it within 3 s, a restart as host.
 * The address in the NCM descriptor is the computer's adapter's own address, which is why the stick offers the one
 * its radio has: the bridge then never rewrites an address. */
#include <string.h>
#include "app.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "jank_sysex.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_net.h"
#include "tusb.h"
#include "usb_audio.h"
#include "board.h"
#include "jank_usbrole.h"
#include "usb_host_mode.h"

#define RING_SIZE 64u                  /* a power of two; a clock tick is one byte */
#define TEXT_OUT_SIZE 2048u
#define PUSH_OUT_SIZE 512u             /* beat reports: ~25 bytes each, at most a few a second */
#define WRITE_TRIES 50                 /* milliseconds a full MIDI buffer may hold us up */
#define MIDI_TASK_CORE 0              /* core 1 is the MIDI clock's alone (midi.c) */
#define MIDI_TASK_PRIO 24             /* above Wi-Fi (23) on core 0: it runs a few us a tick, straight into the endpoint (measured 2026-11-09: at 6, USB clock sd 220 us) */

/* tusb_get_mac_string_id() with CDC off and MSC on: esp_tinyusb writes the MAC there, so STR_MAC has to be that index */
/* esp_tinyusb takes at most 8 strings (USB_STRING_DESCRIPTOR_ARRAY_SIZE, include_private/descriptors_control.h): more and
 * tinyusb_driver_install fails ("String descriptors exceed limit"), usb_start returns an error and the start aborts. The
 * speaker is named by STR_MIDI's "Jank", which is what it is called anyway. */
enum { STR_LANG, STR_MAKER, STR_PRODUCT, STR_SERIAL, STR_DISK, STR_NET, STR_MAC, STR_MIDI, STR_COUNT };
_Static_assert(STR_COUNT <= 8, "esp_tinyusb takes at most 8 string descriptors");
enum { ITF_NET, ITF_NET_DATA, ITF_MIDI, ITF_MIDI_STREAM, ITF_DISK, ITF_AUDIO, ITF_AUDIO_STREAM, ITF_TOTAL };
enum { EP_NET_NOTIF = 1, EP_NET_DATA, EP_MIDI, EP_DISK, EP_AUDIO };
#define CONFIG_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_MSC_DESC_LEN + UAC_DESC_LEN)

static const char *STRINGS[STR_COUNT] = {
    [STR_LANG] = "\x09\x04", [STR_MAKER] = "Jank", [STR_PRODUCT] = "Jank", [STR_SERIAL] = NULL,
    [STR_DISK] = "Jank page", [STR_NET] = "Jank network", [STR_MAC] = "", [STR_MIDI] = "Jank",
};

static const uint8_t CONFIG_DESC[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_TOTAL, 0, CONFIG_LEN, 0, 500),
    TUD_CDC_NCM_DESCRIPTOR(ITF_NET, STR_NET, STR_MAC, 0x80 | EP_NET_NOTIF, 64, EP_NET_DATA, 0x80 | EP_NET_DATA, 64, CFG_TUD_NET_MTU),
    TUD_MIDI_DESCRIPTOR(ITF_MIDI, STR_MIDI, EP_MIDI, 0x80 | EP_MIDI, 64),
    TUD_MSC_DESCRIPTOR(ITF_DISK, STR_DISK, EP_DISK, 0x80 | EP_DISK, 64),
    UAC_DESCRIPTOR(ITF_AUDIO, STR_MIDI, EP_AUDIO),
};

static const char *TAG = "usb";
static usb_frame_fn s_rx;
static usb_free_fn s_tx_done;
static usb_line_fn s_line_rx;
static DRAM_ATTR TaskHandle_t s_task;
static StreamBufferHandle_t s_text_out;
static SemaphoreHandle_t s_text_lock;
static MessageBufferHandle_t s_push_out;   /* beat reports: the beat task writes, the MIDI task sends */
static volatile uint8_t s_host_spoke;      /* a control line came from the computer (ota.c confirms a new image on it) */

/* the timer's interrupt fills it, the MIDI task empties it */
static DRAM_ATTR uint8_t s_ring[RING_SIZE];
static DRAM_ATTR uint32_t s_head, s_tail, s_dropped;
static uint8_t s_host;

static esp_err_t on_frame(void *buf, uint16_t len, void *ctx)
{
    (void)ctx;
    s_rx(buf, len);
    return ESP_OK;
}

static void on_tx_done(void *token, void *ctx)
{
    (void)ctx;
    s_tx_done(token);
}

/* USB MIDI CLOCK (the setting usbmidi): off, no clock byte reaches the USB-MIDI port (DIN / TRS MIDI unaffected). Turned
 * off: one MIDI Stop goes out after the last clock, so gear that was playing stops instead of waiting for clocks */
static volatile uint8_t s_clock_on = 1, s_peers_ok = 1, s_forced, s_stop_due;

/* the clock goes out while the setting allows it AND there is somebody to be the clock for (usb_midi_clock_peers) or the Launchpad started the transport (usb_midi_clock_force) */
#define CLOCK_OUT() (s_clock_on && (s_peers_ok || s_forced))

static void clock_changed(uint8_t was_on)
{
    if (was_on && !CLOCK_OUT() && s_task) {   /* (at boot, before the port runs: nothing was sent, nothing to stop) */
        s_stop_due = 1;
        xTaskNotifyGive(s_task);
    }
}

void usb_midi_clock(int on)
{
    uint8_t was = (uint8_t)CLOCK_OUT();
    if ((on != 0) == s_clock_on)
        return;
    s_clock_on = (uint8_t)(on != 0);
    clock_changed(was);
}

/* Host mode (owner 2026-11-24): Jank sends the clock to the Digitone only while it has peers on the Jank / Link network (bridge_peers_heard); alone it is not
 * the master of anything, and a Digitone that was following it stops (one MIDI Stop goes out, as when the setting is turned off). Called about once a second by
 * usb_lp.c; 1 in every other mode */
void usb_midi_clock_peers(int has_peers)
{
    uint8_t was = (uint8_t)CLOCK_OUT();
    if ((has_peers != 0) == s_peers_ok)
        return;
    s_peers_ok = (uint8_t)(has_peers != 0);
    clock_changed(was);
}

/* Host mode (owner 2026-11-24, "start stop on note mode is not working"): the Launchpad's start / stop scene button is an explicit start, so the clock and the Start byte
 * go to the Digitone even when Jank has no peers (measured: peers=0, every clock and Start byte was dropped by the peers rule above, the button did nothing). On while the
 * Launchpad has the transport started, off when it stops it (one MIDI Stop goes out if that ends the clock). ui.c */
void usb_midi_clock_force(int on)
{
    uint8_t was = (uint8_t)CLOCK_OUT();
    if ((on != 0) == s_forced)
        return;
    s_forced = (uint8_t)(on != 0);
    clock_changed(was);
}

bool IRAM_ATTR usb_midi_isr(const uint8_t *bytes, size_t n)
{
    BaseType_t woken = pdFALSE;
    uint32_t head = s_head;
    uint32_t tail = __atomic_load_n(&s_tail, __ATOMIC_ACQUIRE);
    if (!n || !s_task || !CLOCK_OUT())
        return false;
    if (RING_SIZE - (head - tail) < n) {
        ++s_dropped;
        return false;
    }
    for (size_t i = 0; i < n; ++i)
        s_ring[(head + i) & (RING_SIZE - 1u)] = bytes[i];
    __atomic_store_n(&s_head, head + n, __ATOMIC_RELEASE);
    vTaskNotifyGiveFromISR(s_task, &woken);
    return woken == pdTRUE;
}

/* TinyUSB's task: something arrived from the computer */
void tud_midi_rx_cb(uint8_t itf)
{
    (void)itf;
    if (s_task)                        /* the host can be quicker than usb_start */
        xTaskNotifyGive(s_task);
}

static void write_all(const uint8_t *data, size_t n)
{
    for (int tries = 0; n && tries < WRITE_TRIES; ++tries) {
        uint32_t did = tud_midi_stream_write(0, data, (uint32_t)n);
        data += did;
        n -= did;
        if (n)
            vTaskDelay(1);
    }
}

static void send_clock(void)
{
    uint32_t head = __atomic_load_n(&s_head, __ATOMIC_ACQUIRE);
    uint32_t tail = s_tail;
    uint8_t b[RING_SIZE + 1u];         /* (+1: the Stop below) */
    size_t n = 0;
    while (tail != head)
        b[n++] = s_ring[tail++ & (RING_SIZE - 1u)];
    __atomic_store_n(&s_tail, tail, __ATOMIC_RELEASE);
    if (s_stop_due && !CLOCK_OUT()) {   /* USB MIDI CLOCK just turned off (or no peers left): a Stop after what was queued */
        s_stop_due = 0;
        b[n++] = 0xFCu;
    }
    if (n && s_host) {
        uint8_t packets[(RING_SIZE + 1u) * 4u];
        usb_midi_host_send(packets, jx_realtime_packets(b, n, packets, sizeof packets));
    } else if (n && tud_midi_mounted()) {
        write_all(b, n);
    }
}

static void read_host(struct jx_in *in)
{
    uint8_t b[64];
    uint32_t n;
    while ((n = tud_midi_stream_read(b, sizeof b)) != 0) {
        uint8_t text[sizeof b];
        size_t got = 0;
        for (uint32_t i = 0; i < n; ++i) {
            int c = jx_feed(in, b[i]);
            if (c >= 0)
                text[got++] = (uint8_t)c;
        }
        if (got) {
            s_host_spoke = 1;
            s_line_rx(text, got);
        }
    }
}

/* the beat reports (jank_sysex.h JX_PUSH_*): whole SysEx messages, one at a time, the clock first between them */
static void send_push(void)
{
    char text[JX_PUSH_TEXT_MAX];
    uint8_t frame[JX_PUSH_FRAME_MAX];
    size_t n;
    while ((n = xMessageBufferReceive(s_push_out, text, sizeof text, 0)) != 0) {
        size_t w = jx_push_frame(JX_PUSH_BEAT, text, n, frame);
        if (w && tud_midi_mounted())
            write_all(frame, w);
        send_clock();
    }
}

/* one frame at a time, so that the clock goes first between frames */
static void send_text(void)
{
    char text[JX_TEXT_MAX];
    uint8_t frame[JX_FRAME_MAX];
    size_t n;
    while ((n = xStreamBufferReceive(s_text_out, text, sizeof text, 0)) != 0) {
        if (tud_midi_mounted())
            write_all(frame, jx_frame(text, n, frame));
        send_clock();
    }
}

/* the only task that writes to the MIDI port: TinyUSB's byte stream parser holds state between calls */
static void midi_task(void *arg)
{
    struct jx_in in;
    (void)arg;
    jx_init(&in);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5));
        send_clock();
        if (!s_host) {
            read_host(&in);
            send_text();
            send_push();
        }
    }
}

/* The host start's guard. A host start that hangs (a wait that never ends, a task that never runs) or a host run that crashes leaves a
 * unit with no Wi-Fi, no control line and nothing to read. So: a periodic timer; at HANG_TICKS (20 s) with the start-up (USB, then the
 * radio: bridge.c) not done, the step in flight is kept in flash and the unit restarts as a device (usbrole_host_failed), reachable and
 * saying what hung; at STABLE_TICKS (60 s) the run counts as stable and a later reset is no start-up crash (usbrole.c find_host_crash
 * reads the same mark at the next start). No memory is needed at the moment of the failure: the timer exists from the start. */
#define GUARD_MS 5000
#define HANG_TICKS 4u
#define STABLE_TICKS 12u

static esp_timer_handle_t s_guard;
static volatile uint8_t s_guard_ticks, s_boot_done, s_stage;

static void guard_tick(void *arg)
{
    (void)arg;
    s_guard_ticks++;
    if (s_guard_ticks == HANG_TICKS && !s_boot_done) {
        uint8_t step = s_stage;
        if (step == JHD_STEP_HOSTLIB && usb_midi_host_start_step() != JHD_STEP_NONE)
            step = usb_midi_host_start_step();       /* inside the host client's own start: which part */
        usbrole_host_failed(step, ESP_ERR_TIMEOUT);  /* no return */
    }
    if (s_guard_ticks >= STABLE_TICKS) {
        usbrole_host_inflight(0);
        esp_timer_stop(s_guard);
    }
}

static void guard_start(void)
{
    const esp_timer_create_args_t a = {.callback = guard_tick, .name = "hostguard"};
    usbrole_host_inflight(1);
    s_stage = JHD_STEP_HOSTLIB;
    if (esp_timer_create(&a, &s_guard) == ESP_OK)
        esp_timer_start_periodic(s_guard, (uint64_t)GUARD_MS * 1000u);
}

void usb_host_stage(uint8_t step) { s_stage = step; }
void usb_host_boot_done(void)
{
    s_boot_done = 1;
    if (s_host)
        hostdiag_start_saver();           /* now: after the bridge and the tracker have their memory */
}

esp_err_t usb_start(const uint8_t mac[6], usb_frame_fn rx, usb_free_fn tx_done, usb_line_fn line_rx)
{
    tinyusb_config_t cfg = TINYUSB_DEFAULT_CONFIG();
    cfg.task.xCoreID = 0;             /* TinyUSB's task and the USB interrupt it installs: off the clock's core 1 */
    tinyusb_net_config_t net = {.on_recv_callback = on_frame, .free_tx_buffer = on_tx_done};
    esp_err_t e;
    s_host = (uint8_t)(usbrole_boot() == JUR_STACK_HOST);
    if (s_host) {
        uint8_t step = JHD_STEP_NONE;
        guard_start();
        hostdiag_trace_reset();
        hostdiag_mark(JHD_STEP_HOSTLIB);
        e = usb_midi_host_start();
        s_stage = JHD_STEP_MIDI_TASK;
        hostdiag_mark(JHD_STEP_MIDI_TASK);
        if (e != ESP_OK) {
            step = usb_midi_host_start_step();
        } else if (xTaskCreatePinnedToCore(midi_task, "usb_midi", 4096, NULL, MIDI_TASK_PRIO, &s_task, MIDI_TASK_CORE) != pdPASS) {
            e = ESP_ERR_NO_MEM;
            step = JHD_STEP_MIDI_TASK;
        }
        if (e == ESP_OK) {
            esp_err_t a = BOARD_HOST_AUDIO ? usb_uac_host_start() : ESP_OK;   /* (not on the XIAO: the RAM is the host library's, board.c) */
            if (a != ESP_OK)            /* the clock goes on without the audio */
                ESP_LOGE(TAG, "host audio: %s", esp_err_to_name(a));
            s_stage = JHD_STEP_ROLE;
            hostdiag_mark(JHD_STEP_ROLE);
            e = usbrole_start();
            if (e != ESP_OK)
                step = JHD_STEP_ROLE;
        }
        if (e != ESP_OK)
            usbrole_host_failed(step, e);   /* the reason kept, a restart as a device: the unit stays reachable and says what failed; no return */
        hostdiag_clear_failure();           /* it worked: an older failure is no longer news */
        s_stage = JHD_STEP_RADIO;           /* the radio comes next (bridge.c), still under the guard until bridge_start is through */
        hostdiag_mark(JHD_STEP_RADIO);
        return e;
    }
    s_rx = rx;
    s_tx_done = tx_done;
    s_line_rx = line_rx;
    s_text_out = xStreamBufferCreate(TEXT_OUT_SIZE, 1);
    s_text_lock = xSemaphoreCreateMutex();
    s_push_out = xMessageBufferCreate(PUSH_OUT_SIZE);
    if (!s_text_out || !s_text_lock || !s_push_out)
        return ESP_ERR_NO_MEM;
    cfg.descriptor.full_speed_config = CONFIG_DESC;
    cfg.descriptor.string = STRINGS;
    cfg.descriptor.string_count = STR_COUNT;
    if (BOARD_VBUS_SENSE_GPIO >= 0) {
        /* a board with its own power: the D+ pull-up only while VBUS is there (esp_tinyusb's VBUS monitor on VBUS_SENSE;
         * the GPIO interrupt service is usbrole_boot's) */
        cfg.phy.self_powered = true;
        cfg.phy.vbus_monitor_io = BOARD_VBUS_SENSE_GPIO;
    }
    memcpy(net.mac_addr, mac, 6);
    e = drive_start();
    if (e != ESP_OK)                     /* the page on a web site still works: the network and MIDI go on without it */
        ESP_LOGE(TAG, "drive: %s", esp_err_to_name(e));
    /* The order matters: tinyusb_driver_install's task overwrites every string descriptor (tinyusb_descriptors_set) and
     * only then returns, so a MAC set earlier is lost and the computer reads an empty one. The host reads strings well
     * after this returns. */
    e = tinyusb_driver_install(&cfg);
    if (e == ESP_OK)
        e = tinyusb_net_init(&net);
    if (e == ESP_OK && xTaskCreatePinnedToCore(midi_task, "usb_midi", 4096, NULL, MIDI_TASK_PRIO, &s_task, MIDI_TASK_CORE) != pdPASS)
        e = ESP_ERR_NO_MEM;
    if (e == ESP_OK)
        e = usbrole_start();             /* the XIAO's HOST: no computer within 3 s -> a restart as host */
    if (e != ESP_OK)
        ESP_LOGE(TAG, "start: %s", esp_err_to_name(e));
    return e;
}

int usb_up(void) { return !s_host && tud_mounted(); }

int usb_host_spoke(void) { return s_host_spoke; }
int usb_is_host(void) { return s_host; }

/* from the beat task; never waits: a report that does not fit is dropped (the next one comes within a second) */
void usb_beat_line(const char *text, size_t n)
{
    if (s_host || !s_push_out || !tud_midi_mounted() || n == 0 || n > JX_PUSH_TEXT_MAX)
        return;
    if (xMessageBufferSend(s_push_out, text, n, 0) == n)
        xTaskNotifyGive(s_task);
}

esp_err_t usb_send(const uint8_t *eth, size_t len, void *token)
{
    return tinyusb_net_send_async((void *)eth, (uint16_t)len, token);
}

void usb_serial_write(const char *text, size_t n)
{
    if (s_host || !tud_midi_mounted())
        return;                          /* nobody listening: nothing to wait for */
    xSemaphoreTake(s_text_lock, portMAX_DELAY);
    xStreamBufferSend(s_text_out, text, n, pdMS_TO_TICKS(50));
    xSemaphoreGive(s_text_lock);
    xTaskNotifyGive(s_task);
}
