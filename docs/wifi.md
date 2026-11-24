# Talking to a jank over Wi-Fi (read-only)

A unit in MODE WIFI (it has an address on a network: `docs/air.md`) answers a small UDP protocol on port **7447**, so what a USB
host cannot show (no control line, no log: `docs/usb.md`, "Host mode") can still be read from a computer on the same network.
There is no IP stack on the stick (`core/jank_net.h`): the unit answers ARP for its DHCP address and reads UDP datagrams
itself (`core/jank_wsvc.c`, host-tested with frames built by hand in `tests/test_wsvc.c`).

## Use

```
python scripts/jankwifi.py get                       # to everyone on the network: each jank answers with the start of `get` (its name)
python scripts/jankwifi.py --ip 192.168.1.10 diag    # to one: the whole answer
```

A request sent to everyone (the broadcast address) is answered with one short chunk of `get` only, enough to see which units are
there and at what address; ask one by its address for anything else.

- The unit must be in MODE WIFI: over USB (`scripts/ctl.py`) `set net wifi`, with `set ssid <network>` and `set pass <password>` for
  the network (the password goes to the unit, never into this repository). MODE WIFI is read at start-up, so restart the unit after
  setting it (unplug and plug it in again). It changes what the unit's Link session runs over (the network instead of the private
  jank-to-jank air).
- Lines it runs: `get`, `status`, `about`, `home`, `beat`, `diag`, `line`, `drive`. All read-only; anything else answers
  `err not allowed over wifi` (a `set`, `ota`, `reboot`, `bootloader`, `scan`, `play`, `tempo`, ...).
- While the unit is a USB host, `diag` ends with the `hostdiag_` lines live: the stack, the devices that came, their names and IDs, the
  interface and endpoints chosen, the claim result, packets from the Launchpad and the Digitone (`core/jank_hostdiag.h`).
- The LED's two short flashes every 2.4 s say the host stack is running.

## Protocol

Request: one UDP datagram to port 7447 (to the unit, or to the broadcast address), the text of one control line, 1 to 120 printable ASCII
characters; a trailing line end is ignored. A datagram that is not a clean request (a bad checksum, a fragment, a control character, too
long) gets no answer.

Answer: datagrams from port 7447 back to the sender, each one byte then text of at most 200 bytes. The byte is the chunk's index
(0..127); bit 7 marks the last chunk. The text, chunks in index order, is the control line's answer, ending `ok` or `err ...`. A client that
misses a chunk asks again (`scripts/jankwifi.py` does once, at half its timeout).

## Safety

Unauthenticated, for a network one trusts: read-only is the whole defence. Anyone on the network can read what `get`, `status` and `diag`
say: the unit's name, its network's name, addresses, the last panic's text, firmware details (never the password: `pass=set`).

An answer can be about 40 times the size of its request (`status`), and the sender's address in a datagram can be forged. So an answer goes
only to a host on the unit's own subnet with an ordinary address (not the subnet's network or broadcast address, not the unit's own, a
unicast MAC, a source port), and a request to everyone is answered with one short chunk. That limits the reflection to the local network;
it does not stop a forged address of a neighbour. Two requests wait at a time; a third is dropped.

## Updating the firmware over Wi-Fi (signed)

The text service above is read-only and stays so. The one thing on the same port that can change the unit is the **signed update**
(`core/jank_wota.h`): the firmware sent in UDP frames, each signed with a key only the owner holds. It exists because in USB host mode Wi-Fi is the
only link to the unit, and every update used to mean moving it to the computer.

```
python scripts/otakey.py                                   # once, the stick on this Mac over USB: makes the key and gives it to the stick
python scripts/jankwifi.py --ip 192.168.1.10 ota build/jank.bin    # then, from anywhere on the network
```

- **The key** is 32 random bytes, made by `scripts/otakey.py` into `~/.config/jank/otakey` (rights 600, folder 700, outside every repository, printed
  nowhere), and given to the stick by the control line `otakey <64 hex>` (USB only: the Wi-Fi service refuses it, like every line it does not
  list). The stick keeps it in flash and never says it: `otakey` answers only `wota_key=set` or `none`. `scripts/otakey.py --status`, `--clear`
  (the stick forgets it: no Wi-Fi update until a new one is set) and `--rotate` (a new key on both sides). `wipe settings` removes it too.
  **No key, no update: every signed frame is ignored without an answer.**
- **The frames**: a HELLO (`01 01`) gets a fresh random 8-byte nonce; every request after it is `01 02 <counter> <op> <args> <mac>`, where the mac is
  HMAC-SHA-256 (key, nonce || the frame) cut to 16 bytes, and so is the unit's reply, so the sender knows it is the unit that answered. A frame
  with a wrong mac, a counter that does not rise (a replay), or outside a session (no HELLO, or 30 s of silence) gets no answer: no oracle to guess
  with, nothing to reflect at a stranger. A HELLO while a session is busy (a signed request in the last 5 s) is ignored, so nobody else on the
  network can knock a running update off. Ops: BEGIN (size, CRC), DATA (offset, CRC, up to 1024 bytes), END, ABORT, CONFIRM.
- **The transfer** is the control line's `ota` transfer (`core/jank_ota.h`): the same chunk CRCs, the offset to go on from, the image checked as the
  loader sees it (header, segments, SHA-256) before the boot choice is made, and a chunk the unit already has answered "ok, next offset", so a
  lost answer costs a resend, not a corrupt image. Stop and wait: about 1.5 MB in roughly a minute and a half over a good link (not measured).
- **Probation and rollback** as for any update: the new image stays "pending verify" until it is confirmed, and the next reset of any kind
  puts the old image back. Over USB the computer's control line confirms it; in host mode there is none, so `jankwifi.py ota` waits for the unit to come
  back (its uptime goes down), waits until it has run 20 s, and sends a signed CONFIRM. An image that cannot bring Wi-Fi up never gets one: the
  unit restarts into the old image after 2 minutes, so a bad update cannot strand a unit that only has Wi-Fi.
- **What it does not protect against**: someone who has the key file (it is as good as the firmware), or who can read the traffic and the unit's
  flash. The frames are signed, not encrypted: the image goes over the air in the clear. The key is shared by every unit one gives it to; a unit
  with no key takes no update. A button on the unit to open a window for updates (`we might later gate it`, the owner, 2026-11-24) would add a
  physical check; it is not there yet.
- **Memory**: a 1 KB frame slot in PSRAM at start-up in MODE WIFI, about 100 bytes of RAM for the session and the key, no more on the internal
  heap. The update runs in the control task, never in the network task (a flash erase stalls it for tens of milliseconds).

## Not verified on hardware

The signed update has been host-tested only (the frames against Python's own HMAC, a mock unit with lost replies and a damaged chunk), not yet
on the unit: the first Wi-Fi update has to be preceded by one update over USB or the drive (that image has the Wi-Fi update in it), then
`scripts/otakey.py`.

Everything else above is host-tested only: ARP and UDP on a real access point, the DHCP address being the one the client sees, and the Link
session keeping its timing with the extra frames. A request wakes the control task at once (it otherwise sleeps up to a second between
passes); how fast it answers is not measured. Memory: about 4 KB of heap is taken at start-up in MODE WIFI (the queues) and a 2 KB buffer at
the first request, not measured against the mini 3's free heap (`heap_min` in `status`).
