# Firmware boards

All are ESP32-S3: the S2, S3 and P4 have USB OTG, the C3 and C6 only USB-Serial-JTAG, and the plain ESP32 no USB at
all. The S3's two cores split the clock timer and Wi-Fi. PSRAM is not needed. Pins are in `firmware/main/board.h`.

## M5Stack AtomS3U (`scripts/build.sh atoms3u`)

A cased USB-A stick with a side button (G41), a WS2812 LED (G35) and a Grove port for MIDI:

| Grove wire | Through | TRS type A | DIN |
|---|---|---|---|
| White, G1 (source, driven high) | 33 ohm | Ring | Pin 4 |
| Yellow, G2 (UART TX) | 10 ohm | Tip | Pin 5 |
| Black, GND | | Sleeve | Pin 2 |

Leave the red 5 V wire unconnected. The port has 5 V but no 3.3 V, so G1 driven high is the source: a deviation from
CA-033, unmeasured. The data line must not use a 5 V source. The jack sits outside the case on the Grove cable.

## Seeed XIAO ESP32-S3 (`scripts/build.sh xiao`)

The button is BOOT (GPIO0), the LED the yellow one on GPIO21 (one colour: only the timing shows). MIDI: 3V3 through
33 ohm to ring / pin 4, D6 (GPIO43) through 10 ohm to tip / pin 5, GND to sleeve / pin 2.

**That wiring is weak.** The module has 499 ohm in series between GPIO43 and the D6 pad (v1.2 schematic, R6), so the
loop carries about 2.3 mA instead of about 6.4 mA (a calculation, not measured): below CA-033 and likely too little for
some receivers. GPIO43 also carries the ROM's boot log. `board.h` still uses it; the PCB uses GPIO1, 2, 5, 4 instead.

## The jank PCB (no firmware entry yet)

The XIAO on the board in `hardware/pcb`: four outputs, MIDI IN on OUT1, a button ladder, an encoder, two RGB LEDs and
a power cut. Its pin table and the board.h block it needs are in `hardware/pcb/README.md`, "The pin table".

## Flashing

The first flash of a blank board needs download mode by hand: on the AtomS3U hold the side RESET button until the
internal green LED lights; on the XIAO hold BOOT while plugging in (Seeed's usual way, not tried here). Later, `bootloader` on the control line, the
page's button or the button held 10 s should do it (unproven on any S3). Then:

```
bash -c '. scripts/env.sh && cd "$JANK_SCRATCH/build-atoms3u" && python -m esptool --chip esp32s3 -b 460800 --before default_reset --after no_reset write_flash "@flash_args"'
```

(`build-xiao` for the XIAO.) Unplug and plug back in afterwards.
