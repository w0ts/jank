# Launchpad X plays a Digitone II (Note mode)

In USB host mode (docs/usb.md), a Novation Launchpad X and an Elektron Digitone on a powered hub: the Launchpad's pads play
the Digitone, and its pads light for the notes the Digitone sounds. USB only; Jank is the clock, so the Launchpad's
flashing and pulsing follow it. First slice: **Note mode on stock MIDI**, nothing changed in the Digitone.

## What it does today

- Pads play the Digitone's current track on that track's MIDI channel (track n on channel n, a table in `core/jank_lpapp.h`).
  Layout: diatonic, C major, octave 3 (Launchpad 95's `MelodicPattern`, `core/jank_lpnote.c`; chromatic and diatonic chords
  are in the core, no button picks them yet).
- Buttons in Note mode (owner, 2026-11-23): up / down = octave, left / right = the previous / next track, one at a time, no
  wrap. On an 8-track page (Mixer, later) up / down will be the bank 1-8 / 9-16 and left / right do nothing.
- Pad lights: the scale's root blue, the other scale notes white, a note the Digitone sounds (or a pad that is down) green;
  one note lights every pad with that pitch (the layouts overlap). The arrows and the Note button are lit.
- A pad that is down when the track changes ends its note on the channel it started on; a Launchpad that goes away ends
  every note it had down.
- **Scale page: hold Note.** While the Note button is held the pads are Launchpad 95's scale editor, rows from the top: (1) absolute
  root (pad 1), horizontal (pad 2, diatonic only), the layouts: chords (5), diatonic (6), chromatic (7); (2) the black keys, the
  relative scale (pad 3), the circle of fifths back (pad 7); (3) the white keys C to B, the circle of fifths forward (pad 8); (4) the
  octaves; (5 to 8) the scales. Selected is green; the keys are white and dim grey, the layouts cyan, the octaves yellow, the scales
  blue, the relative and fifths pads orange. Let go of Note and the pads play again; a note that was down when the page opened still
  ends. The scales are a **starter list of 32** (`core/jank_lpscales.h`: standard names and intervals, neither Live's nor the
  Digitone's list). LP95's two guitar / not-staggered layouts, the drum rack and quick scale are not there. Host-tested; **not
  verified on the Launchpad itself** (how the page looks and feels).

## What is not there yet

The Digitone's own scale list (the scale page uses a starter list), Step, Mixer, Session, the song
row. Order decided with the owner: Note, Step sequencer, Mixer, then patterns and song mode last. The Digitone's current
track, scale and step data need new commands in dn2-wishes (its test channel only answers when asked); that research is
separate.

## Verified against Novation's Launchpad X Programmer's reference manual

- Two USB-MIDI interfaces: DAW (the first) and MIDI (the second). Programmer mode works on the MIDI one, so for a device
  named "Launchpad X" `usb_midi_host.c` takes the **second** MIDI streaming interface.
- Programmer mode: `F0 00 20 29 02 0C 0E 01 F7` (0 = Live mode). **Measured** (2026-11-24, Wi-Fi `diag`): the Launchpad X sends the
  same message back (three packets, the last `07 0E 01 F7`). A replugged Launchpad stayed dark after the unit sent the message the
  moment it was claimed (cause not proven; a firmware that is not up yet is the guess). So the unit waits 500 ms after a Launchpad
  appears, sends the message, waits for the answer before any light, and sends it again every 400 ms (at most 4 times, then the lights
  go anyway). `diag`: `hostdiag_lp=...,programmer:<sent>,acked:<answered>`.
- **Two more messages are needed before any light shows** (measured 2026-11-24: with the Launchpad in Programmer mode, answering, and taking about 40
  light messages a second, every LED stayed dark, from the unit and from a Mac alike; Launchpad 95's `Launchpad.py` sends them and the pads then
  lit): `F0 00 20 29 02 0C 0A 00 01 F7` (LED feedback: the pads' own light off, the host's messages on) and `F0 00 20 29 02 0C 09 01 F7` (sleep
  mode off). The unit sends both after the Launchpad's answer (`jlx_setup`, `diag` `setup:`), then waits 400 ms, then the lights, and every 2 s it
  sends all lights again and asks which mode the Launchpad is in (`diag` `mode:`; a Live answer makes it enter Programmer mode again, `reentered:`).
- A pad is `10 x row + column` (rows 1..8 from the bottom): 11..88. Top buttons CC 91..98 (up, down, left, right, session,
  note, custom, capture), side buttons CC 89 (top) down to 19, logo CC 99. A pad sends Note On, velocity 0 for the release.
- A light is a Note On (a CC for the buttons) on channel 1 static, 2 flashing, 3 pulsing; the velocity is the palette index
  (0 off). A flash flips between the static colour (A) and the flashing one (B), so a flash is sent as A then B.

## Not verified on hardware

- That the product strings are "Launchpad X" / "Digitone..." (roles are told apart by those substrings).
- That the Digitone's *sequencer* notes come out of its USB port (the pad lights depend on it), and its cable number (0 assumed).
- That the Launchpad takes lights right after the Programmer mode message (a 150 ms wait is a guess, `usb_lp.c`).
- Pipes: the controller has 8 (`usb_hostlib.c`). Hub (2) + Launchpad (control, OUT, IN) + Digitone (control, OUT, IN) = 8, so
  the **Digitone's USB audio is not used** (the beat tracker's audio client skips both devices, `usb_uac_host.c`). A Digitone
  alone, without the Launchpad, is not affected by the pipe count but is skipped the same way.
- A multi-function USB-C dock (hub + card reader + video adapter) used up the pipes (measured 2026-11-24: the Digitone's claim failed
  with 262, the Launchpad's with 257). The unit learns what to leave alone (`core/jank_hostfilter.h`): a device with neither a MIDI
  nor an audio interface is noted; if a Launchpad or Digitone then cannot be claimed for want of pipes, the noted devices go on a
  list kept in flash and the unit restarts once. From then on the library's enumeration filter skips the listed devices behind a hub,
  before they hold a pipe. At most 8 entries (so at most 8 such restarts, ever); `wipe settings` clears the list; `diag` shows it
  (`hostdiag_filter=...,learned:VID:PID+...`). Not verified on hardware yet.
- The Digitone II declares **512-byte bulk endpoints on a full-speed link** (measured 2026-11-24 over Wi-Fi `diag`: `mps:512/512`), which a
  full-speed wire cannot carry and which the USB library's claim refuses for good (`ESP_ERR_NOT_SUPPORTED`, 262, at the interface step) because
  bulk OUT is limited to 64 bytes here. `core/jank_fsclamp.h` corrects the declared size to 64 in the library's copy of the configuration
  descriptor before the claim (`usb_midi_host.c`); `diag` shows how many endpoints it changed (`clamp:`). Not verified on hardware yet.
- No log leaves the unit in host mode (no USB control line, console off): what the pads do is the only readout. Over Wi-Fi
  (`docs/wifi.md`) `diag` ends with the `hostdiag_` lines live.

## Files

`core/jank_lpx.[ch]` (the Launchpad's protocol), `core/jank_lpnote.[ch]` (layouts), `core/jank_lpapp.[ch]` (what a press sends,
what the pads show), `firmware/main/usb_lp.c` (the one state and the two devices), `firmware/main/usb_midi_host.c` (IN endpoints,
interface choice). Host tests: `tests/test_lpx.c`, `test_lpnote.c`, `test_lpapp.c` (in `tests/run.sh`).
