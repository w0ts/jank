# jank on the Waveshare ESP32-S3-LCD-2 (board `lcd2`)

Stage 1 (2026-11-17): the firmware builds for the board (`scripts/build.sh lcd2`); nothing has run on hardware. The screen
is the same 128 x 64 1-bit UI as the OLED's, drawn in landscape (320 x 240). Stage 1 drew it in one foreground colour;
stage 2 (2026-11-17) draws it in the approved colour design: neutral near-white on near-black, with colour only where it informs
(docs/lcd2-colors.md: the confidence number, link glyph, signal bars, battery, input level, the bar's first beat while it is lit).

## Sources (read in this session)

| Short name | What | Where |
|---|---|---|
| wiki | Waveshare wiki page for the ESP32-S3-LCD-2 | https://www.waveshare.com/wiki/ESP32-S3-LCD-2 (fetched; it lists no pins) |
| SCH | the schematic, one A4 sheet titled "ESP32-S3-Touch-LCD-2" | https://files.waveshare.com/wiki/ESP32-S3-LCD-2/ESP32-S3-Touch-LCD-2-SchDoc.pdf (downloaded, the GPIO table, the LCD block, the PinOut block and U2 / U5 read from the rendered page) |
| demo | Waveshare's demo package, ESP-IDF projects | https://files.waveshare.com/wiki/ESP32-S3-LCD-2/ESP32-S3-LCD-2-Demo.zip (downloaded; `ESP-IDF/06_lvgl_example/main/main.c` and its `sdkconfig`, `ESP-IDF/03_lvgl_brightness`, `ESP-IDF/04_lvgl_battery`) |

## Verified pins

| Function | GPIO | Where it comes from |
|---|---|---|
| LCD MOSI (SDA) | 38 | SCH GPIO table, row IO38 "LCD_MOSI" (shared with SD_MOSI); demo `EXAMPLE_PIN_NUM_MOSI 38` |
| LCD SCLK (SCL) | 39 | SCH row IO39 "LCD_SCLK"; demo `EXAMPLE_PIN_NUM_SCLK 39` |
| LCD CS | 45 | SCH row IO45 "LCD_CS", LCD block: LCD_CS through R18 0R to J3 pin 13; demo `EXAMPLE_PIN_NUM_LCD_CS 45` |
| LCD DC (RS) | 42 | SCH row IO42 "LCD_DC"; demo `EXAMPLE_PIN_NUM_LCD_DC 42` |
| LCD RST | none (software reset) | SCH row IO0 lists "LCD_RST"; LCD block: net LCD_RST goes through R16 "NC/0R" to the panel's RESET (J3 pin 8), with R14 10 k up to 3V3 and C30 "100nF/NC". The demo passes `EXAMPLE_PIN_NUM_LCD_RST -1`. We do the same (see UNVERIFIED 1) |
| Backlight | 1 | SCH row IO1 "LCD_BL"; LCD block: LCD_BL, R12 1 k, base of T1 SS8050 (NPN) whose collector pulls LEDK to GND, LEDA from 3V3 through R11 6.8 R. **Active high**: 1 = on. Demo `EXAMPLE_PIN_NUM_BK_LIGHT 1`, `gpio_set_level(..., 1)` (and LEDC PWM on the same pin in `03_lvgl_brightness`) |
| BOOT button | 0 | SCH row IO0 "Other: BOOT"; BOOT-button block (Key2 net "BOOT"). jank's NO button |
| USB D- / D+ | 19 / 20 | SCH rows IO19 "USB_N", IO20 "USB_P"; TYPE C block: USB_P / USB_N through R23 / R26 22 R to the 16-pin USB-C (H1). Native USB, the same pins as the XIAO |
| UART0 TXD / RXD | 43 / 44 | SCH rows IO43 "U0_TXD", IO44 "U0_RXD"; header P2 pins 3 and 4. jank's MIDI TX stays GPIO43, as on the XIAO |
| IMU I2C SCL / SDA | 47 / 48 | SCH rows IO47 / IO48 (also "TP_SCL" / "TP_SDA" in the LCD column); header P2 pins 5 and 6. IMU INT1 is IO3 |
| Battery ADC | 5 | SCH row IO5 "BAT_ADC": R19 200 k / R20 100 k from VBAT (BAT block); demo `04_lvgl_battery` `ADC_CHANNEL_4` (ADC1 channel 4 is GPIO5). Not used by this firmware yet |
| Octal PSRAM (not usable) | 33 .. 37 | SCH GPIO table lists IO33 .. IO37 with no function (shown in a different colour), U2 pins crossed out; PSRAM clock / CS in the demo's sdkconfig: `SPIRAM_CLK_IO 30`, `SPIRAM_CS_IO 26` |
| SD card | 38 (MOSI), 39 (SCLK), 40 (MISO), 41 (CS) | SCH SD_Card column; the SD card shares the LCD's SPI pins |
| Touch panel connector | 46 INT, 47 SCL, 48 SDA | SCH LCD column (TP_INT, TP_SCL, TP_SDA); the sku 30306 has no touch |

Headers (SCH PinOut block, P1 pins 1 to 14: IO2, IO4, IO6, IO16, IO17, IO18, IO21, IO8, IO7, IO10, IO20, IO19, GND, 5V;
P2 pins 1 to 14: 3V3, GND, IO43, IO44, IO47, IO48, IO15, IO13, IO11, IO12, IO14, IO9, GND, VBAT).

LEDs (SCH): LED1 is the charger's STAT indicator (ETA6098), LED2 the 3V3 rail's power LED. **No LED is on a GPIO**: the
board has no user LED (`BOARD_LED_MONO_GPIO` is -1; the screen is the language).

### Conflicts with what jank uses

| jank signal (XIAO / mini 2.2, firmware/main/board.h) | On the lcd2 |
|---|---|
| USB D- / D+ GPIO19 / GPIO20 | same pins, on the board's own USB-C |
| MIDI TX GPIO43 (XIAO; the mini 2.2 uses 21) | GPIO43 is on header P2.3 and free (console is off: `CONFIG_ESP_CONSOLE_NONE`) |
| BOOT button GPIO0 | same pin |
| XIAO LED GPIO21 | GPIO21 is header P1.7, no LED there |
| bench CLK / RST GPIO2 / GPIO1 (XIAO only) | not defined on the lcd2: GPIO1 is the backlight |
| UI bench D-pad GPIO4 / 7 / 8 / 9 and I2C 5 / 6 | not used: 4, 6, 7, 8, 9 are free header pins but no D-pad pins are chosen yet; GPIO5 is the battery divider |
| mini 2.2 pins GPIO35, 36, 37 (LEDs, D-pad), 38, 39, 42 (VBUS sense, CC INT, host enable) | **taken on the lcd2**: 35 .. 37 by the PSRAM, 38 / 39 / 42 by the LCD |
| mini 2.2's GPIO10, 11, 12, 16, 21, 4, 9 | free on the headers (P1: 10, 16, 21, 4; P2: 11, 12, 9) |

The firmware's `BOARD_BTN_UP/DOWN/YES` are -1 on `lcd2` until the PCB chooses header pins; with only BOOT the menus cannot
be walked, so this board is a screen and MIDI bench for now.

## Memory

| Item | Value | Source |
|---|---|---|
| Flash | 16 MB, a W25Q128JVSI | SCH U5 "W25Q128JVSI"; wiki "External 16MB Flash"; demo sdkconfig `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y` |
| Flash mode | not decided here: left at the IDF default (DIO) | demo sdkconfig has `CONFIG_ESPTOOLPY_FLASHMODE="dio"`, 80 MHz |
| PSRAM | 8 MB, octal, 80 MHz | SCH U2 "ESP32-S3R8"; wiki "Onboard 8MB PSRAM"; demo sdkconfig `CONFIG_SPIRAM_MODE_OCT=y`, `CONFIG_SPIRAM_SPEED_80M=y` |

`firmware/sdkconfig.lcd2` sets `FLASHSIZE_16MB` and the octal PSRAM (copied from `sdkconfig.xiao`, the XIAO's is octal
too). The partition table is `partitions_usb.csv` unchanged (two 2 MB app slots, otadata, 2 MB drive): it ends at
0x620000 and is valid in 16 MB; the rest of the flash stays unused. The XIAO's partitions are untouched.

## Panel setup (esp_lcd, ST7789)

From the demo's `display_init` (`06_lvgl_example/main/main.c`): SPI2, mode 0, 8-bit command and parameter,
`esp_lcd_new_panel_st7789` with `LCD_RGB_ELEMENT_ORDER_RGB`, 16 bits per pixel, `esp_lcd_panel_invert_color(true)`,
display on. The demo's LVGL has `CONFIG_LV_COLOR_16_SWAP=y`, i.e. the panel wants the high byte first: `lcd.c` swaps the
foreground colour once. The controller is an ST7789T3 (wiki: "Embedded with ST7789T3 driver chip, using SPI interface").
Portrait is 240 x 320; landscape is `swap_xy(true)` plus the mirror switches `JANK_LCD_MIRROR_X` (default on) and
`JANK_LCD_MIRROR_Y` (default off).

Colours: the palette in `core/jank_lcd.h` (`JLC_PALETTE`, the RGB of `ui/lcd2.js`), converted to RGB565 once in `core/jank_lcd.c`. Nothing is
measured on the panel: the colours as it shows them, its gamma and viewing angle are unknown (the values are the sRGB design values).

## Drawing

The lcd2 layout has its own 160 x 120 grid (ui/oled.js, `ctx.layout 'lcd2'`; C: `core/jank_oled_lcd2.c`, `jo_draw_lcd2`), painted **exactly x2**
on 320 x 240: no bands, no centring (`core/jank_lcd.c`, host-tested in `tests/test_lcd.c`). The frame is 160 x 120 bits (2.4 KB, page
layout), the ink plane 160 x 120 bytes (19.2 KB). Colours: the palette in `core/jank_lcd.h` (`JLC_PALETTE`, `JLC_INK_MAP`), checked against `ui/lcd2.js` by `tests/test_lcd_palette.js`, converted to RGB565 once. Nothing is measured on the panel
(colours as shown, gamma, viewing angle).

Only the changed 16-row strips are sent (the first frame in full); SPI clock 40 MHz; the task runs on core 0 at priority 1.

RAM (from the linked image): the three frames 3 x 2.4 KB and the DMA chunk buffers 5 KB are internal `.bss` / `.data`; the four ink planes
(UI, pending, being sent, shown: 76.8 KB) are heap, from PSRAM when the board has it (else internal, where the allocation may fail). Not
observed: whether they land in PSRAM.

## UNVERIFIED

1. **Whether R16 is fitted.** The schematic marks R16 "NC/0R" (not fitted, or 0 ohm). If it is fitted, the panel's RESET
   is wired to GPIO0, the BOOT button: pressing NO would then reset the panel (and the 10 k R14 / C30 are on that net).
   The demo passes reset -1, so the firmware does too; check on the bench by pressing BOOT while the screen shows
   something.
2. Which landscape orientation is upright for the way the board is held (the mirror switches exist for it). Not derivable
   from the documents read.
3. The panel's maximum SPI clock on this board: the demo runs 80 MHz; 40 MHz is the firmware's choice, not tested.
4. The wiki page is silent on pins; it calls the USB port "Type-C" in the text and "USB Type-A" in the specification
   table (the schematic has a 16-pin USB-C).
5. The schematic is titled "ESP32-S3-Touch-LCD-2" and includes the camera connector and touch pins; that sku 30306 (no
   touch, no camera) has the same netlist for the pins above is assumed, not read.
6. The flash mode (the demo says "dio" with the sdkconfig's QIO choice also set); the flash size and the PSRAM were not
   read from the chip (no device access).
7. IO0, IO45 and IO46 are strapping pins and IO45 / IO0 are in use by the LCD CS and the BOOT button: the demo works with
   this, but a jank-specific effect at reset (a pin held low by the screen or a button) has not been looked at.
8. What the LCD costs, measured: CPU on core 0, SPI/DMA bus time per frame, internal RAM after start-up (`s_buf` is
   5 KB, the task stack 3 KB, the SPI driver's own allocations are not counted), and whether the MIDI clock's jitter on
   core 1 changes (docs/midi.md). None of it was run.
9. The charge-and-power behaviour of the board (ETA6098, VBAT, the USB-C CC resistors): not part of this stage, see the
   project memory note.
