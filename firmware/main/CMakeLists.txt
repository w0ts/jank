# SPDX-License-Identifier: GPL-3.0-only
set(core "../../core")
set(link "../../third_party/link")
set(beat "${core}/beat")
# a BLE MIDI peripheral for the control line (Kconfig JANK_BLE_MIDI, sdkconfig.ble)
set(ble_srcs "")
if(CONFIG_JANK_BLE_MIDI)
    set(ble_srcs "ble_midi.c" "${core}/jank_blemidi.c")
endif()
# BEAT-FOLLOW BENCH BUILD: core/beat/beat_model.c holds madmom's network weights (CC BY-NC-SA 4.0): never push or ship an image
message(NOTICE "jank: BEAT-FOLLOW BENCH BUILD with madmom's weights (CC BY-NC-SA 4.0): bench only, never ship")
idf_component_register(
    SRCS "main.c" "bridge.c" "radio.c" "usb.c" "usb_midi_host.c" "midi.c" "ui.c" "ctl.c" "store.c" "ota.c" "download.c" "drive.c" "clkprobe.c"
         "usb_audio.c" "beat.c" "linein.c" "${core}/jank_line.c"
         "usb_hostlib.c" "usb_uac_host.c" "usbrole.c" "usbport_tusb320.c" "pump.c"
         "${core}/jank_usbrole.c" "${core}/jank_tusb320.c" "${core}/jank_uacdesc.c" "${core}/jank_pcm.c" "${core}/jank_midiq.c"
         "${core}/jank_net.c" "${core}/jank_core.c" "${core}/jank_ctl.c" "${core}/jank_ui.c" "${core}/jank_clock.c" "${core}/jank_peer.c" "${core}/jank_sysex.c" "${core}/jank_ota.c" "${core}/jank_beatlink.c" "${core}/jank_start.c"
         "${core}/jank_phone.c" "${core}/jank_vdrive.c"
         "${beat}/jb_beat.c" "${beat}/jb_dbn.c" "${beat}/jb_fe.c" "${beat}/jb_lstm.c" "${beat}/jb_lstm_q16.c" "${beat}/jb_mode.c" "${beat}/beat_model.c"
         "${beat}/jb_down.c" "${beat}/down_model.c"
         "${link}/link.c" ${ble_srcs}
    INCLUDE_DIRS "." "${core}" "${link}" "${beat}"
    LDFRAGMENTS "linker.lf"
    PRIV_REQUIRES app_update esp_partition usb esp_wifi esp_event esp_netif nvs_flash esp_driver_gptimer esp_driver_uart esp_driver_gpio esp_driver_mcpwm esp_driver_i2c esp_driver_ledc esp_timer hal soc esp_hw_support esp_system heap bt esp_coex esp_adc
)
# a test build's clock probe at the pin (clkprobe.c): JANK_CLOCK_PROBE=1 scripts/build.sh xiao
if(DEFINED ENV{JANK_CLOCK_PROBE} AND "$ENV{JANK_CLOCK_PROBE}" STREQUAL "1")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE JANK_CLOCK_PROBE=1)
endif()
# the drive's page: the made-up JANK volume serves it from the image (drive.c, core/jank_vdrive.h); nothing is stored on a
# FAT partition any more (the XIAO's table still has its 2 MB "storage", unused)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../../page/jank.html" TEXT)
