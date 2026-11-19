/* SPDX-License-Identifier: GPL-3.0-only */
/* Pins by board. Choose the board in sdkconfig.<board>. BOARD_LED_RGB_GPIO >= 0: an addressable RGB LED (the PCB's
 * SK6812s), else BOARD_LED_MONO_GPIO is a plain LED, active low. BOARD_MIDI_SOURCE_GPIO >= 0: the MIDI source is a GPIO
 * driven high, else a 3V3 pin. */
#ifndef BOARD_H
#define BOARD_H
#include "sdkconfig.h"

#if CONFIG_JANK_BOARD_XIAO
#define BOARD_NAME "xiao"
/* D6 = GPIO43 for data; the board's 3V3 pad is the source. The module has 499 ohm in series on D6 (v1.2 schematic, R6):
 * MIDI current falls to about 2.3 mA (docs/boards.md); the PCB uses GPIO1, 2, 5, 4 instead. The button is BOOT, the LED the single yellow one. */
#define BOARD_BUTTON_GPIO 0
#define BOARD_BUTTON_ACTIVE_LOW 1
#define BOARD_LED_RGB_GPIO (-1)
#define BOARD_LED_MONO_GPIO 21
#define BOARD_MIDI_TX_GPIO 43
#define BOARD_MIDI_SOURCE_GPIO (-1)
/* USB host mode (usbrole.c): no CC chip, no 5 V of its own, no line in. Host mode through an OTG adapter, the gear
 * self-powered, or nothing. */
#define BOARD_CC_SCL_GPIO (-1)
#define BOARD_CC_SDA_GPIO (-1)
#define BOARD_CC_INT_GPIO (-1)
#define BOARD_HOST_EN_GPIO (-1)
#define BOARD_VBUS_SENSE_GPIO (-1)
#define BOARD_LINE_DETECT_GPIO (-1)
#define BOARD_PUMP_GPIO (-1)
#define BOARD_USBPORT_TUSB320 0
/* Bench outputs (midi.c, from the clock's interrupt, beside the UART byte): D1 = GPIO2 changes level on every MIDI clock
 * (F8), D0 = GPIO1 goes high with each MIDI start (FA) and low at the next clock. 3.3 V logic: into an audio interface
 * through 1 kohm (docs/midi.md). The XIAO only: the PCBs have real CLK / RST outputs (through the pump) on their own pins. */
#define BOARD_BENCH_CLK_GPIO 2
#define BOARD_BENCH_RST_GPIO 1
#define BOARD_LED_COUNT 1
#if CONFIG_JANK_UI_BENCH
/* The UI bench (Kconfig JANK_UI_BENCH, off by default): a 128 x 64 I2C OLED module (SSD1309, or SSD1306 with
 * JANK_UI_BENCH_SSD1306) on the XIAO's I2C pads, D4 = GPIO5 SDA, D5 = GPIO6 SCL, and four buttons to GND on free pads
 * (internal pull-ups): UP D3 = GPIO4, DOWN D8 = GPIO7, YES D9 = GPIO8, NO D10 = GPIO9 or the BOOT button. Not D0 / D1
 * (the bench CLK / RST above) nor D6 (MIDI). */
#define BOARD_I2C_SDA_GPIO 5
#define BOARD_I2C_SCL_GPIO 6
#define BOARD_BTN_UP_GPIO 4
#define BOARD_BTN_DOWN_GPIO 7
#define BOARD_BTN_YES_GPIO 8
#define BOARD_BTN_NO2_GPIO 9                      /* a second NO, beside BOOT (BOARD_BUTTON_GPIO) */
#define BOARD_OLED 1
#else
#define BOARD_I2C_SDA_GPIO (-1)
#define BOARD_I2C_SCL_GPIO (-1)
#define BOARD_BTN_UP_GPIO (-1)
#define BOARD_BTN_DOWN_GPIO (-1)
#define BOARD_BTN_YES_GPIO (-1)
#define BOARD_BTN_NO2_GPIO (-1)
#define BOARD_OLED 0
#endif
/* LINE IN: none on the XIAO. The bench option (Kconfig JANK_LINEIN_BENCH, sdkconfig.linein-bench) samples pad D3 = GPIO4
 * (ADC1_CH3; not a strapping pin, unused by this firmware; Seeed's XIAO ESP32S3 pin table) as a line input through a bias
 * network (docs/linein.md), so the LINE IN path can be tried on the XIAO. No cell, no plug detect: `set source line`. */
