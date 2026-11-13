# Beat follow (bench build)

**Bench only. The network weights in `core/beat/beat_model.c` are madmom's (CC BY-NC-SA 4.0): never push or ship an
image of this build.** Our own network will replace that file. The build says so at configure time, in the boot log, in
the `beat` answer (`beat_weights=`) and on the control page.

The stick is a USB speaker called **jank**. What the computer plays to it goes to a beat tracker on core 0 (madmom's
online tracker in C: `core/beat/`), and the tracker is the Link session's master: its tempo, beat grid and stop/start go
into the session (`core/jank_beatlink.c`). Core 1 stays the MIDI clock's alone.

## Build and flash

```
JANK_SCRATCH=$PWD/../beat-follow-scratch JANK_JOBS=3 scripts/build.sh xiao
```

The image is `$JANK_SCRATCH/build-xiao/` (`jank.bin`, bootloader, partition table, the page's drive). The build makes
`third_party/esp_tinyusb` from the registry's 3.0.0 with `third_party/esp_tinyusb-uac2.patch` (the audio class on) and
sets the CPU to 240 MHz (`firmware/sdkconfig.beat`; the release runs 160). Flash as in [firmware.md](firmware.md), with
`JANK_SCRATCH` set to that scratch folder: download mode first, then the `write_flash "@flash_args"` line. Settings in
NVS are kept; the beat settings are not stored and start at their defaults (`beat on`, `beat_latency 25`).

## Test with a Mac

1. Plug the stick in. Audio MIDI Setup lists an output **jank**: 2 ch, 16-bit, 44100 Hz (the only format).
2. Either set the Mac's output to jank (silent, tracker only), or make a **Multi-Output Device** of the speakers and
   jank, 44.1 kHz, the speakers as the clock source and drift correction on for jank, and play a track through it.
3. Live (or any Link app) with Link on joins over the stick's network adapter as usual. Within some beats the session's
   tempo follows the track, the stick's LED flashes on the beat and Live's Link beat lines up with what is heard. When
   the music stops, Link stops; when it starts again, Link starts on the next bar (quantum 4).
4. Line up the phase by ear with the page's **Latency, ms** (or `set beat_latency <0..300>`): the music is heard that
   long after its samples reach the stick (speaker output and USB buffering). Default 25. `set beat off` stops the
   tracker steering Link and leaves everything else as it was.

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
  isochronous OUT endpoint 5 (180 bytes, adaptive: the IN endpoints are all used, so no feedback endpoint). FIFO words of the
  S3's 256: DMA info 14, RX 120 (180-byte packets), TX 80 (EP0, NCM x2, MIDI, drive 16 each): 214 used, 42 spare.
- **Audio path.** Each 1 ms packet is mixed to mono int16 into a 4096-sample ring in the USB interrupt
  (`usb_audio.c`); the tracker task (priority 3, below Wi-Fi 23, Link/bridge 20, TinyUSB 5) takes it 441 samples at a time.
- **Time.** A sample's local time is `offset + s / 44100 + latency`, the offset being the smoothed difference between a
  packet's arrival (the Link host clock, `midi_now_us`) and its sample count, so the computer's audio clock sets the
  stream and jitter in the packets averages out.
- **Link master.** Tempo is set when it differs from the session's by more than 0.02 BPM; the beat grid is forced
  (`lk_force_beat`, new in `third_party/link`) only when more than 5 ms from the tracked beat; stopped: Link stop; running
  again: Link start at the next bar line at least 150 ms ahead.
