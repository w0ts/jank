/* SPDX-License-Identifier: GPL-3.0-only */
/* The screen's interface for the UI (uiscreen.c). The I2C OLED's driver (oled.c): found at the start, frames sent page by
 * page, only the pages that changed. On the lcd2 (BOARD_LCD) open, present and start go to its colour screen (lcd.c); its frames carry an ink plane,
 * so uiscreen.c shows them with lcd_show there, not oled_show. */
#ifndef OLED_H
#define OLED_H
#include <stdint.h>
#include "board.h"
#include "esp_err.h"
#include "jank_oled.h"
#if BOARD_LCD
#include "lcd.h"
#endif

#if BOARD_LCD
static inline int oled_open(uint8_t contrast)
{
    (void)contrast;                      /* the backlight is on or off: no contrast on this screen */
    return lcd_open();
}
static inline int oled_present(void) { return lcd_found(); }
static inline esp_err_t oled_start(void) { return lcd_start(); }
#else
/* app_main, before the radio starts: looks for a module at 0x3C / 0x3D, sets it up blank and switches its panel on
 * (the only time: the 2.42"'s power rule). 1 found */
int oled_open(uint8_t contrast);
int oled_present(void);
/* the driver task (core 0, the lowest priority); nothing without a screen */
esp_err_t oled_start(void);
/* hand a frame over (copied); the task sends the pages that differ from the panel's, and the contrast when it changed */
void oled_show(const uint8_t fb[JO_FB_BYTES], uint8_t contrast);
void oled_stats(uint32_t *pages, uint32_t *errors);
#endif
#endif