#if CONFIG_JANK_LINEIN_BENCH
#define BOARD_HAS_LINEIN 1
#define BOARD_LINEIN_GPIO 4
#else
#define BOARD_HAS_LINEIN 0
#endif
#define BOARD_CELL_GPIO (-1)
#elif CONFIG_JANK_BOARD_MINI22
#define BOARD_NAME "mini22"
/* jank mini 2.2, battery order (ESP32-S3-WROOM-1U-N8R2 since 2026-11-15; was the bare FH4R2), from hardware/mini2/parts.py and README.md on hw/mini2.2 (2026-11-15,
 * being routed: check again when it is ordered). Only what this firmware drives so far: NO (the BOOT button, SW1), the two
 * RGB LEDs (WS2812B-2020, GPIO35), the D-pad, the screen, MIDI OUT 1 (UART1 GPIO21, 68 ohm sinks, a 3V3 source pin), and the USB
 * host mode's parts. The usb order (no U17, no Q9) answers no CC chip: it runs as the XIAO does (HOST = a device first,
 * the host stack after 3 s with no computer; no 5 V of its own). */
#define BOARD_USBPORT_TUSB320 1                   /* the port HAL: usbport_tusb320.c */
#define BOARD_BUTTON_GPIO 0                       /* SW1: NO and BOOT, R16 up */
#define BOARD_BUTTON_ACTIVE_LOW 1
/* the D-pad (SW3 UP, SW4 DOWN, SW5 YES, to GND, internal pull-ups; NO is SW1 above) and the screen header J10 / J10B
 * (1.54" or 2.42" SSD1309 module) on the shared I2C, SCL GPIO11 / SDA GPIO12 with R60 / R61 10 k */
#define BOARD_BTN_UP_GPIO 10
#define BOARD_BTN_DOWN_GPIO 37
#define BOARD_BTN_YES_GPIO 36
#define BOARD_BTN_NO2_GPIO (-1)
#define BOARD_I2C_SCL_GPIO 11
#define BOARD_I2C_SDA_GPIO 12
#define BOARD_OLED 1                              /* looked for at the start: none answers -> the LED language */
/* the two WS2812B-2020 (chained, LED1 then LED2): GPIO35 since the WROOM-1U-N8R2 module (2026-11-15, was GPIO33 with
 * the bare chip; the charge pump that had GPIO35 is gone) */
#define BOARD_LED_RGB_GPIO 35
#define BOARD_LED_COUNT 2
#define BOARD_LED_MONO_GPIO (-1)
#define BOARD_MIDI_TX_GPIO 21
#define BOARD_MIDI_SOURCE_GPIO (-1)
#define BOARD_MIDI_TX_DRIVE GPIO_DRIVE_CAP_3      /* the board README's MIDI trade-off: drive 3 */
#define BOARD_CC_SCL_GPIO 11                      /* the shared I2C (R60 / R61 10 k up): U17 TUSB320LI at 0x47 */
#define BOARD_CC_SDA_GPIO 12
#define BOARD_CC_INT_GPIO 39                      /* U17's INT_N, open drain */
#define BOARD_HOST_EN_GPIO 42                     /* high: the 5 V signal on VBUS (Q8 -> Q7), R91 100 k down */
#define BOARD_VBUS_SENSE_GPIO 38                  /* the board's VBUS (after Q9) through R88 22 k / R89 33 k */
#define BOARD_LINE_DETECT_GPIO 16                 /* J7's switch, R38 1 M up: low = no plug */
#define BOARD_PUMP_GPIO (-1)                      /* the charge pump is gone (2026-11-15): pump_hold answers not supported, so
                                                     USB host mode's 5 V signal waits for the 5 V aux boost's enable pin */
