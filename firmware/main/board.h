/* SPDX-License-Identifier: GPL-3.0-only */
/* Pins by board. Choose the board in sdkconfig.<board>. */
#ifndef BOARD_H
#define BOARD_H
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"

#if CONFIG_JANK_BOARD_ATOMS3U
/* The Grove port: G2 data through 10 ohm, G1 held high through 33 ohm is the 3.3 V source (the port has 5 V but no 3.3 V). */
#define BOARD_BUTTON_GPIO 41
#define BOARD_BUTTON_ACTIVE_LOW 1
#define BOARD_LED_RGB_GPIO 35
#define BOARD_LED_MONO_GPIO (-1)
#define BOARD_MIDI_TX_GPIO 2
#define BOARD_MIDI_SOURCE_GPIO 1
#elif CONFIG_JANK_BOARD_XIAO
/* D6 = GPIO43 for data; the board's 3V3 pad is the source. The module has 499 ohm in series on D6 (v1.2 schematic, R6):
 * MIDI current falls to about 2.3 mA (docs/boards.md); the PCB uses GPIO1, 2, 5, 4 instead. The button is BOOT, the LED the single yellow one. */
#define BOARD_BUTTON_GPIO 0
#define BOARD_BUTTON_ACTIVE_LOW 1
#define BOARD_LED_RGB_GPIO (-1)
#define BOARD_LED_MONO_GPIO 21
#define BOARD_MIDI_TX_GPIO 43
#define BOARD_MIDI_SOURCE_GPIO (-1)
#else
#error "no board chosen"
#endif

/* The ROM's download mode by software, for flashing with no button. */
#define BOARD_FORCE_DOWNLOAD() REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT)
#endif
