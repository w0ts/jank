/* SPDX-License-Identifier: GPL-3.0-only */
/* The port HAL (usbport.h) of the jank mini 3 (lcd2 board): no CC chip. J4 (TO DEVICE) is the host-side receptacle; the
 * LCD-2's own USB-C (VBUS sense, GPIO10) is where a computer or a charger goes. Wiring (hardware/jank-mini-3/parts.py):
 *   HOST_EN (GPIO17, R91 100 k down) -> U21 pin 11 -> two inverters (74AHCT04 on V5C) -> HOST5G -> JP13 -> HOST5
 *   HOST5 -> R93 680 -> D16 -> VUSB (J4's VBUS pads A4 / B9, C28 1 u), and HOST5 -> R160 / R161 56 k -> CC2O / CC1O (Rp).
 * So HOST_EN high = a 680 ohm "5 V present" signal and Rp on J4: no power switch, no power (a short on J4's VBUS draws ~6.6 mA),
 * and it needs V5_EN (the 5 V boost, driven high by power_early_init) for U21's supply. J1's VBUS and CC are open.
 * The rules are core/jank_usbhost.h's; here they meet the pins:
 *   - open(): HOST_EN low first, VBUS sense an input, a shutdown handler that drives HOST_EN low before any restart;
 *   - read(): every role step (20 ms): sample VBUS sense, step the gate, drive HOST_EN from its answer, report "a computer or a
 *     charger" (cc host: the role core goes to the device stack) or "nothing there" (cc device: the host stack may run, gear
 *     on J4 cannot be seen without a CC chip, so the signal is the same with or without it);
 *   - vbus_on(): the role core's ask, which the gate may refuse; vbus_on(0) is HOST_EN low at once.
 * A pin that reads wrong: VBUS sense stuck high or floating high: device only, HOST_EN stays low. Stuck low (a wrong pin map)
 * with a computer on the LCD-2's USB-C: HOST_EN can go high; it is a 680 ohm signal into J4, harmless there. The V5_EN pin is
 * not readable back (an output), so the gate trusts power_early_init and refuses when the board file has no V5_EN. */
#include "board.h"
#include "usbport.h"
#include "app.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "jank_usbhost.h"
#include "jank_usbrole.h"

#define STEP_MS 20u                    /* the role task's step (usbrole.c): VBUS sense is read this often */

static const char *TAG = "usbport";

static struct juh s_gate;
static uint8_t s_want_en;              /* the role core's ask (vbus_on) */
static uint8_t s_v5_known;
static uint32_t s_v5_since_ms;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void host_en(int level) { gpio_set_level((gpio_num_t)BOARD_HOST_EN_GPIO, level ? 1 : 0); }

static void shutdown_low(void) { host_en(0); }

/* sample, gate, drive: the only place HOST_EN goes high */
static void gate_step(struct juh_out *o)
{
    struct jk_cfg cfg;
    struct juh_in in;
    app_cfg_get(&cfg);
    in.setting_host = (uint8_t)(cfg.usb == JUR_USB_HOST);
    in.vbus_raw = (uint8_t)(gpio_get_level((gpio_num_t)BOARD_VBUS_SENSE_GPIO) != 0);
    in.want_en = s_want_en;
    in.v5_known = s_v5_known;
    in.v5_since_ms = s_v5_since_ms;
    in.now_ms = now_ms();
    juh_step(&s_gate, &in, o);
    host_en(o->host_en);
}

/* HOST_EN low before anything else (power_early_init did already; R91 holds it low until then), VBUS sense an input (usbrole.c
 * sets it up again, no pull: the board has its divider). No pins in board.h: no port. */
static int open_port(void)
{
    gpio_config_t out = {.pin_bit_mask = 1ull << BOARD_HOST_EN_GPIO, .mode = GPIO_MODE_OUTPUT};
    gpio_config_t in = {.pin_bit_mask = 1ull << BOARD_VBUS_SENSE_GPIO, .mode = GPIO_MODE_INPUT};
    esp_err_t e;
    if (BOARD_HOST_EN_GPIO < 0 || BOARD_VBUS_SENSE_GPIO < 0)
        return 0;
    host_en(0);
    e = gpio_config(&out);
    host_en(0);
    if (e == ESP_OK)
        e = gpio_config(&in);
    if (e == ESP_OK)
        e = esp_register_shutdown_handler(shutdown_low);   /* every esp_restart(): HOST_EN low first */
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "pins: %s", esp_err_to_name(e));
        return 0;
    }
    s_v5_known = (uint8_t)(BOARD_V5_EN_GPIO >= 0);         /* driven high by power_early_init, before this */
    s_v5_since_ms = now_ms();                              /* counted from here: later than the truth */
    juh_init(&s_gate, gpio_get_level((gpio_num_t)BOARD_VBUS_SENSE_GPIO), now_ms());
    return 1;
}

static int read_port(struct usbport_state *st)
{
    struct juh_out o;
    gate_step(&o);
    st->cc = o.vbus ? JUR_CC_HOST : JUR_CC_DEVICE;
    st->dir = 0;
    return 0;
}

static int set_mode(uint8_t mode)
{
    (void)mode;
    return 0;                          /* no chip: nothing to write */
}

static esp_err_t vbus_prep(int on)
{
    return on && !s_v5_known ? ESP_ERR_INVALID_STATE : ESP_OK;   /* the 5 V rail is on already; the gate waits its time */
}

static void vbus_on(int on)
{
    struct juh_out o;
    s_want_en = (uint8_t)(on != 0);
    if (on)
        gate_step(&o);                 /* the gate may refuse */
    else
        host_en(0);                    /* at once */
}

static const struct usbport PORT = {
    .name = "VBUS sense",
    .prep_ms = JUH_V5_SETTLE_MS,
    .irq_gpio = -1,
    .poll_ms = STEP_MS,
    .open = open_port,
    .read = read_port,
    .set_mode = set_mode,
    .vbus_prep = vbus_prep,
    .vbus_on = vbus_on,
};

const struct usbport *usbport_mini3(void) { return &PORT; }
