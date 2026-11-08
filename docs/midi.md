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

## Timing (estimates, none measured)

| | Expected |
|---|---|
| Timer to UART, tick to tick | Under about 50 us: interrupt latency, plus up to one 32 us bit |
| Clock against the session | Link's own measurement error plus the above; steps when Link re-measures (30 s), then slews |
| Bar start | The `0xF8` goes 320 us after `0xFA`, one byte on the wire |
| Bridge, one way | 2 to 4 ms: USB, about 1 ms of air for 100 bytes at 1 Mb/s, the queue |

`status` reports the stick's own part as `lat_air_max_us` and `lat_usb_max_us`.

## Wiring a bare board

MIDI cannot power the stick (a port gives about 5 mA); power comes from USB. Values from
[CA-033](https://midi.org/wp-content/uploads/wpforo/default_attachments/1709416667-ca33-MIDI-10-Electrical-Specification-Update.pdf)
for 3.3 V: 33 ohm 0.5 W on the source line, 10 ohm 0.25 W on the data line. TRS type A: data on tip, source on ring,
ground on sleeve; DIN: data pin 5, source pin 4, ground pin 2. Type B gear swaps tip and ring. See
[boards.md](boards.md) for each board's pins.
