# jank

jank (jam + link) is a cheap USB stick that does two jobs in an FM-1 MODE JAM group.

- **Bridge.** Plugged into a computer, it appears as an ordinary USB network adapter on macOS, Windows and Linux, with no driver or app. Ableton Live or any Link software sees a network with the FM-1s on it.
- **MIDI clock.** The stick is a Link peer itself. It follows tempo, beat and start/stop, and sends MIDI clock out of a TRS or DIN jack, and on an ESP32-S3 also to the computer as a USB MIDI port called jank. No computer is needed: a charger or power bank will do.

The on-air format is Optimist's MODE JAM, and jank carries Ableton Link unchanged. Link is Ableton's trademark, and jank is not an Ableton product.

> **Barely run on hardware yet.** Only the Lolin32 Lite build has run (2026-11-07: MODE JAM with an FM-1 Link peer, the beat LED). The ESP32-S3 builds compile and the host tests pass, but none has been flashed. See [What must be proved](#what-must-be-proved), in order.

## The pick

**M5Stack AtomS3U.** EUR 13.43 with VAT (EUR 11.10 without) at [DigiKey Belgium](https://www.digikey.be/en/products/detail/m5stack-technology-co-ltd/K125/21286980), 93 in stock when checked on 2026-11-06. DigiKey's shipping is on top and was not checked.

It is the only one seen that is a real cased stick and also has everything else: a USB-A plug, a 3D antenna, a side button, an RGB LED, and free pins on a Grove port for the jack.

**Runner-up: Seeed XIAO ESP32S3.** EUR 7.70 with VAT at [DigiKey France](https://www.digikey.fr/en/products/detail/seeed-technology-co-ltd/102010635/26553888), EUR 11.56 at [TME](https://www.tme.eu/lt/en/katalog/p,seeed-studio_1325/). It is the cheapest and has the best range (u.FL with an external antenna), and all its pads are free, including 3V3. But it needs a USB-C cable and a case, and its LED has one colour.

| Board | Price seen | Form | Antenna | Button + LED | Free pins for MIDI |
|---|---|---|---|---|---|
| **AtomS3U** | EUR 13.43 DigiKey BE; [EUR 13.85, sold out](https://openelab.io/a/s/products/m5stack-atoms3u); [GBP 12](https://thepihut.com/products/atoms3u-esp32s3-development-kit-with-usb-a) | USB-A stick, cased | 3D ([FCC filing](https://fcc.report/FCC-ID/2AN3WM5ATOMS3U/6449253.pdf)) | G41 + WS2812 | Grove G1, G2 (5 V, no 3.3 V); header G14, G17, G40, G42 |
| **XIAO ESP32S3** | EUR 7.70 DigiKey FR | USB-C, needs a cable | u.FL, antenna included | BOOT + 1-colour LED | All pads, with 3V3 |
| LilyGO T-Dongle-S3 | [EUR 14, 8 left](https://www.tinytronics.nl/nl/development-boards/microcontroller-boards/met-wi-fi/lilygo-t-dongle-s3-esp32-s3); [USD 11.99](https://lilygo.cc/products/t-dongle-s3); [USD 11.11 AliExpress](https://www.aliexpress.com/item/1005004860003638.html) | USB-A stick, LCD | Built in | BOOT + APA102 | None in the [pinout](https://wiki.lilygo.cc/products/t-dongle-series/t-dongle-s3/) read: it loses |
| Waveshare ESP32-S3-GEEK | [EUR 13.50](https://www.botnroll.com/en/others/5226-waveshare-esp32-geek.html) | USB-A stick, LCD | On board | Not confirmed, no LED found | Not confirmed |
| Waveshare ESP32-S3-Zero | [EUR 5.45, unavailable](https://elty.pl/en_US/p/ESP32-S3-Zero-Mini-Development-Board-with-ESP32-S3-Wi-Fi-Bluetooth-5/3946) | USB-C, needs a cable | Not confirmed | Not confirmed | Pads |
| Espressif ESP32-S3-USB-Bridge | No price found | USB-A or USB-C | Not confirmed | Not confirmed | Not confirmed |

Prices are the ones seen on 2026-11-06, one reading per shop. Generic AliExpress dongles were listed at USD 6.41 to 16.90, but without enough detail to rank them.

**The chip.** It has to be an ESP32-S3. The S2, S3 and P4 have USB OTG; the [C3 and C6 have only USB-Serial-JTAG](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html), and the vanilla ESP32 has no USB at all (not checked in those pages). The S3 has two cores, which the clock uses: the timer runs on one and Wi-Fi on the other. PSRAM is not needed: the firmware is 731 KB and runs in internal RAM.

## MIDI out

MIDI's power cannot run the stick, because a MIDI port gives about 5 mA. Power comes from USB: a computer, charger or power bank. A power bank may switch itself off at such a small load; that is untested.

Values from [CA-033](https://midi.org/wp-content/uploads/wpforo/default_attachments/1709416667-ca33-MIDI-10-Electrical-Specification-Update.pdf), for 3.3 V +/- 5%: RA 33 ohm 5% 0.5 W on the source line, RC 10 ohm 5% 0.25 W on the data line.

**Parts:** one 33 ohm 0.5 W resistor, one 10 ohm 0.25 W resistor, a Grove (HY2.0-4P) cable, and either a 3.5 mm TRS socket or a 5-pin DIN socket.

| AtomS3U Grove wire | Through | [TRS type A](https://minimidi.world/) | DIN socket |
|---|---|---|---|
| White, G1 (source, held high) | 33 ohm | Ring | Pin 4 |
| Yellow, G2 (UART TX) | 10 ohm | Tip | Pin 5 |
| Black, GND | | Sleeve | Pin 2 |

Pins 1 and 3 of the DIN stay unconnected. Do not connect the red 5 V wire. Gear wired as TRS type B needs tip and ring swapped.

The jack does not fit inside the AtomS3U's case: it sits at the end of the short Grove cable, in a small box or heat-shrink. Its header has the same signals on G14 and G17 if a plug-on board is preferred, which needs a change in `firmware/main/board.h`.

**One deviation from the spec.** The AtomS3U exposes 5 V but no 3.3 V, so G1 is driven high to be the source. A GPIO high at 5 mA should stay within the 3.135 V the spec allows, but that is unmeasured. The data line must not use a 5 V source: a 3.3 V GPIO cannot turn the current off against it.

On the XIAO: 3V3 through 33 ohm to ring or pin 4, D6 (GPIO43) through 10 ohm to tip or pin 5, GND to sleeve or pin 2.

## The button

| Action | What it does |
|---|---|
| One click | Resync |
| Double click | Next group found on the air (MODE JAM) |
| Held 1.5 s, then let go | MODE JAM <-> the saved Wi-Fi network, with a restart. No network saved: 3 red pulses, nothing changes |
| Held 10 s | Download mode |

**The LED** shows the Link session's beat: in MODE JAM it rests off and flashes on each beat, on a Wi-Fi network it rests on and blinks off. The first beat of the bar (by Beats a bar, as MIDI start counts it) flashes longer, 150 ms against 80, and no flash is longer than a third of a beat. With no session it stays at rest. On an RGB LED, green in MODE JAM, blue on a network. Scans and group changes still show their own patterns over it.

Debounce is 20 ms and the double-click window 350 ms (`JB_*` in `core/jank_ui.h`, host-tested).

**Resync, exactly.** The click is recognised 350 ms after the release, since a second press could still come. Then:

1. Before the next clock, up to one tick later (20.8 ms at 120 BPM): `0xFC`.
2. `0xF8` carries on at every tick. It never stops.
3. At the first bar line after that tick: `0xFA`, then that bar line's `0xF8`.

A bar is `quantum` beats, 4 by default, on Link's own beat count, so the receiver lands on the session's downbeat. The wait is at most one bar (2 s at 120 BPM). A click exactly on a bar line waits for the next one: stop and start are never in the same tick. A click starts stopped gear too, and it stays started until the session's next stop.

**Next group found.** The stick listens for frames with a BSSID of `02:66:6D:31:6A:GROUP`.

- Groups on the home channel need no scan, because the radio hears them all the time: anything heard in the last 3 s counts. Groups 1, 4 and 7 share channel 1, 2, 5 and 8 share 6, and 3, 6 and 9 share 11.
- The scan visits the two other channels for 350 ms each, longer than Link's 250 ms announce. **It takes about 0.7 s**, plus channel switching, which is unmeasured.
- It then joins the next group heard after the current one, wrapping. If none is heard, it stays.

**Lost during the scan:** for about 0.7 s the stick neither hears nor sends on the home channel. Frames from the computer to the air are dropped and counted as `scan_lost`, and so are the peer's. Link's peers live for 5 s, so nobody should forget the stick, and a measurement ping that goes unanswered is sent again. One announce heard on a dwell is enough, but a lost one means a group can be missed: double-click again.

**The clock during the scan** runs free on the hardware timer from the last tempo and phase, untouched. After joining another group, the clock jumps to that session's phase if it differs, so a click to resync is worth doing.

## LED

| Pattern | Meaning |
|---|---|
| Blue, N blinks every 4 s | GROUP N |
| Green between blinks | Units heard |
| Amber | A computer, no units heard |
| Red | No computer, no units heard |
| Magenta, steady | Scanning |
| White, N flashes | Joined GROUP N |
| Red, 3 long pulses | No other group found, stayed |

On the XIAO's one-colour LED, the colours are lost and only the timing shows.

## How it works

**The air format.** Open data frames at 1 Mb/s, as the FM-1 sends in MODE JAM:

- Frame control `08 00`: ToDS = FromDS = 0.
- addr1 the destination, addr2 the sender, addr3 the group's BSSID `02:66:6D:31:6A:GROUP`.
- Sequence number in the top 12 bits of the sequence field, then LLC/SNAP `AA AA 03 00 00 00` + EtherType, then the payload.
- Channels 1, 6, 11 repeating with GROUP 1..9; addresses 169.254.x.y; no beacon, no key.
- Group frames are not acknowledged; unicast is.

**The bridge** (`core/jank_core.c`) has no routing, NAT or DHCP. The computer gives itself a 169.254.x.y address.

**The MAC.** `esp_wifi_80211_tx` can send a chosen addr2. Disassembled in IDF 5.5.6's `libnet80211.a`, `ieee80211_raw_frame_sanity_check` refuses Protected and QoS frames, but compares addr2 and ToDS only when associated, and jank never associates. The limit is the radio's receive filter, not the transmit path: it acknowledges only its own address. So the bridge rewrites no address at all. The stick offers its radio's MAC as the address in the NCM descriptor, which is the computer's own adapter address. Computer and radio share one MAC, and the FM-1's unicast and ARP both reach it. If a computer overrides that MAC, the stick adopts the new one and restarts the radio. The fallback has only run in host tests.

**The air filter.** At 1 Mb/s each byte costs 8 us, and a computer's mDNS and IPv6 chatter would eat the air. So by default only ARP, IPv4 unicast and Link's multicast go out. `set filter all` makes it the dumb bridge; test it before relying on it.

**Size limit.** `esp_wifi_80211_tx` takes at most 1500 bytes, which leaves 1468 bytes of payload. Larger frames are dropped and counted. Link's are at most 512.

**The Link peer** (`core/jank_peer.c`, on top of the bridge, separate from it) runs Optimist's Link peer, `third_party/link/link.c` (copied from fm1-base, see its README), which builds with no FM-1 dependency.

- On the air it shares the MAC and has its own IP address, as a computer with two addresses would.
- Towards the computer it uses a second MAC, because a computer cannot send to its own.
- It stays silent until it hears Link, and leaves after 8 s alone. Its session is therefore always the younger one, so it joins and never pushes 120 BPM on a group.
- With a computer attached, the FM-1 sees one extra Link peer and Live sees the stick as one too.

**The clock** (`core/jank_clock.c`, `firmware/main/midi.c`): every 5 ms a task turns the session into a straight line of tick times. A 1 MHz hardware timer's interrupt, in IRAM at level 3 on core 1, sends each byte straight into the UART FIFO, with multiply and shift only. Small session moves slew by up to half a tick per tick; moves over 6 ticks jump.

## Latency and jitter

Estimates, none measured:

| | Expected |
|---|---|
| Timer to UART, tick to tick | Under about 50 us: interrupt latency, plus up to one 32 us bit if the UART aligns its start bit |
| MIDI clock against the session | Link's own measurement error plus the above. It steps when Link re-measures (30 s), then slews |
| Bar start | The `0xF8` goes 320 us late behind `0xFA`, a byte's time on the wire |
| Bridge, one way | 2 to 4 ms: up to 1 ms of USB, about 1 ms of air for a 100-byte frame at 1 Mb/s, and the stick's queue |

The stick's own part is in `status`: `lat_air_max_us` and `lat_usb_max_us`.

## Build

```
scripts/build.sh atoms3u      # or: xiao, lolin32
tests/run.sh                  # the host tests
```

Everything big stays in `worktrees/jank-scratch/` (git-ignored): ESP-IDF v5.5.6, its tools and Python environment, `managed_components`, `sdkconfig`, the build output. The build works on a copy there, because IDF would otherwise put them in the checkout. To install it, run `scripts/install_idf.sh`. It needs Python 3.13; Homebrew's default 3.15 is too new for IDF.

## USB

An S3 stick is one USB device with three functions: the network adapter (CDC-NCM), a MIDI port and a small read-only drive holding the control page, so there is no link to remember: open `jank.html` from the drive in Chrome or Edge. Clock goes to the TRS or DIN jack and to USB at the same moment, from the same timer interrupt; on USB it passes through a small queue and a task, as TinyUSB cannot be called from an interrupt, and the computer then reads it within a USB frame or so.

There is no serial port on an S3: the chip has 5 IN endpoints with EP0 included, NCM takes 2, MIDI 1 and the drive 1, which uses them all. The control line goes as SysEx on the MIDI port instead (`core/jank_sysex.h`), and the control page uses Web MIDI there. It asks for SysEx permission on connecting. A vanilla ESP32 has no USB of its own, so it keeps its serial port.

### Host mode

With no computer on the USB port, the stick sends its clock to USB-MIDI gear instead: up to 4 devices, through a hub. It starts as a device, and when no computer has spoken within 3 s it restarts once as host, so a clock on a charger starts about 3 s later. The choice lasts until the next start. In host mode there is no network adapter, no drive and no control page over USB; output only.

jank does not power the device. On a XIAO the 5V pin *is* the USB-C socket's VBUS, with no diode (v1.2 schematic), so 5 V on that pin reaches the device, or a powered hub does the job. A XIAO also says "device" on the CC pins, 5.1k on both: USB-A gear and hubs behind a C-to-A adapter do not care, USB-C gear plugged straight in may ignore it.

**Never feed the 5V pin with a computer on the USB port**: nothing separates the two supplies. Build that needs both gets a diode or a switch.

## Flashing

The first flash of a blank board needs download mode by hand.

1. AtomS3U: plugged in, hold the side RESET button until the internal green LED lights, then release. (Later on: `bootloader` on the control line, the control page's button, or the main button held 10 s. These are unproven.)
2. Flash:

```
bash -c '. scripts/env.sh && cd worktrees/jank-scratch/build-atoms3u && python -m esptool --chip esp32s3 -b 460800 --before default_reset --after no_reset write_flash "@flash_args"'
```

3. Unplug it and plug it back in.

The USB ID is Espressif's VID with a made-up PID, 0x4A4B: fine on your own desk, but anything distributed needs a real PID.

## Control

Open `page/jank.html` in Chrome or Edge and press Connect. It is one static file with no build step and nothing external. **It does not flash:** esptool-js would mean bundling a large library into the page, against the one-file rule. It has a Download mode button, and the command above does the rest.

The same protocol works from any terminal, one command a line. Answers end with `ok` or `err <why>`.

```
get
status
scan
set group 3
set name front of house
set filter link|all
set peer on|off
set transport link|auto
set quantum 4
set offset -12
reboot
bootloader
```

`scan` shows which groups are running and joins nothing. It takes about 1.05 s: a dwell at home, then 0.7 s away on the two other channels, with the same losses as the button's scan and the clock running on. The answer is one line per group heard, or `scan=none`:

```
scan=3,11,4,3,1,-50      group, channel, frames, units, of which janks, best RSSI
```

An FM-1 and a jank look the same on the air: same frames, same BSSID. The only tell is the address: a jank's starts `02:6A:6B`. So a jank whose computer overrode the MAC counts as a unit, not a jank. Units stop counting at 8 a group. The page's Scan button shows this as a table with a Join for each row, which sends `set group N`.

`transport link` follows Link's Start Stop Sync; `auto` runs whenever a session is joined, for sessions where it is off. `offset` moves the clock against the session in ms, for gear that lags.

## Tests

`tests/run.sh` builds with address and undefined-behaviour sanitizers. It covers the frame vectors, written out by hand from the format; the filter and MAC handling; control protocol; button; scan logic; the tick maths, with no drift over an hour at any tempo; and the control page's logic.

`tests/crosscheck_optimist.c`, which is not shipped, compiles the FM-1's own `wifi_jam.c`, `wifi_ip.c` and `link.c` and swaps real frames with jank. In that model, jank joins an FM-1's session with no computer, in both directions.

These show that the two sides agree on bytes and logic. They say nothing about radios: delays in the models are made up, so a phase error of 0 there is not a measurement.

## Lolin32

A WEMOS LOLIN32 (vanilla ESP32) can try the air today. It has no USB device, so **there is no network adapter and no bridge**: a computer cannot join through it. Everything else is there: the radio, groups and scan, the Link peer, MIDI clock, and the RST button as the mode switch. Control is the same line protocol on UART0, through the board's USB-serial chip, so `page/jank.html` works too. It covers proofs 1 and 2 below, and 6 and 7 except for power bank and download mode.

Built for the ESP32 by `scripts/build.sh lolin32`: 680 KB. TinyUSB is not in it. The S3 builds are unchanged by this target: every loaded section of both is identical to before, except the compile time in the app descriptor.

**Board**, from [the Arduino pin file](https://raw.githubusercontent.com/espressif/arduino-esp32/master/variants/lolin32/pins_arduino.h) and [espboards](https://www.espboards.dev/esp32/lolin32): LED on GPIO5, active low; 4 MB of flash; but the board on hand is a Lolin32 Lite, whose LED is on GPIO22 (found with a pin test, 2026-11-07), and the firmware uses that; CP2104 USB-serial (a Wemos schematic was not found, and one source says CP2102). Sources differ on a BOOT button, and the firmware does not need one.

| Function | Pin | Wiring |
|---|---|---|
| MIDI data | GPIO17 | 10 ohm 0.25 W to TRS tip, or DIN pin 5 |
| MIDI source | 3V3 | 33 ohm 0.5 W to TRS ring, or DIN pin 4 |
| MIDI ground | GND | TRS sleeve, or DIN pin 2 |

The source here is a real 3.3 V rail, so this wiring has no deviation from CA-033. MIDI is only needed for its own test.

**The on-air path, ESP32 against S3.** No difference was found. `ieee80211_raw_frame_sanity_check` in IDF 5.5.6 was disassembled for both: the same checks and the same messages, differing only in layout, padding and one `memw`. Raw TX and promiscuous use the same API, the same 1 Mb/s default and the same limits. ACK behaviour is in closed hardware and could not be compared: it is unproven on both.

**The clock** uses the same code on the same second core. Its jitter is unmeasured on both chips.

**The button is RST.** The Lolin32 Lite has one button, on the chip's EN pin, so the firmware only ever sees the boot after a press. Two presses within 4 s are its long press: the second boot switches MODE JAM <-> the saved Wi-Fi network (no network saved: it stays in MODE JAM), with no further restart. The flag is kept in NVS, since RTC memory does not survive the EN reset on an ESP32. Any two resets within 4 s count, a plug-in followed by a press too. There is no click or double click: resync and the group scan come from the control page.

**Differences in use:**

- `bootloader` and the 10 s hold only restart the board, since an ESP32 cannot enter download mode by software. esptool does not need it here: it resets the board by DTR and RTS, if the board's auto-reset circuit is as documented.
- Opening the serial port may reset the board.
- The LED's colours are lost, as on the XIAO: only the timing shows.

### Flash it

Nothing is needed but the USB cable. User, from this folder:

1. Plug in the board. Find its port: `ls /dev/cu.*`, which should show something like `/dev/cu.usbserial-XXXX` or `/dev/cu.SLAB_USBtoUART`. If none appears, install the CP210x driver from Silicon Labs.
2. Build, if `build-lolin32` is not there:

```
scripts/build.sh lolin32
```

3. Flash, with your port:

```
bash -c '. scripts/env.sh && cd "$JANK_SCRATCH/build-lolin32" && python -m esptool --chip esp32 -p /dev/cu.usbserial-XXXX -b 460800 --before default_reset --after hard_reset write_flash "@flash_args"'
```

If esptool cannot connect, hold GPIO0 to GND (the BOOT button, if the board has one) while it says `Connecting...`.

4. Read the log. Quit with Ctrl-].

```
bash -c '. scripts/env.sh && python -m serial.tools.miniterm /dev/cu.usbserial-XXXX 115200'
```

Close it before using the control page: a port has one reader.

A line a second:

```
# up=12 group=1 ch=1 heard=120,0,0,0,0,0,0,0,0 peers=1 rssi=-44 sent=34 tx_fail=0 uni_ok=5 uni_fail=0 link=1 link_peers=1 bpm=120.00
```

| Field | Meaning |
|---|---|
| `heard` | Frames of groups 1 to 9 since boot, on the channels the radio has been on |
| `peers`, `rssi` | Units heard in this group in the last 3 s, and the best signal |
| `sent`, `tx_fail` | Frames handed to the radio, and those it refused |
| `uni_ok`, `uni_fail` | The driver's result for each unicast sent. Whether "ok" means acknowledged is not documented |
| `link`, `link_peers`, `bpm` | The peer is running, peers in its session, the session's tempo |

Type `status` for all the counters, and `set group 3` to change group.

### Test 1: the stick hears an FM-1

1. FM-1: SYSTEM > WI-FI > MODE to JAM, GROUP 1. The stick starts on group 1.
2. Keep them within a few metres. Watch the log for 10 s.

**Pass:** the first number of `heard` climbs, about 4 frames a second or more, with `peers=1` and an RSSI.
**If not:** if `heard` stays at zero, try `set group 2` on the stick and GROUP 2 on the FM-1, to rule out a busy channel 1. If that fails too, promiscuous reception of the FM-1's 1 Mb/s frames is the suspect.

### Test 2: the FM-1 accepts the stick's frames

Test 1 must pass first. Hearing an FM-1 starts the peer by itself.

1. Watch the log for 15 s: `link=1` comes up, `sent` climbs.
2. FM-1: on its Wi-Fi page, STATE and LINK PEERS.

**Pass:** the stick's `link_peers=1` with the FM-1's tempo in `bpm`, and the FM-1's LINK PEERS shows one more. A session needs frames both ways, so that alone proves it.
**Also note, for the ACK question:** `uni_ok` against `uni_fail`, and the FM-1's RETRY count. Many fails and retries with a working session mean the ESP does not acknowledge in this mode. It works anyway, with wasted air.
**If not:** `sent` climbs but the FM-1 shows nothing: the FM-1 is not taking the frames. `tx_fail` climbs: the radio refuses them.
Change the tempo on the FM-1: `bpm` should follow within a second.

## What must be proved

In this order, each on real hardware. Steps 1 to 5 are for the bridge, 6 and 7 for the clock.

1. **The stick hears an FM-1 in JAM.** `status` shows `peers=1` and an RSSI. Promiscuous receive of these frames is assumed.
2. **The FM-1 accepts the stick's frames.** The FM-1's STATE shows one more heard; watch its RETRY too. If unicast retries pile up, the ESP is not acknowledging while promiscuous and unassociated, which is unproven. It works without, with wasted air.
3. **macOS sees the adapter**, takes the MAC, self-assigns, and Live shows Link 1. This depends on macOS using the NCM descriptor's MAC, and binding NCM in a composite device with the serial: neither is checked. The first run needs Live's Local Network permission.
4. **Phase measured**, Live against an FM-1, by audio or a scope.
5. **Windows.** Reports differ on whether UsbNcm.sys binds by itself: it may need a descriptor change.
6. **MIDI out on a scope**, then on gear: levels at the jack, tick jitter, and whether GPIO-as-source passes.
7. **The button**, the scan's real time and what it costs Link, the download-mode restart, and running on a power bank.
