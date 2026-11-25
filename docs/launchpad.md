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
- Pad lights, **the colours of Launchpad 95** (its `SkinMK2.py`, palette indexes from its `ColorsMK2.py`): the scale's root **49 (PURPLE_HALF; owner 2026-11-25, picked on the Session colour chart: "root note must be bright")**, its **3rd and 5th
  step LIGHT_BLUE (37, Note.Pads.Highlight: `scale[2]` and `scale[4]`, so for a pentatonic the 3rd and 5th step of that scale, not a fixed interval)**, the other
  scale notes LIGHT_BLUE_HALF (38), a note outside the scale **43 (owner's pick on the colour chart, 2026-11-25; before DARK_GREY 1; never white, the track pads' colour)**, a note the Digitone sounds (or a pad that is
  down) GREEN (21). **The colours are relative to the current scale, except for the Digitone's Chromatic (all 12 notes, where nothing would stand out):
  its reference is the MAJOR scale of the key** (root, 3rd and 5th as in major). Against major, a note that is in natural minor but not in major (the flat
  3rd, 6th and 7th) is **46** (BLUE_HALF, owner's pick on the colour chart, 2026-11-25; before: amber too loud, PURPLE_HALF 49 and PURPLE_THIRD 50 too bright, then 51), a note in neither 43; against any other scale there is no 46. One note lights every pad with that pitch (the layouts overlap). The arrows are GREEN (Note.Octave) and dim to GREEN_THIRD (23)
  where they can go no further (the lowest or highest octave, the first or last track); the Note button is LIGHT_BLUE (37, Mode.Note).
- A pad that is down when the track changes ends its note on the track it started on; a Launchpad that goes away ends
  every note it had down.
- **Scale page: hold Note.** While the Note button is held the pads are Launchpad 95's scale editor, rows from the top: (1) absolute
  root (pad 1), horizontal (pad 2, diatonic only), the layouts: chords (5), diatonic (6), chromatic (7); (2) the black keys, the
  relative scale (pad 3), the circle of fifths back (pad 7); (3) the white keys C to B, the circle of fifths forward (pad 8); (4) the
  scales' page pads; (5, 6) the scales; (7, 8) **the 16 tracks, a press selects one** (owner 2026-11-24; the octave row Launchpad 95 has here is gone, up / down do the octave):
  tracks 1-8 in row 7, 9-16 in row 8, as the Digitone's trig keys, the selected one AMBER, the others WHITE (RED while it sounds); it says TRACK n and the Digitone's active track follows.
  Colours as Launchpad 95's `Scale.*`, each bright when on and its "third" when off: the keys and Horizontal
  GREEN, AbsoluteRoot and the layouts RED, the scales BLUE, the relative scale and the circle of fifths plain BLUE. Let go of
  Note and the pads play again; a note that was down when the page opened still
  ends. **The scales are the Digitone's own 36** (owner 2026-11-24; `core/jank_lpscales.h`: its names, its order, its index; the intervals are stock OS 1.12's table as the
  Digitone session read it, not measured on the unit), **two rows of eight a page, three pages** (1-16, 17-32, 33-36); the row above them holds the **three page pads**, in AMBER
  (bright for the page shown), a different colour from the scales. The page of the scale in use is shown when the page opens and whenever the scale changes; a page pad
  only browses (it says SCALES 1-16 / 17-32 / 33-36). LP95's two guitar / not-staggered layouts, the drum rack and quick scale are not there. Host-tested; **not
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
  selected one **AMBER (9)**, the others **WHITE (3)**, a track that plays RED (5). Not blue (it reads as the scale root), not green or purple (the Digitone's own mute
  modes): owner 2026-11-24. When live control comes up the pads' current track is sent to the Digitone.
- **FOLLOW: left and right together** (a press of both makes no track step; the first press's step is put back). The Launchpad's track then follows the
  Digitone's as well (TRACK with no argument reads its active track, every 250 ms, accepted by dn2-wishes for stage 5; a Digitone without it: "DN2 CANNOT
  FOLLOW"), and left / right show **AMBER** (9, dim 11 at the first / last track). The Launchpad's own choice is not overridden for 500 ms (the Digitone takes a
  select at its next UI pass). The pair again: off.
