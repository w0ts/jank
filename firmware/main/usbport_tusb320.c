/* SPDX-License-Identifier: GPL-3.0-only */
/* The port HAL (usbport.h) of jank mini 2.2, battery order: the TUSB320LI over I2C (core/jank_tusb320.h, SLLSEP2D), its
 * INT_N, and the host's 5 V "signal": the CLK / RST charge pump (pump.c) feeds U16 HT7550 -> V5H, and HOST_EN (GPIO42,
 * Q8 -> Q7) puts it through R93 2.2 k onto VBUS while Q9 cuts the board's own VBUS off (hardware/mini2 README, "USB host
 * mode"). The usb order has no U17: open() answers 0 and the port is device only. */
#include "board.h"
#include "usbport.h"
#if BOARD_USBPORT_TUSB320
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_i2c.h"
#include "jank_tusb320.h"
#include "jank_usbrole.h"
#include "usb_host_mode.h"

#define I2C_HZ 100000u                 /* the TUSB320 takes up to 400 kHz (SLLSEP2D 6.6, fSCL) */
#define I2C_TIMEOUT_MS 20
#define PUMP_PREP_MS 10u               /* the README: start the pump, wait ~10 ms */

static const char *TAG = "usbport";

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

static const struct jt_io IO = {NULL, cc_read, cc_write, cc_delay};

static void vbus_on(int on) { gpio_set_level(BOARD_HOST_EN_GPIO, on ? 1 : 0); }

/* HOST_EN driven low before anything else (R91 holds it low until then); then the chip on the board's I2C bus, shared
 * with the screen (board_i2c.c: SCL / SDA, R60 / R61) */
static int open_port(void)
{
    gpio_config_t out = {.pin_bit_mask = 1ull << BOARD_HOST_EN_GPIO, .mode = GPIO_MODE_OUTPUT};
    i2c_master_bus_handle_t bus = NULL;
    i2c_device_config_t dc = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = JT_ADDR, .scl_speed_hz = I2C_HZ};
    gpio_config_t in = {.pin_bit_mask = 1ull << BOARD_CC_INT_GPIO, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    esp_err_t e;
    gpio_set_level(BOARD_HOST_EN_GPIO, 0);
    e = gpio_config(&out);
    gpio_set_level(BOARD_HOST_EN_GPIO, 0);
    if (e == ESP_OK)
        e = gpio_config(&in);          /* INT_N: open drain, its pull-up (usbrole.c adds the edge interrupt) */
    if (e == ESP_OK)
        e = board_i2c_bus(&bus);
    if (e == ESP_OK)
        e = i2c_master_bus_add_device(bus, &dc, &s_dev);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "pins / I2C: %s", esp_err_to_name(e));
        return 0;
    }
    if (jt_probe(&IO) != 0) {
        ESP_LOGW(TAG, "no TUSB320 at 0x%02x: the usb order, device only", JT_ADDR);
        return 0;
    }
    return 1;
}

static int read_port(struct usbport_state *st)
{
    static const uint8_t MAP[4] = {JUR_CC_NONE, JUR_CC_DEVICE, JUR_CC_HOST, JUR_CC_ACCESSORY};   /* SLLSEP2D Table 9 */
    struct jt_status t;
    if (jt_read(&IO, &t) == -1)        /* -2 (a flag that would not clear) still gave the state */
        return -1;
    st->cc = MAP[t.attached & 3u];
    st->dir = t.cable_dir;
    return 0;
}

static int set_mode(uint8_t mode)
{
    if (mode == JUR_PORT_SINK)
        return jt_set_mode(&IO, JT_MODE_UFP, JT_PREF_STANDARD);
    return jt_set_mode(&IO, JT_MODE_DRP, JT_PREF_TRY_SNK);   /* jur_port_mode: the sink against another dual role */
}

static esp_err_t vbus_prep(int on)
{
    if (on)
        return pump_hold();
    pump_release();
    return ESP_OK;
}

static const struct usbport PORT = {
    .name = "TUSB320LI",
    .prep_ms = PUMP_PREP_MS,
    .irq_gpio = BOARD_CC_INT_GPIO,
    .open = open_port,
    .read = read_port,
    .set_mode = set_mode,
    .vbus_prep = vbus_prep,
    .vbus_on = vbus_on,
};

const struct usbport *usbport_board(void) { return &PORT; }
#else
const struct usbport *usbport_board(void) { return NULL; }
#endif
