// SPDX-License-Identifier: MIT
// The control page's logic, with no browser and no jank: the SysEx line, its transports' framing (BLE-MIDI against the C
// framing's own vectors), the HOME line, the line layer, the iPhone apps' Web MIDI shape. Exit status: non-zero on a failure.
// Run by tests/run.sh, which writes the C vectors first (BLEMIDI_VECTORS).
const fs = require("fs");
const path = require("path");
const vm = require("vm");
const assert = require("assert");

const html = fs.readFileSync(path.join(__dirname, "..", "page", "jank.html"), "utf8");
const expandedTemplate = require("./load_html.js")(path.join(__dirname, "..", "page", "src", "jank.html"));
const logic = html.match(/<script id="logic">([\s\S]*?)<\/script>/)[1];
const ui = html.match(/<script id="ui">([\s\S]*?)<\/script>/)[1];
const box = {};
vm.createContext(box);
vm.runInContext(logic, box);
// classes and consts are the script's lexical names, not properties of its global: hand them out
vm.runInContext("this.SysexIn = SysexIn; this.BleMidiIn = BleMidiIn; this.LineClient = LineClient; this.JX_PUSH_HOME = JX_PUSH_HOME;", box);
new vm.Script(ui);                       // the DOM part at least parses

let checks = 0;
function eq(a, b) {
  checks++;
  assert.deepStrictEqual(JSON.parse(JSON.stringify(a)), b);
}
function ok(c, what) {
  checks++;
  assert.ok(c, what);
}

