# Tooltips of the control page

Every control the user can change has a `title` (the face's D-pad also an aria-label). `tests/test_page.js` checks the generated page for a title, an aria-label or a titled ancestor on every button, select, input, textarea, summary and focusable element, and that no tooltip is empty or over 240 characters; `tests/test_connect.js` checks the run-time built ones (folder rows, breadcrumbs, D-pad). A label that holds its control carries the title for both. No exceptions, except the player card's `tabindex=-1` (a programmatic focus target, not a control).

Persistence statements come from `core/jank_core.h` (`JK_CFG_STORED`: what is above `beat` is stored; beat, follower, skip, graded are back at their defaults (graded: On) at every start) and `docs/firmware.md` (a changed setting is saved once stopped; unplugging while it is pending loses it). The page shows 'Saved on the jank.' after any `set`, also for the settings that are not stored; the tooltips say which are not.

| Control | Tooltip | Source of the claim |
|---|---|---|
| #c-midi | Connects to the jank over USB MIDI (SysEx). Later visits connect by itself once the browser remembers the permission; a file:// page always asks. Pressing it while connected disconnects and turns that off. | page code (connectMidi, autoConnect, permission rule in the browser) |
| #c-advanced-box | Shows the player and the advanced tuning and details on this page. Changes only this page (remembered in this browser); nothing is sent to the stick. | page code (paintAdvanced, localStorage jank.advanced) |
| #c-ports-box | Picks which jank to use when the browser sees more than one. Switching disconnects and connects to the one you pick. | page code (showJankChoice) |
| #pl-help | Shows or hides the help text of the player. Only this page. | page code |
| #pl-folder | Choose the music folder the player browses. Remembered in this browser; the files stay on this computer. | page code (IndexedDB handle); no network code in the page |
| #pl-play | Plays or pauses the file. It plays on this computer (monitor) and through the stick's USB audio, so the stick hears it too. | page code (stick path + monitor), the player's help text |
| #pl-next | Plays the next track, or a random one when 'then' is random. | page code (then) |
| #pl-dirpick | Choose the music folder with the browser's file picker. The files stay on this computer. | page code (file input fallback) |
| #pl-reopen | Opens the remembered folder again. The browser asks once for permission to read it. | page code (grantAndOpen) |
| #pl-scope | On: next, random and the end of a track use every audio file under the folder you picked. Off: only the folder you are looking at. Only this page. | page code (scope), the old title |
| #pl-seek | Moves the playing position in the file, on this computer and through the stick's audio. | page code |
| #pl-pitch-num | Pitch in percent (-8 to +8, varispeed): changes the speed and pitch you hear, so the tempo the stick tracks changes too. Not a stick setting. | page code (varispeed, -8..8), player help text |
| #pl-pitch | Pitch in percent (-8 to +8, varispeed): changes the speed and pitch you hear, so the tempo the stick tracks changes too. Not a stick setting. | as above |
| #pl-vol | Changes what you hear on this computer only. The stick gets the file at its true level. | the player's help text (the knob and monitor only change what you hear here) |
| #pl-change | Choose again which audio output is the stick's USB audio, in the browser's output chooser. Remembered in this browser. | page code, player help text (chosen once, remembered) |
| #pl-out | Which audio output is the stick's USB audio. None: the file plays on this computer only. | page code |
| #pl-pick | Opens the browser's output chooser for the stick's USB audio. If it asks for the microphone, that is only to read the device names; nothing is recorded. | ui/player.js OUTPUT_HELP |
| #pl-rescan | Looks through the picked folder again for audio files. Only this page. | page code (scope scan) |
| #bd-slider | Beat display delay in ms: delays (+) or advances (-) the beat boxes this page draws. Changes only this page, never the stick's settings. | docs/page-beat-sync.md; the page's note |
| #bd-estimate | Goes back to the page's own estimate of the delay (monitor - stick path - the stick's beat_latency, to 5 ms) and forgets the value you set. Only this page. | ui/stick.js beatDelayEstimate; docs/page-beat-sync.md |
| #al-on | On: while the player plays through the stick over USB, the page sets the stick's auto latency from the two output paths. Remembered in this browser; off sends nothing. | ui/autolat.js, page code (localStorage jank.autolat) |
| #al-resend | Sends the page's auto latency suggestion to the stick again now, ignoring the 3 ms and 5 s limits. Overwrites the stick's auto base; only while playing through the stick over USB. | page code (force bypasses the 3 ms guard) |
| #scan | Listens on the air for the jank and FM-1 groups and lists them. Joins nothing and changes no setting. | docs/firmware.md (scan joins nothing) |
| #net-apply | Sends the Join choice (jank mode, or the Wi-Fi name and password) to the stick and restarts it; give it a few seconds. Kept after the restart. | page code networkCommands (set net/ssid/pass + reboot); docs/firmware.md (saved before a restart) |
| #bootloader | Restarts the stick into the ROM's download mode so esptool can flash it over USB. Unplug and replug it afterwards to run again. | docs/firmware.md (bootloader), the page's own message |
| Beat follow | Whether the stick's beat tracker steers the Link session's tempo and beat. Changes the stick now; not kept after a restart (back to on). | core/jank_core.h (beat: after the stored line, not stored; default on) |
| Follower | Which beat follower the stick runs: tuned or robust (an A/B variant). Changes the stick now; not kept after a restart (back to tuned). | core/jank_ctl.c comment (not stored) |
| Skip | Skip mode of the robust follower: Off, ACT or PAT. Changes the stick now; not kept after a restart (back to Off). | core/jank_ctl.c comment (on the robust follower; not stored) |
| Graded confidence | On: the confidence shown on the jank moves with how well the beats fit (display only; the stop and start decisions keep using the gate's own value). Off: the gate's value is shown. Not kept after a restart (back to On). | core/jank_ctl.c comment (both followers, display only, not stored, on at every start) |
| Spectral rate | Lets a pitch move read from the spectrum steer the beat follower; Off uses the beats alone. Changes the stick; kept after a restart. | core/jank_core.h (rate: stored since M2i; 0.4 % CPU) |
| Audio shift, ms | Shifts when the stick puts its beat out, -600 to 600 ms, on top of the auto latency (+ is later). Kept per audio source on the stick. Fine-tune by ear. | docs/firmware.md AUDIO SHIFT (lat_trim, per source, stored); core/jank_ctl.c range |
| Auto latency, ms | The auto base of the audio source the stick hears now, -300 to 300 ms. The page's Auto latency overwrites it; Audio shift adds on top. Kept per source on the stick. | docs/firmware.md beat_latency per audio source (stored); core/jank_ctl.c range |
| Air filter | What goes out on the Wi-Fi air: Link only (ARP, IPv4 unicast, Link multicast) or Everything. Changes the stick; kept after a restart. | docs/air.md (the filter), core stored prefix |
| Link peer | Whether the stick is a Link peer itself in its session. Changes the stick; kept after a restart. | core/jank_core.h comment (jank is a Link peer itself); stored |
| Beats a bar | Beats in a bar (1 to 16) for the MIDI start, so a receiver starts on the session's downbeat. Changes the stick; kept after a restart. | docs/midi.md (quantum); stored |
| Clock offset, ms | Moves the MIDI clock against the Link session, -100 to 100 ms, for gear that lags. Changes the stick; kept after a restart. | docs/midi.md (offset, gear that lags); stored |
| Beat display delay, ms | Delays (+) or advances (-) the beat boxes this page draws, -400 to 400 ms. Changes only this page, never the stick's settings. | docs/page-beat-sync.md |
| Name | The stick's name, up to 23 printable characters. Changes the stick; kept after a restart. | core/jank_core.h (JK_NAME_MAX 23, stored) |
| Join | Jank mode or a Wi-Fi network. Nothing changes until Save and restart. | page code networkCommands |
| Wi-Fi name | The Wi-Fi network's name (up to 32 characters), used when Join is a Wi-Fi network. Sent with Save and restart. | page code (32 chars) |
| Password | 8 to 63 characters, sent over USB only; empty keeps the saved one. The stick keeps it in its flash, unencrypted, and never sends it back. | page code (8..63, empty keeps), the page's net note |
| then (select) | What plays when a track ends: the next file, a random one, or stop. Only this page. | page code (parseAfterMode) |
| monitor (checkbox) | On: you hear the file on this computer's speakers. Off mutes only your speakers; the stick still gets the audio. | the player's help text |
| details (player summary) | Shows or hides the technical details of the player. Only this page. | page code |
| Details (summary) | Shows or hides the stick's details: signal, Link, peers and firmware. Only this page. | page code |
| D-pad UP / DOWN / NO / YES (SVG title and aria-label, built at run time) | <KEY>: presses the stick's <KEY> button. The stick acts as on its own screen, so this can change its settings. Keys: Up / Down = UP / DOWN, Left / Esc / Backspace = NO, Right / Enter = YES | ui/stick.js header (the commands each button sends), KEY_HINT |
| the face (card) | Keys: Up / Down = UP / DOWN, Left / Esc / Backspace = NO, Right / Enter = YES (unchanged) | ui/stick.js KEY_HINT |
| breadcrumb buttons | Go back to this folder. | page code (goTo) |
| folder row '..' | Go up one folder. | page code |
| folder row | Open this folder. | page code |
| file row | Play this file; the player goes on as 'then' says. | page code (startFrom, queue) |
| show more | Shows more of the files and folders in this list. | page code |
| Join (group list) | Makes the stick join this group (set group). Saved on the stick. | page code; docs/firmware.md (group stored) |

## Uncertain, said with fewer words

- **Link peer**: only 'whether the stick is a Link peer itself' (the struct comment); what changes for a user is not spelled out in the docs.
- **Follower / Skip / Graded confidence**: bench A/B settings; the tooltips give only the values and that they are not kept after a restart.
- **Beat follow**: 'steers the Link session' is from the struct comment; the effect on a running session is not described beyond that.
- **Air filter**: 'Everything' is documented as a plain bridge, untested on hardware.
- **Clock offset / Beats a bar / Name / Air filter / Link peer** 'kept after a restart': from the stored part of `struct jk_cfg`; not verified on a real stick. A change made while playing is saved once stopped.
- **Pick output / Choose again**: the claim 'remembered in this browser' is from the player help text; the microphone only reads device names (ui/player.js).
- **Download mode** 'unplug and replug afterwards': from the page's own message after the restart; the stick may need it only if esptool does not reset it.
- **Folder / file rows**: static texts; the file name is the button's own text.