/* LINE IN (linein.c): the LMV321 summer's output through R35 120 ohm / C36 100 nF to GPIO9 = ADC1_CH8; the cell through
 * R10 / R11 100 k / 100 k (C8 100 nF) to GPIO4 = ADC1_CH3, read in the same ADC pattern (TIMING.md trap 6). The usb order
 * holds GPIO4 at 0 V: no cell, no percent. The cell's empty point: ~3.3 V with the TPS63802 (POWER-STUDY.md, 2026-11-15) */
#define BOARD_HAS_LINEIN 1
#define BOARD_LINEIN_GPIO 9
#define BOARD_CELL_GPIO 4
#define BOARD_CELL_DIV 2                          /* the cell's volts / the pin's */
#define BOARD_CELL_EMPTY_MV 3300
#elif CONFIG_JANK_BOARD_LCD2
/* Waveshare ESP32-S3-LCD-2 (sku 30306, ESP32-S3R8, 16 MB flash), pins from the board's schematic (docs/lcd2.md has the
 * sources and what is not verified). The screen is the 240 x 320 ST7789 on SPI, drawn in landscape (lcd.c); no I2C OLED.
 * NO is the BOOT button (and GPIO21, beside it); the D-pad, MIDI TX and HOST_EN are on header pins chosen from the jank
 * mini 3 board layout (hardware/jank-mini-3/parts.py GPIO, the source of truth; docs/lcd2.md has the table; provisional
 * until the board is ordered). The board has no LED on a GPIO (its two LEDs are the charger's and the 3V3 rail's).
 * power.c drives GPIO4 V5_EN (5 V aux boost enable; R131 holds it ON until then), 2 RST and 15 CLK (idle low) at boot. The
 * other pins have no firmware use yet and stay undefined here: 8 LINE_SENSE is defined below but not sampled yet;
 * free: 12, 14 (no pad), 47, 48 (pads, the IMU
 * bus). */
#define BOARD_NAME "lcd2"
#define BOARD_LCD 1
#define BOARD_LCD_MOSI_GPIO 38
#define BOARD_LCD_SCLK_GPIO 39
#define BOARD_LCD_CS_GPIO 45
#define BOARD_LCD_DC_GPIO 42
#define BOARD_LCD_RST_GPIO (-1)                   /* the net LCD_RST is GPIO0 through R16 "NC/0R": the panel is reset by command */
#define BOARD_LCD_BL_GPIO 1                       /* high: backlight on (R12 1 k into an SS8050 that pulls LEDK down) */
#define BOARD_BUTTON_GPIO 0
#define BOARD_BUTTON_ACTIVE_LOW 1
#define BOARD_LED_RGB_GPIO (-1)
#define BOARD_LED_MONO_GPIO (-1)
#define BOARD_LED_COUNT 0
#define BOARD_MIDI_TX_GPIO 13                     /* the mini 3's MIDI OUT 1 (J16.8) */
#define BOARD_MIDI_TX_MORE_GPIOS {9}              /* the mini 3's MIDI OUT 2 (J16.12): the same UART TX, routed to both */
#define BOARD_MIDI_TX_INVERT 1                    /* the mini 3 sinks the loop through AO3400A gates: the line idles LOW */
#define BOARD_MIDI_SOURCE_GPIO (-1)
#define BOARD_I2C_SDA_GPIO (-1)
#define BOARD_I2C_SCL_GPIO (-1)
#define BOARD_BTN_UP_GPIO 11                      /* the mini 3: GPIO 11, J16.9 */
#define BOARD_BTN_DOWN_GPIO 6                     /* the mini 3: GPIO 6, J15.3 */
#define BOARD_BTN_YES_GPIO 16                     /* the mini 3: GPIO 16, J15.4 */
#define BOARD_BTN_NO2_GPIO 21                     /* a second NO, beside BOOT (BTN_NO of the mini 3: GPIO 21, J15.7) */
#define BOARD_OLED 0
#define BOARD_CC_SCL_GPIO (-1)
#define BOARD_CC_SDA_GPIO (-1)
#define BOARD_CC_INT_GPIO (-1)
#define BOARD_HOST_EN_GPIO 17                     /* the mini 3: high = the 5 V presence signal + Rp on J4, only through usbport_mini3.c
                                                     (VBUS sense low, setting HOST); R91 and the LCD-2's R6 hold it low */
