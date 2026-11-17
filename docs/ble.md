# A phone over Bluetooth (BLE MIDI)

A jank with no screen is set up from a phone: the control page (`page/jank.html`, the same page as over USB) talks to it
over BLE MIDI. Built with `CONFIG_JANK_BLE_MIDI` (`firmware/sdkconfig.ble`, `sdkconfig.ble-tuned`); `main/ble_midi.c`,
`core/jank_phone.h`, `core/jank_blemidi.h`. **Built only; nothing of it has run on hardware yet.**

## When Bluetooth is on

The radio is shared with Wi-Fi and Wi-Fi comes first, so the Bluetooth controller is **off** unless a phone needs it: on
only while a pair window is open or a phone is connected. Off means off (`nimble_port_deinit`: the controller disabled
and its memory freed), not idle: MODE JANK's frames then have the radio to themselves.

| What | Bluetooth |
|---|---|
| Three clicks of the button (or `ble pair` on the control line, the page's "Open a Bluetooth window" over USB) | A pair window: on and advertising as `jank XXXX` for 2 minutes. The LED blinks blue 4 times a second |
| A phone connects | Stays on as long as it is connected; no more advertising (one phone at a time) |
| The phone leaves | 30 s of advertising for it to come back, then off |
| The window ends with no phone | Off |
| `ble close` (the page's "Done: Bluetooth off") | Off now, the phone's link too |

The setting `ble` (`set ble pair|auto|off`, saved):

- `pair` (default): a window only when asked for on the device.
- `auto`: also a window at every start, for a unit with no button at hand.
- `off`: never. Three clicks give 3 red pulses, `ble pair` answers `err ble is off`.

## What goes over it

- **The settings**: `get`, `set`, `scan`, and `tempo`, `play`, `stop`: the control line (`core/jank_sysex.h`) as SysEx in
  BLE-MIDI packets, several 30-character frames per packet when they fit (`jbm_pack`).
- **HOME**: the jank pushes `F0 7D 'J' 'B' 0x11 <text> F7` while a phone listens, at most 2.5 times a second, and once in
  2 s when nothing changed:

  ```
  h <bpm x100> <net> <transport> <bar> <quantum> <link peers> <air peers> <battery> <level> <conf> <min> <max> <mode>
  h 12000 j3 1 275 4 2 0 - -24 87 -31 -18 1
  ```

  `net` is `j<group>` (MODE JANK) or `w1`/`w0` (on the Wi-Fi network / looking for it); `transport` 0 stopped, 1
  playing, 2 a start armed for a bar ahead (in AUDIO mode: the clock outputs, `core/jank_hold.h`); `bar` the session's
  place in the bar in hundredths of a beat (`-` with no session); `battery` percent (`-`: no gauge; jank mini 2.2 reads its cell, docs/linein.md);
  `level` the input meter (RMS over 0.3 s, a value every 250 ms, dBFS), `conf` the confidence in percent (how well the
  last 8 s of tracked beats fit the published grid, `core/jank_conf.h`: LOW CONFIDENCE's), `min` / `max` the meter's lowest and highest values over the last 3 s (`-` with no audio
  stream), `mode` 1 AUDIO (the source has sound: the tracker leads, play / stop hold the outputs) or 0 LINK. `home` on the control line answers
  the same line (the page polls it over USB).
- **Nothing else**: no beat reports (they stay on USB), no MIDI clock (connection events 30..60 ms apart, some given up to
  Wi-Fi, cannot carry 24 PPQN), no screen.
- **Refused over Bluetooth** (`err ... over USB only`): `ota` (a dropped link would leave a slot half written),
  `bootloader`, and `set pass`: the link is not encrypted, so the Wi-Fi password would cross the air in clear text. The
  page greys the password out over Bluetooth; a saved password stays.

## The link

- **No bonding, no pairing needed.** A phone gets on only during a window opened on the device, and nothing is
  remembered: a remembered phone would need the controller on all the time (to advertise to it or listen for it), which
  is what the rule forbids. A phone that asks to pair gets "just works" encryption for that connection, no keys kept.
- **Connection parameters** asked for: interval 30..60 ms, peripheral latency 4 (the jank may skip 4 events in a row when
  it has nothing to send, which gives Wi-Fi those slots; a line from the phone then waits up to 300 ms), supervision
  timeout 4 s. Within Apple's rules as stage 1 quoted them (Accessory Design Guidelines, Bluetooth LE: max x (latency + 1)
  <= 2 s, timeout > 3 x that); the phone may keep its own.
- Advertising every 152.5 ms; the service's UUID in the advertisement, the name `jank XXXX` (the MAC's last two bytes)
  in the scan response. The coexistence preference is Wi-Fi (`esp_coex_preference_set`, deprecated in IDF 5.5, effect on
  the S3 undocumented).

## The phone side: the page

`page/jank.html`, served by the jank's own drive over USB, and by any web server for a phone. One SysEx line layer, two
transports, a button each:

- **MIDI port**: Web MIDI with SysEx. The USB port (Chrome or Edge on a computer), or, inside an iPhone MIDI browser app,
  the jank the app connected over Bluetooth. Port named `jank` = USB; `jank XXXX` = Bluetooth (the light mode: HOME
  pushed, no detail polling, no password). Calls kept to what the apps' polyfills take (Web MIDI Browser's open source;
  MIDIWeb Browser's is closed): one `.then(ok, fail)` on `requestMIDIAccess`, ports through `forEach`, input through
  `onmidimessage`, SysEx put back together from chunks, `send` in a try (it throws when the app's SysEx switch is off).
- **Bluetooth**: Web Bluetooth (Chrome on a computer or Android), straight to the BLE-MIDI GATT service; the packets made
  in the page, ported from `core/jank_blemidi.c` and checked byte for byte against `tests/test_blemidi.c`'s vectors
  (`tests/test_page.js`). Writes of 20 bytes at most (the default ATT MTU, which every link takes: Web Bluetooth does
  not tell the MTU).

## iPhone test checklist

No app of our own: **MIDIWeb Browser** (App Store id6757226617, preferred) or **Web MIDI Browser** (id953846217).

1. Serve the page where the phone can reach it, e.g. from the Mac on the same network:
   `python3 -m http.server 8093 --directory ~/GitHub/jank/worktrees/ble-midi/page`, then `http://<the Mac's address>:8093/jank.html`.
   Whether the apps give Web MIDI to a plain `http` page is not known: if MIDI port finds no access, an `https` copy is
   needed.
2. Three clicks on the jank: the LED blinks blue.
3. In MIDIWeb Browser: its Bluetooth MIDI settings, connect `jank XXXX`; note the exact name shown. The LED stops
   blinking (connected). Turn SysEx on in the app's settings.
4. Open the page; press **MIDI port**. Note the port name the page shows (`link-state`: "Bluetooth" expected). If a port
   chooser appears, pick the jank.
5. The settings fill in (the `get` answer arrived whole). HOME: BPM, beat boxes moving, network, peers.
6. Change a setting (beats a bar), set a tempo, Play / Stop; each takes effect on the jank (and on Live, through Link).
7. Watch HOME for 5 minutes: the beat boxes keep moving, no "no answer" errors.
8. **Done: Bluetooth off**: the link ends, the port goes away, the jank's Bluetooth is off (`status` over USB:
   `ble_state=off`).
9. Three clicks again and reconnect from the app; then from the page without reloading; then after a reload.
10. Lock the phone 30 s, and switch apps for 30 s: does the link survive, does HOME come back?
11. Leave the phone connected and let the window time pass (2 min): the link stays. Turn the phone's Bluetooth off: the jank
    advertises 30 s (LED blinks), then stops.
12. With the jank in MODE JANK in a group with an FM-1: compare `status` (`air_in`, peers) with Bluetooth off, in a window,
    and with the phone connected: the frames lost to Bluetooth.
13. Repeat 3-6 in Web MIDI Browser.
14. Chrome on a computer or Android: three clicks, **Bluetooth**, pick `jank XXXX`; repeat 5-8.

Report: the app's name for the port, MTU and heap (`diag` over USB: `ble=... mtu=... heap_taken=...`), anything that did
not arrive.

## Size

XIAO image with BLE (`sdkconfig.ble` + `ble-tuned`): 1,256,784 bytes; internal RAM left after static use 51,629 B (image
M1d without BLE: 29,949 B), because the tuned build moves static buffers to PSRAM. Bluetooth's runtime cost inside the
chip when on is 13..18 KB (stage 1's estimate, unmeasured); when off, none. FH4R2 (`sdkconfig.fh4r2`, two 1.6 MB slots):
1,254,368 B in a 1,638,400 B slot (384,032 B, 23 % free), 54,117 B internal left.
