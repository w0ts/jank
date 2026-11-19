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

## Boot order on the mini 3 (lcd2)

`power_early_init()` (`power.c`) is the first line of `app_main`. HOST_EN (GPIO17), CLK (GPIO15) and RST (GPIO2) are driven low (each level is set before the pin becomes an output), then V5_EN (GPIO4) is driven high and one log line is printed. HOST_EN comes first because it is the only driver that puts a voltage on a connector. R131 already holds V5_EN high until the firmware drives it, so driving it changes nothing electrically: it makes the state deliberate (the MIDI loops, the CLK / RST buffer and the host signal depend on the 5 V rail). The pins come from `hardware/jank-mini-3/parts.py`; the other boards have -1 and do nothing. Nothing else is driven there yet; CLK / RST pulses and the host port come later (`docs/mini3-drivers.md`).

## Button and LED (as the firmware does it today)

One button (`BOARD_BUTTON_GPIO`; debounce 20 ms, double-click window 350 ms, `core/jank_ui.h`):

| Action | What it does |
|---|---|
| Click | Resync: stop now, start on the next bar line ([docs/midi.md](midi.md)) |
| Double click | Scan the other channels and join the next group heard; none heard: stay (jank mode only). Said on the second release |
| Held 1.5 s, let go | Switch between jank mode and the saved Wi-Fi network, with a restart. None saved: 3 red pulses |
| Held 10 s | Restart into download mode (`bootloader`, see below; unproven) |

The LED shows the Link session's beat: in jank mode it rests off and flashes on each beat (green on an RGB LED), on a
Wi-Fi network it rests on and blinks off (blue). The bar's first beat flashes 150 ms, the others 80 ms, never more than
a third of a beat. With no session it stays at rest. Over that: magenta while scanning, white N flashes on joining
GROUP N, 3 red pulses when no other group was found. A one-colour LED shows only the timing.

The PCB's ladder, encoder, second LED, MIDI IN mode and power cut have no firmware yet.

## Control page and protocol

The stick's USB drive (label JANK) holds `page/jank.html`; open it in Chrome or Edge and press MIDI port. The page talks to the stick over the
Web MIDI USB port in one SysEx line layer. It shows HOME
(tempo, the beat in the bar, play / stop, network, peers, battery, input level and beat confidence: the `home` line) and
the settings (`get` / `set`), the details (`status`: air and frame counters, peers heard) and download
mode. It does not flash.

One command a line; each answer ends with `ok` or `err <why>`:

```
get | status | about | home | scan | help | reboot | bootloader
tempo 20..999 (BPM, up to two decimals) | play | stop
set group 1..9 | name <text> | filter link|all | peer on|off | transport link|auto | lead on|off
set sleep never|30s|1min|5min | bright 10..100
set quantum 1..16 | offset -100.00..100.00 (ms, 10 us steps) | net jank|wifi | ssid <name> | pass <8..63 chars>
```

`lead` (`get` key `lead=on|off`) is LINK TRANSPORT's FOLLOW+LEAD (on) or FOLLOW (off), with `transport link`; the menu's own preference, not a setting: `set lead` and the menu both write `s_prefs.lead` (`firmware/main/uiscreen.c`, `uis_lead_get` / `uis_lead_set`; the ui task applies it and saves it by the usual 400 ms rule), `get` reads it through `struct jk_snapshot`'s `lead`, and `set lead` answers `ok` with the action `JK_CTL_LEAD` (`jk_ctl_lead` parses the value; `cfg` is untouched). The menu's three choices are therefore `set transport auto` (IGNORE), `set transport link` + `set lead off` (FOLLOW), `set transport link` + `set lead on` (FOLLOW+LEAD). An older jank has no `lead` key and answers `err bad setting`; the page then keeps the old behaviour (FOLLOW refused).

