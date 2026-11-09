# Firmware boards

The chip is the ESP32-S3: the S2, S3 and P4 have USB OTG, the C3 and C6 only USB-Serial-JTAG, and the plain ESP32 no
USB at all. The S3's two cores split the clock timer and Wi-Fi. PSRAM is not needed. Pins are in `firmware/main/board.h`.

## Seeed XIAO ESP32-S3, bare (`scripts/build.sh xiao`)

The module alone, for tests. The button is BOOT (GPIO0), the LED the yellow one on GPIO21 (one colour: only the timing
shows). MIDI: 3V3 through 33 ohm to ring / pin 4, D6 (GPIO43) through 10 ohm to tip / pin 5, GND to sleeve / pin 2.

**That wiring is weak.** The module has 499 ohm in series between GPIO43 and the D6 pad (v1.2 schematic, R6), so the
loop carries about 2.3 mA instead of about 6.4 mA (a calculation, not measured): below CA-033 and likely too little for
some receivers. GPIO43 also carries the ROM's boot log. `board.h` still uses it; the PCB uses GPIO1, 2, 5, 4 instead.

## The jank PCB (no firmware entry yet)

The XIAO on the board in `hardware/pcb`: four outputs, MIDI IN on OUT1, a button ladder, an encoder, two RGB LEDs and
a power cut. Its pin table and the board.h block it needs are in `hardware/pcb/README.md`, "The pin table".

## Flashing

The first flash of a blank XIAO needs download mode by hand: hold BOOT while plugging in (Seeed's usual way, not tried
here). Later, `bootloader` on the control line, the page's button or the button held 10 s should do it (unproven; [firmware.md](firmware.md)). Updates of a running stick need none of it: copy `jank.bin` onto the drive and eject ([usb.md](usb.md)). Then:

```
bash -c '. scripts/env.sh && cd "$JANK_SCRATCH/build-xiao" && python -m esptool --chip esp32s3 -b 460800 --before default_reset --after no_reset write_flash "@flash_args"'
```

Unplug and plug back in afterwards.
