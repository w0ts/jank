# Launchpad X plays a Digitone II (Note mode)

In USB host mode (docs/usb.md), a Novation Launchpad X and an Elektron Digitone on a powered hub: the Launchpad's pads play
the Digitone, and its pads light for the notes the Digitone sounds. USB only; Jank is the clock, so the Launchpad's
flashing and pulsing follow it. First slice: **Note mode**, played through the Digitone's live control (SysEx, below), not MIDI channels.

## What it does today

- Pads play the Digitone's current track by live control (below), on no MIDI channel.
  Layout: diatonic, C major, octave 3 (Launchpad 95's `MelodicPattern`, `core/jank_lpnote.c`; chromatic and diatonic chords
  are in the core, no button picks them yet).
- Buttons in Note mode (owner, 2026-11-23): up / down = octave, left / right = the previous / next track, one at a time, no
  wrap. On an 8-track page (Mixer, later) up / down will be the bank 1-8 / 9-16 and left / right do nothing.
- Pad lights, **the colours of Launchpad 95** (its `SkinMK2.py`, palette indexes from its `ColorsMK2.py`): the scale's root BLUE (45), the other
  scale notes LIGHT_BLUE_HALF (38), a note outside the scale DARK_GREY (1, the chromatic layout), a note the Digitone sounds (or a pad that is
  down) GREEN (21); one note lights every pad with that pitch (the layouts overlap). The arrows are GREEN (Note.Octave) and dim to GREEN_THIRD (23)
  where they can go no further (the lowest or highest octave, the first or last track); the Note button is LIGHT_BLUE (37, Mode.Note).
- A pad that is down when the track changes ends its note on the track it started on; a Launchpad that goes away ends
  every note it had down.