`sleep` (`never|30s|1min|5min`, the menu's SCREEN OFF) and `bright` (10..100 %, the menu's BRIGHTNESS) are the same kind: `get` keys `sleep=` / `bright=`, `set sleep` / `set bright` write `s_prefs.sleep` / `s_prefs.bright` (the 'ui1' blob, 400 ms rule, `uis_sleep_get` / `uis_sleep_set` / `uis_bright_get` / `uis_bright_set`; the started bit in bit 7 of the sleep byte is a separate field and stays), answer `ok` with the action `JK_CTL_DISPLAY` (`jk_ctl_display` parses; `cfg` untouched), and a bad value answers `err bad setting`. The jank (XIAO) has no screen: the value is only stored and shown back by the page's menu. An older jank has neither key: the page refuses the two menu rows (`this jank has no screen setting`).
`home` answers `home=h ...`, the HOME line the page polls (`core/jank_home.h`). `scan` answers one `scan=group,channel,frames,units,janks,rssi` line per group heard (or `scan=none`) and joins
nothing. `net jank` is the protocol's name for jank mode. `tempo`, `play` and `stop` change the whole Link session, as
any Link app may; with no settled session (joining one) they answer `err no session`. Alone the stick runs a session of
its own in either mode (`core/jank_peer.h` jp_solo; LINK mode since M2l, as a lone Ableton Live: the last tempo known or
120 BPM, stopped until the user's Start, which starts it on its next bar line with the MIDI clock, whatever LINK TRANSPORT
while alone); in AUDIO mode the tracker leads it and the MIDI clock starts on its bar line once the tracker locks.

