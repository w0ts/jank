/* SPDX-License-Identifier: GPL-3.0-only */
/* The I2C OLED's driver (oled.c): found at the start, frames sent page by page, only the pages that changed. */
#ifndef OLED_H
#define OLED_H
#include <stdint.h>
#include "esp_err.h"
#include "jank_oled.h"

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
