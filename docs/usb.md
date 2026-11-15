# USB

An S3 build is one USB device with three functions: a network adapter (CDC-NCM), a MIDI port called jank, and a small
drive (label JANK, FAT12, 512-byte sectors) holding `jank.html`, the control page, and taking firmware updates.

- **No serial port.** The S3 has 5 IN endpoints including EP0; NCM takes 2, MIDI 1 and the drive 1. The control line
  goes as SysEx on the MIDI port instead (`core/jank_sysex.h`: `F0 7D 'J' 'K'`, 7-bit text, `F7`, 30 characters a
  frame).
- **Clock to the computer.** The timer interrupt that writes the UART also queues the bytes for USB; a task hands them
  to TinyUSB, which cannot be called from an interrupt.
- **USB ID.** Espressif's VID with a made-up PID, `0x4A4B`: fine on one's own desk, not for distribution.

## Updating: copy jank.bin onto the drive, eject

No button, no download mode, no tool. The first install of this build still needs download mode once (it changes the
partition table and the bootloader; see [boards.md](boards.md), "Flashing"); after that:

1. Copy the app image (`jank.bin`, from `scripts/build.sh xiao`: `$JANK_SCRATCH/build-xiao/jank.bin`) onto the JANK drive.
   Any `*.bin` in the root will do; with several, the largest.
2. **Eject the drive** (Finder: eject; Windows: Eject; Linux: `udisksctl power-off`/`eject`). The ejecting is the signal
   (SCSI START STOP UNIT with LoEj): esp_tinyusb then takes the volume from the computer and mounts it for the app.
