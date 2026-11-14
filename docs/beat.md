# Beat follow (bench build)

**Bench only. The network weights in `core/beat/beat_model.c` are madmom's (CC BY-NC-SA 4.0): never push or ship an
image of this build.** Our own network will replace that file. The build says so at configure time, in the boot log, in
the `beat` answer (`beat_weights=`) and on the control page.

The stick is a USB speaker called **jank**. What the computer plays to it goes to a beat tracker on core 0 (madmom's
online tracker in C: `core/beat/`), and the tracker is the Link session's master: its tempo, beat grid and stop/start go
into the session (`core/jank_beatlink.c`). Core 1 stays the MIDI clock's alone.

## Build and install

```
JANK_SCRATCH=$PWD/../usbaudio-scratch JANK_JOBS=3 scripts/build.sh xiao
```

The app image is `$JANK_SCRATCH/build-xiao/jank.bin`. The build makes `third_party/esp_tinyusb` from the registry's 3.0.0
with `third_party/esp_tinyusb-uac2.patch` (the audio class on) and sets the CPU to 240 MHz (`firmware/sdkconfig.beat`;
the release runs 160). It installs over a stick running fw/ota with `python scripts/ota.py <jank.bin>` (app only: the
bootloader and the partition table stay). Settings in NVS are kept; the beat settings are not stored and start at their
defaults (`beat on`, `beat_latency 25`).

**Rollback.** The new image confirms itself only after the computer has configured USB *and* spoken on the MIDI control
line (ota.py's `get` after the restart does that, as does a control page), and it has run 20 s. Not confirmed within
120 s, it restarts and the bootloader starts the old image. A crash before that, or a start with no computer, also puts
the old image back (`main/ota.c`, `main/usb.c`).

## Test with a Mac

1. Plug the stick in. Audio MIDI Setup lists an output **jank**: 1 ch, 16-bit, 44100 Hz (the only format).
2. Play to it: beat-listener's player with `--xiao` (a second output stream with the mid of the track, besides the
   speakers), or set the Mac's output to jank (silent, tracker only), or make a **Multi-Output Device** of the speakers
   and jank, 44.1 kHz, the speakers as the clock source and drift correction on for jank.
3. Live (or any Link app) with Link on joins over the stick's network adapter as usual. Within some beats the session's
   tempo follows the track, the stick's LED flashes on the beat and Live's Link beat lines up with what is heard. When
   the music stops, Link stops; when it starts again, Link starts on the next bar (quantum 4).
4. Line up the phase by ear with the page's **Latency, ms** (or `set beat_latency <-300..300>`): the music is heard that
   long after its samples reach the stick (speaker output and USB buffering); negative when the stick gets the audio
   after the speakers play it. Default 25; not stored (back to 25 at each start; the set itself writes no flash: the
   stored settings are unchanged and NVS skips an identical blob). `set beat off` stops the
   tracker steering Link and leaves everything else as it was.

## Beat reports

On its own, with no question asked, the stick sends a SysEx `F0 7D 'J' 'B' 0x10 <text> F7` on the MIDI port: `b <n>
<bpm x100> <mode> <audio> <lag us>` on each tracked beat (n counts the beats) and `t <n> <bpm x100> <mode> <audio>
<latency ms>` once a second (n: the last beat's). `mode` is stopped, steady, faster or slower; `audio` is 1 while the
computer streams. `lag` is how long before the report the beat is taken to be heard (its sample's arrival +
`beat_latency`, as the Link grid uses it): a reader that stamps the report's arrival subtracts it to get that instant.
`latency` is the `beat_latency` setting. The control line's readers (ota.py, this page) match `F0 7D 'J' 'K'` only and
never see them (`core/jank_sysex.h`, `JX_PUSH_*`). beat-listener's `player --xiao` scores them against Traktor's beats.

## Reading the numbers

On the control page (Chrome or Edge, Connect), the section **Beat follow** shows the `beat` answer every second. Without
the page, send `beat` on the control line (SysEx on the MIDI port, [jank_sysex.h](../core/jank_sysex.h)):

| Key | Meaning |
|---|---|
| `audio` | 1 while the computer streams to jank |
| `mode`, `bpm` | the tracker's mode (stopped, steady, speeding_up, slowing_down) and tempo |
| `heap_free`, `heap_min`, `heap_largest` | internal heap now, lowest since boot, largest block; Wi-Fi, Link, USB and the tracker running |
| `stack_free` | bytes the tracker task's stack never used |
| `cpu_pct` | the tracker task's share of core 0 over the last second, preemption by Wi-Fi included (an upper bound) |
| `hop_avg_us`, `hop_max_us`, `hop_avg_cycles` | wall time and CPU cycles of the work for one 10 ms frame |
| `packets`, `gaps`, `overflows`, `resets` | audio packets taken; 1 ms frames with no packet while streaming; ring overflows (the tracker restarts); restarts |
| `tempo_sets`, `phase_forces`, `starts`, `stops` | what the tracker did to the Link session |
| `err_us` | the session's beat grid against the tracked beat before the last correction; stays within 5 ms when they agree |
| `offset_us`, `latency_us` | the sample-count-to-local-time offset (µs since the timer started) and the latency setting |

## How it works

- **USB.** A UAC2 function joins NCM, MIDI and the drive: interface association, clock source 4 (fixed 44.1 kHz), input
  terminal 1 (USB streaming), output terminal 3 (speaker), no feature unit (no volume), one streaming interface with an
  isochronous OUT endpoint 5 (mono, 92 bytes, adaptive: the IN endpoints are all used, so no feedback endpoint). FIFO
  words of the S3's 256: DMA info 14, RX 76 (92-byte packets), TX 80 (EP0, NCM x2, MIDI, drive 16 each): 170 used, 86 spare.
- **Audio path.** Each 1 ms packet goes as it is (mono int16) into a 4096-sample ring in the USB interrupt
  (`usb_audio.c`); the tracker task (priority 3, below Wi-Fi 23, Link/bridge 20, TinyUSB 5) takes it 441 samples at a time.
- **Network weights.** Built once at the start into RAM as int16, one float scale per gate unit, with a tanh table
  (`core/beat/jb_lstm_q16.c`, 61.7 KB): read from flash every frame they cost more than a core. Host test: the same 61
  beats on the same frames as the float network on 30 s of a 124 BPM loop, activation error < 1e-3.
- **Time.** A sample's local time is `offset + s / 44100 + latency`, the offset being the smoothed difference between a
  packet's arrival (the Link host clock, `midi_now_us`) and its sample count, so the computer's audio clock sets the
  stream and jitter in the packets averages out.
- **Link master.** Tempo is set when it differs from the session's by more than 0.02 BPM; the beat grid is forced
  (`lk_force_beat`, new in `third_party/link`) only when more than 5 ms from the tracked beat; stopped: Link stop; running
  again: Link start at the next bar line at least 150 ms ahead.
