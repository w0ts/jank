/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB port's role on the board (core/jank_usbrole.h decides; this drives the pins): the setting USB DEVICE / HOST, the
 * TUSB320LI CC chip over I2C (core/jank_tusb320.h), its INT_N, HOST_EN, VBUS_SENSE, the charge pump, and the audio source
 * (AUDIO > SOURCE with the line in's plug detect). On the XIAO only the setting and "did a computer configure us" exist.
 *
 * Order of things, every step:
 *   1. HOST_EN low first whenever the core says so: the 5 V signal leaves VBUS before anything else changes;
 *   2. the pump: held while the core wants it, released after HOST_EN is low;
 *   3. HOST_EN high only when the core says so (a device attached, the host stack running, the pump's rail up, VBUS not
 *      powered from outside);
 *   4. the CC chip's mode, when the core allows the write (it drops an attachment);
 *   5. a restart into the other stack: HOST_EN low, the pump released, the wanted stack in RTC memory (usb.c reads it
 *      through usbrole_boot at the next start).
 * The chip's INT_N (open drain, low while INTERRUPT_STATUS is set, SLLSEP2D 7.2.4) wakes the task; it also polls every
 * POLL_CC_MS in case an edge was missed. */
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
#include "jank_tusb320.h"
#include "jank_usbrole.h"
#include "tusb.h"
#include "usb_audio.h"
#include "usb_host_mode.h"
#if BOARD_CC_SDA_GPIO >= 0
#include "driver/i2c_master.h"
#endif

#define STEP_MS 20u                    /* the role task's period: HOST_EN follows a detach within this plus the I2C read */
#define POLL_CC_MS 500u
#define I2C_HZ 100000u                 /* the TUSB320 takes up to 400 kHz (SLLSEP2D 6.6, fSCL) */
#define I2C_TIMEOUT_MS 20
#define ROLE_PRIO 6                    /* over the beat tracker (3) and the control task (4): HOST_EN must not wait for them */
#define HINT_MAGIC 0x4A4B5552u         /* RTC memory across one restart: garbage after power-on is not this */

static const char *TAG = "usbrole";
static RTC_NOINIT_ATTR uint32_t s_hint_magic, s_hint;
static TaskHandle_t s_task;
static uint8_t s_stack, s_has_cc, s_cc_mode = 0xFF, s_pump_held, s_computer;
static struct jur s_role;
static struct jur_line s_line;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static struct jk_usb s_pub;            /* under s_mux: what usbrole_snapshot gives */

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static int pin_level(int pin) { return pin >= 0 ? gpio_get_level((gpio_num_t)pin) : 0; }

static void host_en(int on)
{
#if BOARD_HOST_EN_GPIO >= 0
    gpio_set_level(BOARD_HOST_EN_GPIO, on ? 1 : 0);
#else
    (void)on;
#endif
}

/* ---- the CC chip over I2C ---- */
#if BOARD_CC_SDA_GPIO >= 0
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

/* SLLSEP2D 7.4: a write of the sub-address, a stop, then the read */
static int cc_read(void *ctx, uint8_t reg, uint8_t *buf, size_t n)
{
    (void)ctx;
    if (i2c_master_transmit(s_dev, &reg, 1, I2C_TIMEOUT_MS) != ESP_OK)
        return -1;
    return i2c_master_receive(s_dev, buf, n, I2C_TIMEOUT_MS) == ESP_OK ? 0 : -1;
}

static int cc_write(void *ctx, uint8_t reg, uint8_t v)
{
    uint8_t b[2] = {reg, v};
    (void)ctx;
    return i2c_master_transmit(s_dev, b, sizeof b, I2C_TIMEOUT_MS) == ESP_OK ? 0 : -1;
}

static void cc_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    vTaskDelay(pdMS_TO_TICKS(ms) + 1u);
}

static const struct jt_io CC_IO = {NULL, cc_read, cc_write, cc_delay};

/* the I2C bus is shared with the screen later (SCL / SDA, R60 / R61): the integrator moves the bus to whoever owns it */
static int cc_open(void)
{
    i2c_master_bus_config_t bc = {
        .i2c_port = -1,
        .sda_io_num = BOARD_CC_SDA_GPIO,
        .scl_io_num = BOARD_CC_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_device_config_t dc = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = JT_ADDR, .scl_speed_hz = I2C_HZ};
    esp_err_t e = i2c_new_master_bus(&bc, &s_bus);
    if (e == ESP_OK)
        e = i2c_master_bus_add_device(s_bus, &dc, &s_dev);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "I2C: %s", esp_err_to_name(e));
        return 0;
    }
    if (jt_probe(&CC_IO) != 0) {
        ESP_LOGW(TAG, "no TUSB320 at 0x%02x: the usb order, device only", JT_ADDR);
        return 0;
    }
    return 1;
}