3. The stick reads the file, checks it (the image header, the ESP32-S3 chip id, a size that fits the slot, then
   `esp_ota_end`'s check of the segments and SHA-256), writes it to the *other* app slot, picks it for the next start and
   restarts. The drive comes back in a few seconds with `jank.html` and nothing else. If the file was refused, the reason is
   in `UPDATE.TXT` on the drive and the old firmware keeps running. Copying and ejecting is a few seconds' work on the
   stick's side: the image is 0.9 MB.

Why eject and not "watch the drive": the computer keeps the FAT and the directory in a cache and writes its copy back
whenever it likes. A volume changed behind its back loses the change or is corrupted, and a file seen half-copied is
not a file. So the stick never touches the volume while the computer has it: it wipes the drive and writes `jank.html`
at every start (before USB is up), and reads the update only after the eject, when the computer has flushed and let go.
Restoring the page continuously would need the stick to tell the computer the medium changed (a SCSI unit attention, which
macOS answers by dropping the volume and mounting it again): that works, but every restore then pulls the drive away from
under whatever the computer is doing with it, so it is not done. A `jank.html` the user deletes is back after the next
start. Unplugging the stick without ejecting also triggers a look for a `.bin` (esp_tinyusb mounts the volume for the app
on a USB disconnect), which is harmless: a file that is not whole fails the SHA-256 check.

The storage partition is 2 MB (an image up to about 1.9 MB fits; the app slots are 2 MB each), the FAT's label survives the
wipe. A volume that will not mount or wipe is erased once and formatted afresh at the next start (the label is then the
default).

### Rollback

A new image starts "pending verify". The old one comes back at the next reset, of any kind, unless the new one confirms
itself with `esp_ota_mark_app_valid_cancel_rollback()`. It does that (`main/ota.c`, `ota_validate_start`) when the computer
has configured the USB device (`tud_mounted`) and the control task has then gone round once (about a second): that is
the path the next update needs. A start as USB host does not confirm it: `ota.c` restarts into the old image after 120 s.
A crash, a hang that resets the watchdog or a power cut before that puts the old image back.

## Updating over USB MIDI (optional)

`scripts/ota.py` sends the image over the control line instead; no drive needed.

```
python scripts/ota.py [path/to/jank.bin]        # python-rtmidi; --check only looks at the image
```

It finds the MIDI port named exactly `jank`, reads `get`, sends the image, waits for the stick to restart and prints `get`
again. Protocol (core/jank_ota.h), one line each, answered with `ok ..` or `err <name> <offset>`:

```
ota begin <size> <crc32 hex>       -> ok 0 120           the image's size and zlib CRC-32; 120 = bytes per chunk
ota d <offset> <crc32 hex> <b64>   -> ok <next offset>   a chunk, its own CRC-32, base64 (7-bit safe, no control characters)
ota end                            -> ok                 size and CRC checked, esp_ota_end, new boot slot, restart
ota abort                          -> ok
```

One chunk at a time, each acknowledged; a lost or damaged chunk is sent again from the offset the stick names (`err gap`,
`err crc`); a chunk below the expected offset is an old one sent again and is not written twice. A transfer is dropped after
20 s of silence. Expected speed (a guess, not measured): 5 to 10 ms a chunk, 12 to 24 KB/s, 40 to 70 s for 0.9 MB. Base64
costs 25 % (8-to-7 packing would cost 12.5 %, but its bytes would include the line end).
Flash writes run in the control task; a sector erase stalls the CPU for tens of milliseconds, during which the clock's timer
interrupt (IRAM) goes on but the clock bytes reach USB late: do not update during a performance.
Beat-follow bench build only: the device is also a UAC2 speaker, an OUT endpoint, see [beat.md](beat.md).

## Host mode

The setting `usb` (`set usb device|host`, the menu's USB: DEVICE / HOST later; stored, default `device`) chooses.
**DEVICE**: always a device, as above. **HOST**: jank hosts self-powered USB gear:

- **MIDI**: the clock (the same ticks, starts on the bar line and stops as MIDI OUT) to every USB-MIDI device's first
  MIDI OUT port, up to 4, through a hub too (`main/usb_midi_host.c`).
- **Audio**: the first device with a 44.1 or 48 kHz PCM stream (UAC 1.0 or 2.0, 16 or 24 bits, mono to 8 channels) goes
  to the beat tracker, mixed to mono ((L + R) / 2) and resampled to 44.1 kHz (`main/usb_uac_host.c`, `core/jank_uacdesc.c`,
  `core/jank_pcm.c`). The tracker sees it as it sees the computer's stream: same ring, same `beat_latency`.
- `status` says `usb_role`, `usb_cc` (what the cable has: `device`, `host` for a computer or a charger, `none`;
  `nochip` on the XIAO), `usb_dev=<name>,midi+audio` for each device, `usb_audio=<rate>/<channels>/<bits>`.

Where the role comes from (`core/jank_usbrole.c`, host-tested):
- **XIAO** (no CC chip): with HOST it starts as a device; when no computer configured it within 3 s it restarts once as
  host (as before), so a clock on a charger starts about 3 s late. `set usb device` from a computer brings it back.
- **jank mini 2.2, battery order** (TUSB320LI, `main/usbport_tusb320.c`): a device on the cable -> the host stack and
  the 5 V signal (pump, then HOST_EN); a computer or a charger -> HOST_EN low at once and the device stack; nothing -> the
  host stack, waiting. Against another dual-role port (a USB-C laptop, tablet or power bank) jank takes the sink side
  (Try.SNK), so a computer always gets the control line. A different stack is a restart (about a second; the clock stops
  for it); a stack runs at least 3 s before the next one. The setting DEVICE keeps the port a sink: never 5 V, the charger
  always charges. The usb order (no TUSB320) behaves as the XIAO.
- The port chip sits behind a small HAL (`main/usbport.h`: attach state and orientation, set role, prepare and switch
  VBUS out): another chip (a power-bank IC with its own role logic) is another file, the role logic stays as it is.

`set source auto|line|usb` (AUDIO > SOURCE, stored, default `auto`): AUTO hears the line in when a plug is in, else USB
audio when a stream is open (the computer's in device mode, the hosted device's in host mode). The line in's own audio
path is not in this firmware yet: LINE hears nothing for now. Host mode has no network adapter, drive or control page.

jank does not switch power to the device. On a bare XIAO the 5V pin is the USB-C socket's VBUS with nothing between
them (v1.2 schematic), so 5 V on that pin powers the device, or a powered hub does. The XIAO presents 5.1 k on both CC
pins (a sink): USB-A gear behind an adapter does not care; USB-C gear plugged straight in may not power up.

**Never feed a bare XIAO's 5V pin with a computer on its USB port**: nothing separates the two supplies. The PCB adds an
ideal diode and a firmware cut for this (`hardware/pcb/README.md`, "The power path"); that cut is not in the firmware
yet.
