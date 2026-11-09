/* SPDX-License-Identifier: GPL-3.0-only */
/* The ROM's download mode on USB, by software: the `bootloader` command, the page's button, the button held 10 s.
 *
 * Why the plain "set FORCE_DOWNLOAD_BOOT and restart" is not enough (measured on the XIAO, 2026-11-11: it answered ok and came
 * back as jank): the S3 has one internal USB PHY and two controllers that can own it, the USB-OTG controller (TinyUSB's) and
 * the USB-Serial-JTAG controller (the ROM's download mode, "USB JTAG/serial debug unit"). RTC_CNTL_USB_CONF says who owns
 * it, and it lives in the RTC domain, which a software restart does not reset. TinyUSB had set SW_HW_USB_PHY_SEL and
 * SW_USB_PHY_SEL (the PHY is the OTG controller's by software choice), so the ROM's controller was left with no PHY: a host saw
 * nothing of it. The sequence below gives the PHY back before the restart, as arduino-esp32 does for its
 * usb_persist_restart(RESTART_BOOTLOADER) (cores/esp32/esp32-hal-tinyusb.c, usb_switch_to_cdc_jtag):
 *   1. soft-disconnect (drop D+'s pull-up) so the host sees the device leave and gives up the old address;
 *   2. reset and clock-gate the OTG controller (periph_ll_reset, periph_ll_disable_clk_set_rst on PERIPH_USB_MODULE);
 *   3. clear RTC_CNTL_SW_HW_USB_PHY_SEL, SW_USB_PHY_SEL and USB_PAD_ENABLE: the PHY is Serial-JTAG's again (hardware choice);
 *   4. clear USB_SERIAL_JTAG_CONF0.PHY_SEL: that controller uses the internal PHY, not an external one;
 *   5. clear a pull override, set DP_PULLUP and USB_PAD_ENABLE last: its pads and D+ pull-up are on, the host re-enumerates;
 *   6. set RTC_CNTL_OPTION1.FORCE_DOWNLOAD_BOOT and restart: the ROM boots into download mode on the PHY it was given.
 * What arduino-esp32 does beyond this is drive D+/D- low through the GPIO matrix to force a bus reset. Not done here: that
 * setting survived the restart and kept the chip off the bus until a power cycle (measured on the same stick, 2026-11-11);
 * step 1 does the host's disconnect, and the restart in step 6 resets the Serial-JTAG controller, which the host sees as a
 * second re-enumeration.
 * Sources: ESP-IDF v5.5.6 hal/esp32s3 usb_wrap_ll.h (usb_wrap_ll_phy_enable_external) and usb_serial_jtag_ll.h
 * (usb_serial_jtag_ll_phy_enable_external: the same two RTC bits and PHY_SEL), esp_system/port/soc/esp32s3/clk.c (start-up
 * gates Serial-JTAG's clock and pad when it is unused, so the clock is switched back on first), the TRM's USB PHY mux.
 * NOT VERIFIED ON HARDWARE. */
#include "app.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/clk_gate_ll.h"
#include "hal/usb_serial_jtag_ll.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/system_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#include "tusb.h"

#define HOST_NOTICE_MS 100            /* the host needs a moment to see the pull-up go */

static void otg_off(void)
{
    if (tud_inited())
        tud_disconnect();             /* host mode has no device controller to disconnect */
    vTaskDelay(pdMS_TO_TICKS(HOST_NOTICE_MS));
    periph_ll_reset(PERIPH_USB_MODULE);
    periph_ll_disable_clk_set_rst(PERIPH_USB_MODULE);
}

static void phy_to_serial_jtag(void)
{
    CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG, RTC_CNTL_SW_HW_USB_PHY_SEL | RTC_CNTL_SW_USB_PHY_SEL | RTC_CNTL_USB_PAD_ENABLE);
    SET_PERI_REG_MASK(SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_USB_DEVICE_CLK_EN);   /* start-up gates it when nothing uses Serial-JTAG */
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PHY_SEL);
    usb_serial_jtag_ll_phy_disable_pull_override();
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
}

void usb_to_rom_download(void)
{
    otg_off();
    phy_to_serial_jtag();
    esp_rom_delay_us(20000);          /* the pull-up is on and the host starts on it before the restart takes it away again */
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
}
