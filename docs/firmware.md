# jank firmware

The firmware of the [jank device](../README.md), for the ESP32-S3 (ESP-IDF). Boards and pins: [boards.md](boards.md).

## Firmware boards

| Board | Build | Notes |
|---|---|---|
| Seeed XIAO ESP32-S3, bare | `scripts/build.sh xiao` | The module alone: BOOT button, yellow LED; MIDI on D6, which is weak (see [boards.md](boards.md)) |
| jank PCB | not written yet | The XIAO on the board: pins in [`hardware/pcb/README.md`](../hardware/pcb/README.md). I2C: SDA = D8 = GPIO7, SCL = D9 = GPIO8 |

## Build and flash

```
scripts/install_idf.sh              # once: ESP-IDF v5.5.6 into the scratch folder (needs Python 3.13)
scripts/build.sh xiao
```

Everything big stays in `$JANK_SCRATCH` (default `worktrees/jank-scratch/`, git-ignored): ESP-IDF, its tools,
`managed_components`, the `sdkconfig`s and `build-<board>/`. The build works on a copy there. Flash (download mode
first, see [boards.md](boards.md)):

```
bash -c '. scripts/env.sh && cd "$JANK_SCRATCH/build-xiao" && python -m esptool --chip esp32s3 -b 460800 --before default_reset --after no_reset write_flash "@flash_args"'
```

## Button and LED (as the firmware does it today)

One button (`BOARD_BUTTON_GPIO`; debounce 20 ms, double-click window 350 ms, `core/jank_ui.h`):

| Action | What it does |
|---|---|
| Click | Resync: stop now, start on the next bar line ([docs/midi.md](midi.md)) |
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
set quantum 1..16 | offset -100..100 | net jank|wifi | ssid <name> | pass <8..63 chars>
```

`scan` answers one `scan=group,channel,frames,units,janks,rssi` line per group heard (or `scan=none`) and joins
nothing. `net jank` is the protocol's name for jank mode.

## Tests

```
FM1BASE=../optimist/fm1base tests/run.sh
```

Host tests, built with address and undefined-behaviour sanitizers into `$JANK_SCRATCH/hosttest`: frame vectors, the
filter and MAC handling, the control protocol, button and scan logic, the clock maths (no drift over an hour at any
tempo), the Link peer, SysEx framing and the control page's logic (needs node). `tests/crosscheck_optimist.c` compiles
the FM-1's own `wifi_jank.c`, `wifi_ip.c` and `link.c` from `FM1BASE` and swaps frames with jank. These prove that both
sides agree on bytes and logic, nothing about radios: delays in the models are made up.

## On the air (summary)

Open data frames at 1 Mb/s on channels 1, 6, 11 with the group's BSSID `02:66:6D:31:6A:GROUP`, no association, no key.
The bridge does no routing or NAT; the computer self-assigns 169.254.x.y and shares the radio's MAC. By default only
ARP, IPv4 unicast and Link multicast go to the air. Details, the Link peer and the scan: [air.md](air.md).
USB functions and host mode: [usb.md](usb.md). Clock and timing: [midi.md](midi.md).

