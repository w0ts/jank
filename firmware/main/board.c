/* SPDX-License-Identifier: GPL-3.0-only */
/* The boards' pin tables and, in the auto build, which board this is (board.h). The values are the ones board.h held
 * per build until 2026-11-22; their sources stay in the comments. */
#include "board.h"
#include "esp_attr.h"
#include "esp_flash.h"
#include "esp_log.h"

static const char *TAG = "board";

#if CONFIG_JANK_BOARD_XIAO || CONFIG_JANK_BOARD_AUTO
/* Seeed XIAO ESP32S3. D6 = GPIO43 for MIDI data, the board's 3V3 pad the source; the module has 499 ohm in series on D6
 * (v1.2 schematic, R6): MIDI current about 2.3 mA (docs/boards.md). The button is BOOT, the LED the single yellow one
 * (GPIO21, active low). No CC chip, no 5 V of its own, no line in: USB host mode through an OTG adapter with self-powered
 * gear. Bench outputs (midi.c): D1 = GPIO2 changes level on every MIDI clock, D0 = GPIO1 goes high with each start
 * (docs/midi.md). The UI bench (JANK_UI_BENCH): a 128 x 64 I2C OLED on D4 = GPIO5 SDA / D5 = GPIO6 SCL, UP D3 = GPIO4,
 * DOWN D8 = GPIO7, YES D9 = GPIO8, NO D10 = GPIO9 or BOOT. The LINE IN bench (JANK_LINEIN_BENCH): pad D3 = GPIO4
 * (ADC1_CH3) through a bias network (docs/linein.md), no cell, no plug detect. */
static DRAM_ATTR const struct board XIAO = {
    .name = "xiao", .model = "XIAO",
    .button_gpio = 0, .button_active_low = 1,
    .led_rgb_gpio = -1, .led_mono_gpio = 21, .led_count = 1,
    .midi_tx_gpio = 43, .midi_source_gpio = -1, .midi_tx_more = {-1, -1}, .midi_tx_invert = 0, .midi_tx_drive = -1,
    .bench_clk_gpio = 2, .bench_rst_gpio = 1,
    .clk_gpio = -1, .rst_gpio = -1, .v5_en_gpio = -1,
#if CONFIG_JANK_UI_BENCH
    .i2c_sda_gpio = 5, .i2c_scl_gpio = 6,
    .btn_up_gpio = 4, .btn_down_gpio = 7, .btn_yes_gpio = 8, .btn_no2_gpio = 9, .oled = 1,
#else
    .i2c_sda_gpio = -1, .i2c_scl_gpio = -1,
    .btn_up_gpio = -1, .btn_down_gpio = -1, .btn_yes_gpio = -1, .btn_no2_gpio = -1, .oled = 0,
#endif
    .lcd = 0, .lcd_mosi_gpio = -1, .lcd_sclk_gpio = -1, .lcd_cs_gpio = -1, .lcd_dc_gpio = -1, .lcd_rst_gpio = -1, .lcd_bl_gpio = -1,
    .cc_scl_gpio = -1, .cc_sda_gpio = -1, .cc_int_gpio = -1,
    .host_en_gpio = -1, .vbus_sense_gpio = -1, .line_detect_gpio = -1, .pump_gpio = -1,
    .usbport_tusb320 = 0, .usbport_mini3 = 0,
    .bridge_slots_psram = 1,              /* (2026-11-22: with both boards' drivers in one image the 30 KB no longer fit in
                                             internal RAM: ui_start failed with ESP_ERR_NO_MEM, measured with `diag`) */
    .host_audio = 0,                      /* host mode: no audio client, no beat tracker: the Digitone and the Launchpad together need the RAM
                                             (the second one's claim failed with ESP_ERR_NO_MEM, 2026-11-24); the tracker did not run there anyway */
#if CONFIG_JANK_LINEIN_BENCH
    .has_linein = 1, .linein_gpio = 4,
#else
    .has_linein = 0, .linein_gpio = -1,
#endif
    .linesense_gpio = -1, .cell_gpio = -1, .cell_div = 1, .cell_empty_mv = 0,
};
#endif

#if CONFIG_JANK_BOARD_LCD2 || CONFIG_JANK_BOARD_AUTO
/* Waveshare ESP32-S3-LCD-2 (sku 30306, ESP32-S3R8, 16 MB flash) on the jank mini 3, pins from the LCD-2's schematic and
 * hardware/jank-mini-3/parts.py (docs/lcd2.md has the sources and what is not verified). The screen: 240 x 320 ST7789 on
 * SPI, landscape (lcd.c); LCD_RST is GPIO0 through R16 "NC/0R" (the panel is reset by command); backlight GPIO1 high.
 * NO is BOOT and GPIO21; MIDI OUT 1 GPIO13 (J16.8) and OUT 2 GPIO9 (J16.12) from one UART TX, inverted (AO3400A sinks: the
 * line idles low). HOST_EN 17 only through usbport_mini3.c; VBUS_SENSE 10 (10 k / 15 k: 3.0 V at 5 V); LIN_DET 18 (high =
 * a plug); LINE IN node A GPIO7 (ADC1_CH6), node B GPIO8 (not sampled yet); the cell on BAT_ADC GPIO5 (R19 200 k / R20
 * 100 k: a third); V5_EN 4, CLK 15, RST 2 (power.c drives them at boot). Bridge slots in PSRAM: internal DRAM is full. */
