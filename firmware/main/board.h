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
/* D6 = GPIO43 for data; the board's 3V3 pad is the source. The button is BOOT, the LED the single yellow one. */
#define BOARD_BUTTON_GPIO 0
#define BOARD_BUTTON_ACTIVE_LOW 1
#define BOARD_LED_RGB_GPIO (-1)
#define BOARD_LED_MONO_GPIO 21
#define BOARD_MIDI_TX_GPIO 43
#define BOARD_MIDI_SOURCE_GPIO (-1)
#elif CONFIG_JANK_BOARD_LOLIN32
/* An external button from GPIO25 to GND. GPIO17 for data; a 3V3 pin is the source. The LED is on GPIO5, active low. */
#define BOARD_BUTTON_GPIO 25
#define BOARD_BUTTON_ACTIVE_LOW 1
#define BOARD_LED_RGB_GPIO (-1)
#define BOARD_LED_MONO_GPIO 5
#define BOARD_MIDI_TX_GPIO 17
#define BOARD_MIDI_SOURCE_GPIO (-1)
#else
#error "no board chosen"
#endif

/* The ROM's download mode by software exists on the S3 only. The ESP32 just restarts: esptool's DTR and RTS do the rest. */
#if CONFIG_IDF_TARGET_ESP32S3
#define BOARD_FORCE_DOWNLOAD() REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT)
#else
#define BOARD_FORCE_DOWNLOAD() ((void)0)
#endif
#endif
