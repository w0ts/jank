# SPDX-License-Identifier: GPL-3.0-only
set(core "../../core")
set(link "../../third_party/link")
if(IDF_TARGET STREQUAL "esp32s3")
    set(transport "usb.c" "usb_midi_host.c" "${core}/jank_sysex.c")
    set(disk_requires fatfs wear_levelling esp_partition usb)
else()
    set(transport "serial.c" "${core}/jank_log.c")
endif()
idf_component_register(
    SRCS "main.c" "bridge.c" "radio.c" ${transport} "midi.c" "ui.c" "ctl.c" "store.c" "reset_button.c"
         "${core}/jank_net.c" "${core}/jank_core.c" "${core}/jank_ctl.c" "${core}/jank_ui.c" "${core}/jank_clock.c" "${core}/jank_peer.c"
         "${link}/link.c"
    INCLUDE_DIRS "." "${core}" "${link}"
    PRIV_REQUIRES ${disk_requires} esp_wifi esp_event esp_netif nvs_flash esp_driver_gptimer esp_driver_uart esp_driver_gpio esp_timer hal soc esp_hw_support esp_system
)
if(IDF_TARGET STREQUAL "esp32s3")
    # the drive the computer sees: the page, built into a FAT image from page/
    fatfs_create_spiflash_image(storage "${CMAKE_CURRENT_LIST_DIR}/../../page" FLASH_IN_PROJECT)
endif()