static DRAM_ATTR const struct board LCD2 = {
    .name = "lcd2", .model = "LCD2",
    .button_gpio = 0, .button_active_low = 1,
    .led_rgb_gpio = -1, .led_mono_gpio = -1, .led_count = 0,
    .midi_tx_gpio = 13, .midi_source_gpio = -1, .midi_tx_more = {9, -1}, .midi_tx_invert = 1, .midi_tx_drive = -1,
    .bench_clk_gpio = -1, .bench_rst_gpio = -1,
    .clk_gpio = 15, .rst_gpio = 2, .v5_en_gpio = 4,
    .i2c_sda_gpio = -1, .i2c_scl_gpio = -1,
    .btn_up_gpio = 11, .btn_down_gpio = 6, .btn_yes_gpio = 16, .btn_no2_gpio = 21, .oled = 0,
    .lcd = 1, .lcd_mosi_gpio = 38, .lcd_sclk_gpio = 39, .lcd_cs_gpio = 45, .lcd_dc_gpio = 42, .lcd_rst_gpio = -1, .lcd_bl_gpio = 1,
    .cc_scl_gpio = -1, .cc_sda_gpio = -1, .cc_int_gpio = -1,
    .host_en_gpio = 17, .vbus_sense_gpio = 10, .line_detect_gpio = 18, .pump_gpio = -1,
    .usbport_tusb320 = 0, .usbport_mini3 = 1, .bridge_slots_psram = 1, .host_audio = 1,
    .has_linein = 1, .linein_gpio = 7, .linesense_gpio = 8,
    .cell_gpio = 5, .cell_div = 3, .cell_empty_mv = 3300,
};
#endif

#if CONFIG_JANK_BOARD_MINI22
/* jank mini 2.2 (ESP32-S3-WROOM-1U-N8R2), from hardware/mini2/parts.py and README.md on hw/mini2.2 (2026-11-15): NO = BOOT
 * (SW1), two WS2812B-2020 on GPIO35, the D-pad 10 / 37 / 36 to GND, the screen J10 on I2C SCL 11 / SDA 12 (R60 / R61 10 k),
 * MIDI OUT 1 UART GPIO21 (68 ohm sinks, drive 3), U17 TUSB320LI at 0x47 on the same I2C with INT_N 39, HOST_EN 42, VBUS
 * sense 38 (R88 22 k / R89 33 k), LINE IN plug switch 16 (low = no plug), LINE IN GPIO9 (ADC1_CH8), the cell GPIO4 (ADC1_CH3,
 * R10 / R11 100 k / 100 k), empty at ~3.3 V (the TPS63802). */
static DRAM_ATTR const struct board MINI22 = {
    .name = "mini22", .model = "MINI 2.2",
    .button_gpio = 0, .button_active_low = 1,
    .led_rgb_gpio = 35, .led_mono_gpio = -1, .led_count = 2,
    .midi_tx_gpio = 21, .midi_source_gpio = -1, .midi_tx_more = {-1, -1}, .midi_tx_invert = 0, .midi_tx_drive = 3,
    .bench_clk_gpio = -1, .bench_rst_gpio = -1,
    .clk_gpio = -1, .rst_gpio = -1, .v5_en_gpio = -1,
    .i2c_sda_gpio = 12, .i2c_scl_gpio = 11,
    .btn_up_gpio = 10, .btn_down_gpio = 37, .btn_yes_gpio = 36, .btn_no2_gpio = -1, .oled = 1,
    .lcd = 0, .lcd_mosi_gpio = -1, .lcd_sclk_gpio = -1, .lcd_cs_gpio = -1, .lcd_dc_gpio = -1, .lcd_rst_gpio = -1, .lcd_bl_gpio = -1,
    .cc_scl_gpio = 11, .cc_sda_gpio = 12, .cc_int_gpio = 39,
    .host_en_gpio = 42, .vbus_sense_gpio = 38, .line_detect_gpio = 16, .pump_gpio = -1,
    .usbport_tusb320 = 1, .usbport_mini3 = 0, .bridge_slots_psram = 0, .host_audio = 1,
    .has_linein = 1, .linein_gpio = 9, .linesense_gpio = -1,
    .cell_gpio = 4, .cell_div = 2, .cell_empty_mv = 3300,
};
#endif

#if CONFIG_JANK_BOARD_XIAO || CONFIG_JANK_BOARD_AUTO
DRAM_ATTR const struct board *g_board = &XIAO;
#elif CONFIG_JANK_BOARD_LCD2
DRAM_ATTR const struct board *g_board = &LCD2;
#elif CONFIG_JANK_BOARD_MINI22
DRAM_ATTR const struct board *g_board = &MINI22;
#else
#error "no board chosen"
#endif

static const char *s_how = "built for it";

const char *board_how(void)
{
    return s_how;
}

#if CONFIG_JANK_BOARD_AUTO
/* The two boards differ in their flash chip: the LCD-2 has a 16 MB W25Q128 (docs/lcd2.md, its schematic U5), the XIAO 8 MB
 * (one measured 2026-11-22: GigaDevice c8 4017, 8 MB). The chip says its own size: no pin is touched. (A first version
 * probed the LCD-2's motion sensor on GPIO47 / 48; on the XIAO that hung the start, measured 2026-11-22.) A XIAO "Plus"
 * (16 MB) would be taken for the LCD-2: it is not a supported board. */
void board_init(void)
{
    uint32_t flash = 0;
    if (esp_flash_get_physical_size(NULL, &flash) != ESP_OK)
        flash = 0;
    if (flash >= 16u * 1024u * 1024u) {
        g_board = &LCD2;
        s_how = "flash 16 MB";
    } else {
        g_board = &XIAO;
        s_how = flash ? "flash 8 MB" : "default";
    }
    ESP_LOGI(TAG, "%s (%s; flash %lu MB)", g_board->name, s_how, (unsigned long)(flash >> 20));
}
#else
void board_init(void)
{
    ESP_LOGI(TAG, "%s (built for it)", g_board->name);
}
#endif