// --- the page as a file ---
ok(html === expandedTemplate, "page/jank.html is the expansion of page/src/jank.html: run make page");
ok(!/<!--@include/.test(html), "no include marker left");
ok(/<script id="face-ui">/.test(html) && /<script id="advanced-ui">/.test(html) && /const Stick = /.test(html) && /const Face = /.test(html), "the face's, Advanced's and the player's scripts are in the page");
ok(/id="c-midi"[^>]*aria-pressed="false"/.test(html) && /id="c-bluetooth"[^>]*aria-pressed="false"/.test(html), "Connect: MIDI port and Bluetooth, none pressed before a connection");
ok(!/id="c-sim"|>Demo<|>Simulator<|Sim\.|const Sim|sim\.js|\?sim=/.test(html), "no Demo / simulator mode left");
ok(!/face-stick|face-sim|face-bar|face-tag|<h2>Device|<h2>Connect|link-state|id="c-off"/.test(html), "no Device or Connect heading, no tag, no status text, no Disconnect button");
ok(!/>Connected\.|say\("Connected\.|>not connected<|Details \(USB\)/.test(html), "no 'Connected.' message, no 'not connected' label, no 'Details (USB)'");
ok(/<summary>Details<\/summary>/.test(html), "the Details section is still there");
ok(/id="live" class="hide"/.test(html) && /id="c-advanced-box" class="hide"/.test(html) && /id="player" class="hide"/.test(html) && /id="advanced-extra" class="hide"/.test(html), "the face, the Advanced box, the Player card and the Advanced extra start hidden");
ok(!/face-hint|The stick has no screen: this is a virtual screen of its state/.test(html), "no key-hint line and no 'no screen' note around the device");
ok(/#face \.lbl, #face \.brand \{ display: none; \}/.test(html) && !/max-width: 520px/.test(html), "the edge labels and the caption are hidden on this page; the drawing is no longer capped at 520 px");
ok(/setAttribute\("viewBox", "-8\.5 -6 87 80\.5"\)/.test(html) && /card\.title = Stick\.KEY_HINT/.test(html), "the box is cropped to its jacks and the keys are a tooltip");
ok(!/pl-test|testStick|Test stick output|createOscillator/.test(html), "no 'Test stick output': no link, no handler, no tone generator");
ok(/id="pl-out-row" class="note pl-out hide"/.test(html), "the 'stick output' line starts hidden: it shows only when something needs the user");
ok(/Allow on every visit/.test(html), "the ? help says what 'Allow on every visit' does");
const faceUi = html.match(/<script id="face-ui">([\s\S]*?)<\/script>/)[1];
ok(/#ledView, #seamGlow \{ display: none; \}/.test(html) && !/makeLeds|paintLed|Stick\.leds/.test(faceUi), "the control page shows no LED dots and draws none (the Mac simulator still does)");
ok(!/lastBeat|takeBeat|parseBeat/.test(html), "the page keeps no beat-report code any more: only the LEDs used it");
ok(/Stick\.phaseFeed/.test(faceUi) && /id="bd-num"/.test(html) && /id="bd-slider"/.test(html), "the beat boxes run on the smooth phase, with the display delay row under Advanced");
ok(!/id="home"|id="h-|id="t-(tempo|set|play|up|down)"|id="n-(beat|ms)/.test(html), "the HOME card is gone");
for (const id of ["c-group", "c-transport", "c-usbmidi", "c-lowconf", "c-threshold", "c-autobar", "c-source", "c-ble", "ble-pair"]) ok(!new RegExp('id="' + id + '"').test(html), id + ": the stick's menu has it, the page does not");
for (const id of ["c-name", "net-mode", "net-ssid", "net-pass", "net-apply", "scan", "groups", "ble-close", "c-beat", "c-follower", "c-skip", "c-graded", "c-rate", "c-lat_trim", "c-beat_latency", "c-filter", "c-peer", "c-quantum", "c-offset"])
  ok((html.match(new RegExp('id="' + id + '"', "g")) || []).length === 1, id + " on the page once");
ok(!/id="pl-file"|id="pl-prev"|id="pl-reset"|dragover|type="file" accept/.test(html), "no single-file button, no Previous, no drag and drop");
ok(/id="pl-folder"[^>]*aria-label="Music folder"[^>]*><svg/.test(html) && /id="pl-then"/.test(html) && /id="pl-help"/.test(html), "the music folder is an icon button, 'then' and the help toggle are there");
const playerUi = html.match(/<script id="player-ui">([\s\S]*?)<\/script>/)[1];
ok(!/analyser\.connect/.test(playerUi), "the meter's analyser is only listened to: it feeds no output");
ok(/id="pl-meter"/.test(html) && /id="pl-status"/.test(html), "the player card shows a level meter and a status line");
ok(/id="pl-bpm"/.test(html) && /Player\.expectedBpm\(/.test(html), "the player shows the tag BPM times the pitch through Player.expectedBpm");
ok(/id="pl-monitor" type="checkbox" checked/.test(html), "the monitor check box is on by default");
ok(/SCREEN|placeScreen\(svg, "2\.42"\)/.test(html), "the face uses the 2.42 in screen");
ok(!/https?:\/\//.test(html), "the page loads nothing external");
ok(!/<script[^>]+src=/.test(html) && !/<link/.test(html), "no script or style from elsewhere");
ok(/<title>jank<\/title>/.test(html), "title");
ok(!/innerHTML/.test(html), "device text goes in through textContent only");
// the iPhone apps' polyfills (Web MIDI Browser's open source; MIDIWeb Browser's is closed): one .then(ok, fail) on
// requestMIDIAccess and no chaining on it, ports through forEach, input through onmidimessage
ok(/requestMIDIAccess\(\{ sysex: true \}\)\.then\(resolve, reject\)/.test(ui), "requestMIDIAccess: a single then(ok, fail)");
ok(!/requestMIDIAccess\([^)]*\)\s*\.then\([^)]*\)\s*\.(then|catch)/.test(ui), "no chained then or catch on the polyfill's promise");
ok(!/for \(const \w+ of (access|\w+\.inputs|\w+\.outputs)/.test(ui), "no for..of over the port maps");
ok(!/Array\.from\((access|ev\.data|\w+\.inputs|\w+\.outputs)/.test(ui), "no Array.from over the polyfill's objects");
ok(/onmidimessage = /.test(ui), "input through onmidimessage");

// --- the control line in SysEx: the same frame as core/jank_sysex.c ---
eq(box.textToSysex("get\n"), [[0xf0, 0x7d, 0x4a, 0x4b, 0x67, 0x65, 0x74, 0x0a, 0xf7]]);
eq(box.textToSysex("x".repeat(31)).map((f) => f.length), [35, 6]);
eq(box.textToSysex("é")[0][4], 0x3f);
eq(box.sysexToText([0xf0, 0x7d, 0x4a, 0x4b, 0x6f, 0x6b, 0x0a, 0xf7]), "ok\n");
eq(box.sysexToText(new Uint8Array([0xf0, 0x7d, 0x4a, 0x4b, 0x6f, 0xf7])), "o");
eq(box.sysexToText([0xf0, 0x7d, 0x41, 0x42, 0x6f, 0xf7]), null);
eq(box.sysexToText([0xf8]), null);

// --- reports: 'J' 'B' <type> ---
const home = [0xf0, 0x7d, 0x4a, 0x42, 0x11, ..."h 12000 j3 1 275 4 2 0 64 -24 87".split("").map((c) => c.charCodeAt(0)), 0xf7];
eq(box.pushOf(home), { type: 0x11, text: "h 12000 j3 1 275 4 2 0 64 -24 87" });
eq(box.pushOf([0xf0, 0x7d, 0x4a, 0x4b, 0x6f, 0xf7]), null);
eq(box.pushOf([0xf0, 0x7d, 0x4a, 0x42, 0xf7]), null);

// --- the HOME line, as tests/test_phone.c makes it ---
eq(box.parseHome("h 12000 j3 1 275 4 2 0 64 -24 87"),                       // older firmware: no min / max
   { bpm: 120, net: { kind: "jank", group: 3 }, transport: "playing", bar: 275, quantum: 4, link: 2, air: 0, battery: 64, level: -24, conf: 87, confHold: false, confCoast: false,
     levelMin: null, levelMax: null, audio: false, fold: "" });
eq(box.parseHome("h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18"),
   { bpm: 120, net: { kind: "jank", group: 3 }, transport: "playing", bar: 275, quantum: 4, link: 2, air: 0, battery: 64, level: -24, conf: 87, confHold: false, confCoast: false,
     levelMin: -31, levelMax: -18, audio: false, fold: "" });
eq(box.parseHome("h 0 j3 0 - 4 0 0 - - - - -"),
   { bpm: 0, net: { kind: "jank", group: 3 }, transport: "stopped", bar: null, quantum: 4, link: 0, air: 0, battery: null, level: null, conf: null, confHold: false, confCoast: false,
     levelMin: null, levelMax: null, audio: false, fold: "" });
eq(box.parseHome("h 12000 w0 2 150 3 2 0 - - -").net, { kind: "wifi", up: false });
// M2b/M2c: the mode (0 LINK, 1 AUDIO, x2 x4 /2 /4 AUDIO folded), the hold as the transport in AUDIO mode
{
  const link = box.parseHome("h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18 0");
  eq([link.audio, link.fold, link.levelMin, link.levelMax], [false, "", -31, -18]);
  eq(box.parseHome("h 12000 j3 0 275 4 2 0 64 -24 87 -31 -18 1").transport, "stopped");
  eq([box.parseHome("h 0 j3 2 - 4 0 0 - -30 12 -40 -28 1").transport, box.parseHome("h 0 j3 2 - 4 0 0 - -30 12 -40 -28 1").bpm], ["armed", 0]);
  const folded = box.parseHome("h 25600 j3 1 0 4 2 0 - -12 90 -15 -9 x2");
  eq([folded.audio, folded.fold], [true, "x2"]);
  eq(box.parseHome("h 6400 j3 1 0 4 2 0 - -12 90 -15 -9 /2").fold, "/2");
  eq(box.parseHome("h 12000 j3 1 0 4 2 0 - -12 90 -15 -9 x3").fold, "");
  // M2k: the confidence holds: "<pct>h" -> the shown value and the hold flag
  const hold = box.parseHome("h 12000 j3 1 0 4 2 0 - -12 93h -15 -9 1");
  eq([hold.conf, hold.confHold], [93, true]);
  eq([box.parseHome("h 12000 j3 1 0 4 2 0 - -12 93 -15 -9 1").conf, box.parseHome("h 12000 j3 1 0 4 2 0 - -12 93 -15 -9 1").confHold], [93, false]);
  // the robust follower's coast: "<pct>c" -> a measured value and the coast flag
  const coast = box.parseHome("h 12000 j3 1 0 4 2 0 - -12 61c -15 -9 1");
  eq([coast.conf, coast.confHold, coast.confCoast], [61, false, true]);
  // Bluetooth from `status`: the window's countdown, a phone, off; over Bluetooth this phone
  eq(box.bleView({ ble_state: "window", ble_left: "95" }, 0, false), "pairing 1:35");
  eq(box.bleView({ ble_state: "window", ble_left: "95" }, 5500, false), "pairing 1:30");
  eq(box.bleView({ ble_state: "window", ble_left: "3" }, 4000, false), "off");
  eq(box.bleView({ ble_state: "connected" }, 0, false), "phone connected");
  eq(box.bleView({ ble_state: "off" }, 0, false), "off");
  eq(box.bleView(null, 0, true), "this phone");
  eq(box.bleView({}, 0, false), "-");
  // the audio lines of status
  eq(box.audioView({ audio: "1", audio_live: "1", audio_source: "usb", outputs: "armed", peak_db: "-3" }),
     { audio: "sound (AUDIO), usb", outputs: "armed", peak_db: "-3" });
  eq(box.audioView({ audio: "1", audio_live: "0", audio_source: "usb" }).audio, "silent (LINK), usb");
  eq(box.audioView({ audio: "0", audio_live: "0", audio_source: "none" }).audio, "none (LINK)");
  eq(box.audioView({}), { audio: "-", outputs: "-", peak_db: "-" });
  // the new settings
  eq(box.setCommand("source", "link"), "set source link");
  eq(box.setCommand("source", "mic"), null);
  eq(box.setCommand("autobar", "start"), "set autobar start");
  eq(box.setCommand("autobar", "always"), "set autobar always");
  eq(box.setCommand("autobar", "on"), null);
  // M2d: the USB MIDI clock setting, the nudges in AUDIO mode
  eq(box.setCommand("usbmidi", "off"), "set usbmidi off");
  eq(box.setCommand("usbmidi", "no"), null);
  // M2h: two layers each: PHASE on top of AUTO BAR's bar, AUDIO SHIFT on top of the auto latency; 0 is AUTO
  eq(box.nudgeSaid("beat", 1, { nudge_beats: "1.5" }), "Phase +1.5 beats, on top of AUTO BAR's bar.");
  eq(box.nudgeSaid("beat", -1, { nudge_beats: "-0.5" }), "Phase -0.5 beats, on top of AUTO BAR's bar.");
  eq(box.nudgeSaid("beat", -1, { nudge_beats: "0" }), "Phase: AUTO (AUTO BAR's bar alone).");
  eq(box.nudgeSaid("ms", 1, { beat_latency: "-62", lat_auto: "-65", lat_trim: "3" }), "Audio shift +3 ms, on top of the auto -65 ms.");
  eq(box.nudgeSaid("ms", -1, { beat_latency: "-65", lat_auto: "-65", lat_trim: "0" }), "Audio shift: AUTO (-65 ms).");
  eq(box.nudgeSaid("ms", 1, { beat_latency: "26" }), "Audio in offset +26 ms.");           // a firmware before M2h
  eq(box.latField({ beat_latency: "-62", lat_auto: "-65" }), "-65");                       // the field is the auto base
  eq(box.latField({ beat_latency: "26" }), "26");
  eq(box.latNote("usb", { lat_trim: "3" }), "(the computer's) + audio shift +3 ms");
  eq(box.latNote("usb", { lat_trim: "0" }), "(the computer's)");
  eq(box.latNote("host:1935:000c", {}), "(host:1935:000c)");
  ok(/AUDIO mode/.test(box.nudgeError("err link mode")) && /limit/.test(box.nudgeError("err nudge beat at its limit, -3 to 3 beats in all")), "nudge errors in words");
  eq(box.nudgeError("err odd"), "err odd");
  ok((html.match(/id="c-beat_latency_src"/g) || []).length === 1, "the latency source note on the page once");
  // M2e: LOW CONFIDENCE and THRESHOLD, the confidence meter's tick and note
  eq(box.setCommand("lowconf", "stop"), "set lowconf stop");
  eq(box.setCommand("lowconf", "run"), "set lowconf run");
  eq(box.setCommand("lowconf", "keep"), null);
  eq(box.setCommand("threshold", "80"), "set threshold 80");
  eq(box.setCommand("threshold", "0"), "set threshold 0");
  eq(box.setCommand("threshold", "101"), null);
  eq(box.setCommand("threshold", "7.5"), null);
  eq(box.setCommand("threshold", ""), null);
}
eq(box.parseHome("h 12000 w1 2 150 3 2 0 - - -").transport, "armed");
eq(box.parseHome("h 12000 x3 1 275 4 2 0 - - -"), null);
eq(box.parseHome("h 12000 j3 1 275"), null);
eq(box.parseHome("b 1 12000 steady 1"), null);
// --- SysEx out of MIDI chunks (the iPhone apps' polyfills may cut a message) ---
{
  const s = new box.SysexIn(), got = [];
  s.feed([0x12, 0xf0, 0x7d, 0x4a], (m) => got.push(m));
  s.feed([0xf8, 0x4b, 0x6f], (m) => got.push(m));          // a clock byte inside: dropped, the message goes on
  s.feed([0x6b, 0xf7, 0xf0, 0x01, 0x90, 0x3c, 0xf0, 0x02, 0xf7], (m) => got.push(m));
  eq(got, [[0xf0, 0x7d, 0x4a, 0x4b, 0x6f, 0x6b, 0xf7], [0xf0, 0x02, 0xf7]]);   // a status byte ends a message unfinished
}

// --- BLE-MIDI against the C framing's vectors (tests/test_blemidi.c --vectors) ---
const vectorsPath = process.env.BLEMIDI_VECTORS;
if (!vectorsPath || !fs.existsSync(vectorsPath)) {
  console.error("page: BLEMIDI_VECTORS not set or missing: run tests/run.sh, which writes them from test_blemidi");
  process.exit(1);
}
const vectors = JSON.parse(fs.readFileSync(vectorsPath, "utf8"));
ok(vectors.send.length >= 20, "send vectors");
for (const v of vectors.send) eq(box.bleSend(box.textToSysex(v.text), v.cap, v.ms), v.packets);
{
  const d = new box.BleMidiIn(), bytes = [];
  eq(vectors.decode.packets.map((p) => d.decode(p, (b) => bytes.push(b))), vectors.decode.results);
  eq(bytes, vectors.decode.bytes);
}
// round trips at every payload from the minimum, lines of every length, through the decoder and the line layer's pieces
for (let cap = 3; cap <= 64; cap++)
  for (const text of ["ok\n", "get\n", "set name " + "n".repeat(23) + "\n", "x".repeat(95) + "\nok\n"]) {
    const d = new box.BleMidiIn(), s = new box.SysexIn();
    let back = "";
    for (const p of box.bleSend(box.textToSysex(text), cap, 0x1abc)) {
      ok(p.length <= cap, "packet within its payload");
      const bytes = [];
      eq(d.decode(p, (b) => bytes.push(b)), 0);
      s.feed(bytes, (m) => { back += box.sysexToText(m); });
    }
    eq(back, text);
  }
eq(box.blePack([0x90, 0x3c, 0x40], 0, [], 20), false);
eq(box.bleSysexPacket([0xf0, 0x80, 0xf7], { at: 0 }, 0, 20), null);

// --- the line layer ---
function fakeTimers() {
  const t = { next: 1, live: new Map() };
  t.set = (f, ms) => { const id = t.next++; t.live.set(id, f); return id; };
  t.clear = (id) => t.live.delete(id);
  t.fire = () => { const fs_ = [...t.live.values()]; t.live.clear(); fs_.forEach((f) => f()); };
  return t;
}
const tick = () => new Promise((r) => setImmediate(r));
async function lineTests() {
  const timers = fakeTimers(), written = [];
  const line = new box.LineClient(async (text) => { written.push(text); }, timers);
  const pushes = [];
  line.onPush = (p) => pushes.push(p);
  const frames = (text) => box.textToSysex(text).forEach((f) => line.receive(f));
  // an answer across frames, a push in the middle of it
  let p = line.send("get");
  await tick();
  eq(written, ["get\n"]);
  frames("group=3\nname=front of house\nble=pa");
  line.receive(home);
  frames("ir\nok\n");
  const a = await p;
  eq(a.values, { group: "3", name: "front of house", ble: "pair" });
  eq(pushes.map((x) => x.type), [box.JX_PUSH_HOME]);
  // two commands queue: the second goes out only after the first's answer
  const p1 = line.send("set ble off"), p2 = line.send("get");
  await tick();
  eq(written.slice(1), ["set ble off\n"]);
  frames("ok\n");
  await p1;
  await tick();
  eq(written.slice(1), ["set ble off\n", "get\n"]);
  frames("err bad setting\n");
  await assert.rejects(p2, /err bad setting/);
  checks++;
  // lines with nobody waiting are dropped; a timeout rejects and the next command still runs
  frames("stray=1\nok\n");
  p = line.send("status");
  await tick();
  timers.fire();
  await assert.rejects(p, /no answer to status/);
  checks++;
  p = line.send("home");
  await tick();
  frames("home=h 0 j3 0 - 4 0 0 - - -\nok\n");
  eq((await p).values.home, "h 0 j3 0 - 4 0 0 - - -");
  // a transport that cannot write rejects the command; close rejects the one waiting
  const broken = new box.LineClient(async () => { throw new Error("The MIDI port refused SysEx"); }, fakeTimers());
  await assert.rejects(broken.send("get"), /refused SysEx/);
  checks++;
  p = line.send("scan");
  await tick();
  line.close("gone");
  await assert.rejects(p, /gone/);
  checks++;
}

// --- Web MIDI ports: Maps (Chrome) and the polyfill's forEach-only objects ---
{
  const usb = { id: "1", name: "jank", state: "connected" }, ble = { id: "2", name: "jank 1A2B", state: "connected" };
  const other = { id: "3", name: "FM1-E040", state: "connected" }, gone = { id: "4", name: "jank", state: "disconnected" };
  const polyfill = { forEach(f) { [other, ble, usb, gone].forEach((p, i) => f(p, String(i))); } };
  eq(box.portList(polyfill).map((p) => p.id), ["3", "2", "1"]);
  eq(box.portList(new Map([["a", usb], ["b", gone]])).map((p) => p.id), ["1"]);
  eq(box.portList(undefined), []);
  eq(box.jankPorts(box.portList(polyfill)).map((p) => p.name), ["jank", "jank 1A2B"]);   // USB first
  eq(box.overBluetooth("jank 1A2B"), true);
  eq(box.overBluetooth("jank"), false);
  eq(box.overBluetooth("FM1-E040"), false);
  eq(box.outputFor(ble, [usb, ble]).id, "2");
  eq(box.outputFor({ name: "Bluetooth" }, [other, usb]).id, "1");
  eq(box.outputFor({ name: "x" }, [other]), null);
}

// --- forms ---
eq(box.networkCommands("jank", "", "", false), ["set net jank", "reboot"]);
eq(box.networkCommands("wifi", "Home Net", "correct horse", false), ["set ssid Home Net", "set pass correct horse", "set net wifi", "reboot"]);
eq(box.networkCommands("wifi", "Home", "", true), ["set ssid Home", "set net wifi", "reboot"]);
eq(box.networkCommands("wifi", "Open", "", false), ["set ssid Open", "set pass", "set net wifi", "reboot"]);
eq(box.networkCommands("wifi", "", "correct horse", false), null);
eq(box.networkCommands("wifi", "Home", "short", false), null);
eq(box.networkCommands("wifi", "Höme", "correct horse", false), null);
eq(box.networkCommands("wifi", "x".repeat(33), "correct horse", false), null);
// over Bluetooth: never a password (the jank refuses it there)
eq(box.networkCommands("wifi", "Home", "correct horse", false, true), null);
eq(box.networkCommands("wifi", "Home", "", false, true), ["set ssid Home", "set net wifi", "reboot"]);
eq(box.networkLine({ net: "jank", group: "2" }), "MODE JANK, group 2");
eq(box.networkLine({ net: "wifi", ssid: "Home" }), "Wi-Fi network Home");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "0" }), "Looking for Home");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "1", wifi_ip: "0.0.0.0" }), "On Home, waiting for an address");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "1", wifi_ip: "192.168.1.5", wifi_rssi: "-50", wifi_channel: "6" }), "On Home, 192.168.1.5, -50 dBm, channel 6");
eq(box.splitLines("", "ab"), { lines: [], rest: "ab" });
eq(box.splitLines("ab", "c\nd"), { lines: ["abc"], rest: "d" });
eq(box.splitLines("", "a\r\nb\n"), { lines: ["a", "b"], rest: "" });
{
  const a = box.parseAnswer(["group=3", "name=front of house", "peer=8c:aa:bb:cc:dd:ee,-44,100,5", "peer=8c:aa:bb:cc:dd:ef,-60,20,1", "not a pair"]);
  eq(a.values, { group: "3", name: "front of house" });
  eq(a.units[0], { mac: "8c:aa:bb:cc:dd:ee", rssi: -44, age: 100, frames: 5 });
  eq(box.parseAnswer(["peer=on"]).values.peer, "on");
  eq(box.parseAnswer(["scan=3,11,4,3,1,-50"]).scans[0], { group: 3, channel: 11, frames: 4, units: 3, janks: 1, rssi: -50 });
  eq(box.parseAnswer(["scan=none"]).scans, []);
}
eq(box.peerText({ units: 3, janks: 1 }), "3, of which 1 jank");
eq(box.peerText({ units: 2, janks: 0 }), "2");
eq(box.isEnd("ok"), true);
eq(box.isEnd("err bad setting"), true);
eq(box.isEnd("okay"), false);
eq(box.faultDrops({ drop_big: "1", drop_bad: "2", tx_fail: "3", queue_full: "4", drop_foreign: "5", drop_filter: "99" }), 15);
eq(box.setCommand("group", "4"), "set group 4");
eq(box.setCommand("group", "0"), null);
eq(box.setCommand("group", "1.5"), null);
eq(box.setCommand("offset", "-0.05"), "set offset -0.05");
eq(box.setCommand("offset", "1.275"), null);
eq(box.setCommand("quantum", "16"), "set quantum 16");
eq(box.setCommand("name", "front of house"), "set name front of house");
eq(box.setCommand("name", ""), null);
eq(box.setCommand("name", "x\nreboot"), null);
eq(box.setCommand("name", "café"), null);
eq(box.setCommand("filter", "all"), "set filter all");
eq(box.setCommand("filter", "some"), null);
eq(box.setCommand("ble", "auto"), "set ble auto");
eq(box.setCommand("ble", "on"), null);
eq(box.setCommand("pass", "secret12"), null);         // not through the settings form

lineTests().then(() => console.log("page: " + checks + " checks passed"), (e) => { console.error(e); process.exit(1); });