- **The scale is synced with the Digitone, both ways** (owner 2026-11-24). *Reading:* Jank asks SCALE (50) every second with `first = 35, kind = 1` (the header plus one short
  name, about 20 bytes) and takes the Digitone's scale, and its root from protocol 5 on (stage 6), **when its reading changes** (and at the first reading after live control
  came up); a reading that has not changed leaves what the Launchpad chose alone, so a Digitone that cannot be written to (stage 5) does not undo the Launchpad's choice. The
  Launchpad then shows the page of that scale. No message: the Digitone's own screen says it. *Writing:* a scale, key, relative-scale or circle-of-fifths press on the scale
  page sends SCALE SET (55: `track 255 (the active one), scale 0..35, root 0..11`), applied at the Digitone's next UI pass; the Digitone writes it per track or per pattern
  as its own setting says. A build without SCALE SET answers status 2 once and no more are sent. **Formats are the Digitone session's planned ones for stage 6** (the SCALE
  reply has the root and key mode from `data[6]` when LIVE STATUS says protocol >= 5; confirmed by them); stage 6 is not installed yet, so only the reading of the scale index
  can work today. `diag`: `hostdiag_scale=dn2:<index>,root:<0..11 or -1>,reads:,sets:,read_ok:,set_ok:`. Not tried on hardware.
- **No Digitone, no lights** (owner 2026-11-25: "when DN2 is disconnected, LP should be black"): while no Digitone is attached every light of the Launchpad is
  off (`jlapp_set_dn2`, from the attach / detach).
