JANK
====

This drive is the jank stick. It is made up by the stick each time it starts:
nothing on it is stored, and what you add (other than a firmware image) is
gone at the next restart. Files here:

  README.txt   this text
  jank.html    the control page
  UPDATE.TXT   what happened at the last firmware update (after one)

CONTROL PAGE
------------
Open jank.html in Chrome or Edge, press Connect and pick the MIDI port named
"jank". Tempo, source, network, offsets and firmware are on that page. A phone
can use the same page over Bluetooth (docs/ble.md in the repository).

UPDATE THE FIRMWARE
-------------------
1. Build the image in the repository: "make build" (or "make flash", which
   builds and installs in one go). The image is jank.bin.
2. Copy jank.bin onto this drive. No eject, no button, no tool.
3. The stick takes the file as it lands, checks it, and restarts by itself
   after the computer has been quiet for 1.5 s. The drive comes back with a
   new UPDATE.TXT saying what happened. macOS may say the drive was not
   ejected: that is normal here.
4. Stay connected to a computer for about 20 s after the update: an image
   that never talks to a computer is rolled back to the old one.

No drive, or the copy does nothing: update over USB MIDI instead,
  python3 scripts/ota.py jank.bin
(needs python-rtmidi; stop playing first, the stick answers "err playing"
otherwise; close other programs that use the stick's MIDI port).

A wrong or damaged image is refused and the old firmware keeps running.
The first install of an update-capable build needs download mode once (it
changes the partition table): see docs/boards.md, "Flashing".

THE BUTTON
----------
  click         resync: stop now, start on the next bar line
  double click  go to the next group heard on the air
  hold 1.5 s    switch MODE JANK <-> the Wi-Fi network (restarts)
  hold 10 s     download mode, for flashing

THE LED
-------
Flashes on the beat, longer on the first beat of the bar. On the Wi-Fi
network the pattern is inverted.

HELP AND SOURCE
---------------
  https://github.com/w0ts/jank
  docs/usb.md       this drive, updates, rollback
  docs/firmware.md  settings, build, flashing
  docs/beat.md      the beat follower
Report a problem there, with the UPDATE.TXT text if it is about an update.
