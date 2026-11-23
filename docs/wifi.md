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

## Not verified on hardware

Everything above is host-tested only: ARP and UDP on a real access point, the DHCP address being the one the client sees, and the Link
session keeping its timing with the extra frames. A request wakes the control task at once (it otherwise sleeps up to a second between
passes); how fast it answers is not measured. Memory: about 4 KB of heap is taken at start-up in MODE WIFI (the queues) and a 2 KB buffer at
the first request, not measured against the mini 3's free heap (`heap_min` in `status`).
