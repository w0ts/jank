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
#elif CONFIG_JANK_BOARD_MINI22
/* jank mini 2.2, battery order (ESP32-S3FH4R2), from hardware/mini2/parts.py and README.md on hw/mini2.2 (2026-11-15,
 * being routed: check again when it is ordered). Only what this firmware drives so far: NO (the BOOT button, SW1), the two
 * RGB LEDs (WS2812B-2020, GPIO33: one is driven), MIDI OUT 1 (UART1 GPIO21, 68 ohm sinks, a 3V3 source pin), and the USB
 * host mode's parts. The usb order (no U17, no Q9) answers no CC chip: usbrole.c runs it as device only. */
#define BOARD_BUTTON_GPIO 0
#define BOARD_BUTTON_ACTIVE_LOW 1
#define BOARD_LED_RGB_GPIO 33
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
#define BOARD_PUMP_GPIO 35                        /* the CLK / RST charge pump through U13: 416,667 Hz, 50 % */
#else
#error "no board chosen"
#endif
#endif
