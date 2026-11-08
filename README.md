# jank

jank is an ESP32-S3 device that joins the FM-1 synth's Wi-Fi group mode as an Ableton Link peer. It is a MIDI clock
(with start/stop and tempo) for MIDI gear, a USB network adapter that puts a computer running Link on the group, a
USB-MIDI host for gear with a USB port, and, on the PCB, a MIDI thru box (OUT1 becomes a MIDI IN in software; that
firmware is not written yet). The mode is called
**jank** (MODE JANK); FM-1 firmware from before the rename shows it as MODE JAM.

Link is Ableton's trademark; jank is not an Ableton product.

> **Status:** no S3 build has been flashed, and the PCB is not ordered. See [What is proved](#what-is-proved).

## Hardware

- [`hardware/pcb`](hardware/pcb/README.md): a carrier for a Seeed XIAO ESP32-S3 on headers, 84 x 52 mm. Four MIDI
  outputs, each a DIN or a 3.5 mm TRS in one footprint with a TRS A/B switch; OUT1 doubles as MIDI IN. MODE and
  PLAY/STOP buttons and an EC11 push encoder on one resistor ladder, two RGB LEDs, a power-only USB-C with an ideal
  diode and a firmware-controlled cut. Prices: [`BOM.md`](hardware/pcb/BOM.md).
- [`hardware/box`](hardware/box/README.md): its printed box (OpenSCAD), with the DIN and TRS sockets braced by the box.
- `hardware/check_stl.py`: checks that each STL is one closed solid (used by both make scripts).

## Firmware boards

| Board | Build | Notes |
|---|---|---|
| Seeed XIAO ESP32-S3, bare | `scripts/build.sh xiao` | The module alone: BOOT button, yellow LED; MIDI on D6, which is weak (see below) |
| jank PCB | not written yet | The XIAO on the board: pins in [`hardware/pcb/README.md`](hardware/pcb/README.md) |

Wiring and pins: [docs/boards.md](docs/boards.md).

## Build and flash

```
scripts/install_idf.sh              # once: ESP-IDF v5.5.6 into the scratch folder (needs Python 3.13)
scripts/build.sh xiao
```

Everything big stays in `$JANK_SCRATCH` (default `worktrees/jank-scratch/`, git-ignored): ESP-IDF, its tools,
`managed_components`, the `sdkconfig`s and `build-<board>/`. The build works on a copy there. Flash (download mode
first, see [docs/boards.md](docs/boards.md)):

```
bash -c '. scripts/env.sh && cd "$JANK_SCRATCH/build-xiao" && python -m esptool --chip esp32s3 -b 460800 --before default_reset --after no_reset write_flash "@flash_args"'
```

## Button and LED (as the firmware does it today)

One button (`BOARD_BUTTON_GPIO`; debounce 20 ms, double-click window 350 ms, `core/jank_ui.h`):

| Action | What it does |
|---|---|
| Click | Resync: stop now, start on the next bar line ([docs/midi.md](docs/midi.md)) |
| Double click | Scan the other channels and join the next group heard; none heard: stay (jank mode only) |
| Held 1.5 s, let go | Switch between jank mode and the saved Wi-Fi network, with a restart. None saved: 3 red pulses |
| Held 10 s | Restart into download mode (unproven) |

The LED shows the Link session's beat: in jank mode it rests off and flashes on each beat (green on an RGB LED), on a
Wi-Fi network it rests on and blinks off (blue). The bar's first beat flashes 150 ms, the others 80 ms, never more than
a third of a beat. With no session it stays at rest. Over that: magenta while scanning, white N flashes on joining
GROUP N, 3 red pulses when no other group was found. A one-colour LED shows only the timing.

The PCB's ladder, encoder, second LED, MIDI IN mode and power cut have no firmware yet.

## Control page and protocol

The stick's USB drive (label JANK) holds `page/jank.html`; open it in Chrome or Edge and press Connect. It talks Web
MIDI to the stick's MIDI port, with the control line as SysEx (`core/jank_sysex.h`), and asks for SysEx permission. It
shows status, units heard, settings, a group scan with Join buttons and a download-mode button. It does not flash.

One command a line; each answer ends with `ok` or `err <why>`:

```
get | status | scan | help | reboot | bootloader
set group 1..9 | name <text> | filter link|all | peer on|off | transport link|auto
set quantum 1..16 | offset -100..100 | net jam|wifi | ssid <name> | pass <8..63 chars>
```

`scan` answers one `scan=group,channel,frames,units,janks,rssi` line per group heard (or `scan=none`) and joins
nothing. `net jam` is the protocol's name for jank mode.

## Tests

```
FM1BASE=../optimist/fm1base tests/run.sh
```

Host tests, built with address and undefined-behaviour sanitizers into `$JANK_SCRATCH/hosttest`: frame vectors, the
filter and MAC handling, the control protocol, button and scan logic, the clock maths (no drift over an hour at any
tempo), the Link peer, SysEx framing and the control page's logic (needs node). `tests/crosscheck_optimist.c` compiles
the FM-1's own `wifi_jam.c`, `wifi_ip.c` and `link.c` from `FM1BASE` and swaps frames with jank. These prove that both
sides agree on bytes and logic, nothing about radios: delays in the models are made up.

## On the air

Open data frames at 1 Mb/s on channels 1, 6, 11 with the group's BSSID `02:66:6D:31:6A:GROUP`, no association, no key.
The bridge does no routing or NAT; the computer self-assigns 169.254.x.y and shares the radio's MAC. By default only
ARP, IPv4 unicast and Link multicast go to the air. Details, the Link peer and the scan: [docs/air.md](docs/air.md).
USB functions and host mode: [docs/usb.md](docs/usb.md). Clock and timing: [docs/midi.md](docs/midi.md).

## Licensing

The firmware (`firmware/`, `core/`, `tests/`, `scripts/`) is GPL-3.0-only, as it links Optimist's `link.c`
(`third_party/link`, copied from fm1-base, see its README). The control page and its test are MIT. The hardware
scripts carry GPL-3.0-only SPDX tags (`hardware/pcb/lib/stitch.py` CC0-1.0); the KiCad footprint of the XIAO is Seeed's,
CC BY-SA 4.0. See [LICENSE](LICENSE).

## What is proved

Measured on hardware:
- The air format and the Link peer on a plain-ESP32 test board (2026-11-07): it ran in group mode with an FM-1 as Link
  peer, with the beat LED. That test board is not supported here; no S3 board has run.

Read from datasheets and schematics, not measured: the XIAO's 499 ohm on D6, its VBUS wiring and CC resistors, and
every PCB and box dimension marked *unverified* in their READMEs.

Not proved:
- Any S3 build on hardware: USB enumeration (NCM in a composite device, macOS taking the descriptor's MAC, Windows
  binding NCM by itself), the drive, Web MIDI control, host mode, download mode by software.
- Whether the ESP acknowledges unicast while promiscuous and unassociated (it works without, at a cost in air time).
- Phase against Live or an FM-1, MIDI levels and jitter on a scope, the scan's real time, a power bank's behaviour.
- The PCB and the box: nothing ordered, built or printed.

## Next

1. The PCB's `board.h` entry and firmware for its ladder, encoder, second LED, MIDI IN mode and power cut (pins in
   `hardware/pcb/README.md`).
2. Flash an S3 build; in order: the stick hears an FM-1, the FM-1 accepts its frames, macOS sees the adapter and Live
   shows the session, phase measured, Windows, MIDI on a scope then on gear, the button and scan.
3. Move the bare XIAO's MIDI data off D6 (GPIO43), or document it as test-only.

A Lolin32 (plain ESP32) test setup for the air lives outside the repo, in `worktrees/jank-scratch/lolin32/`.