#define BOARD_USBPORT_MINI3 1                     /* the port HAL: usbport_mini3.c (no CC chip, VBUS sense decides) */
#define BOARD_BRIDGE_SLOTS_PSRAM 1                /* bridge.c's 20 frame slots (30 KB) in PSRAM: internal DRAM is full with LINE IN */
#define BOARD_VBUS_SENSE_GPIO 10                  /* the mini 3's VBUS_SENSE (J15.10, parts.py): 10 k / 15 k, 3.0 V at 5 V on the LCD-2's own
                                                     USB-C; high = a computer or charger there. An ADC1 pin read as a plain digital input, no
                                                     internal pull (the board has its divider; usbrole.c input() sets none) */
#define BOARD_LINE_DETECT_GPIO 18                 /* the mini 3's LIN_DET (J15.6): J7's ring switch, R38 1 M up: high = a plug, low = none.
                                                     A plain input, no internal pull (the board has its own) */
#define BOARD_PUMP_GPIO (-1)
#define BOARD_USBPORT_TUSB320 0
/* LINE IN (linein.c): node A = the tip + ring mix on GPIO7 = ADC1 channel 6 (bias 1.467 V), first version A alone; node B
 * (the jack's sleeve) on GPIO8 = ADC1 channel 7 is for the later A - B step and is not sampled */
#define BOARD_HAS_LINEIN 1
#define BOARD_LINEIN_GPIO 7
#define BOARD_LINESENSE_GPIO 8
/* the cell: the LCD-2's own BAT_ADC, GPIO5 = ADC1 channel 4, R19 200 k / R20 100 k from VBAT (docs/lcd2.md): the pin sees a third,
 * 1.4 V at 4.2 V. Read in the line in's ADC pattern [LINE IN, cell], as on the mini 2.2 (linein_cell_step then does nothing) */
#define BOARD_CELL_GPIO 5
#define BOARD_CELL_DIV 3                          /* the cell's volts / the pin's */
#define BOARD_CELL_EMPTY_MV 3300
#define BOARD_V5_EN_GPIO 4                        /* the mini 3: the 5 V aux boost's enable (J15.2, parts.py V5_EN_IO) */
#define BOARD_CLK_GPIO 15                         /* the mini 3: CLK out (J16.7, parts.py CLK_IO); the XIAO bench's BENCH_CLK is another thing */
#define BOARD_RST_GPIO 2                          /* the mini 3: RST out (J15.1, parts.py RST_IO) */
#else
#error "no board chosen"
#endif
#ifndef BOARD_V5_EN_GPIO
#define BOARD_V5_EN_GPIO (-1)
#define BOARD_CLK_GPIO (-1)
#define BOARD_RST_GPIO (-1)
#endif
#ifndef BOARD_LCD
#define BOARD_LCD 0
#endif
#ifndef BOARD_USBPORT_MINI3
#define BOARD_USBPORT_MINI3 0
#endif
#ifndef BOARD_BRIDGE_SLOTS_PSRAM
#define BOARD_BRIDGE_SLOTS_PSRAM 0
#endif
#endif
