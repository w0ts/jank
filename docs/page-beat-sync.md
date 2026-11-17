# Why the page's beat boxes may not match what you hear, and the Beat display delay

The four beat boxes on the control page's face (the OLED's boxes, drawn from the stick's state) show the position in the bar of the stick's
Link session. They are not computed from the audio the page plays. This is the chain between a beat in the file and the box lighting up,
with who can know each delay. (The page has no LED dots any more; the boxes are the only beat display.)

## What drives the boxes

- **AUDIO mode and LINK mode alike: the HOME line** (`home` on the control line, `core/jank_phone.h`): `h <bpm x100> <net> <transport> <bar> <quantum> ...`.
  `bar` is the session's place in the bar in hundredths of a beat (0.01 beat = 5 ms at 120 BPM), taken when the stick builds the line. In AUDIO
  mode the tracker steers that session, so the box follows the beat the tracker found, aligned by the stick's `beat_latency` (below).
- **How often:** the page asks for `home` every 300 ms over USB (`HOME_EVERY_USB_MS`). Over Bluetooth the stick pushes it at most 2.5 times a
  second and once in 2 s when nothing changed; the page asks itself only when the push stops for 3 s.
- **Between lines the boxes run on the page's own clock** (`Stick.phaseFeed` / `Stick.phaseAt`, `ui/stick.js`, drawn with
  `requestAnimationFrame`): the position advances at the last tempo, and each new line corrects it softly: a quarter of the error,
  backwards by 0.02 beat at most, snapping only past half a beat (a new lock, a moved bar). A stopped transport holds the position; with no
  line for 1.5 s, or a line with no session or tempo (a lost lock), the boxes go empty instead of running on old data.
- **The `status` poll (every 1.5 s over USB)** does not move the boxes: it feeds the gate, the bar estimator, the Bluetooth window.
- **The beat reports** (`F0 7D 'J' 'B' 0x10`, `b <n> <bpm x100> <mode> <audio> <lag us> <link us>` on each tracked beat, `core/jank_sysex.h`) are
  not used by the page any more (only the LED dots used them). Their `lag` field is how long after the instant the stick takes the beat to be
  HEARD (its sample's arrival + `beat_latency`) the report was made: tracker detection delay minus `beat_latency`. A report made about 250 ms
  after the heard instant, with `beat_latency` -186 ms, is normal.

## The delay chain, in order

1. **File position to the page's two outputs.** The same `<audio>` element feeds both.
   - *Monitor* (this computer's speakers): the element plays on the default output. Delay = the output device's latency
     (`AudioContext.baseLatency + outputLatency` of a default-sink context, which is what the page reads).
   - *Stick path*: `audio.captureStream()` into an `AudioContext` whose sink is the stick (`sinkId`), 44.1 kHz mono. Delay = that context's
     `baseLatency + outputLatency`, then the USB audio buffering of the stick's device, which no browser API shows.
2. **The stick hears the audio** at (file position + stick path delay + USB buffering). The tracker needs some audio after a beat to detect
   it (tens of ms, `lag` shows it); the stick then puts the beat where it is *heard* by adding `beat_latency` to the sample's arrival time:
   `beat_latency` = how long after the samples reach the stick the music is heard at the ear. It is negative when the stick gets the audio
   after the speakers play it. It has two layers: `lat_auto` (the auto base per audio source) + `lat_trim` (the AUDIO SHIFT, `nudge ms`).
   Values of -186 / -148 / -38 (sum / auto / shift) were seen on a stick: the stick path was then about 186 ms behind the speakers.
3. **The report to the page.** HOME line over USB MIDI (SysEx, 1 ms USB frames), the Web MIDI event and the JavaScript task: a few ms, jittering.
   The page stamps a line with its arrival time (`performance.now()`), so the boxes are late by that flight time (about 1-3 ms over USB) and by the
   quantisation (up to 5 ms); over Bluetooth by the connection interval (30-60 ms, up to 300 ms with the peripheral latency) and the 400 ms push period
   (the local phase hides the period, not the delay).
4. **The page draws.** One animation frame (up to 16 ms), the 1.5 s `status` poll plays no part.

## Who can know what

| delay | known by |
|---|---|
| monitor output latency, stick path output latency (the AudioContext part) | **the page**: `baseLatency + outputLatency` of each context, shown read-only in the Advanced row |
| `beat_latency`, `lat_auto`, `lat_trim`, the tracker's detection delay (`lag`) | **the stick** (the page reads `beat_latency` from `get`) |
| the stick's USB audio buffering, the speakers' acoustic path, the room, your ears | **nobody**, without a loopback measurement (a microphone at the speaker and the stick's input) |

If the stick's `beat_latency` was set up for another path (the Mac player, another browser, another output device), the session's beat is
aligned for that path and the boxes are off by the difference.

## The Beat display delay (Advanced)

A number box and a slider, -400 to +400 ms in 5 ms steps. **Positive delays the boxes** (the music reaches your ears later than the stick's
beat), **negative advances them** with the local phase. It changes only what this page draws and never the stick's settings. It is remembered
in this browser (`jank.beat.delay`).

Default (until you move it): the estimate `monitor latency - stick path latency - beat_latency`, to 5 ms, the two latencies read from the page's
own contexts and beat_latency from the stick (what the stick already corrects: the LED and the boxes share it, so it must not be
corrected twice; it is 0 once Auto latency has sent monitor - stick); 0 when either latency is unknown.

Auto latency (Advanced, ui/autolat.js, as the Mac simulator does): while Advanced is ticked over USB, the stick's audio source is USB and the
player plays through the stick path, the page sends `set beat_latency <monitor - stick, -300..300>` when it is more than 3 ms from the
stick's `lat_auto`, at most once per 5 s, never at load. Unmeasured: the stick's USB buffering and the capture chain, so the value is
incomplete; finish by ear with Audio shift (`lat_trim`). It covers the part of the difference the browser can read; the stick's USB buffering is inside `beat_latency` (which the
page shows next to it) and is not in the estimate. 'Use the estimate' goes back to it.

Set the rest by ear: play a track with a clear beat, move the slider until the box changes on the beat you hear.
