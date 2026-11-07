# SPDX-License-Identifier: GPL-3.0-only
set(core "../../core")
set(link "../../../fm1base/link")
if(IDF_TARGET STREQUAL "esp32s3")
    set(transport "usb.c")
else()
    set(transport "serial.c" "${core}/jank_log.c")
endif()
idf_component_register(
    SRCS "main.c" "bridge.c" "radio.c" ${transport} "midi.c" "ui.c" "ctl.c" "store.c"
         "${core}/jank_core.c" "${core}/jank_ctl.c" "${core}/jank_ui.c" "${core}/jank_clock.c" "${core}/jank_peer.c"
         "${link}/link.c"
    INCLUDE_DIRS "." "${core}" "${link}"
    PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_driver_gptimer esp_driver_uart esp_driver_gpio esp_timer hal soc esp_hw_support esp_system
)
