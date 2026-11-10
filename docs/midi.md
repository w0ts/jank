# MIDI clock

## How it runs (`core/jank_clock.c`, `firmware/main/midi.c`)

Every 5 ms a task turns the Link session into a straight line of tick times (24 PPQN). A 1 MHz hardware timer's
interrupt, in IRAM at level 3 on core 1 (Wi-Fi is on core 0), writes each byte straight into UART1's FIFO at 31250
baud, with multiply and shift only. Small session moves slew by up to half a tick per tick; moves over 6 ticks jump
(`JC_RESYNC_TICKS`). A board may list more pads for the same TX signal (`BOARD_MIDI_TX_MORE_GPIOS`, routed through the
GPIO matrix); no board uses it yet.

`transport link` follows Link's Start Stop Sync; `transport auto` runs whenever a session is joined. `offset` moves the
clock against the session by -100..100 ms, for gear that lags. `quantum` (1..16, default 4) is the bar for start.

## Resync (a click)

The click is recognised 350 ms after the release (a second press could still come). Then:

1. Before the next clock, up to one tick later (20.8 ms at 120 BPM): `0xFC`.
2. `0xF8` carries on at every tick; it never stops.
3. At the first bar line after that tick: `0xFA`, then that bar line's `0xF8`.

The bar is `quantum` beats on Link's own beat count, so the receiver lands on the session's downbeat; the wait is at
most one bar. A click exactly on a bar line waits for the next one: stop and start never share a tick. A click starts
stopped gear too, which stays started until the session's next stop.

## Timing (measured 2026-11-09, XIAO ESP32-S3, 120 BPM, in a session with Live and FM-1s)

**At the pin** (the MIDI TX pad, against the stick's own grid; a test build, `JANK_CLOCK_PROBE=1 scripts/build.sh xiao`,
`probe` on the control line: the MCPWM capture unit timestamps the start bit at 80 MHz, `firmware/main/clkprobe.c`):

| 2 min, 5760 ticks | mean | sd | worst |
|---|---|---|---|
| start bit after the tick | 0.30 us | 0.07 us | 2.3 us |

What it took: the alarm goes 2 us early (`CLK_LEAD_US`: interrupt entry ~2.0 us, the UART 0.3 us, which starts the byte at
once, not on a bit boundary), and core 1 runs the clock interrupt alone: with TinyUSB's task on core 1 the worst tick was
37 us (a critical section there masks the level-3 interrupt).

**Over USB-MIDI** (the Mac's Link library against arrival times, `aalink`): sd 110 to 240 us, p99 about 1 ms, a mean of
0.4 to 0.6 ms late. USB full speed polls every 1 ms, so this path is bounded by USB; a computer is better served by Link
itself, over the stick's network adapter.

**Against Live** (2026-11-10: the pin through 1 kohm into a MOTU input, Live's clock out into another, both over ADAT
into the Fireface at 44.1 kHz, edges interpolated between samples; the stick's ticks against Live's 16th pulses, 60 s):

| path to Live | offset | stick - Live, mean | sd | p1 .. p99 |
|---|---|---|---|---|
| the stick's USB network adapter | 0 | -1268 us | 33 us | -1351 .. -1221 us |
| the stick's USB network adapter | +1.27 ms | +42 us | 44 us | -24 .. +181 us |
| Wi-Fi through a router, the stick on a charger | +1.27 ms | +305 us | 94 us | +205 .. +560 us |

Live's own pulses wobble 24 to 48 us (sd) against a straight line. The ~1 ms constant is mostly Live's (its output
latency as the driver reports it); the path adds ~0.26 ms (Link's round-trip estimate over uneven Wi-Fi delays). Over the
air the stick's grid wanders more (78 us sd against its own straight line, 23 us over USB): Link's time estimate between
measurements, the next thing to improve. MODE JANK's direct frames with an FM-1 are not measured yet.

`status` reports the stick's own part as `lat_air_max_us` and `lat_usb_max_us`.

## Wiring a bare board

MIDI cannot power the stick (a port gives about 5 mA); power comes from USB. Values from
[CA-033](https://midi.org/wp-content/uploads/wpforo/default_attachments/1709416667-ca33-MIDI-10-Electrical-Specification-Update.pdf)
for 3.3 V: 33 ohm 0.5 W on the source line, 10 ohm 0.25 W on the data line. TRS type A: data on tip, source on ring,
ground on sleeve; DIN: data pin 5, source pin 4, ground pin 2. Type B gear swaps tip and ring. See
[boards.md](boards.md) for each board's pins.
