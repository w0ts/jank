/* SPDX-License-Identifier: GPL-3.0-only */
/* The board's shared I2C bus (board_i2c.c): the port chip and the screen add their devices to the same handle. */
#ifndef BOARD_I2C_H
#define BOARD_I2C_H
#include "driver/i2c_master.h"
#include "esp_err.h"

/* the bus, made at the first call (from app_main, before any task uses it); ESP_ERR_NOT_SUPPORTED: no I2C on the board */
esp_err_t board_i2c_bus(i2c_master_bus_handle_t *out);
#endif
