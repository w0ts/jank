/* SPDX-License-Identifier: GPL-3.0-only */
/* The board's one I2C bus (board_i2c.h): the jank mini 2.2's SCL GPIO11 / SDA GPIO12 carry the TUSB320 (0x47) and the
 * screen (0x3C); the XIAO's UI bench has the screen alone on D4 / D5. Each device adds itself at its own speed; the
 * driver keeps one transaction on the bus at a time. Made by the first caller, from app_main (core 0: the bus's
 * interrupt stays off the clock's core 1, TIMING.md trap 2). */
#include "board_i2c.h"
#include "board.h"
#include "esp_log.h"

static i2c_master_bus_handle_t s_bus;
static esp_err_t s_err = ESP_ERR_INVALID_STATE;
static int s_tried;

esp_err_t board_i2c_bus(i2c_master_bus_handle_t *out)
{
#if BOARD_I2C_SDA_GPIO >= 0 && BOARD_I2C_SCL_GPIO >= 0
    if (!s_tried) {
        i2c_master_bus_config_t bc = {
            .i2c_port = -1,
            .sda_io_num = BOARD_I2C_SDA_GPIO,
            .scl_io_num = BOARD_I2C_SCL_GPIO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,   /* (the boards' 10 k, the modules' own, do the work) */
        };
        s_tried = 1;
        s_err = i2c_new_master_bus(&bc, &s_bus);
        if (s_err != ESP_OK)
            ESP_LOGE("i2c", "bus: %s", esp_err_to_name(s_err));
    }
    *out = s_bus;
    return s_err;
#else
    *out = NULL;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
