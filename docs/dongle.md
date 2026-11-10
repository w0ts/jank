# A MIDI-powered jank (study, 2026-11-10)

The idea: a small board anyone orders assembled from JLCPCB, in a printed case. Two 3.5 mm jacks, plugged with TRS-to-DIN
adaptors: one into the gear's MIDI IN (jank's clock goes in), one into its MIDI OUT or THRU (power taken from it). One
button, USB-C to configure and update and as the power of last resort. With a computer on USB it is the stick (network
adapter, USB-MIDI, the drive). Nothing here is built or measured yet: figures marked *estimate* or *from the datasheet*.

## Power from a MIDI OUT

MIDI's spec gives a 5 mA current loop and no available current (CA-033, 2014). From its resistor values, with the
dongle drawing from pin 4 (and pin 5, high between notes) to pin 2, at a load of V:

| output | per pin | notes |
|---|---|---|
| 5 V, 220 ohm | (5 - V) / 220: 7.7 mA at 3.3 V, 4.5 mA at 4.0 V; 5.4 mA at 3.3 V at the low tolerance (4.5 V) | both pins: about twice |
| 3.3 V, 33 + 10 ohm | (3.3 - V) / 33 | nothing left above 3.3 V: such gear cannot run a 3.3 V chip; USB power |

Real gear varies: some outputs are weaker (MIDI Solutions needs 4 V, or 3 V for its newer boxes; a Digitakt rebooted a
MIDI-powered box). Budget: **under 5 mA average**. Measure the user's gear first (below).

## The radio only listens

jank keeps the Wi-Fi receiver on (60-90 mA): 10 to 20 times the budget. The dongle instead listens a few ms around a
**sync frame** and free-runs on its crystal in between, with the drift learned (`third_party/link`'s filter):

- A new frame on the air, beside today's (older units ignore it): session id, sequence number, tempo, and the session's
  beat when the frame really left. The sender learns that moment only after the channel was free (TX done), so it puts
  it in its next frame for the previous sequence number (two steps, as PTP). The receiver takes its own hardware receive
  timestamp (`rx_ctrl.timestamp`, us). One-way over air: microseconds of delay, known from the frame length.
- Sent on every beat by any jank in a session (FM-1s once Optimist adds it): the dongle needs one sender in the group.
- The dongle never transmits: no 300 mA peaks. It wakes for about 10 ms a second (*estimate*: Wi-Fi wake from light
  sleep is not documented anywhere; measure it).
- The first lock after power-on: the beat's place is unknown, so it listens in chunks (8 ms, then ~120 ms recharging),
  a few seconds once.

## Chips (datasheet figures at 3.3 V; prices are LCSC/JLC snapshots, not live; all Extended parts at JLC)

| | RX peak | light sleep | USB device (network adapter, USB-MIDI) | cores for the clock | module, 1 pc | average, listening design (*estimate*) |
|---|---|---|---|---|---|---|
| ESP32-C2 (ESP8684) | 65 mA | 140 uA | no USB | 1 | ESP8684-MINI-1 C3013916 ~$2.0-2.4 | ~1.8 mA |
| ESP32-C3 | 84 mA | 130 uA | Serial/JTAG only | 1 | C3-MINI-1-N4 C2838502 $1.3-3.1 (NRND?) | ~1.9 mA |
| ESP32-C6 | 78 mA | 180 uA | Serial/JTAG only | 1 + LP core with an LP UART | C6-MINI-1-N4 C5736265 $3.2-4.9 | ~1.3 mA |
| ESP32-S2 | 63 mA | 750 uA | OTG | 1 (+ ULP, bit-banged UART) | S2-MINI-2-N4 C3013906 ~$1.9 | ~2.1 mA |
| **ESP32-S3** | 88 mA | 240 uA | OTG (NCM + MIDI + MSC proven on the stick) | 2 (the clock alone on core 1) | S3-MINI-1-N8 C2913206 $3.9-4.7 | ~2.1-2.5 mA |

Sources: Espressif datasheets (C2 v2.3 T20/22, C3 v2.4 T5-7..9, C6 v1.5 T5-7..11, S2 v1.9 T5-7..9, S3 v2.2 T5-7..10),
ESP-IDF v5.5.5 low-power Wi-Fi guides, the USB Serial/JTAG console guide (fixed function: no custom USB classes).

**Choice: the ESP32-S3.** One board is both the stick (computer on USB) and the dongle (MIDI power, alone); the same code;
two cores keep the clock where it measured 0.3 us at the pin. The MIDI ticks in the dongle: the CPU wakes from light
sleep ~1.5 ms early (the RTC timer's wake has jitter) and waits for the exact microsecond. Fallback: the S2 (cheaper, one
core: the clock shares it, as on the stick before core 1 was given to it; 3x the light-sleep current).

## Power path

- Pins 4 and 5 of the power jack through Schottky diodes and a current limit (the source's drivers are not to be
  stressed) into a plain capacitor bank: ~1000 uF for the listening windows (0.6-0.8 mC each, *estimate*), a few mF for
  a slow start (the CPU at a low clock, the RF calibration kept in flash, not redone).
- A 60 nA buck (TPS62840, C2071859, Extended, works from 1.8 V) rather than an LDO: the bank may sag from ~4.7 V to
  ~3.5 V, and the input current is below the output's.
- USB-C wins when present (power and data). The firmware reads the bank's voltage and waits when it is low.
- No supercap: no SMD one found at JLC (the small ones are through-hole), and not needed for listening only.

## To measure first (on the XIAO ESP32-S3, on USB power, with a meter)

1. Wi-Fi from light sleep back to receive: time and charge. The whole budget rests on it.
2. Promiscuous receive across light sleep (re-entered each window?).
3. The MIDI tick timing when the CPU wakes for each one (the pin probe, `JANK_CLOCK_PROBE`).
4. The user's gear: each MIDI OUT's current into ~330 ohm (or a 3.3 V load) between pin 4 and pin 2, and pin 5 to pin 2,
   on 2-3 devices: the real budget.

Then: the sync frame in jank and Optimist, the board (S3-MINI-1, two PJ-320 jacks, one button, USB-C, the buck, the
cap bank), the case.