- **Scale page: hold Note.** While the Note button is held the pads are Launchpad 95's scale editor, rows from the top: (1) absolute
  root (pad 1), horizontal (pad 2, diatonic only), the layouts: chords (5), diatonic (6), chromatic (7); (2) the black keys, the
  relative scale (pad 3), the circle of fifths back (pad 7); (3) the white keys C to B, the circle of fifths forward (pad 8); (4) the
  octaves; (5 to 8) the scales. Colours as Launchpad 95's `Scale.*`, each bright when on and its "third" when off: the keys and Horizontal
  GREEN, AbsoluteRoot, the layouts and the octaves RED, the scales BLUE, the relative scale and the circle of fifths plain BLUE. Let go of
  Note and the pads play again; a note that was down when the page opened still
  ends. The scales are a **starter list of 32** (`core/jank_lpscales.h`: standard names and intervals, neither Live's nor the
  Digitone's list). LP95's two guitar / not-staggered layouts, the drum rack and quick scale are not there. Host-tested; **not
  verified on the Launchpad itself** (how the page looks and feels).

## SysEx only: no MIDI channels (live control)

Owner, 2026-11-24: playing must not depend on how the Digitone's MIDI channels are set, and must not mix with the MIDI it receives otherwise; "SysEx only"
(2026-11-24: "go for 2"). So the pads **never use a MIDI channel**: the Digitone is played, told its track and read through its **live control** (the SysEx channel
of dn2-wishes, `docs/live-control.md` there; `core/jank_dn2live.[ch]` here; needs SETTINGS > SYSTEM > USB CONFIG > REMOTE CONTROL ON). The MIDI the Digitone
sends is not read at all. (The clock stays MIDI: Jank is the clock.)

- Jank asks for live control (LIVE ON, with the note push) as soon as both devices are up, keeps it (a request at least every 4 s; the lease is 10 s) and
  asks again every second until it answers; if the Digitone says REMOTE CONTROL is off, or has no live control, it asks again every 5 s. `diag` shows it:
  `hostdiag_live=state:live,play:1,asked:..,replies:..,plays:..,selects:..,pushes:..,notes:..,dropped:..,reads:..,msgs:..`.
- **A pad plays by PLAY** (command 53, in the Digitone's stage 5, 19:00-20:00 on 2026-11-24 at the earliest; 50 is its SCALE; `track, op, note, velocity`). Without live control, or on a Digitone that has no PLAY,
  **the pads play nothing and say why** (a message, at most every 2 s): REMOTE CONTROL OFF, NO LIVE CONTROL, DN2 HAS NO PLAY, DN2 NOT READY. A note ends by the
  same route, on the track it started on, whatever changed meanwhile. Right after live control comes up Jank asks once whether PLAY exists (an "all notes off", which a Digitone with PLAY takes silently and one without refuses), so the answer is known before the first press: measured 2026-11-24 the first press used to go out and light, was refused, and only the later ones were blocked. Lost requests are counted (`dropped:`) and never half sent.
- **Left / right choose the Digitone's active track** (TRACK, 49) and, for a second, the 16 bottom pads show which, as the Digitone's trig keys are laid out: tracks 1-8 in the upper of the two rows, 9-16 in the lower, the
  chosen one WHITE (3), the others LIGHT_BLUE_THIRD (39). When live control comes up the pads' current track is sent to the Digitone.
- **FOLLOW: left and right together** (a press of both makes no track step; the first press's step is put back). The Launchpad's track then follows the
  Digitone's as well (TRACK with no argument reads its active track, every 250 ms, accepted by dn2-wishes for stage 5; a Digitone without it: "DN2 CANNOT
  FOLLOW"), and left / right show **AMBER** (9, dim 11 at the first / last track). The Launchpad's own choice is not overridden for 500 ms (the Digitone takes a
  select at its next UI pass). The pair again: off.
- **Track pads (hold Note, pad 8 of the top row, GREEN when on):** the two bottom rows play the 16 tracks on each track's own default note (PLAY op 2, ended
  with op 0 and note 255, confirmed by dn2-wishes); the note layout moves up to the six rows left. The current track is BLUE (45), a track that sounds or is held
  GREEN (21), the rest LIGHT_BLUE_THIRD.
- **Lights come from the Digitone's own note push**: a note pad lights for the pitch the current track played, a track pad for its track, each for 150 ms (the
  pushed note's length is not mapped yet, so it is a fixed time, not measured).
- **Messages** (what Launchpad 95 writes in Live's status bar; at most 20 characters, upper case): every change on the scale page (ABSOLUTE ROOT ON, HORIZONTAL
  OFF, DIATONIC ONLY, LAYOUT CHORDS, TRACK PADS ON, KEY C#, A MINOR, SCALE DORIAN, OCTAVE 2, NO RELATIVE SCALE), up / down (OCTAVE 4), left / right (TRACK 3),
  FOLLOW ON / OFF, and why a press cannot play. **Jank's own screen shows them** as its one-second overlay box (`uis_message`, the ui task). **Without a screen
  they go to the Digitone**: a MESSAGE command (54) was asked of dn2-wishes (a full-screen popup of about a second) and is **not confirmed (stage 5)**, so
  nothing is sent yet and a unit without a screen shows no message. `diag` `msgs:` counts them.

### Modes: Note, melodic step, drum step (`core/jank_lpstep.[ch]`)

Owner, 2026-11-24. **Custom** (top button 7) enters the step modes and cycles **melodic <-> drum** (green / amber); **a tap on Note** goes back to Note mode (held it is the
scale page in every mode, as before; a quick press that touched a page pad is not a tap). One layout for both, after Launchpad 95's drum step sequencer (its drum rack
lower left, the sequence on top, the loop selector lower right):

- **Top 4x8 = 32 steps** as the Digitone shows them: steps 1-8 on the top row, 9-16 below (one page of 16), then the next page on the two rows under it. A press toggles
  the trig (GREEN on, DARK_GREY off, dark past the pattern's length). **The playhead** (owner 2026-11-24, like Launchpad 95's) runs over the step rows while the
  transport runs: **BLUE** on a step without a trig, **RED** on one that plays (its metronome and playing colours). Its position is Jank's own clock, not the Digitone's
  playing-step byte (wrong on firmware 1.12): the session's beat x 4 (sixteenth notes, sampled by the ui task, extrapolated between samples), modulo the pattern's
  length, and a pushed note (its step is exact) corrects the phase. Not measured on hardware: whether the Digitone's step and Jank's beat line up (they should when the
  Digitone follows Jank's clock from a Start on the bar). Without a running clock only the step that just played shows, WHITE for 150 ms. The pages come from the Digitone
  (STEPS, 46, every 400 ms for the two shown); a press is shown at once and the next read does not undo it for 300 ms; a page the Digitone has not answered is dark and
  does nothing.
- **Lower right 4x4:** the top row of it holds four page pads (the Digitone's 64 steps): the page the top rows start at.
- **Lower left 4x4, drum:** the 16 tracks in reading order (track 1 top left of the quadrant), coloured as Launchpad 95's drum rack (owner 2026-11-24): **YELLOW** (13) tracks,
  the track the steps show GREEN (21, its step sequencer's selected note), a pad that is down RED (5, its playing note). A press selects the track the steps show and edit. **Melodic:** a 4x4 keyboard
  (the Note mode's layout, scale and octave; up / down are the octave in melodic, nothing in drum): a press picks the note a step press places. With a key picked a step
  press sets that note (STEP op 2; the same note again clears it); with none, the trig with the track's own note.
- **The quadrant flashes what plays** (owner 2026-11-24; both modes, from the Digitone's note push, 150 ms, RED as Launchpad 95's playing note): a drum track pad when
  that track plays, a melodic keyboard key when the current track plays that pitch (every key of that pitch).
- **The quadrant also plays while the transport is stopped** (the track's own note / the key), and only selects while it runs (owner's rule). **Scene button 5 starts / stops
  the transport** (Launchpad 95's Note mode: its start / stop; green running, dim red stopped): Jank is the transport master, the ui task calls `bridge_play`.
- Reads and edits need live control; without it a step press says why (as a pad does). STEP's refusals (2) and full queue (5) are counted in `diag` (`hostdiag_steps`).
- Velocity, length, quantise, random etc. (Launchpad 95's other scene buttons: drum 3 quant, 5/6 up/down, 7 velocity, 8 mute; melodic 4 random, 5 length, 6 octave, 7
  velocity, 8 notes; Note mode 2 undo, 8 record; mixer 1-4 volume / pan / send A / send B) are **not built yet**.

**The clock to the Digitone (owner 2026-11-24):** in host mode Jank sends MIDI clock to the Digitone **only while it has peers on the Jank / Link network**
(`bridge_peers_heard`, checked each second by the ui task; `usb_midi_clock_peers` in usb.c). With none the clock stops, with one MIDI Stop after what was queued (as when the
USB MIDI CLOCK setting is turned off); with one it starts again. Device mode (a computer on the port) is not affected.

### Colour language so far (to be decided with the owner)

Launchpad 95's own colours stay as they are (scale page, Note mode). What this port adds: **green** a direction or switch that is available / on, dim green
at a limit or off; **amber** FOLLOW (left / right); **light blue** the Note mode button and idle tracks, **blue** the current track and the scale root,
**white** a track just chosen (for a second), **green** a note or track that sounds.

Not verified on hardware: all of it. TRACK and the push work with the installed stage-2 Digitone build; PLAY (so any playing), FOLLOW's read and MESSAGE need
the Digitone's stage 4 / 5.

## What is not there yet

The Digitone's own scale list (the scale page uses a starter list), Step, Mixer, Session, the song
row. Order decided with the owner: Note, Step sequencer, Mixer, then patterns and song mode last. The Digitone's current
track, scale and step data need new commands in dn2-wishes (its test channel only answers when asked); that research is
separate.

## Verified against Novation's Launchpad X Programmer's reference manual

- **One USB-MIDI interface, two cables** (measured 2026-11-24: `diag` `ifs:1`; a Mac lists the two ports "LPX DAW" and "LPX MIDI"). "DAW" and "MIDI" are
  the cable numbers in the high nibble of each USB-MIDI packet: 0 is DAW, 1 is MIDI. Programmer mode's lights and setup messages take effect on **cable 1**;
  the unit sent everything on cable 0 at first and the LEDs stayed dark (the Launchpad answered the Programmer message, reported Programmer mode and took
  every light message without an error). (An earlier version of this file said the Launchpad has two interfaces: wrong, only the code's clamp to the last
  interface kept it working.)
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
- The Digitone's cable number for its live-control SysEx (0 assumed; the lights no longer depend on its MIDI notes, they come from the push).
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
- A Launchpad or Digitone whose claim fails with ESP_ERR_NO_MEM (257) or NOT_SUPPORTED (262) is **tried again** every 2 s for two minutes (60 tries; on the
  same `diag` device line; the library keeps the device attached meanwhile). Measured 2026-11-24: the Launchpad's claim failed with 257 with 20 KB of
  heap free after it had worked on the same build, in the seconds when an update client was also asking the unit questions (the Wi-Fi service allocates a
  buffer at its first request). A passing memory shortage is the likely cause, not a proven one. After a Wi-Fi update of 2026-11-24 the claim failed for over 30 s (the first version tried for 9 s) and worked at once after a replug, which supports it.
- No log leaves the unit in host mode (no USB control line, console off): what the pads do is the only readout. Over Wi-Fi
  (`docs/wifi.md`) `diag` ends with the `hostdiag_` lines live.

## Files

`core/jank_lpx.[ch]` (the Launchpad's protocol), `core/jank_lpnote.[ch]` (layouts), `core/jank_lpapp.[ch]` (what a press asks for, as acts,
what the pads show), `core/jank_dn2live.[ch]` and `core/jank_dn2rpc.[ch]` (the Digitone's live control: the requests, the answers, the note push), `firmware/main/usb_lp.c` (the one state and the two devices), `firmware/main/usb_midi_host.c` (IN endpoints,
interface choice). Host tests: `tests/test_lpx.c`, `test_lpnote.c`, `test_lpapp.c`, `test_dn2live.c`, `test_dn2rpc.c` (in `tests/run.sh`).
