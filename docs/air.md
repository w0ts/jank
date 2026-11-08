# On the air

jank speaks the FM-1's group mode (shown as MODE JAM on FM-1 firmware from before the rename) and carries Ableton
Link unchanged.

## The frame format

Open data frames at 1 Mb/s, as the FM-1 sends them:

- Frame control `08 00`: ToDS = FromDS = 0.
- addr1 the destination, addr2 the sender, addr3 the group's BSSID `02:66:6D:31:6A:GROUP`.
- Sequence number in the top 12 bits of the sequence field, then LLC/SNAP `AA AA 03 00 00 00` + EtherType, then the
  payload.
- GROUP 1..9 on channels 1, 6, 11 in turn (1, 4, 7 on 1; 2, 5, 8 on 6; 3, 6, 9 on 11); addresses 169.254.x.y; no
  beacon, no key.
- Group frames are not acknowledged; unicast is (whether the ESP acknowledges in this mode is unmeasured).

An FM-1 and a jank look the same on the air. The only tell is the MAC: a jank's starts `02:6A:6B`, so a jank whose
computer overrode the MAC counts as a unit, not a jank.

## The bridge (`core/jank_core.c`)

No routing, NAT or DHCP: the computer gives itself a 169.254.x.y address.

- **The MAC.** `esp_wifi_80211_tx` can send a chosen addr2 (from a disassembly of IDF 5.5.6's
  `ieee80211_raw_frame_sanity_check`: it refuses Protected and QoS frames and checks addr2 only when associated, and
  jank never associates). The limit is the receive filter, which acknowledges only the radio's own address. So the
  stick offers its radio's MAC in the NCM descriptor, the computer's adapter takes it, and no address is rewritten. If
  a computer overrides that MAC, the stick adopts the new one and restarts the radio (host-tested only).
- **The filter.** At 1 Mb/s a byte costs 8 us, so by default only ARP, IPv4 unicast and Link's multicast go out.
  `set filter all` makes it a plain bridge (untested on hardware).
- **Size.** `esp_wifi_80211_tx` takes at most 1500 bytes: 1468 bytes of payload. Larger frames are dropped and
  counted. Link's are at most 512.

## The Link peer (`core/jank_peer.c`)

Runs `third_party/link/link.c`, Optimist's Link peer (see its README). On the air it shares the MAC with its own IP
address; towards the computer it uses a second MAC, since a computer cannot send to its own. It stays silent until it
hears Link and leaves after 8 s alone (`JP_ALONE_MS`), so its session is always the younger one: it joins and never
pushes 120 BPM on a group. With a computer attached, the FM-1 and Live each see the stick as one more peer.

## Group scan

- The home channel needs no dwell: anything heard there in the last 3 s counts (`JS_HEARD_MS`).
- The scan visits the two other channels for 350 ms each (`JS_DWELL_MS`, longer than Link's 250 ms announce), about
  0.7 s plus channel switching (unmeasured). The button's scan joins the next group heard after the current one,
  wrapping, or stays. The control page's `scan` adds a 350 ms dwell at home and joins nothing (about 1.05 s).
- While away nothing is heard or sent on the home channel: frames for the air are dropped and counted (`scan_lost`).
  Link peers live 5 s, so nobody should forget the stick. One lost announce can make a group look absent: scan again.
- The MIDI clock runs free from the last tempo and phase during the scan; after joining another group it jumps to the
  new session's phase, so a resync click is worth doing.

## Normal Wi-Fi

`set net wifi` (with `set ssid`, `set pass`) makes the stick a station on an ordinary network instead, after a restart:
the Link peer then talks on that network. `set net jam` goes back to group mode.