static int cc_status(struct jt_status *st)
{
    return jt_read(&CC_IO, st) == -1 ? -1 : 0;     /* a flag that would not clear still gave the state */
}

static int cc_set_mode(uint8_t mode)
{
    return jt_set_mode(&CC_IO, mode == JUR_CCMODE_UFP ? JT_MODE_UFP : JT_MODE_DRP, mode == JUR_CCMODE_UFP ? JT_PREF_STANDARD : JT_PREF_TRY_SRC);
}
#else
static int cc_open(void) { return 0; }
static int cc_status(struct jt_status *st)
{
    (void)st;
    return -1;
}
static int cc_set_mode(uint8_t mode)
{
    (void)mode;
    return -1;
}
#endif

static uint8_t cc_of(const struct jt_status *st)
{
    static const uint8_t MAP[4] = {JUR_CC_NONE, JUR_CC_DEVICE, JUR_CC_HOST, JUR_CC_ACCESSORY};   /* SLLSEP2D Table 9 */
    return MAP[st->attached & 3u];
}

#if BOARD_CC_INT_GPIO >= 0
static void IRAM_ATTR on_cc_int(void *arg)
{
    BaseType_t woken = pdFALSE;
    (void)arg;
    if (s_task)
        vTaskNotifyGiveFromISR(s_task, &woken);
    portYIELD_FROM_ISR(woken);
}
#endif

static esp_err_t pins_up(void)
{
    esp_err_t e = ESP_OK;
#if BOARD_HOST_EN_GPIO >= 0
    gpio_config_t out = {.pin_bit_mask = 1ull << BOARD_HOST_EN_GPIO, .mode = GPIO_MODE_OUTPUT};
    gpio_set_level(BOARD_HOST_EN_GPIO, 0);      /* low before it drives (R91 holds it low until then) */
    e = gpio_config(&out);
    gpio_set_level(BOARD_HOST_EN_GPIO, 0);
#endif
#if BOARD_VBUS_SENSE_GPIO >= 0
    if (e == ESP_OK) {
        gpio_config_t in = {.pin_bit_mask = 1ull << BOARD_VBUS_SENSE_GPIO, .mode = GPIO_MODE_INPUT};   /* R88 / R89 */
        e = gpio_config(&in);
    }
#endif
#if BOARD_LINE_DETECT_GPIO >= 0
    if (e == ESP_OK) {
        gpio_config_t in = {.pin_bit_mask = 1ull << BOARD_LINE_DETECT_GPIO, .mode = GPIO_MODE_INPUT};  /* R38 1 M up */
        e = gpio_config(&in);
    }
#endif
#if BOARD_CC_INT_GPIO >= 0 || BOARD_VBUS_SENSE_GPIO >= 0
    if (e == ESP_OK) {
        /* the GPIO interrupt service: INT_N here, and esp_tinyusb's VBUS monitor (tinyusb_vbus_monitor.c asks the app for
         * it); already installed is fine */
        e = gpio_install_isr_service(0);
        if (e == ESP_ERR_INVALID_STATE)
            e = ESP_OK;
    }
#endif
#if BOARD_CC_INT_GPIO >= 0
    if (e == ESP_OK) {
        gpio_config_t in = {.pin_bit_mask = 1ull << BOARD_CC_INT_GPIO, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_NEGEDGE};
        e = gpio_config(&in);
        if (e == ESP_OK)
            e = gpio_isr_handler_add(BOARD_CC_INT_GPIO, on_cc_int, NULL);
    }
#endif
    return e;
}