- **Air statistics** (MODE JANK; `status` keys `air_lost_pct`, `air_retry_pct`, `rssi_min`, `rssi_max`; the NETWORK screen's `LOST`, `RX RETRY`
  and `LEVEL` rows). Every frame of a peer carries the sender's 12-bit sequence number (802.11 sequence control), so what the peers' frames
  say about the air is counted when they arrive (`core/jank_core.c` `note_peer`, all peers together, over a tumbling 10 s window; the last
  completed window is the one shown, nothing until the first one completes). In the window:
  *heard* = peer frames counted; a copy of a frame already counted (Retry bit set and the same number as that peer's last) is a *duplicate*
  and is not heard again; *missed* = the sequence numbers a peer skipped between two counted frames, `((seq - last) & 0xFFF) - 1`, a gap above
  2000 counted as 0 (the peer restarted, or was away: the count starts again); *retried* = frames with the Retry bit set, the duplicates too;
  the level = the weakest and the strongest rssi over the counted frames.
  `air_lost_pct` = round(100 x missed / (missed + heard)); `air_retry_pct` = round(100 x retried / (heard + duplicates)); each is `-1`
  (not known) with fewer than 10 frames in its base, and while no window has completed or no peer was heard in it. `rssi_min` / `rssi_max`
  are `0` for none, like `rssi`. The per-peer `peer=` line is unchanged.
  Limits: *RX RETRY is the peers' retries as received here, not this device's own*: a group-addressed frame is never acknowledged, so a sender
  cannot know it was retried and the radio gives no per-frame retry count; there is no number for this device's own sends. A loss is a frame
  the peer numbered and this device did not receive (or did not decode), so a unicast to another device that was heard counts as heard, and a
  peer table that is full (`JK_PEERS`) leaves the units beyond it out of every number. Joined to a router (Wi-Fi client) nothing of this exists:
  the keys read -1 / 0 and the screen shows SIGNAL only (the router's rssi, `wifi_rssi`).
- **`about`** (the ABOUT menu screen's numbers, for the page's simulation of that screen). One `key=value` line each, then `ok`:
  `uptime_s` (seconds since boot), `reset` (why the chip last reset, upper case: `POWERON`, `PANIC`, `INT WDT`, ...), `temp_c` (chip temperature, whole
  degrees), `cpu_pct` (core 0's load: 100 minus its idle task's share, `beat` `core0_idle_x10`), `cpu1_pct` (core 1's load: the MIDI clock interrupt's share of the cycles, `beat` `core1_busy_x10`, counted in `midi.c`'s `on_alarm`; the FreeRTOS tick and other interrupts on core 1 are not in it; `-1` before the first second), `heap_kb` (free internal heap), `heap_min_kb` (the
  least free since boot), `psram_kb` (free PSRAM, `-1` on a board without), `flash_kb` (the flash chip), `app_used_kb` (the running app image),
  `app_part_kb` (its partition), `clock_late_us` (the MIDI clock alarm's worst lateness since boot), `ui_loop_us` and `beat_loop_us` (the longest pass
  of the UI task / the beat task since boot). The firmware's own sampler (`firmware/main/uiscreen.c`, at most every 2 s in the ui task, never on the beat, clock
  or audio path) fills them; the screen and `about` read the same values, nothing is sampled for the reply (`uis_sys`). Unknown: `-1`, `temp_c=-999`,
  `reset=` empty (the screen's own unknowns; the page shows `-`). `panic=<task>@0x4200abcd` (after `reset`) is the last panic since power was lost (the task and the faulting address), empty when there was none; the ABOUT screen's PANIC row shows it. The name and version are not in it (`get` has `version=`). It is a second reply because
  `status` has no room for it: the answer buffer is `JK_CTL_OUT_MAX` = 2040 bytes (under usb.c's 2048; the SysEx layer chunks any length into 30-character
  frames), the longest `status` is 1894 bytes (`tests/test_ctl.c` prints it) and `about` is up to 273. The page asks for `about` only while its simulated
  screen shows ABOUT (every 1.5 s, one at a time), so its other polling is unchanged; a jank without the command answers `err unknown command`, and the
  page then leaves the screen's values as `-`.
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
  manual state: stopped") **unless the user's last press was Start**: that press (HOME's YES, `play` / `stop`, a click; never the gate's or
  the silence's stop) is stored in the `ui1` blob (bit 7 of the SCREEN OFF byte, the usual 400 ms rule, flushed at a restart), and a
  jank that comes up with a stored Start does that Start at boot (nothing is sent to a Link session). An old blob has no such bit: stopped.
  Stopped, the outputs wait for the user's Start (`play`, the page's Start,
  a click of a screenless unit's button: with the user's layer stopped a click is that Start, otherwise the resync); a
  session already playing is not stopped for it. The outputs run only when user = started AND automatic = open:

  | user | automatic | outputs |
  |---|---|---|
  | stopped (power-on, `stop`) | anything | stopped; only the user's `play` ends it |
  | started | open | run (from the session's bar line once the tracker is locked) |
  | started, not yet running | gate closed (conf < threshold) | stopped; open at threshold + 5: run, no new Start |
  | started, running | conf < threshold (tuned follower, STOP / START) | keeps running: once the outputs run only LOST, silence or the follower giving up stop them (a drop of the same song holds, 2026-11-19) |
  | started | silence (0.5 s under -60 dBFS) | stopped at once; ends a Start's override: the next music waits for the gate |
  | started | LOST (docs/beat.md): 2 near-silent lines, 2 bars of strong beats off the grid, or (`set newsong on`) a new song's beat-less intro | stopped (the gate closes); 3 strong beats or kicks on the grid: run again |
  | started | after a pause (silence or LOST's near silence) | the old grid is not taken back; RESUME (docs/beat.md) finds the song's new phase by cross-correlation and runs at once (~1.5 s after the music returns), or the follower locks anew |
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
  until the bar line. The screen's top-left icon: triangle = running, blinking triangle = armed, square = the user's Stop (`hold=1`, either mode),
  two bars (pause) = not running, not armed and `hold=0` (the machine layer holds the outputs: no bpm yet, gate, silence, LOST). The HOME line's `conf` is the new confidence.
  **NEW SONG** (`set newsong on|off`, default off, not stored; `get` says `newsong=`; the tuned follower with STOP /
  START): LOST also stops the outputs when, after 4 grid lines with no strong beat or kick, the harmony of the last 6 s
  is not the song's (core/beat/jb_song.h: catches about half of the cuts to a beat-less intro, but trips in ~1 drop in 7
  of the same song: an option).
  **The confidence SHOWN is graded** (`set graded on|off`, default on, not stored: on again at every start; both followers; `get`
  and `status` say `graded=`): the CONFIDENCE bar, HOME's and `status`'s `conf` are the graded value (docs/beat.md "The graded
  confidence") instead of the gate's, which is about 100 % whenever the follower is locked and 0 % when not. The gate itself is
  unchanged: STOP / START, the threshold, `gated`, `outputs`, the LED's low-confidence blink, the state word's COAST and the
  CONFIDENCE colour judge the gate's own value, `status` `conf_gate=` (equal to `conf` while `graded=off`). With `graded=off`
  the gate's value is shown, as before. The CONFIDENCE notch (the stop threshold) is not drawn while the graded value is
  shown: the threshold applies to the gate's value, not to the number beside it.
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
  plays. The tracker's start waits up to 12 tracked beats after its lock (8 before M2k) for the estimator: found, the start lands on the
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
  The menu's USB MIDI CLOCK OUT edits `cfg.usbmidi` (ON / OFF).
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
- `diag` after a panic: the chip has no console and no core dump, so `main/panic_info.c` wraps ESP-IDF's panic handler
  (`-Wl,--wrap=esp_panic_handler`; the real handler still runs) and keeps a 92-byte record in RTC memory (survives the
  restart, not power loss; magic word and checksum, garbage is dropped). The next boot moves it out of RTC memory, and
  `diag` then carries, until the next restart: `panic_pc=0x4200abcd` (the faulting address), `panic_cause=<EXCCAUSE>`,
  `panic_vaddr=0x...` (EXCVADDR), `panic_a0=0x...` (return address), `panic_sp=0x...`, `panic_core`, `panic_task` (the
  task that ran), `panic_up_ms` (uptime at the panic) and `panic_msg` (the panic text, first 39 characters; for an abort()
  its message). None of them appear when nothing panicked. `reset=panic` (or `int_wdt`, `task_wdt`) goes with them.
  **Where the saved panic can be read** (all from the same record, kept until power is lost): (1) the file `PANIC.TXT` on the JANK drive,
  rebuilt at every boot (`drive.c`, `jk_panic_file` in `core/jank_panic.c`; it exists only when a panic is recorded): the uptime at the panic, the
  task and core, `EXCCAUSE` (with its name when it is a common one), `pc` / `a0` / `sp` / `vaddr`, the message, the firmware version and the decode
  line `xtensa-esp32s3-elf-addr2line -pfiaC -e jank.elf <pc> <a0>` (the elf must belong to exactly this firmware); (2) the page's Advanced card, a
  line `Last panic: <task> at 0x4200abcd (up 12 s)` under `boots n · reset: x`, shown only when `diag` has the `panic_*` keys; (3) the ABOUT
  screen's last row `PANIC`: `-` when none, else `<task> 0x4200abcd` (the task cut to 3 characters on the OLED, 7 on the colour screen, the address
  whole). The ABOUT screen has 13 rows (the colour layout pages by 9: rows 9 to 12 on page 2; the OLED by 5: 0, 5, 10).
  **Decoding an address**: with the toolchain on the PATH (`<scratch>/espressif/tools/xtensa-esp-elf/<version>/xtensa-esp-elf/bin`, today `esp-14.2.0_20260121`),
  `xtensa-esp32s3-elf-addr2line -pfiaC -e <build>/jank.elf 0x4200abcd` (do it for `panic_pc` and `panic_a0`: the
  function, file and line; a `panic_a0` of the form `0x8...` is the windowed ABI: replace the leading 8 by 4). The elf must be the one of the image that crashed: the build directory of the flashed
  jank.bin (the coordinator keeps it); an elf of another build gives wrong lines without any error.
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
tempo), the Link peer, SysEx framing, the update protocol (C logic and `scripts/ota.py` against it, over a line that loses, damages and repeats lines; needs python3), the HOME line (`test_home`), the made-up JANK drive and the image streaming into a modelled slot (`test_vdrive`; `JANK_TEST_IMAGE=<jank.bin>` adds a real image) and the control page's logic (needs node). `tests/crosscheck_optimist.c` compiles
the FM-1's own `wifi_jank.c`, `wifi_ip.c` and `link.c` from `FM1BASE` and swaps frames with jank. These prove that both
sides agree on bytes and logic, nothing about radios: delays in the models are made up.

## On the air (summary)

Open data frames at 1 Mb/s on channels 1, 6, 11 with the group's BSSID `02:66:6D:31:6A:GROUP`, no association, no key.
The bridge does no routing or NAT; the computer self-assigns 169.254.x.y and shares the radio's MAC. By default only
ARP, IPv4 unicast and Link multicast go to the air. Details, the Link peer and the scan: [air.md](air.md).
USB functions and host mode: [usb.md](usb.md). Clock and timing: [midi.md](midi.md).