- **Track pads (hold Note, pad 8 of the top row) cycle OFF -> TRIGGER -> SELECT** (owner 2026-11-25: "should cycle between: on/trigger/select. the pads are
  yellow (like drum sequencer yellow) when in trigger mode. and white in select mode"); the switch is Note mode's light blue (37) off, yellow in TRIGGER, white in SELECT, and says
  TRACK PADS OFF / TRIGGER / SELECT. With them on, the two bottom rows are the 16 tracks and the note layout moves up to the six rows left. **TRIGGER**: a pad
  plays its track on its own default note (PLAY op 2, ended with op 0 and note 255, confirmed by dn2-wishes), lit as the drum quadrant: YELLOW (13), the current
  track GREEN (21), a track that sounds or is held RED. **SELECT**: a pad selects its track (as the scale page's track rows; nothing sounds), lit WHITE, the
  current track AMBER, one that sounds RED. The setting is remembered (an older stored "on" is TRIGGER). (The drum step quadrant keeps Launchpad 95's drum rack colours, yellow / green / red, as asked: green there is the selected track.)
- **Lights come from the Digitone's own note push**: a note pad lights for the pitch the current track played, a track pad for its track, each for 150 ms (the
  pushed note's length is not mapped yet, so it is a fixed time, not measured).
- **Messages** (what Launchpad 95 writes in Live's status bar; at most 20 characters, upper case): every change on the scale page (ABSOLUTE ROOT ON, HORIZONTAL
  OFF, DIATONIC ONLY, LAYOUT CHORDS, TRACK PADS ON, KEY C#, A MINOR, SCALE DORIAN, OCTAVE 2, NO RELATIVE SCALE), up / down (OCTAVE 4), left / right (TRACK 3),
  FOLLOW ON / OFF, and why a press cannot play. **Jank's own screen shows them** as its one-second overlay box (`uis_message`, the ui task). **Without a screen
  they go to the Digitone**: live control MESSAGE (54, stage 5 of dn2-wishes: `u8 duration` in 100 ms units, 0 = 1 s, then 1..20 characters 0x20..0x7e; a box over the
  Digitone's page that changes nothing and never covers one of stock's dialogs; a newer one replaces it). Jank sends them with duration 0. A Digitone build without it
  (stage 4) answers status 2 once and nothing more is sent. A message made while live control is not up (the "why the pads do not play" ones) cannot be shown and is dropped.
  `diag`: `msgs:` made, `msgs_sent:` sent to the Digitone, `msg_ok:` not refused. Needs stage 5 installed on the Digitone; **not tried on hardware**.

### Modes: Note, melodic step, drum step (`core/jank_lpstep.[ch]`)

Owner, 2026-11-24. **Custom** (top button 7) enters the step modes and cycles **melodic <-> drum** (green / amber); **a tap on Note** goes back to Note mode (held it is the
scale page in every mode, as before; a quick press that touched a page pad is not a tap). One layout for both, after Launchpad 95's drum step sequencer (its drum rack
lower left, the sequence on top, the loop selector lower right):

- **Top 4x8 = 32 steps** as the Digitone shows them: steps 1-8 on the top row, 9-16 below (one page of 16), then the next page on the two rows under it. A press toggles
  the trig (GREEN on, DARK_GREY off, dark past the pattern's length). **The playhead** (owner 2026-11-24, like Launchpad 95's) runs over the step rows while the
  transport runs: **BLUE** on a step without a trig, **RED** on one that plays (its metronome and playing colours). Its position is Jank's own clock, not the Digitone's
  playing-step byte (wrong on firmware 1.12): the session's beat x 4 (sixteenth notes, sampled by the ui task, extrapolated between samples), modulo the pattern's
  length, and a pushed note (its step is exact) corrects the phase. **Measured 2026-11-24: that was not enough**: with the sequencer started by the Digitone's own play key,
  Jank's transport is stopped and no playhead showed. So the playhead **follows the Digitone's own MIDI transport** while its clock ticks (Start / Continue / Stop and 6
  clock ticks a sixteenth, read-only, no channel involved: `jlst_on_rt`), whoever started it; Jank's session beat is the fallback when the Digitone's
  clock is silent (500 ms). Not measured on hardware: whether the steps line up (the pushed notes correct the phase). Without any clock only the step that just played
  shows, WHITE for 150 ms. The pages come from the Digitone
  (STEPS, 46, every 400 ms for the two shown); a press is shown at once and the next read does not undo it for 300 ms; a page the Digitone has not answered is dark and
  does nothing.
- **Lower right 4x4:** the top row of it holds four page pads (the Digitone's 64 steps): the page the top rows start at.
- **Lower left 4x4, drum:** the 16 tracks in reading order (track 1 top left of the quadrant), coloured as Launchpad 95's drum rack (owner 2026-11-24): **YELLOW** (13) tracks,
  the track the steps show GREEN (21, its step sequencer's selected note), a pad that is down RED (5, its playing note). A press selects the track the steps show and edit. **Melodic:** a 4x4 keyboard
  of **16 different notes** (owner 2026-11-24: Note mode's layout repeats pitches between rows, this one never does): the scale's steps one after the other from the root at
  the octave, bottom left first, a row up = 4 steps (C major, octave 3: 48 50 52 53 / 55 57 59 60 / 62 64 65 67 / 69 71 72 74; the chromatic layout: semitones). Scale, key,
  absolute / relative and octave are Note mode's; up / down are the octave in melodic, nothing in drum. A press picks the note a step press places. With a key picked a step
  press sets that note (STEP op 2; the same note again clears it); with none, the trig with the track's own note.
- **The quadrant flashes what plays** (owner 2026-11-24; both modes, from the Digitone's note push, 150 ms, RED as Launchpad 95's playing note): a drum track pad when
  that track plays, a melodic keyboard key when the current track plays that pitch (every key of that pitch).
- **The quadrant also plays while the transport is stopped** (the track's own note / the key), and only selects while it runs (owner's rule). **Scene button 5 starts / stops
  the transport** (Launchpad 95's Note mode: its start / stop; green running, green third when stopped, owner 2026-11-25): Jank is the transport master, the ui task calls `bridge_play`. **Measured 2026-11-24 (owner: "start stop on note mode is not working"): the unit had `peers=0`, and the rule "the clock goes to the Digitone only
  with peers on the Jank / Link network" (`usb_midi_clock_peers`) dropped every clock and Start byte, so the button did nothing.** The button now overrides that rule while the Launchpad has
  the transport started (`usb_midi_clock_force`, ui.c): the clock goes out until it stops it (or the transport stops some other way). Not tried on the unit after the fix (the update was rolled back, see the state notes).
  **Since the Digitone's stage 8 the transport rule below decides who scene 5 starts** (owner 2026-11-25); the paragraph above is what happens with Link leading, or with a
  Digitone that has no TRANSPORT.

### Who starts and stops: the transport rule (`core/jank_lptrans.[ch]`)

Owner, 2026-11-25: "if there are no link peers, then DN2 is master. but if there are then link is", and "play and rec button state should be synced bi-directionally".
"Peers" is the count that already lets the clock out to the Digitone (`bridge_peers_heard`: Link peers on a Wi-Fi network, the Jank radio's peers otherwise; the ui task
hands it to `usb_lp.c` every second).

- **No peers: the Digitone leads.** Scene 5 sends **TRANSPORT** (61, the Digitone's stage 8, **provisional** until its lp8 build passes: `u8 op` 0 STOP, 1 PLAY (start, or
  continue when paused; nothing when playing), 2 PAUSE, 255 read -> `u8 state` 0 stopped, 1 playing, 2 paused; it runs the Digitone's own key code): PLAY when the Digitone
  is stopped or paused, STOP when it plays. Jank's own session is not touched and **no clock and no Start byte go to the Digitone** (nothing asks the ui task for a start, so
  nothing forces the clock on; the peers rule keeps it off, which matches what was measured on the unit: with no peers, no clock reached the Digitone). The light follows
  **the Digitone's transport**: from the LIVE STATUS of protocol 7, or, before protocol 7, a TRANSPORT read every 500 ms; a press shows at once and an older status does not
  put it back for 500 ms. A clock Jank had forced on earlier (a start from before) ends at the next peers check.
- **Peers there: Link leads.** Scene 5 starts / stops Jank's session as before, and the clock (Start and 0xF8 at Link's tempo) goes to the Digitone (`CLOCK_OUT` in usb.c:
  the setting on and peers there; read from the code, not measured with peers). **The Digitone's own PLAY / STOP keys** (seen as a change of its transport in the
  status) **start / stop Jank's session too** (the ui task calls `bridge_play`), so all stay together. **The guard against a loop:** a change of the Digitone's transport
  counts only when it differs from what Jank's session does, and not within 1 s of a change Jank made itself (its Start reaches the Digitone, whose transport then changes:
  that must not come back as a request). Not measured: what a Digitone with CLOCK RECEIVE on does with its own PLAY key while it follows a clock.
- **Who leads is only decided after TRANSPORT has answered**: Jank reads it once when live control comes up. Until then, and on a Digitone that refuses it (status 2:
  nothing said (the probe runs at each live-control start; owner's screen); no more TRANSPORT requests until live control is asked for afresh), scene 5 is Jank's session with the forced clock, as before stage 8.
- **LIVE STATUS protocol 7** (provisional): the 73 bytes of protocol 6 unchanged, then `u8 transport, u8 playing pattern, u8 next (255 = the same), u8 mode, u16 chain count,
  u16 chain position (0xFFFF cued), u32 writes applied, u32 writes refused`. Read when the protocol says 7 and the answer is long enough (the two counters only when they are
  there); protocols 5 and 6 read as before, and Record still follows byte 72. Once the Digitone says protocol 7, Jank asks LIVE STATUS every 250 ms instead of every 4 s (it
  carries the transport and the patterns; what that costs the Digitone is not measured).
- `diag`: `hostdiag_tr=sent:<TRANSPORT sent>,ok:<not refused>,dn2:<0 stopped, 1 playing, 2 paused, -1 none read>,peers:<1: Link leads>,follows:<starts / stops the Digitone's keys
  asked of Jank's session>,chains:<CHAIN sent>,chain_ok:<not refused>`. Host-tested (`tests/test_lptrans.c`); not tried on hardware.
- **Delete = scene button 6, held, in Note and step modes** (owner 2026-11-24: "hold Delete wipes the current track under the playhead"; PINK_THIRD dim, PINK while held; in the mixer scene 6 is a view): the Digitone
  **erases the track's steps as its own playhead passes them** (LIVE ERASE, 44 sub 3 with a u16 track mask, there since its lp2, dn2-wishes: it deletes each step just before it plays, never sounds it, so it is the Digitone's playhead and not Jank's
  extrapolation; it ends on LIVE OFF, the 10 s lease, REMOTE CONTROL OFF or USB down). The track is the current one when the button went down; letting go (or the Launchpad going away) sends mask 0. **In the step modes (melodic and drum) a step pad pressed while Delete is held clears that step** (owner: "delete should clear pressed steps in step seq";
  STEP clear, whatever the trig held; an empty step stays empty and no trig is ever set while Delete is held). Known limit (dn2-wishes): the first step after PLAY
  is played from the record made at STOP, so it is erased on the next pass. **Record = scene button 8** (red, owner; Note and step modes): the Digitone's **own live record**, `58 RECORD {u8 on: 0 / 1 / 255 read} -> {on}` (dn2-wishes stage 7, format from their measurements 2026-11-24, **their firmware is not built or installed yet**: lp7). On = what [RECORD]+[PLAY] does on the Digitone but it
  does not start the sequencer: it arms, and the Digitone records while its sequencer plays; PLAY's notes are recorded as its keys' are (its quantise setting, PLAY's velocity, on the track PLAYED, so the drum track pads and the melodic keyboard record in the step modes too). Off = what [PLAY] does during live record. The light: RED when armed or recording,
  RED_THIRD when not, **dark on a Digitone without RECORD** (it answers 2: the button says `RECORD: DN2 TOO OLD`). The light follows the Digitone's own state (LIVE STATUS protocol 6, 73 bytes, byte 72: bit 0 armed, 1 recording, 2 quantised, 3 GRID, 4 STEP; read with the 4 s keep-alive; a press holds the light for 1 s so an older status does not put it back);
  status 6 (GRID or STEP RECORDING is on) says `GRID/STEP REC IS ON` and puts the light off. Host-tested only; not tried on a Digitone.
- **Session button (top, 5th): a short press is the Session page (below); held for a second it is the colour chart** (owner 2026-11-24, to pick colours; held since
  2026-11-25): the Launchpad X's 128 palette indexes on the 64 pads, reading order, index 0 top left. Holding Session opens it on 0-63 (it opens at the second, while the
  button is still down); in the chart Session shows 64-127, then goes back to the mode it came from (the Session page too); a pad says its index (`COLOUR 51`) on Jank's
  screen or the Digitone's. Session is lit: white on the Session page or in the chart, dim grey otherwise. Note, Custom and Capture leave both as always.
- Reads and edits need live control; without it a step press says why (as a pad does). STEP's refusals (2) and full queue (5) are counted in `diag` (`hostdiag_steps`).
- Velocity, length, quantise, random etc. (Launchpad 95's other scene buttons: drum 3 quant, 5/6 up/down, 7 velocity, 8 mute; melodic 4 random, 5 length, 6 octave, 7
  velocity, 8 notes; Note mode 2 undo, 8 record; mixer 1-4 volume / pan / send A / send B) are **not built yet**.

**The clock to the Digitone (owner 2026-11-24):** in host mode Jank sends MIDI clock to the Digitone **only while it has peers on the Jank / Link network**
(`bridge_peers_heard`, checked each second by the ui task; `usb_midi_clock_peers` in usb.c). With none the clock stops, with one MIDI Stop after what was queued (as when the
USB MIDI CLOCK setting is turned off); with one it starts again. Device mode (a computer on the port) is not affected.

### Remembered settings (`core/jank_lpprefs.[ch]`)

Owner, 2026-11-24: "jank should remember the settings, like enabling the 16 bottom pads". Kept across a restart (an update restarts the unit): the note **layout** and its
**horizontal / absolute** switches, the **octave**, the **track pads** (the 16 bottom pads), the **step mode Custom opens** (melodic / drum), the **mixer's view** (stored, but Capture always opens the mixer on its overview, so it is not restored) and
**FOLLOW**. **Not** kept: the scale and the key (while the Digitone is live they come from it, so a stored pair would only be stale), the mode (it starts in Note mode), the
mixer's bank, the step page and the track. One 10-byte blob (`jlp` / `p` in NVS: version, 8 settings, a checksum); a blob that is short, of another version, damaged or with a
value out of range is ignored and the defaults stay. It is written **400 ms after the last change and never while the transport plays** (the rule of `core/jank_prefs.h`: no
flash writes then, so it waits until it stops); a change that returns to what is stored writes nothing. `wipe settings` clears it. Not tried on hardware.

### Mixer (`core/jank_lpmix.[ch]`)

Owner, 2026-11-24, after Launchpad 95's mixer (read from its source on the owner's Mac, `SubSelectorComponent.py`, `PreciseButtonSliderElement.py`). **Capture** (top button 8,
MINT: dim when it is not on, full when it is) opens the mixer **on its overview**, and Capture again (from a sub mode) comes back to it; a tap on Note or Custom leaves it. Each of
the 8 pad columns is **one track**; **up / down are the banks**: up the upper row, tracks 1-8, down tracks 9-16, as the Digitone's trig keys lie (it says TRACKS 1-8 / 9-16).

**The overview** (owner: "the mixer page is a mixer overview, and then the scene buttons on the right switch to the sub modes"): a pad row per thing, from the top, in the scene
buttons' order (Launchpad 95: volume, pan, send A, send B, stop, mute, solo, arm):

| row (from the top) | shows | a press |
|---|---|---|
| 1 - 5 | volume, pan, delay, reverb, chorus, each in its fader's colour (below) | **opens that sub mode** (the same as its scene button; the owner chose this over Launchpad 95's reset-to-default, which needs the Digitone's defaults) |
| 6 | mute: bright GREEN, dim when the track is muted (owner 2026-11-25: Elektron's mute colour; was Launchpad 95's yellow) | toggles that track's mute (the whole new mask is written) |
| 7 | solo: BLUE when soloed, BLUE_THIRD when not | toggles that track's solo |
| 8 | dark | nothing |

**Colours** (owner 2026-11-25, picked on the Session colour chart; Launchpad 95 has MINT for all three): **volume 25** (row 4 col 2), **pan 17** (row 3 col 2), **the
three sends 50** (purple, row 7 col 3; first 60, row 8 col 5, which showed red on its scene button); the view buttons (scene 1-5) are **in their fader's colour: full (25 / 17, the sends 49) while their view is shown, the third (27 / 19, the sends 51) otherwise; the send faders 50 (owner's chart picks)** (owner
2026-11-25, after a first all-yellow version: "the scene button color must match their relative faders") (owner 2026-11-25: "scene change
color when i select them in mixer. the should be some kind of yellow", then "each color has full, half, third brightness level ... please use that": the
palette's groups of four are light, full, half, third, e.g. RED 4 5 6 7; one hue, its brightness gives the state). Capture
keeps Launchpad 95's MINT (29, MINT_THIRD 31 when off).

**The scene buttons** (right side) switch to the sub modes, from the overview or from each other (the one shown its fader's colour, the others that colour's third):

| scene | sub mode | shows | a press |
|---|---|---|---|
| 1 | volume | an 8-pad fader (its colour, the empty part dark): nothing lit for 0, else the pads up to the first step the value does not pass | writes that pad's step: **Launchpad 95's volume steps** 0, .14, .31, .40, .55, .70, .85, 1.0 of the range (its -42 -24 -18 -12 -6 0 +6 dB) = 0, 18, 39, 51, 70, 89, 108, 127; the lowest pad sets 0 and lights nothing, as there |
| 2 | pan | a bar from the two middle pads out to the value (the middle pair alone for 63 / 64) | writes the pad's step: 0, 23, 43, 64, 64, 84, 104, 127 (Launchpad 95's pan map) |
| 3, 4, 5 | delay, reverb, chorus send | as volume | **Launchpad 95's send steps** 0, .10, .16, .24, .34, .55, .77, 1.0 = 0, 13, 21, 30, 44, 70, 98, 127 |
| 6, 7 | **mute and solo: one screen, both buttons** (owner 2026-11-24: the whole-column version was useless; "on / off buttons for 16 tracks, both solo and mute, 4 rows") | the **lower four pad rows**, from the top: **mute 1-8, mute 9-16, solo 1-8, solo 9-16** (tracks 1-8 above 9-16, as the Digitone's trig keys); mute bright GREEN / dim when muted (Elektron's colour, owner 2026-11-25), solo BLUE when soloed / BLUE_THIRD when not; the top four rows dark; the bank does not matter | toggles that track's mute or solo (the whole new mask is written) |
| 8 | (none yet; LEVEL could go here) | | |

(The steps are Launchpad 95's fractions of its range laid on the Digitone's 0..127; the Digitone's own volume and send curves are **not measured**.)

**Slow faders** (owner 2026-11-25: "fader mode (hold long to make fader move slower) is not implemented like on lp95"). In Launchpad 95 this is its device mode's
(`DeviceControllerStrip.py` with `ENABLE_TDC`, `ParameterSlides.py`; its mixer faders jump); here it is on every fader view (volume, pan, sends). A fader pad pressed is
only held, and steps through ten colours, one every 0.2 s: RED, AMBER, YELLOW, GREEN, MINT, LIGHT_BLUE, BLUE, PINK, PURPLE, WHITE (`Device.ColorSteps`). Let go, the fader
travels to that pad's step: the time held picks how long a sweep of the whole 0..127 takes, **0, 0.75, 1.5, 3, 5, 8, 12, 17, 25, 40 s** (`TDC_MAP`, 0 to 2 s held in ten
steps), so a tap (under 0.22 s) lands at once and a 2-second hold takes 40 s end to end. The slide writes MIXER SET as it moves, every 100 ms (`jlapp_pump_acts`), its first
50 ms at the release; it keeps going when the view, the bank or the mode changes; a new release on the same track and fader replaces it; and it **stops when the Digitone's
value leaves the path it has travelled** (its own knob moved it), as Launchpad 95's slides stop when something else moves the parameter. Up to 16 slides at once. A pad held
when the screen changes does nothing at its release. Without live control or the mixer writes the slides end. Host-tested; needs stage 6 to be heard.

The faders **move when the Digitone's values do** (MIXER, 45, read every 300 ms while the screen is on: automation, an encoder), and a tap moves its fader at once; the
Digitone's reading is not allowed to put it back for 400 ms (its write applies at its next UI pass, up to 33 ms), nor while a slide runs on the path it travelled. **Writes are the Digitone's stage 6** (MIXER SET 56 and MUTE SET
57, formats confirmed by its session); on a build without them a press says MIXER READ ONLY (at most every 2 s) and the faders are display only. Scene button 5 is chorus here, so
**start / stop is not available inside the mixer**. A dual Launchpad mode is planned (one on tracks 1-8, the other on 9-16). Not tried on hardware. `diag`: `mixreads:`, `mixsets:`.

### Session page: the patterns (`core/jank_lpsess.[ch]`)

Owner, 2026-11-25. **A short press on Session** (let go within a second) opens the page; another short press goes back to the mode it came from (it says SESSION BANKS
A-D / SESSION OFF). Held for a second, Session is the colour chart instead (above).

**What it shows.** The Digitone's 128 patterns are 8 banks, A to H, of 16. Each bank takes two pad rows, four banks a page: from the top A01-A08, A09-A16, then B, C and
D the same way. **Up / down** switch to banks E-H and back (it says BANKS A-D / BANKS E-H); the arrows are green where they can go, dim green at the limit, as in the
mixer. The lights are Launchpad 95's session skin (`SkinMK2.py`, Session): **the playing pattern GREEN (21), pulsing**; **a cued pattern GREEN, flashing** (with dark);
**a pattern of the chain GREEN_THIRD (23)**; **a pattern that holds trigs RED_THIRD (7)**; an empty pattern dark. A bank that has not been read yet is dark too.

**Where it comes from.** PATTERNS (51, the Digitone's stage 4, there today): `u8 bank (0..7, 255 = the playing pattern's bank)` -> `u8 bank, u8 playing pattern, u16 mask`
(bit k: pattern 16 x bank + k holds a trig). It costs the Digitone up to about 0.8 ms per bank in its MIDI task, so Jank asks sparingly: **when the page opens, every bank
once** (one request at least 60 ms after the other, so the page fills within about a second), **then only the playing pattern's bank (255) every 500 ms** while the page is shown, and
**nothing while it is closed**. A bank whose answer never came is asked again with the next 500 ms read, one at a time. When live control lapses the page goes dark; when it
comes back the banks are read again. Other banks' trigs are only read when the page opens (a pattern filled on the Digitone meanwhile shows the next time). **With the LIVE
STATUS of protocol 7** the playing and next patterns come with the status (every 250 ms): no PATTERN read, and the playing bank is read only every 2 s (for its trigs).

**A press cues that pattern**: PATTERN `60 {u8 pattern 0..127, u8 how: 0 cue (switch at the playing pattern's end), 1 jump, 255 read}` -> `{u8 playing, u8 next (255 =
the same), u8 mode, u8 running, u8 jump armed}` (stage 8's format), with how = 0 (it says CUE B10). **PATTERN 60 is provisional**: the Digitone's stage 8, final only after
its lp8 build passes; Jank sends it in that format, as RECORD was built before the Digitone had it. Never a MIDI Program Change (owner: "sysex. we should not rely on this settings"). The pad flashes at
once, and stops when the pattern plays (the 500 ms read or the status says so) or when the Digitone names another next pattern, or none (a "none" within 0.5 s of the
press is taken as older than it). While the page is shown Jank also reads PATTERN (how 255)
with each 500 ms read, so a cue made on the Digitone itself flashes too; that read also goes once when the page opens, so a Digitone without PATTERN is found out before
the first press. **A Digitone without it** answers status 2: the page says **PATTERN: DN2 TOO OLD** (and again at each press), its pads send nothing, and no more PATTERN
requests go until live control is asked for afresh (the Digitone or the Launchpad plugged in again); the reading of the banks goes on. Without live control a press says why,
as a note pad does.

**Chains** (CHAIN, 59, stage 8, provisional: `u8 op` 0 SET `u8 n (2..64), n x pattern` (stock's PTN + held TRIG chain; it loops), 1 CLEAR, 255 READ `u8 first` -> `u8 mode,
u16 count, u16 position, u8 first, u8 k, k x pattern`). **Hold a pad and press others**: the held pad is cued at once and is the chain's first pattern, the others follow
in the order pressed (it says CHAIN 2: B03, ...; up / down may switch pages meanwhile; at most 64, then CHAIN FULL (64)); **letting go of the held pad sends the chain**
(CHAIN SET, it says CHAIN OF 3). The chain's patterns are GREEN_THIRD while it is built and after. **To clear the chain: hold the playing pattern's pad alone for a second**
and let go (CHAIN CLEAR, it says CHAIN CLEARED); any other pad held alone only cues. The chain is read (CHAIN READ, in parts when one answer does not hold it) when the page
opens, after a SET or a CLEAR, and when the status's chain count changes. A Digitone without CHAIN answers 2: nothing said for the probe, CHAIN: DN2 TOO OLD (at the release of a chain that
cannot go); a full queue (status 5) loses that request only (counted). A MIDI Program Change cancels a chain on the Digitone. A CHAIN SET of 64 is the longest request Jank
sends (86 bytes, 29 USB-MIDI packets): it goes out one packet at a time into the Digitone's FIFO, with no buffer for the packets.

**The scene buttons** on this page are Note mode's: 5 start / stop, 6 Delete (held), 8 Record, the others dark. The page has no use for them, and they stay where the hand
knows them; the mixer is the one page that takes them over (for its views). A note pad that was down when the page opened still ends.

`diag`: `hostdiag_pat=reads:<PATTERNS sent>,cues:<PATTERN sent>,ok:<reads not refused>/<PATTERN not refused>,playing:<0..127, -1 none read>`; the chain's in `hostdiag_tr`. Host-tested
(`tests/test_lpsess.c`); **not tried on the Launchpad or the Digitone**.

### Colour language so far (to be decided with the owner)

**A button that does something is always lit: dim when its mode is not on, full brightness when it is** (owner 2026-11-24). Note (light blue, dim in a step mode), Custom (the
step mode it leads to: deep blue (45) melodic / yellow (13) drum, owner 2026-11-25, dim in Note mode), the arrows (dim at a limit), start / stop (green running, green third stopped). A button with no job is dark
(up / down in drum step).

Launchpad 95's own colours stay as they are (scale page, Note mode). What this port adds: **green** a direction or switch that is available / on, dim green
at a limit or off; **amber** FOLLOW (left / right); **light blue** the Note mode button and idle tracks, **blue** the current track, **purple (49)** the scale root,
**white** a track just chosen (for a second), **green** a note or track that sounds.

Not verified on hardware: all of it. TRACK and the push work with the installed stage-2 Digitone build; PLAY (so any playing), FOLLOW's read and MESSAGE need
the Digitone's stage 4 / 5.

## What is not there yet

The song row. The Digitone's stage 8 (TRANSPORT 61, PATTERN 60, CHAIN 59, LIVE STATUS protocol 7) is built against provisional formats: final only after its lp8 build passes. (The scale sync and the mixer are built; their writes, and the scale's root, wait for the Digitone's stage 6, which is not installed yet.) A **dual Launchpad mode** is planned for later (owner 2026-11-24): in the mixer one Launchpad is tracks 1-8 and the other 9-16
(the mixer's bank is already a parameter of its screen). Order decided with the owner: Note, Step sequencer, Mixer, then patterns and song mode last. The Digitone's current
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
- **An IN transfer that keeps failing is never given up** (2026-11-25, the Digitone session's report of a Digitone that stopped answering while still
  listed): after 3 retries at once it is tried again after 200 ms, doubling up to 2 s, for as long as the device is attached (`usb_midi_host.c`
  in_retry_due). The diag's device lines now count them: `in_err:` the errors, `in_status:` the last status (they were never filled before).
- **The restart after an update** (owner 2026-11-25: "it should recover dn2 and lp automatically"). That day every Wi-Fi update with both plugged in ended with the
  Launchpad's claim failing (257 at the interface claim, `where:1`) until a replug. Tried on the unit and dropped: cycling the root port (the Launchpad did not
  even enumerate again), and keeping the port off until the update was confirmed and Wi-Fi quiet (28 s: the same failure). Every restart of another kind
  (power-on, the role switch a replug makes) claimed both at once. Measured at the failed claim (`diag` `hostdiag_mem=...,fail_dma:<free>/<largest>/<aligned>`):
  9675 bytes of internal DMA heap free, its largest block 2944, one 512-aligned 512-byte block available, while a claim books two descriptor lists aligned to 512
  per endpoint: the update's start leaves the heap cut up (what in that start does it is not found). So **in host mode, 5 s after a new image is confirmed, the
  unit restarts once more** (`ota.c` restart_after_confirm, `last_restart=post-update`), as host at once; the image is no longer on probation.
- No log leaves the unit in host mode (no USB control line, console off): what the pads do is the only readout. Over Wi-Fi
  (`docs/wifi.md`) `diag` ends with the `hostdiag_` lines live.

## Files

`core/jank_lpx.[ch]` (the Launchpad's protocol), `core/jank_lpnote.[ch]` (layouts), `core/jank_lpapp.[ch]` (what a press asks for, as acts,
what the pads show), `core/jank_lpsess.[ch]` (the Session page), `core/jank_lpsessapp.c` (its buttons and pads in the app), `core/jank_lptrans.[ch]` (who starts and stops), `core/jank_dn2live.[ch]` and `core/jank_dn2rpc.[ch]` (the Digitone's live control: the requests, the answers, the note push), `firmware/main/usb_lp.c` (the one state and the two devices), `firmware/main/usb_midi_host.c` (IN endpoints,
interface choice). Host tests: `tests/test_lpx.c`, `test_lpnote.c`, `test_lpapp.c`, `test_lpsess.c`, `test_lptrans.c`, `test_dn2live.c`, `test_dn2rpc.c` (in `tests/run.sh`).
