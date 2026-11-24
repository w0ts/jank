/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB port's role: core/jank_usbrole.h decides, the board's port HAL (usbport.h; jank mini 2.2: usbport_tusb320.c)
 * does it. Here: the setting USB DEVICE / HOST, VBUS sense, the role task, the restart into another stack, and the audio
 * source (AUDIO > SOURCE with the line in's plug detect). With no port chip (the XIAO) only the setting and "did a
 * computer configure us" exist.
 *
 * Order of things, every step:
 *   1. VBUS out off first whenever the core says so: jank's 5 V leaves VBUS before anything else changes;
 *   2. the 5 V source prepared while the core wants it, released after VBUS out is off;
 *   3. VBUS out on only when the core says so (a device attached, the host stack running, the source up, VBUS not
 *      powered from outside);
 *   4. the port's mode, when the core allows the write (it drops an attachment);
 *   5. a restart into the other stack: VBUS out off, the source released, the wanted stack in RTC memory (usb.c reads it
 *      through usbrole_boot at the next start).
 * The port chip's interrupt line wakes the task; it also polls every POLL_PORT_MS in case an edge was missed. */
#include <string.h>
#include "app.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jank_usbrole.h"
#include "tusb.h"
#include "usb_audio.h"
#include "usb_host_mode.h"
#include "usbport.h"

#define STEP_MS 20u                    /* the role task's period: VBUS out follows a detach within this plus the chip's read */
#define POLL_PORT_MS 500u
#define ROLE_PRIO 6                    /* over the beat tracker (3) and the control task (4): VBUS out must not wait for them */
#define HINT_MAGIC 0x4A4B5552u         /* RTC memory across one restart: garbage after power-on is not this */

#define BLOCK_MAGIC 0x4A4B4846u        /* a host start failed in this power session (RTC memory across restarts; garbage after power-on is not this) */

static const char *TAG = "usbrole";
static RTC_NOINIT_ATTR uint32_t s_hint_magic, s_hint;
static RTC_NOINIT_ATTR uint32_t s_host_block;
static RTC_NOINIT_ATTR uint32_t s_host_inflight;   /* INFLIGHT_MAGIC while a host run is young (see find_host_crash) */

/* A host start failed (a task or buffer that did not fit, the Wi-Fi radio after it): the reason goes to flash, the host stack is off
 * until the next power cycle, and the unit restarts as a device, so it stays reachable (Wi-Fi, USB) and `diag` says what failed.
 * Without this the failure aborts the whole start, again at every boot, and nothing can say why. */
void usbrole_host_failed(uint8_t step, esp_err_t e)
{
    hostdiag_fail(step, e);
    s_host_block = BLOCK_MAGIC;
    s_host_inflight = 0;               /* this restart is ours, not a crash */
    ESP_LOGE("usbrole", "host start failed at step %u: %s: restarting as a device", step, esp_err_to_name(e));
    restart_why_set("host-failed");
    esp_restart();
}

static uint8_t host_blocked(void) { return s_host_block == BLOCK_MAGIC; }

/* A host run is "in flight" until it has lasted HOST_STABLE (usb.c's guard clears it). If the next start finds it still set, with a reset
 * nothing of ours asks for (a panic, a watchdog, a brownout, a CPU lockup: not a restart, not a power-on), the run crashed: the reset
 * reason is kept in flash as the failure (hostdiag_start= in diag), host is blocked until a power cycle, and the unit comes up as a
 * device, reachable. A crash loop in host mode otherwise leaves nothing to read. */
#define INFLIGHT_MAGIC 0x4A4B4946u

void usbrole_host_inflight(int on) { s_host_inflight = on ? INFLIGHT_MAGIC : 0; }

static void find_host_crash(void)
{
    esp_reset_reason_t r;
    if (s_host_inflight != INFLIGHT_MAGIC)
        return;
    s_host_inflight = 0;
    r = esp_reset_reason();
    if (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT || r == ESP_RST_BROWNOUT || r == ESP_RST_PWR_GLITCH
        || r == ESP_RST_CPU_LOCKUP) {
        hostdiag_fail(JHD_STEP_CRASH, (esp_err_t)r);
        s_host_block = BLOCK_MAGIC;
        ESP_LOGE("usbrole", "the last host run ended in a reset (%d): host is off until a power cycle", (int)r);
    }
}
static const struct usbport *s_port;   /* NULL: no port chip, or it did not answer */
static TaskHandle_t s_task;
static uint8_t s_stack, s_mode = 0xFF, s_prepared, s_prep_failed, s_computer;
static struct jur s_role;
static struct jur_line s_line;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static struct jk_usb s_pub;            /* under s_mux: what usbrole_snapshot gives */

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static int pin_level(int pin) { return pin >= 0 ? gpio_get_level((gpio_num_t)pin) : 0; }

static void IRAM_ATTR on_port_irq(void *arg)
{
    BaseType_t woken = pdFALSE;
    (void)arg;
    if (s_task)
        vTaskNotifyGiveFromISR(s_task, &woken);
    portYIELD_FROM_ISR(woken);
}

static esp_err_t input(int pin, int pull_up)
{
    gpio_config_t in = {.pin_bit_mask = 1ull << pin, .mode = GPIO_MODE_INPUT, .pull_up_en = pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE};
    return pin >= 0 ? gpio_config(&in) : ESP_OK;
}

/* VBUS sense (R88 / R89 on mini 2.2) and the plug detect (R38 1 M up): plain inputs; the GPIO interrupt service for the
 * port chip's line and for esp_tinyusb's VBUS monitor (tinyusb_vbus_monitor.c asks the app for it) */
static esp_err_t pins_up(void)
{
    esp_err_t e = input(BOARD_VBUS_SENSE_GPIO, 0);
    if (e == ESP_OK)
        e = input(BOARD_LINE_DETECT_GPIO, 0);
    if (e == ESP_OK && (BOARD_VBUS_SENSE_GPIO >= 0 || (s_port && s_port->irq_gpio >= 0))) {
        e = gpio_install_isr_service(0);
        if (e == ESP_ERR_INVALID_STATE)
            e = ESP_OK;                /* already installed */
    }
    if (e == ESP_OK && s_port && s_port->irq_gpio >= 0) {
        e = gpio_set_intr_type((gpio_num_t)s_port->irq_gpio, GPIO_INTR_NEGEDGE);
        if (e == ESP_OK)
            e = gpio_isr_handler_add((gpio_num_t)s_port->irq_gpio, on_port_irq, NULL);
        if (e == ESP_OK)
            e = gpio_intr_enable((gpio_num_t)s_port->irq_gpio);
    }
    return e;
}

uint8_t usbrole_boot(void)
{
    struct jk_cfg cfg;
    struct jur_boot b;
    struct usbport_state st = {JUR_CC_NONE, 0};
    esp_err_t e;
    const struct usbport *p = usbport_board();
    find_host_crash();                     /* first: it can block host for this start */
    s_port = p && p->open() ? p : NULL;    /* open() drives the port's VBUS out off first */
    e = pins_up();
    if (e != ESP_OK)
        ESP_LOGE(TAG, "pins: %s", esp_err_to_name(e));
    app_cfg_get(&cfg);
    memset(&b, 0, sizeof b);
    b.has_port = (uint8_t)(s_port != NULL);
    b.setting = host_blocked() ? JUR_USB_DEVICE : jur_effective_setting(cfg.usb, cfg.usb_set, b.has_port);   /* a XIAO with no choice made acts as HOST; a failed host start keeps it a device */
    if (s_port && s_port->read(&st) == 0)
        b.cc = st.cc;
    b.vbus = (uint8_t)pin_level(BOARD_VBUS_SENSE_GPIO);
    b.hint = s_hint_magic == HINT_MAGIC && s_hint <= 2u ? (uint8_t)s_hint : 0u;
    s_hint_magic = 0;
    s_stack = jur_boot_stack(&b);
    /* the sink before a device stack starts when the setting says DEVICE (a dual role left from a host start) */
    if (s_port && cfg.usb == JUR_USB_DEVICE && s_port->set_mode(JUR_PORT_SINK) >= 0)
        s_mode = JUR_PORT_SINK;
    ESP_LOGI(TAG, "start as %s (setting %s, port %s, cable %s, VBUS %u, hint %u)", jur_usb_name(s_stack), jur_usb_name(cfg.usb),
             s_port ? s_port->name : "none", jur_cc_name(b.cc), b.vbus, b.hint);
    return s_stack;
}

static void vbus_out(int on)
{
    if (s_port)
        s_port->vbus_on(on);
}

/* An update's restart (ota.c): a unit that runs as host comes back as host at once. A XIAO otherwise starts as a device and restarts into host after
 * 3 s with no computer (jur_boot_stack), and a restart while the new image is on probation puts the old one back: measured 2026-11-24, the first
 * Wi-Fi update of a unit in host mode was rolled back that way, before it could be confirmed (which takes 20 s). */
void usbrole_hint_stay(void)
{
    if (s_stack != JUR_STACK_HOST)
        return;
    s_hint = 1u + JUR_STACK_HOST;
    s_hint_magic = HINT_MAGIC;
    s_host_inflight = 0;               /* a restart of ours, not a crash */
}

static void restart_into(uint8_t stack)
{
    vbus_out(0);
    if (s_prepared)
        s_port->vbus_prep(0);
    s_hint = 1u + stack;
    s_hint_magic = HINT_MAGIC;
    s_host_inflight = 0;               /* a restart of ours (a computer came, or went): not a crash */
    ESP_LOGW(TAG, "restarting as %s", jur_usb_name(stack));
    vTaskDelay(pdMS_TO_TICKS(50));     /* the log out */
    restart_why_set("role-switch");
    esp_restart();
}

static void apply(const struct jur_out *o)
{
    if (!o->vbus_on)
        vbus_out(0);
    s_prep_failed = 0;
    if (s_port && o->vbus_prep && !s_prepared) {
        s_prepared = s_port->vbus_prep(1) == ESP_OK;
        s_prep_failed = !s_prepared;   /* the core holds VBUS off and restarts the source's time */
    }
    else if (s_port && !o->vbus_prep && s_prepared) {
        s_port->vbus_prep(0);
        s_prepared = 0;
    }
    if (o->vbus_on && s_prepared)
        vbus_out(1);
    if (s_port && o->port_write && o->port_mode != s_mode) {
        if (s_port->set_mode(o->port_mode) >= 0)
            s_mode = o->port_mode;
        else
            ESP_LOGW(TAG, "port mode not written");
    }
    if (o->restart)
        restart_into(o->want);
}

static void publish(const struct jur_in *in, const struct jur_out *o, uint8_t dir, uint8_t audio)
{
    portENTER_CRITICAL(&s_mux);
    s_pub.stack = s_stack;
    s_pub.has_port = (uint8_t)(s_port != NULL);
    s_pub.cc = in->cc;
    s_pub.cc_dir = dir;
    s_pub.vbus_on = (uint8_t)(o->vbus_on && s_prepared);
    s_pub.vbus = in->vbus;
    s_pub.line = s_line.level;
    s_pub.audio = audio;
    portEXIT_CRITICAL(&s_mux);
}

/* beat_latency per audio source (core/jank_lat.h): another source heard -> its value becomes the live one */
static void follow_latency(const struct jk_cfg *seen, uint8_t audio)
{
    struct jla_key k;
    uint16_t vid = 0, pid = 0;
    int dev = s_stack == JUR_STACK_HOST && usb_uac_host_id(&vid, &pid);
    struct jk_cfg cfg;
    if (!jur_lat_key(audio, s_stack, dev, vid, pid, &k) || jla_same_key(&seen->lat_key, &k))
        return;
    app_cfg_get(&cfg);                 /* (fresh: the settings may have changed since `seen`) */
    if (jk_lat_select(&cfg, &k)) {
        char name[16];
        ESP_LOGI(TAG, "beat_latency %d ms: %s's", cfg.beat_latency_ms, jla_name(&k, name));
        app_cfg_set(&cfg);
    }
}

static void role_task(void *arg)
{
    struct usbport_state st = {JUR_CC_NONE, 0};
    uint32_t last_read = 0;
    uint8_t routed = 0xFF;
    (void)arg;
    for (;;) {
        struct jk_cfg cfg;
        struct jur_in in;
        struct jur_out o;
        uint8_t audio;
        uint32_t t;
        int woke = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(STEP_MS)) != 0;
        t = now_ms();
        app_cfg_get(&cfg);
        if (s_port && (woke || (s_port->irq_gpio >= 0 && pin_level(s_port->irq_gpio) == 0) || t - last_read >= (s_port->poll_ms ? s_port->poll_ms : POLL_PORT_MS))) {
            last_read = t;
            if (s_port->read(&st) != 0) {
                st.cc = JUR_CC_NONE;   /* the chip unreadable: no 5 V on a guess */
                st.dir = 0;
            }
        }
        if (s_stack == JUR_STACK_DEVICE && tud_connected())
            s_computer = 1;            /* a computer's first SETUP (sticky: the XIAO's HOST setting stays a device then) */
        memset(&in, 0, sizeof in);
        in.has_port = (uint8_t)(s_port != NULL);
        in.setting = host_blocked() ? JUR_USB_DEVICE : jur_effective_setting(cfg.usb, cfg.usb_set, in.has_port);
        in.cc = st.cc;
        in.vbus = (uint8_t)pin_level(BOARD_VBUS_SENSE_GPIO);
        in.computer = s_computer;
        in.now_ms = t;
        in.prep_failed = s_prep_failed;
        jur_step(&s_role, &in, &o);
        apply(&o);
        jur_line_step(&s_line, pin_level(BOARD_LINE_DETECT_GPIO), t);
        audio = jur_source(cfg.source, s_line.level, uac_stream_open());
        if (audio != routed) {
            routed = audio;
            uac_source(audio);         /* LINE: linein.c's samples (a board without LINE IN: nothing comes) */
        }
        publish(&in, &o, st.dir, audio);
        follow_latency(&cfg, audio);
    }
}

esp_err_t usbrole_start(void)
{
    uint32_t t = now_ms();
    jur_init(&s_role, s_stack, s_port ? s_port->prep_ms : 0u, t);
    jur_line_init(&s_line, pin_level(BOARD_LINE_DETECT_GPIO), t);
    return xTaskCreatePinnedToCore(role_task, "usbrole", 4096, NULL, ROLE_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void usbrole_snapshot(struct jk_usb *u)
{
    portENTER_CRITICAL(&s_mux);
    *u = s_pub;
    portEXIT_CRITICAL(&s_mux);
    u->ndev = 0;
    if (s_stack == JUR_STACK_HOST) {
        usb_midi_host_list(u);
        usb_uac_host_list(u);
    }
}
