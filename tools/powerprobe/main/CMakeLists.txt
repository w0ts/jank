# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(SRCS "powerprobe.c" PRIV_REQUIRES esp_wifi esp_event nvs_flash esp_timer esp_driver_gpio esp_pm)
