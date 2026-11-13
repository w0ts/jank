# SPDX-License-Identifier: GPL-3.0-only
set(core "../../core")
set(link "../../third_party/link")
set(beat "${core}/beat")
# BEAT-FOLLOW BENCH BUILD: core/beat/beat_model.c holds madmom's network weights (CC BY-NC-SA 4.0): never push or ship an image
message(NOTICE "jank: BEAT-FOLLOW BENCH BUILD with madmom's weights (CC BY-NC-SA 4.0): bench only, never ship")
idf_component_register(
    SRCS "main.c" "bridge.c" "radio.c" "usb.c" "usb_midi_host.c" "midi.c" "ui.c" "ctl.c" "store.c" "ota.c" "download.c" "drive.c" "clkprobe.c"
         "usb_audio.c" "beat.c"
         "${core}/jank_net.c" "${core}/jank_core.c" "${core}/jank_ctl.c" "${core}/jank_ui.c" "${core}/jank_clock.c" "${core}/jank_peer.c" "${core}/jank_sysex.c" "${core}/jank_ota.c" "${core}/jank_beatlink.c"
         "${beat}/jb_beat.c" "${beat}/jb_dbn.c" "${beat}/jb_fe.c" "${beat}/jb_lstm.c" "${beat}/jb_lstm_q16.c" "${beat}/jb_mode.c" "${beat}/beat_model.c"
         "${link}/link.c"
    INCLUDE_DIRS "." "${core}" "${link}" "${beat}"
    PRIV_REQUIRES app_update fatfs wear_levelling esp_partition usb esp_wifi esp_event esp_netif nvs_flash esp_driver_gptimer esp_driver_uart esp_driver_gpio esp_driver_mcpwm esp_timer hal soc esp_hw_support esp_system heap
)
# a test build's clock probe at the pin (clkprobe.c): JANK_CLOCK_PROBE=1 scripts/build.sh xiao
if(DEFINED ENV{JANK_CLOCK_PROBE} AND "$ENV{JANK_CLOCK_PROBE}" STREQUAL "1")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE JANK_CLOCK_PROBE=1)
endif()
# the drive's page, restored at every start (drive.c) ...
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../../page/jank.html" TEXT)
# ... and the volume itself, first flashed with the page on it and the label JANK (scripts/build.sh)
fatfs_create_spiflash_image(storage "${CMAKE_CURRENT_LIST_DIR}/../../page" FLASH_IN_PROJECT)
