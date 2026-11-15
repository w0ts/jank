// SPDX-License-Identifier: MIT
// The control page's logic, with no browser and no jank: the SysEx line, its transports' framing (BLE-MIDI against the C
// framing's own vectors), the HOME line, the line layer, the iPhone apps' Web MIDI shape. Exit status: non-zero on a failure.
// Run by tests/run.sh, which writes the C vectors first (BLEMIDI_VECTORS).
const fs = require("fs");
const path = require("path");
const vm = require("vm");
const assert = require("assert");

const html = fs.readFileSync(path.join(__dirname, "..", "page", "jank.html"), "utf8");
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
   { bpm: 120, net: { kind: "jank", group: 3 }, transport: "playing", bar: 275, quantum: 4, link: 2, air: 0, battery: 64, level: -24, conf: 87,
     levelMin: null, levelMax: null });
eq(box.parseHome("h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18"),
   { bpm: 120, net: { kind: "jank", group: 3 }, transport: "playing", bar: 275, quantum: 4, link: 2, air: 0, battery: 64, level: -24, conf: 87,
     levelMin: -31, levelMax: -18 });
eq(box.parseHome("h 0 j3 0 - 4 0 0 - - - - -"),
   { bpm: 0, net: { kind: "jank", group: 3 }, transport: "stopped", bar: null, quantum: 4, link: 0, air: 0, battery: null, level: null, conf: null,
     levelMin: null, levelMax: null });
eq(box.parseHome("h 12000 w0 2 150 3 2 0 - - -").net, { kind: "wifi", up: false });
eq(box.parseHome("h 12000 w1 2 150 3 2 0 - - -").transport, "armed");
eq(box.parseHome("h 12000 x3 1 275 4 2 0 - - -"), null);
eq(box.parseHome("h 12000 j3 1 275"), null);
eq(box.parseHome("b 1 12000 steady 1"), null);
{
  const h = box.parseHome("h 12000 j3 1 275 4 2 3 - -24 87");
  const v = box.homeView(h, 0);
  eq([v.bpm, v.beat, v.transport, v.net, v.link, v.air, v.battery, v.level, v.conf], ["120", 2, "Playing", "MODE JANK 3", "2", "3", "-", "-24 dB", "87 %"]);
  eq(box.homeView(h, 250).beat, 3);           // 2.75 + 0.5 beats at 120 BPM: the bar's fourth beat
  eq(box.homeView(h, 620).beat, 3);
  eq(box.homeView(h, 630).beat, 0);           // 2.75 + 1.26 beats: round to the bar's first
  eq(box.homeView(box.parseHome("h 12000 j3 0 275 4 2 3 - - -"), 5000).beat, 2);   // stopped: it stays
  eq(box.homeView(box.parseHome("h 12050 w1 1 0 4 2 0 - - -"), 0).bpm, "120.50");
  eq(box.homeView(box.parseHome("h 0 j3 0 - 4 0 0 - - -"), 0).transport, "No Link session yet");
  eq(box.homeView(box.parseHome("h 12000 w1 1 0 4 2 5 - -60 0"), 0).air, "-");
  eq(box.homeView(box.parseHome("h 12000 w1 1 0 4 2 5 - -60 0"), 0).levelPct, 0);
  eq(box.homeView(box.parseHome("h 12000 w1 1 0 4 2 5 - 0 100"), 0).levelPct, 100);
  eq(box.homeView(null, 0), null);
}

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
eq(box.tempoCommand("120"), "tempo 120");
eq(box.tempoCommand(" 128.5 "), "tempo 128.5");
eq(box.tempoCommand("99,25"), "tempo 99.25");
eq(box.tempoCommand("19.99"), null);
eq(box.tempoCommand("1000"), null);
eq(box.tempoCommand("120.125"), null);
eq(box.tempoCommand("1e2"), null);
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
