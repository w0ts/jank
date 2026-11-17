/* SPDX-License-Identifier: GPL-3.0-only */
/* The lcd2's colour screen (lcd.c): the Waveshare ESP32-S3-LCD-2's 240 x 320 ST7789 on SPI, landscape. It takes the 128 x 64
 * 1-bit frame and its ink plane (core/jank_oled.h) and draws them 2 x 2 pixels each, 256 x 128 centred, in the palette of
 * core/jank_lcd.h. */
#ifndef LCD_H
#define LCD_H
#include <stdint.h>
#include "esp_err.h"
#include "jank_oled.h"

/* app_main, before the radio starts: the planes allocated, the SPI bus, the panel set up, a blank frame sent, then the backlight
 * on. 1 found */
int lcd_open(void);
int lcd_found(void);
/* the ink plane the UI draws into (JO_INK_BYTES, jo_draw_ink); valid once lcd_open found the screen */
uint8_t *lcd_ink(void);
/* the driver task (core 0, the lowest priority above idle); nothing without a screen */
esp_err_t lcd_start(void);
/* hand a frame over: fb and the plane lcd_ink() holds are copied; the task sends it */
void lcd_show(const uint8_t fb[JO_FB_BYTES]);
/* send a frame now, on the caller's task: only the frame pages that differ from the panel's (the first time: all of it) */
esp_err_t lcd_present(const uint8_t fb[JO_FB_BYTES], const uint8_t ink[JO_INK_BYTES]);
#endif