uint8_t usbrole_boot(void)
{
    struct jk_cfg cfg;
    struct jur_boot b;
    struct jt_status st = {0};
    esp_err_t e = pins_up();
    if (e != ESP_OK)
        ESP_LOGE(TAG, "pins: %s", esp_err_to_name(e));
    app_cfg_get(&cfg);
    s_has_cc = (uint8_t)cc_open();
    memset(&b, 0, sizeof b);
    b.setting = cfg.usb;
    b.has_cc = s_has_cc;
    if (s_has_cc && cc_status(&st) == 0)
        b.cc = cc_of(&st);
    b.vbus = (uint8_t)pin_level(BOARD_VBUS_SENSE_GPIO);
    b.hint = s_hint_magic == HINT_MAGIC && s_hint <= 2u ? (uint8_t)s_hint : 0u;
    s_hint_magic = 0;
    s_stack = jur_boot_stack(&b);
    /* the sink before a device stack starts when the setting says DEVICE (a dual role left from a host start) */
    if (s_has_cc && cfg.usb == JUR_USB_DEVICE && cc_set_mode(JUR_CCMODE_UFP) >= 0)
        s_cc_mode = JUR_CCMODE_UFP;
    ESP_LOGI(TAG, "start as %s (setting %s, CC %s, VBUS %u, hint %u)", jur_usb_name(s_stack), jur_usb_name(cfg.usb),
             s_has_cc ? jur_cc_name(b.cc) : "no chip", b.vbus, b.hint);
    return s_stack;
}

static void restart_into(uint8_t stack)
{
    host_en(0);
    if (s_pump_held)
        pump_release();
    s_hint = 1u + stack;
    s_hint_magic = HINT_MAGIC;
    ESP_LOGW(TAG, "restarting as %s", jur_usb_name(stack));
    vTaskDelay(pdMS_TO_TICKS(50));     /* the log out */
    esp_restart();
}

static void apply(const struct jur_out *o)
{
    if (!o->host_en)
        host_en(0);
    if (o->pump && !s_pump_held)
        s_pump_held = pump_hold() == ESP_OK;
    else if (!o->pump && s_pump_held) {
        pump_release();
        s_pump_held = 0;
    }
    if (o->host_en && s_pump_held)
        host_en(1);
    if (o->cc_write && o->cc_mode != s_cc_mode) {
        int r = cc_set_mode(o->cc_mode);
        if (r >= 0)
            s_cc_mode = o->cc_mode;
        else
            ESP_LOGW(TAG, "CC mode not written");
    }
    if (o->restart)
        restart_into(o->want);
}

static void publish(const struct jur_in *in, const struct jur_out *o, uint8_t cc_dir, uint8_t audio)
{
    portENTER_CRITICAL(&s_mux);
    s_pub.stack = s_stack;
    s_pub.has_cc = s_has_cc;
    s_pub.cc = in->cc;
    s_pub.cc_dir = cc_dir;
    s_pub.host_en = (uint8_t)(o->host_en && s_pump_held);
    s_pub.vbus = in->vbus;
    s_pub.line = s_line.level;
    s_pub.audio = audio;
    portEXIT_CRITICAL(&s_mux);
}

static void role_task(void *arg)
{
    struct jt_status st = {0};
    uint32_t last_cc = 0;
    uint8_t cc = JUR_CC_NONE, cc_dir = 0, routed = 0xFF;
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
        if (s_has_cc && (woke || pin_level(BOARD_CC_INT_GPIO) == 0 || t - last_cc >= POLL_CC_MS)) {
            last_cc = t;
            if (cc_status(&st) == 0) {
                cc = cc_of(&st);
                cc_dir = st.cable_dir;
            } else {
                cc = JUR_CC_NONE;      /* the chip unreadable: no 5 V on a guess */
                cc_dir = 0;
            }
        }
        if (s_stack == JUR_STACK_DEVICE && tud_connected())
            s_computer = 1;            /* a computer's first SETUP (sticky: the XIAO's HOST setting stays a device then) */
        memset(&in, 0, sizeof in);
        in.setting = cfg.usb;
        in.has_cc = s_has_cc;
        in.cc = cc;
        in.vbus = (uint8_t)pin_level(BOARD_VBUS_SENSE_GPIO);
        in.computer = s_computer;
        in.now_ms = t;
        jur_step(&s_role, &in, &o);
        apply(&o);
        jur_line_step(&s_line, pin_level(BOARD_LINE_DETECT_GPIO), t);
        audio = jur_source(cfg.source, s_line.level, uac_stream_open());
        if (audio != routed) {
            routed = audio;
            uac_route(audio == JUR_AUDIO_USB);   /* the line in's own path is not on this branch: LINE hears nothing yet */
        }
        publish(&in, &o, cc_dir, audio);
    }
}

esp_err_t usbrole_start(void)
{
    uint32_t t = now_ms();
    jur_init(&s_role, s_stack, t);
    jur_line_init(&s_line, pin_level(BOARD_LINE_DETECT_GPIO), t);
    return xTaskCreatePinnedToCore(role_task, "usbrole", 3072, NULL, ROLE_PRIO, &s_task, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
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
