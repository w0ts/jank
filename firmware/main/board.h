/* SPDX-License-Identifier: GPL-3.0-only */
/* Pins by board. Choose the board in sdkconfig.<board>. BOARD_LED_RGB_GPIO >= 0: an addressable RGB LED (the PCB's
 * SK6812s), else BOARD_LED_MONO_GPIO is a plain LED, active low. BOARD_MIDI_SOURCE_GPIO >= 0: the MIDI source is a GPIO
 * driven high, else a 3V3 pin. */
#ifndef BOARD_H
#define BOARD_H
#include "sdkconfig.h"

#if CONFIG_JANK_BOARD_XIAO
/* D6 = GPIO43 for data; the board's 3V3 pad is the source. The module has 499 ohm in series on D6 (v1.2 schematic, R6):
 * MIDI current falls to about 2.3 mA (docs/boards.md); the PCB uses GPIO1, 2, 5, 4 instead. The button is BOOT, the LED the single yellow one. */
#define BOARD_BUTTON_GPIO 0
#define BOARD_BUTTON_ACTIVE_LOW 1
#define BOARD_LED_RGB_GPIO (-1)
#define BOARD_LED_MONO_GPIO 21
#define BOARD_MIDI_TX_GPIO 43
#define BOARD_MIDI_SOURCE_GPIO (-1)
/* USB host mode (usbrole.c): no CC chip, no 5 V of its own, no line in. Host mode through an OTG adapter, the gear
 * self-powered, or nothing. */
#define BOARD_CC_SCL_GPIO (-1)
#define BOARD_CC_SDA_GPIO (-1)
#define BOARD_CC_INT_GPIO (-1)
#define BOARD_HOST_EN_GPIO (-1)
#define BOARD_VBUS_SENSE_GPIO (-1)
#define BOARD_LINE_DETECT_GPIO (-1)
#define BOARD_PUMP_GPIO (-1)
#define BOARD_USBPORT_TUSB320 0
/* Bench outputs (midi.c, from the clock's interrupt, beside the UART byte): D1 = GPIO2 changes level on every MIDI clock
 * (F8), D0 = GPIO1 goes high with each MIDI start (FA) and low at the next clock. 3.3 V logic: into an audio interface
 * through 1 kohm (docs/midi.md). The XIAO only: the PCBs have real CLK / RST outputs (through the pump) on their own pins. */
#define BOARD_BENCH_CLK_GPIO 2
#define BOARD_BENCH_RST_GPIO 1
#define BOARD_LED_COUNT 1
#if CONFIG_JANK_UI_BENCH
/* The UI bench (Kconfig JANK_UI_BENCH, off by default): a 128 x 64 I2C OLED module (SSD1309, or SSD1306 with
 * JANK_UI_BENCH_SSD1306) on the XIAO's I2C pads, D4 = GPIO5 SDA, D5 = GPIO6 SCL, and four buttons to GND on free pads
 * (internal pull-ups): UP D3 = GPIO4, DOWN D8 = GPIO7, YES D9 = GPIO8, NO D10 = GPIO9 or the BOOT button. Not D0 / D1
 * (the bench CLK / RST above) nor D6 (MIDI). */
#define BOARD_I2C_SDA_GPIO 5
#define BOARD_I2C_SCL_GPIO 6
#define BOARD_BTN_UP_GPIO 4
#define BOARD_BTN_DOWN_GPIO 7
#define BOARD_BTN_YES_GPIO 8
#define BOARD_BTN_NO2_GPIO 9                      /* a second NO, beside BOOT (BOARD_BUTTON_GPIO) */
#define BOARD_OLED 1
#else
#define BOARD_I2C_SDA_GPIO (-1)
#define BOARD_I2C_SCL_GPIO (-1)
#define BOARD_BTN_UP_GPIO (-1)
#define BOARD_BTN_DOWN_GPIO (-1)
#define BOARD_BTN_YES_GPIO (-1)
#define BOARD_BTN_NO2_GPIO (-1)
#define BOARD_OLED 0
#endif
#elif CONFIG_JANK_BOARD_MINI22
/* jank mini 2.2, battery order (ESP32-S3-WROOM-1U-N8R2 since 2026-11-15; was the bare FH4R2), from hardware/mini2/parts.py and README.md on hw/mini2.2 (2026-11-15,
 * being routed: check again when it is ordered). Only what this firmware drives so far: NO (the BOOT button, SW1), the two
 * RGB LEDs (WS2812B-2020, GPIO35), the D-pad, the screen, MIDI OUT 1 (UART1 GPIO21, 68 ohm sinks, a 3V3 source pin), and the USB
 * host mode's parts. The usb order (no U17, no Q9) answers no CC chip: it runs as the XIAO does (HOST = a device first,
 * the host stack after 3 s with no computer; no 5 V of its own). */
#define BOARD_USBPORT_TUSB320 1                   /* the port HAL: usbport_tusb320.c */
#define BOARD_BUTTON_GPIO 0                       /* SW1: NO and BOOT, R16 up */
#define BOARD_BUTTON_ACTIVE_LOW 1
/* the D-pad (SW3 UP, SW4 DOWN, SW5 YES, to GND, internal pull-ups; NO is SW1 above) and the screen header J10 / J10B
 * (1.54" or 2.42" SSD1309 module) on the shared I2C, SCL GPIO11 / SDA GPIO12 with R60 / R61 10 k */
#define BOARD_BTN_UP_GPIO 10
#define BOARD_BTN_DOWN_GPIO 37
#define BOARD_BTN_YES_GPIO 36
#define BOARD_BTN_NO2_GPIO (-1)
#define BOARD_I2C_SCL_GPIO 11
#define BOARD_I2C_SDA_GPIO 12
#define BOARD_OLED 1                              /* looked for at the start: none answers -> the LED language */
/* the two WS2812B-2020 (chained, LED1 then LED2): GPIO35 since the WROOM-1U-N8R2 module (2026-11-15, was GPIO33 with
 * the bare chip; the charge pump that had GPIO35 is gone) */
#define BOARD_LED_RGB_GPIO 35
#define BOARD_LED_COUNT 2
#define BOARD_LED_MONO_GPIO (-1)
#define BOARD_MIDI_TX_GPIO 21
#define BOARD_MIDI_SOURCE_GPIO (-1)
#define BOARD_MIDI_TX_DRIVE GPIO_DRIVE_CAP_3      /* the board README's MIDI trade-off: drive 3 */
#define BOARD_CC_SCL_GPIO 11                      /* the shared I2C (R60 / R61 10 k up): U17 TUSB320LI at 0x47 */
#define BOARD_CC_SDA_GPIO 12
#define BOARD_CC_INT_GPIO 39                      /* U17's INT_N, open drain */
#define BOARD_HOST_EN_GPIO 42                     /* high: the 5 V signal on VBUS (Q8 -> Q7), R91 100 k down */
#define BOARD_VBUS_SENSE_GPIO 38                  /* the board's VBUS (after Q9) through R88 22 k / R89 33 k */
#define BOARD_LINE_DETECT_GPIO 16                 /* J7's switch, R38 1 M up: low = no plug */
#define BOARD_PUMP_GPIO (-1)                      /* the charge pump is gone (2026-11-15): pump_hold answers not supported, so
                                                     USB host mode's 5 V signal waits for the 5 V aux boost's enable pin */
#else
#error "no board chosen"
#endif
#endif
