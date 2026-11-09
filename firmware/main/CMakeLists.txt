# SPDX-License-Identifier: GPL-3.0-only
set(core "../../core")
set(link "../../third_party/link")
idf_component_register(
    SRCS "main.c" "bridge.c" "radio.c" "usb.c" "usb_midi_host.c" "midi.c" "ui.c" "ctl.c" "store.c" "ota.c" "download.c" "drive.c"
         "${core}/jank_net.c" "${core}/jank_core.c" "${core}/jank_ctl.c" "${core}/jank_ui.c" "${core}/jank_clock.c" "${core}/jank_peer.c" "${core}/jank_sysex.c" "${core}/jank_ota.c"
         "${link}/link.c"
    INCLUDE_DIRS "." "${core}" "${link}"
    PRIV_REQUIRES app_update fatfs wear_levelling esp_partition usb esp_wifi esp_event esp_netif nvs_flash esp_driver_gptimer esp_driver_uart esp_driver_gpio esp_timer hal soc esp_hw_support esp_system
)
# the drive's page, restored at every start (drive.c) ...
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../../page/jank.html" TEXT)
# ... and the volume itself, first flashed with the page on it and the label JANK (scripts/build.sh)
fatfs_create_spiflash_image(storage "${CMAKE_CURRENT_LIST_DIR}/../../page" FLASH_IN_PROJECT)
