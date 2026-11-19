# SPDX-License-Identifier: GPL-3.0-only
set(core "../../core")
set(link "../../third_party/link")
set(beat "${core}/beat")
# the screen: the I2C OLED, or the lcd2's SPI colour panel behind the same interface (oled.h). PRIV_REQUIRES cannot depend on
# the sdkconfig (it is read before it is loaded): esp_lcd and esp_driver_spi are listed for every board and linked only where used.
if(CONFIG_JANK_BOARD_LCD2)
    set(screen_srcs "lcd.c" "${core}/jank_lcd.c" "${core}/jank_oled_lcd2.c" "${core}/jank_bigfont.c" "${core}/jank_bigfont_tables.c")
else()
    set(screen_srcs "oled.c")
endif()
# BEAT-FOLLOW BENCH BUILD: core/beat/beat_model.c holds madmom's network weights (CC BY-NC-SA 4.0): never push or ship an image
message(NOTICE "jank: BEAT-FOLLOW BENCH BUILD with madmom's weights (CC BY-NC-SA 4.0): bench only, never ship")
idf_component_register(
    SRCS "main.c" "bridge.c" "radio.c" "usb.c" "usb_midi_host.c" "midi.c" "ui.c" "ctl.c" "store.c" "ota.c" "download.c" "drive.c" "clkprobe.c" "fft_dsp.c"
         "usb_audio.c" "beat.c" "linein.c" "${core}/jank_line.c"
         "usb_hostlib.c" "usb_uac_host.c" "usbrole.c" "usbport_tusb320.c" "pump.c" "power.c"
         "panic_info.c" "${core}/jank_panic.c" "board_i2c.c" ${screen_srcs} "uiscreen.c" "${core}/jank_led.c"
         "${core}/jank_oled_gfx.c" "${core}/jank_oled_menu.c" "${core}/jank_oled_press.c" "${core}/jank_oled_draw.c" "${core}/jank_oled_qr.c" "${core}/jank_oled_input.c"
         "${core}/jank_usbrole.c" "${core}/jank_tusb320.c" "${core}/jank_uacdesc.c" "${core}/jank_pcm.c" "${core}/jank_midiq.c"
         "${core}/jank_net.c" "${core}/jank_core.c" "${core}/jank_ctl.c" "${core}/jank_ui.c" "${core}/jank_clock.c" "${core}/jank_peer.c" "${core}/jank_sysex.c" "${core}/jank_ota.c" "${core}/jank_beatlink.c" "${core}/jank_start.c"
         "${core}/jank_home.c" "${core}/jank_vdrive.c" "${core}/jank_live.c" "${core}/jank_hold.c" "${core}/jank_fold.c" "${core}/jank_lat.c" "${core}/jank_conf.c" "${core}/jank_prefs.c"
         "${beat}/jb_beat.c" "${beat}/jb_dbn.c" "${beat}/jb_fe.c" "${beat}/jb_fft_r22.c" "${beat}/jb_lstm.c" "${beat}/jb_lstm_q16.c" "${beat}/jb_mode.c" "${beat}/beat_model.c"
         "${beat}/jb_down.c" "${beat}/down_model.c" "${beat}/jb_follow.c" "${beat}/jb_rate.c" "${beat}/jb_weak.c" "${beat}/jb_lost.c" "${beat}/jb_comb.c" "${beat}/jb_robust.c" "${beat}/jb_skip.c" "${beat}/jb_graded.c"
         "${link}/link.c"
    INCLUDE_DIRS "." "${core}" "${link}" "${beat}"
    LDFRAGMENTS "linker.lf"
    PRIV_REQUIRES app_update esp_partition usb esp_wifi esp_event esp_netif nvs_flash esp_driver_gptimer esp_driver_uart esp_driver_gpio esp_driver_mcpwm esp_driver_i2c esp_driver_ledc esp_timer hal soc esp_hw_support esp_system heap esp_adc esp_driver_tsens spi_flash esp_driver_spi esp_lcd espressif__esp-dsp
)
# the beat front end's FFT: esp-dsp's S3 assembly kernel (fft_dsp.c) instead of core/beat's scalar one (the host tests keep that one)
target_compile_definitions(${COMPONENT_LIB} PRIVATE JB_FFT_FAST=1)
# the filterbank's 1428 weights (5.7 KB) copied into internal RAM once instead of read from flash every hop; not on the lcd2,
# which has 7 KB of static RAM left (jb_fe.c)
if(NOT CONFIG_JANK_BOARD_LCD2)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE JB_FB_IN_RAM=1)
endif()
# the last panic is kept across the restart (panic_info.c): ESP-IDF's panic handler is wrapped, the real one still runs
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
# a test build's clock probe at the pin (clkprobe.c): JANK_CLOCK_PROBE=1 scripts/build.sh xiao
if(DEFINED ENV{JANK_CLOCK_PROBE} AND "$ENV{JANK_CLOCK_PROBE}" STREQUAL "1")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE JANK_CLOCK_PROBE=1)
endif()
# the drive's page: the made-up JANK volume serves it from the image (drive.c, core/jank_vdrive.h); nothing is stored on a
# FAT partition any more (the XIAO's table still has its 2 MB "storage", unused)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../../page/jank.html" TEXT)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../../docs/drive-readme.txt" TEXT)
