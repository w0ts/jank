/* SPDX-License-Identifier: GPL-3.0-only */
/* The board's power and idle levels, set before anything else runs (mini 3 / lcd2 only; the other boards have -1 pins and do
 * nothing). Order: HOST_EN low first (the only driver that puts a voltage on a connector), CLK and RST low (idle), then V5_EN
 * high (the 5 V aux boost: MIDI loops, the CLK / RST buffer, the host signal). R131 already holds V5_EN on until now, so
 * driving it changes nothing electrically; it makes the state deliberate. Each level is set before the direction, no glitch. */
#include "app.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"

static void out_level(int pin, int level)
{
    if (pin < 0)
        return;
    gpio_set_level((gpio_num_t)pin, level);              /* the level first (a no-op until it is an output), then the direction */
    const gpio_config_t c = {.pin_bit_mask = 1ULL << pin, .mode = GPIO_MODE_OUTPUT, .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&c));
    gpio_set_level((gpio_num_t)pin, level);
}

void power_early_init(void)
{
    if (BOARD_V5_EN_GPIO < 0)
        return;
    out_level(BOARD_HOST_EN_GPIO, 0);
    out_level(BOARD_CLK_GPIO, 0);
    out_level(BOARD_RST_GPIO, 0);
    out_level(BOARD_V5_EN_GPIO, 1);
    ESP_LOGI("power", "HOST_EN %d, CLK %d, RST %d low; V5_EN %d high", BOARD_HOST_EN_GPIO, BOARD_CLK_GPIO, BOARD_RST_GPIO,
             BOARD_V5_EN_GPIO);
}
