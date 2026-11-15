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

The first install of the build with two app slots (partition table, bootloader with rollback) needs download mode once;
after that updates are copy-and-eject or `scripts/ota.py`: see [usb.md](usb.md), "Updating". Partition table
(`firmware/partitions_usb.csv`, 8 MB flash): nvs 0x9000, otadata 0xf000, phy 0x11000, `ota_0` 0x20000 (2 MB), `ota_1` 0x220000
(2 MB), `storage` 0x420000 (2 MB, the drive). The saved `sdkconfig.<board>` in the scratch folder overrides the defaults:
after a change of the partition table or `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, move it aside so the defaults apply.

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
| Double click | Scan the other channels and join the next group heard; none heard: stay (jank mode only). Said 350 ms after the second release (a third press may follow) |
| Three clicks | Open Bluetooth for a phone for 2 minutes (BLE builds, [ble.md](ble.md)): the LED blinks blue 4 times a second until a phone connects. Setting `ble off` or no Bluetooth: 3 red pulses |
| Held 1.5 s, let go | Switch between jank mode and the saved Wi-Fi network, with a restart. None saved: 3 red pulses |
| Held 10 s | Restart into download mode (`bootloader`, see below; unproven) |

The LED shows the Link session's beat: in jank mode it rests off and flashes on each beat (green on an RGB LED), on a
Wi-Fi network it rests on and blinks off (blue). The bar's first beat flashes 150 ms, the others 80 ms, never more than
a third of a beat. With no session it stays at rest. Over that: magenta while scanning, white N flashes on joining
GROUP N, 3 red pulses when no other group was found. A one-colour LED shows only the timing.

The PCB's ladder, encoder, second LED, MIDI IN mode and power cut have no firmware yet.

## Control page and protocol

The stick's USB drive (label JANK) holds `page/jank.html`; open it in Chrome or Edge and press MIDI port. The same page
serves a phone over Bluetooth ([ble.md](ble.md)): one SysEx line layer with two transports, Web MIDI (the USB port, or a
jank an iPhone MIDI browser app connected) and Web Bluetooth (Chrome, straight to the BLE-MIDI service). It shows HOME
(tempo, the beat in the bar, play / stop, network, peers, battery, input level and beat confidence: the `home` line) and
the settings (`get` / `set`); over USB also the details (`status`: air and frame counters, peers heard) and download
mode. It does not flash.

One command a line; each answer ends with `ok` or `err <why>`:

```
get | status | home | scan | help | reboot | bootloader
tempo 20..999 (BPM, up to two decimals) | play | stop | ble pair | ble close
set group 1..9 | name <text> | filter link|all | peer on|off | transport link|auto | ble pair|auto|off
set quantum 1..16 | offset -100.00..100.00 (ms, 10 us steps) | net jank|wifi | ssid <name> | pass <8..63 chars>
```

`home` answers `home=h ...`, the HOME line a phone gets by itself over Bluetooth (`core/jank_phone.h`). `ble pair` opens a
pair window (`err ble is off` with the setting off), `ble close` ends the window and the phone's link; `status` says
`ble_state=off|window|connected` and `ble_left=<s>`. Over Bluetooth, `ota`, `bootloader` and `set pass` are refused
(`err ... over USB only`). `scan` answers one `scan=group,channel,frames,units,janks,rssi` line per group heard (or `scan=none`) and joins
nothing. `net jank` is the protocol's name for jank mode. `tempo`, `play` and `stop` change the whole Link session, as
any Link app may; with no settled session they answer `err no session`: alone in LINK mode, the stick has no session of
its own. Alone in AUDIO mode it has one (the tracker's, `core/jank_peer.h` jp_solo): the MIDI clock starts on its bar
line once the tracker locks, as with peers.

- **`play` starts on the next bar line** (`quantum` beats). The start goes out 150 ms before that bar, as "playing from
  bar B" (Link's isPlaying, beats B, time stamp now), so every peer starts on B: Live and LinkHut read B, the FM-1 and the
  stick's own MIDI clock start on the next bar line they have not sent yet (`core/jank_start.h`). The session playing
  already: nothing changes. `stop` stops at once.
- **AUDIO mode** = the tracker's source has SOUND (`beat on`; `core/jank_live.h`): its RMS over 0.75 s above -50 dBFS
  for 1 s. An open stream of zeros (a computer keeps jank's stream open while it is the output, even silent) is LINK
  mode. Back to LINK mode once the RMS stayed under -55 dBFS for 4 s (about 4.75 s after the last sound: longer than any
  quiet break measured in music, see below). In AUDIO mode the tracker starts the session when it has the beat and
  stops it as its rules say (30 s without a steady beat); switching to LINK mode leaves the session as Link has it
  (playing or not: the user starts and stops it then).
  Measured with the detector itself over GTZAN's 1000 clips of 30 s and a 73-minute DJ set: 986 clips went AUDIO at
  1.0 s (the slowest, a quiet hip-hop intro, at 3.4 s; one corrupt clip unread); the DJ set at 1.1 s and never left;
  outside classical music no quiet stretch (RMS under -55 dBFS) lasted over 0.3 s; one classical clip with 12 s of
  near silence went back to LINK mode once, the longest classical pause that did not was 2.25 s.
- **`play` / `stop` in AUDIO mode are Start / Stop of the OUTPUTS** (`core/jank_hold.h`, the emulator's hold): `stop`
  silences the MIDI clock outputs at once (a MIDI Stop) and with LINK TRANSPORT = FOLLOW+LEAD (transport `link`, the
  default) also stops the Link session, which the tracker then does not start while held; with IGNORE (transport `auto`)
  or FOLLOW the session is left alone. The tracker keeps listening and the hold stays through its stops and through LINK
  mode. `play` ends the hold: the outputs wait for the tracker to lock, then start on the next bar line (FOLLOW+LEAD:
  the tracker starts the session on its next bar line and the clocks follow it). In LINK mode `stop` is the same hold:
  the stick's own clocks stop until its `play`, whoever starts the session again; with transport `link` it stops the
  session too, with `auto` the session is left alone. `play` starts them on the next bar line (transport `link`: the
  session too). **Two layers** (2026-11-15, the user: "if I manually stopped, don't start again automatically"): the
  user's `stop` holds in both modes until the user's `play`, through everything automatic (the gate, silence then
  sound, a re-lock, AUTO BAR, a fold, AUDIO <-> LINK mode with the sound, the session of its own ending and restarting, a
  peer joining or starting the session, an audio stream restart); the gate and the silence stop and start the outputs
  only while the user's layer is started, and never change it. **Power-on is stopped** (M2h, the user: "come up with
  manual state: stopped"; a constant, nothing stored): the outputs wait for the user's Start (`play`, the page's Start,
  a click of a screenless unit's button: with the user's layer stopped a click is that Start, otherwise the resync); a
  session already playing is not stopped for it. The outputs run only when user = started AND automatic = open:

  | user | automatic | outputs |
  |---|---|---|
  | stopped (power-on, `stop`) | anything | stopped; only the user's `play` ends it |
  | started | open | run (from the session's bar line once the tracker is locked) |
  | started | gate closed (conf < threshold) | stopped; open again at threshold + 5: run again, no new Start |
  | started | silence (0.5 s under -60 dBFS) | stopped at once; ends a Start's override: the next music waits for the gate |
  | `play` | gate closed | the Start forces the gate open until the confidence has reached threshold + 5 once |

  `status` says `hold=0|1` and `outputs=stopped|armed|running` (the
  transport icon), with `audio=` (the source open), `audio_live=` (sound: AUDIO mode), the input meter `rms_db=` (RMS over
  0.3 s, a value every 250 ms), `rms_min_db=` / `rms_max_db=` (its extremes over 3 s), `peak_db=` (the last second's
  peak), `conf=` (the grid-fit confidence below, percent) and `act=` (the tracker's network activation peak over the
  last second, percent: M2d's `conf=`). The beat reports' audio field is 0 (no source), 1 (open, no sound), 2 (sound).
- **LOW CONFIDENCE** `set lowconf run|stop`, `set threshold <0..100>` (percent), stored (blob `usb1`'s 5th and 6th
  bytes, saved once stopped); STOP / START at 80 % by default, as the emulator (`core/jank_conf.h`, beat-listener's
  live/confidence.py and engine.py). The confidence: how well the tracked beats of the last 8 s fit ModeBpm's published
  grid, an F-measure 0..1 (a slot of the grid is hit by a beat within 10 % of a period; a beat near no slot is an extra);
  0 in silence and in LINK mode; looked at each step (4 times a second and at each tracked beat). With STOP / START the
  gate closes when it is under the threshold and opens again at the threshold + 5 points (the Python's HYSTERESIS: one
  beat hit or missed moves the F-measure 2..4 points at 90..180 BPM, so a gate without a band would flap on every beat
  of a track near the threshold; locked tracks measure 94-97 %, wrong tempos ~40 %). KEEP RUNNING (`run`) or a threshold
  of 0: never gated. SILENCE, whatever the setting: the input under -60 dBFS RMS for 0.5 s gates too, and the beats kept
  go (what comes after is new music). Gated (`core/jank_hold.h` jho_in.gated): the MIDI clock outputs stop at once (a
  MIDI Stop; the bench CLK / RST pins with them); with FOLLOW+LEAD the Link session stops too (once), with IGNORE or
  FOLLOW it is left to the tracker. Open again: the outputs start on the session's next bar line (FOLLOW+LEAD: the
  tracker starts the session on its next bar line, AUTO BAR's start wait included). The user's hold wins: held, the gate
  opening starts nothing. **The user's `play` wins over the gate** (M2g): it disarms it, the outputs start on the next
  bar line once the tracker is locked whatever the confidence (armed until then); the gate re-arms once the confidence
  has reached the threshold + 5, then stops and starts as before; `gated=` says 0 while disarmed. Silence still stops at
  once and ends the override (M2h). LINK mode: no gate
  (as no hold). A first start with STOP / START waits for the window to fill with beats on the grid (about 7 s of music
  at 80 %), as the player. `status` and `beat`: `conf=`, `act=`, `gated=0|1`, `silent=0|1`, `threshold=`, `lowconf=`;
  `get`: `lowconf=`, `threshold=`; the transport icon (`outputs=`) is `stopped` while gated, `armed` once the gate opens
  until the bar line. The HOME line's `conf` is the new confidence.
- **FOLD and AUTO RANGE** (`core/jank_fold.h`, the Python player's fold): `fold x2|/2|1` in AUDIO mode (`err link mode`
  otherwise) answers `fold=<1|2|4|0.5|0.25>`; `status` says `fold=` and `fold_manual=`; the HOME line's last field is
  `x2 x4 /2 /4` while folded. Link, the MIDI clock and the beat reports get the tracked tempo times the fold. x2 keeps
  every tracked beat a beat; /2 keeps the tracked beats that sit on the session's grid (a user's /2: the session's beats
  1 and 3, so the 1 stays a 1). AUTO RANGE: a new tempo (a new lock, a track change, a jump of the tracker, an octave flip
  within a track) is folded to an octave of the reference (the session's tempo while it plays, else the last tempo put out
  in the last 2 minutes) when it lands within 6 % of it: 70 after 140 -> 140, 90 after 175 -> 180, a tracker flipping 128
  -> 64 or 256 -> output still 128, same count; 128 -> 132 or 135, or 90 after 132, is followed as is. A user's fold wins
  until the next new tempo, which is then ranged against the user's output. Not stored: 1 at a start, at Start after
  Stop, at a new stream and when AUDIO mode ends.
- **AUTO BAR** `set autobar off|start|always` (`on` = start), stored. AT START (the default): the bar's 1 is placed only
  while the session is stopped or its start is armed (the start re-targets onto the moved bar line), or once at a new
  start for the bar (Start after Stop, a fold change, a new lock) if the estimator knows the 1 then; never while it
  plays. The tracker's start waits up to 8 tracked beats after its lock for the estimator: found, the start lands on the
  music's 1; not found by then, it starts on the session's next bar line and the 1 is placed at the next start (a bounded
  wait: beat one honest when the estimator can tell, no long silent wait when it cannot). ALWAYS: also mid-play, as
  before. No move while folded (the estimator counts tracked beats).
- **PHASE** (NUDGE BEAT) `nudge beat <+-0.5..3, halves>` (AUDIO mode; the device's NO held + UP/DOWN, beat-listener's
  `shift_beats`): Link's beats jump that much at once (+: earlier, UP) and the phase follower keeps them there from then
  on (the half part is its offset, whole beats are in the count; `core/jank_beatlink.h` jbl_nudge). The user's nudges add
  up, at most +-3 beats in all (`err nudge beat at its limit`). An armed start re-targets the moved grid's next bar line.
  Folded, a half beat is half a beat of Link's (the folded grid's); the offset is kept in tracked time, so an off-beat
  fixed at fold 1 stays fixed at x2 and /2. **Two layers** (M2h, the user: "beat is more phase. So zero is auto"): the
  total is a manual offset on top of the tracker's grid and AUTO BAR's bar. AUTO BAR stays in charge (no MANUAL any more:
  it keeps placing the music's 1, the offset riding on it), nothing automatic changes the offset (a new stream, the
  tracker stopped: kept, in RAM; a restart starts at 0), and back to a total of 0 is AUTO (no offset left). Shown as
  "PHASE +1/2", "AUTO" at 0. `err link mode` in LINK mode (the session is not the tracker's), `err no beat yet`
  before a lock. The answer: `nudge_beats=<total>` and the downbeat estimator before it (`bar=`, `bar_phi=`,
  `bar_count=`, `bar_scores=`, for a refit's hand labels; the serial log has the same line). The MIDI clock: the session's
  half beat is exactly 12 ticks, so the clock jumps its tick count and keeps its times (one clock per grid time, no double
  or missing one: tests/test_beatlink.c test_nudge_clock); MIDI-clock gear counts the clocks it got since its Start, so
  it follows the nudge at its next Start (Stop / Start, or the button's resync), not at once.
- **AUDIO SHIFT** (NUDGE MS) `nudge ms <+-1..10>` (AUDIO mode; the device's YES held + UP/DOWN): the user's shift of
  the heard audio, on top of the source's auto latency, +: every output later. **Two layers** (M2h, the user: "ms is
  actually more an audio shift ... zero is auto"): the auto base (`set beat_latency`, the player's Auto latency, or the
  value remembered) and the AUDIO SHIFT (`nudge ms`, `set lat_trim <ms>`, Calibrate); neither changes the other, the
  value used (`beat_latency=`) is their sum within -300..300, a shift of 0 is the auto base alone. Answers
  `beat_latency=<sum>`, `lat_auto=<base>`, `lat_trim=<shift>`. Shown as "AUDIO SHIFT +3 MS", "AUTO" at 0.
- **beat_latency per audio source** (`core/jank_lat.h`), stored (blobs `lat1`, the bases, and `latt`, the shifts; saved
  once stopped; an older firmware's `lat1` is read as it was, with no shift): the computer's USB audio (default 25),
  each hosted USB audio device by VID:PID (the last 8 given a value; default 0: a box's outs and its USB stream leave it
  together), the line in (a path constant, not measured yet, 0, plus its base). The source heard switches the live value
  (usbrole.c); `set beat_latency` sets the current source's base, NUDGE MS, `set lat_trim` and Calibrate its shift;
  `set beat_latency_usb <ms>` always the computer's base. `get` and `status`: `beat_latency=`, `lat_auto=`, `lat_trim=`
  and `beat_latency_src=` (`usb`, `line`, `host:<vid>:<pid>`).
- **USB MIDI CLOCK** `set usbmidi on|off`, stored (blob `usb1`'s 4th byte), on by default (the clock always went to
  USB-MIDI before; a computer's DAW ignores a clock it is not set to follow). Off: no clock, start or stop byte reaches
  USB-MIDI, in device mode (the computer) and host mode (hosted gear); turned off while running, one MIDI Stop goes out
  first. DIN / TRS MIDI out is unaffected; the beat reports and the control line (SysEx) still use the MIDI port. jank
  never acts on MIDI it receives: the computer's MIDI is read only for the control line's SysEx (realtime bytes are
  dropped, `jank_sysex.c` jx_feed), hosted gear is opened OUT only (usb_midi_host.c).
- **fw/ui note** (the device's buttons, for the integrator): NO held + UP/DOWN calls `beat_nudge(+1 / -1, &total, NULL, 0)`
  (`app.h`; it waits up to 200 ms for the beat task, so call it from the UI task, never from the beat or bridge task;
  `BEAT_NUDGE_*` says why a nudge was refused, the overlay: "NUDGE BEAT +1/2", "MAX 3 BEATS", "NO BEAT YET"). YES held +
  UP/DOWN: `app_cfg_get`, `jk_nudge_ms(&cfg, +1 / -1)`, `app_cfg_set` when it returns 1 (overlay "AUDIO IN +3 MS" from
  `cfg.beat_latency_ms`). Both are AUDIO mode only (`beat_state`'s audio), as the control line refuses them in LINK mode.
  The menu's USB MIDI CLOCK edits `cfg.usbmidi` (ON / OFF).
- **AUDIO > SOURCE** `set source auto|line|usb|link`: AUTO = the line in when a plug is in, else USB audio; LINK = USB
  audio ignored (LINK mode), a plug in the line in still wins. A unit with no screen toggles AUTO <-> LINK with a single
  NO (`jur_src_toggle`, branch fw/ui wires the button).
- **Settled first.** After joining a session (start-up, a replug, another group) the stick waits until it has been in the
  same session for 1 s with no other session left to measure (`jp_settled`): until then it sends no MIDI clock and no
  start, sets no tempo, and the tracker steers nothing. With no Link peer at all it has no session: no clock, nothing to
  start.
- `status` says `settled=0|1`, `armed=0|1` and, while armed, `start=<beat>`: armed is a start not reached yet, either
  waiting to be sent, sent for a bar still ahead, or the session playing while the MIDI start waits for its bar line.
- **No flash writes while playing** (the joined session plays, or with transport `link` the MIDI clock has a start out;
  with transport `auto` the clock runs whenever a peer is there, so only the session's play counts): a flash erase stops
  USB, Wi-Fi and the tracker for tens of ms. An update over MIDI answers `err playing` (`scripts/ota.py`: stop first).
  A jank.bin copied onto the drive while playing is refused, and only `drive` on the control line says why (UPDATE.TXT
  is not rewritten: that is a flash write too); one that became whole just as playing started waits and installs once
  stopped (`drive_image=done, installed once stopped`). A new image confirms itself once stopped. A changed setting is
  saved once stopped (`diag` says `save_pending=1` until then; a restart saves it first; **unplugging while it is
  pending loses it**). `set usb` is saved at once: its restart follows.
- `diag intr` lists the interrupts in use on each core (`c<core> <int> L<level> Used: <source>`): core 1 should have the
  MIDI clock's timer alone.

## The `bootloader` command

Fallback for a stick that cannot be updated by the drive: it restarts into the ROM's download mode, over USB, for esptool.
`main/download.c` explains the sequence in its header: the S3's one internal USB PHY belongs to the USB-OTG controller
(TinyUSB) or to USB-Serial-JTAG (the ROM's "USB JTAG/serial debug unit") according to RTC_CNTL_USB_CONF, which a software
restart does not reset. So the app soft-disconnects, resets and clock-gates the OTG controller, clears
`SW_HW_USB_PHY_SEL`, `SW_USB_PHY_SEL` and `USB_PAD_ENABLE`, clears `USB_SERIAL_JTAG_CONF0.PHY_SEL`, sets the pad and the D+
pull-up of Serial-JTAG, then sets `FORCE_DOWNLOAD_BOOT` and restarts. It does not drive D+/D- through the GPIO matrix (that
kept the chip off the bus until a power cycle). Modelled on arduino-esp32's `usb_switch_to_cdc_jtag`. **Not verified on
hardware.**

## Tests

```
FM1BASE=../optimist/fm1base tests/run.sh
```

Host tests, built with address and undefined-behaviour sanitizers into `$JANK_SCRATCH/hosttest`: frame vectors, the
filter and MAC handling, the control protocol, button and scan logic, the clock maths (no drift over an hour at any
tempo), the Link peer, SysEx framing, the update protocol (C logic and `scripts/ota.py` against it, over a line that loses, damages and repeats lines; needs python3), BLE-MIDI packets (`test_blemidi`, which also writes the vectors the page is checked against), the phone link's on/off and HOME line (`test_phone`), the made-up JANK drive and the image streaming into a modelled slot (`test_vdrive`; `JANK_TEST_IMAGE=<jank.bin>` adds a real image) and the control page's logic (needs node). `tests/crosscheck_optimist.c` compiles
the FM-1's own `wifi_jank.c`, `wifi_ip.c` and `link.c` from `FM1BASE` and swaps frames with jank. These prove that both
sides agree on bytes and logic, nothing about radios: delays in the models are made up.

## On the air (summary)

Open data frames at 1 Mb/s on channels 1, 6, 11 with the group's BSSID `02:66:6D:31:6A:GROUP`, no association, no key.
The bridge does no routing or NAT; the computer self-assigns 169.254.x.y and shares the radio's MAC. By default only
ARP, IPv4 unicast and Link multicast go to the air. Details, the Link peer and the scan: [air.md](air.md).
USB functions and host mode: [usb.md](usb.md). Clock and timing: [midi.md](midi.md).

