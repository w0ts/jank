# USB

An S3 build is one USB device with three functions: a network adapter (CDC-NCM), a MIDI port called jank, and a drive
(label JANK) holding `jank.html`, the control page, and taking firmware updates. The drive is made up by the firmware: a
FAT16 volume of 4 MB that is stored nowhere (`core/jank_vdrive.h`, `main/drive.c`).

- **No serial port.** The S3 has 5 IN endpoints including EP0; NCM takes 2, MIDI 1 and the drive 1. The control line
  goes as SysEx on the MIDI port instead (`core/jank_sysex.h`: `F0 7D 'J' 'K'`, 7-bit text, `F7`, 30 characters a
  frame).
- **Clock to the computer.** The timer interrupt that writes the UART also queues the bytes for USB; a task hands them
  to TinyUSB, which cannot be called from an interrupt.
- **USB ID.** Espressif's VID with a made-up PID, `0x4A4B`: fine on one's own desk, not for distribution.

## Updating: copy jank.bin onto the drive

No button, no download mode, no tool, and no eject needed. The first install of an OTA build still needs download mode
once (it changes the partition table and the bootloader; see [boards.md](boards.md), "Flashing"); after that:

1. Copy the app image (`jank.bin`, from `scripts/build.sh xiao`: `$JANK_SCRATCH/build-xiao/jank.bin`) onto the JANK drive.
2. The stick takes it **as it lands**: the file's first sector is an ESP32-S3 app image header, so from there each sector
   the computer writes goes straight into the spare app slot (each 4 KB flash page erased before its first write). Once
   every byte of the image is in (the length comes from the image's own segment headers), `esp_ota_set_boot_partition`
   checks it (`esp_image_verify`: segments, checksum, SHA-256) and picks it for the next start.
3. The stick restarts when the computer has been quiet for 1.5 s (it writes the file's directory entry and FAT just after
   the data), at the latest 10 s after the image was whole, or at once on an eject. The drive comes back with `jank.html`
   and `UPDATE.TXT`, which says what happened (and `PANIC.TXT` after a panic, see `docs/firmware.md`). macOS may complain that the drive was not ejected: it was pulled while
   mounted, as with any UF2 board.

Refused, with the reason in `UPDATE.TXT` after a restart and the old firmware still running: an image bigger than the slot
or with a header the loader refuses, a failed check, a copy cut short and then ejected. Refused **without** a restart (the
reason only in the control line's `drive` answer), because a restart then would cost something: the running image not
confirmed yet (the bootloader would roll it back; see Rollback: after an update, wait until it has talked to a computer for
20 s), or an update over MIDI going on (it would be cut off). The drive and an update over MIDI never write the spare slot
at once: each takes it under a lock (`slot_take`). A copy started again over a cancelled one starts afresh.
A file that is not an image (notes, the macOS `._` files, `.fseventsd`) is kept in RAM while the drive is mounted, so the
computer reads back what it wrote; nothing of it survives a restart.

**Why made up, not a FAT on flash.** The board's ESP32-S3FH4R2 has 4 MB of flash: two 1.6 MB app slots leave no room for a
drive that could hold a 1.2 MB file before it is installed (`firmware/partitions_fh4r2.csv`). Streaming into the slot needs
no room at all, and a drive that is not stored cannot be corrupted. The XIAO runs the same code; its table's 2 MB
`storage` partition is no longer used.

**How the file is found.** The volume's free space is one run of clusters, and hosts give a new file free clusters in order,
so the image's byte k is in sector (start + k / 512), start being the sector its header came in. Sectors may come in any
order after the header; up to 64 that come before it are kept and moved into the slot when it comes. A host that splits
the file into pieces far apart would fail the SHA-256 check (refused, nothing broken). TinyUSB's mass storage callbacks
are the drive's own (`main/drive.c`); esp_tinyusb's are made weak by `third_party/esp_tinyusb-msc-weak.patch`. Flash
writes run in TinyUSB's task: USB waits out each page erase (tens of ms; how long a whole copy takes is not measured).
Host tests: `tests/test_vdrive.c` reads the volume as a FAT16 reader and copies images onto it in the orders hosts write.

### Rollback

A new image starts "pending verify". The old one comes back at the next reset, of any kind, unless the new one confirms
itself with `esp_ota_mark_app_valid_cancel_rollback()`. It does that (`main/ota.c`, `valid_task`) once the computer has
configured the USB device, has spoken on the MIDI control line (`scripts/ota.py`'s `get` after an update, or the page's
Connect), the control task has gone round once after that, and the image has run 20 s. Not confirmed within 120 s: it
restarts into the old image. So after a drive update, open the page and press Connect within two minutes (any line from
the computer on the control line does it). A crash or a power cut before that
puts the old image back. A start as USB host does not confirm it either: `ota.c` restarts into the old image after 120 s,
so confirm a new image from a computer before setting `usb host`. The confirm itself writes flash: while the session plays it waits until it
stops, and once its conditions are met the 120 s deadline no longer applies.

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

## The EP0 SETUP crash (found with the panic recorder)

About one in three first boots of a new image, the jank panicked 450 ms after boot and the update rolled back. `diag` after
the rollback showed `panic_task=IDLE0`, `panic_msg=LoadProhibited`, `panic_cause=28`, `panic_vaddr=0x00000000`, `panic_pc` in the ROM and
`panic_a0=0x82032ec5` (windowed ABI: the top two bits of a0 are the window increment, the address is `0x42032ec5`). addr2line of that
address on the elf of the flashed image gave `dcd_event_setup_received` inlined by `handle_epout_dma` (`dcd_dwc2.c`, the `setup_phase_done`
branch): the USB interrupt reading the SETUP packet through a null pointer. The branch takes the pointer from `DOEPDMA0 - 8` ("the
last valid SETUP data", programming guide 9.1.2.1) and copies 8 bytes from it. With `DOEPDMA0` = 8 (the pointer never armed with the setup
buffer when the host's first SETUP after a bus reset arrived, which is when a host re-enumerates a restarted jank) that is address 0.
Only the timing of the host decides, so it is intermittent.

`third_party/esp_tinyusb-dwc2-setup-guard.patch` (applied by `scripts/patch_tinyusb.sh`) checks that the pointer lies inside the
setup buffer; if not it drops the event, calls `dma_setup_prepare` and returns. The host then repeats its request (a lost SETUP is
retried by the host). Not proven on the jank: the cause of the stale pointer is read from the code, the crash is intermittent, and only
several first flashes in a row show that it is gone. The slave (FIFO) path has no such pointer (it reads the setup buffer itself), so it
is not patched. The decode recipe is in docs/firmware.md ("Decoding an address").

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
audio when a stream is open (the computer's in device mode, the hosted device's in host mode). The line in goes to the
tracker on boards with one (docs/linein.md; the XIAO only with its bench option). Host mode has no network adapter, drive or USB control
line.

jank does not switch power to the device. On a bare XIAO the 5V pin is the USB-C socket's VBUS with nothing between
them (v1.2 schematic), so 5 V on that pin powers the device, or a powered hub does. The XIAO presents 5.1 k on both CC
pins (a sink): USB-A gear behind an adapter does not care; USB-C gear plugged straight in may not power up.

**Never feed a bare XIAO's 5V pin with a computer on its USB port**: nothing separates the two supplies. The PCB adds an
ideal diode and a firmware cut for this (`hardware/pcb/README.md`, "The power path"); that cut is not in the firmware
yet.
