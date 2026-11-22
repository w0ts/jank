/* SPDX-License-Identifier: GPL-3.0-only */
/* Pins by board, chosen AT RUN TIME (board.c). One table per board; the build decides which tables are in the image:
 *   sdkconfig.auto (CONFIG_JANK_BOARD_AUTO): the XIAO's and the jank mini 3's (lcd2); board_init() tells them apart at the
 *     start, before any pin is driven (board.c says how). The release image.
 *   sdkconfig.xiao / .lcd2 / .mini22: that board's table alone (bench builds, the frozen mini 2.2).
 * The BOARD_* names read the chosen table, so they are run-time values: test them with C `if`, never `#if`.
 * BOARD_LED_RGB_GPIO >= 0: an addressable RGB LED, else BOARD_LED_MONO_GPIO is a plain LED, active low.
 * BOARD_MIDI_SOURCE_GPIO >= 0: the MIDI source is a GPIO driven high, else a 3V3 pin. -1: the board has no such pin.
 * The table lives in internal RAM: the MIDI clock's interrupt reads it while the flash is being written. */
#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
#include "sdkconfig.h"

#define BOARD_MIDI_TX_MORE_MAX 2
/* a build-time fact about which tables are in the image: only the mini 2.2 has RGB LEDs, so only its build carries the RMT
 * LED driver (about 2.7 KB of IRAM the jank mini 3 cannot spare) */
#define BOARD_ANY_RGB_LED CONFIG_JANK_BOARD_MINI22

struct board {
    const char *name;                     /* "xiao", "lcd2", "mini22" (the status' board=) */
    const char *model;                    /* the splash's model line */
    int8_t button_gpio, button_active_low;
    int8_t led_rgb_gpio, led_mono_gpio, led_count;
    int8_t midi_tx_gpio, midi_source_gpio;
    int8_t midi_tx_more[BOARD_MIDI_TX_MORE_MAX];   /* the same UART TX routed to more pins; -1 unused */
    int8_t midi_tx_invert;                /* the loop is sunk through FET gates: the line idles LOW */
    int8_t midi_tx_drive;                 /* gpio_drive_cap_t for the TX pins; -1: the default */
    int8_t bench_clk_gpio, bench_rst_gpio;   /* the XIAO's bench clock / start outputs (docs/midi.md); -1 none */
    int8_t clk_gpio, rst_gpio, v5_en_gpio;   /* the mini 3's CLK / RST jacks and 5 V aux boost enable */
    int8_t i2c_sda_gpio, i2c_scl_gpio;
    int8_t btn_up_gpio, btn_down_gpio, btn_yes_gpio, btn_no2_gpio;
    int8_t oled;                          /* an I2C OLED is looked for at the start */
    int8_t lcd;                           /* the LCD-2's colour screen (lcd.c) */
    int8_t lcd_mosi_gpio, lcd_sclk_gpio, lcd_cs_gpio, lcd_dc_gpio, lcd_rst_gpio, lcd_bl_gpio;
    int8_t cc_scl_gpio, cc_sda_gpio, cc_int_gpio;
    int8_t host_en_gpio, vbus_sense_gpio, line_detect_gpio, pump_gpio;
    int8_t usbport_tusb320, usbport_mini3;   /* the USB port HAL: usbport_tusb320.c / usbport_mini3.c, or none */
    int8_t bridge_slots_psram;            /* bridge.c's frame slots in PSRAM (internal DRAM is short) */
    int8_t has_linein, linein_gpio, linesense_gpio;
    int8_t cell_gpio, cell_div;           /* the cell's divider: its volts / the pin's */
    int16_t cell_empty_mv;
};

extern const struct board *g_board;

/* first thing in app_main: the auto build tells the boards apart (board.c); the others keep their one table */
void board_init(void);
/* how the board was chosen: "built for it", "motion sensor", "flash size", "default" (the status and the log) */
const char *board_how(void);

#define BOARD_NAME (g_board->name)
#define BOARD_MODEL (g_board->model)
#define BOARD_BUTTON_GPIO (g_board->button_gpio)
#define BOARD_BUTTON_ACTIVE_LOW (g_board->button_active_low)
#define BOARD_LED_RGB_GPIO (g_board->led_rgb_gpio)
#define BOARD_LED_MONO_GPIO (g_board->led_mono_gpio)
#define BOARD_LED_COUNT (g_board->led_count)
#define BOARD_MIDI_TX_GPIO (g_board->midi_tx_gpio)
#define BOARD_MIDI_SOURCE_GPIO (g_board->midi_source_gpio)
#define BOARD_BENCH_CLK_GPIO (g_board->bench_clk_gpio)
#define BOARD_BENCH_RST_GPIO (g_board->bench_rst_gpio)
#define BOARD_CLK_GPIO (g_board->clk_gpio)
#define BOARD_RST_GPIO (g_board->rst_gpio)
#define BOARD_V5_EN_GPIO (g_board->v5_en_gpio)
#define BOARD_I2C_SDA_GPIO (g_board->i2c_sda_gpio)
#define BOARD_I2C_SCL_GPIO (g_board->i2c_scl_gpio)
#define BOARD_BTN_UP_GPIO (g_board->btn_up_gpio)
#define BOARD_BTN_DOWN_GPIO (g_board->btn_down_gpio)
#define BOARD_BTN_YES_GPIO (g_board->btn_yes_gpio)
#define BOARD_BTN_NO2_GPIO (g_board->btn_no2_gpio)
#define BOARD_OLED (g_board->oled)
#define BOARD_LCD (g_board->lcd)
#define BOARD_LCD_MOSI_GPIO (g_board->lcd_mosi_gpio)
#define BOARD_LCD_SCLK_GPIO (g_board->lcd_sclk_gpio)
#define BOARD_LCD_CS_GPIO (g_board->lcd_cs_gpio)
#define BOARD_LCD_DC_GPIO (g_board->lcd_dc_gpio)
#define BOARD_LCD_RST_GPIO (g_board->lcd_rst_gpio)
#define BOARD_LCD_BL_GPIO (g_board->lcd_bl_gpio)
#define BOARD_CC_SCL_GPIO (g_board->cc_scl_gpio)
#define BOARD_CC_SDA_GPIO (g_board->cc_sda_gpio)
#define BOARD_CC_INT_GPIO (g_board->cc_int_gpio)
#define BOARD_HOST_EN_GPIO (g_board->host_en_gpio)
#define BOARD_VBUS_SENSE_GPIO (g_board->vbus_sense_gpio)
#define BOARD_LINE_DETECT_GPIO (g_board->line_detect_gpio)
#define BOARD_PUMP_GPIO (g_board->pump_gpio)
#define BOARD_USBPORT_TUSB320 (g_board->usbport_tusb320)
#define BOARD_USBPORT_MINI3 (g_board->usbport_mini3)
#define BOARD_BRIDGE_SLOTS_PSRAM (g_board->bridge_slots_psram)
#define BOARD_HAS_LINEIN (g_board->has_linein)
#define BOARD_LINEIN_GPIO (g_board->linein_gpio)
#define BOARD_LINESENSE_GPIO (g_board->linesense_gpio)
#define BOARD_CELL_GPIO (g_board->cell_gpio)
#define BOARD_CELL_DIV (g_board->cell_div)
#define BOARD_CELL_EMPTY_MV (g_board->cell_empty_mv)
#define BOARD_MIDI_TX_INVERT (g_board->midi_tx_invert)
#define BOARD_MIDI_TX_DRIVE (g_board->midi_tx_drive)
#endif
