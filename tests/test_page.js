// SPDX-License-Identifier: MIT
// The control page's logic, with no browser and no jank: the SysEx line, the HOME line, the line layer, the iPhone apps' Web MIDI
// shape. Exit status: non-zero on a failure. Run by tests/run.sh.
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
vm.runInContext(fs.readFileSync(path.join(__dirname, "..", "ui", "ports.js"), "utf8") + "\nthis.jankPortName = jankPortName; this.jankPorts = jankPorts; this.outputFor = outputFor; this.portNames = portNames;", box);
// classes and consts are the script's lexical names, not properties of its global: hand them out
vm.runInContext("this.SysexIn = SysexIn; this.LineClient = LineClient;", box);
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
ok(!/id="connect"|class="buttons"/.test(html.split('id="live"')[0]), "no connection card: nothing but the title, the message and the connect button above the face");
ok(/<div class="titlebar"><h1><a href="https:\/\/w0ts.github.io\/">w0ts<\/a> \/ Jank<\/h1><\/div>\s*<nav id="home-links" class="site" aria-label="Jank pages">\s*<button id="c-midi"/.test(html) && html.indexOf('id="c-midi"') < html.indexOf('id="connect-slot"') && !/placeConnect/.test(html), "the title bar reads w0ts / Jank (w0ts links to the root page); the connect / eject button leads the nav row under it, and stays there");
ok(html.indexOf('id="dev-bar"') > html.indexOf('id="face"') && !/id="c-advanced|storedAdvanced|rememberAdvanced/.test(html), "the Advanced check box is gone from the device card and from the code");
{
  const at = (s) => html.indexOf(s);
  const order = ['id="face"', 'id="sec-settings"', 'id="sec-player"', 'id="sec-advanced"'].map(at);
  ok(order.every((n, i) => n > 0 && (i === 0 || n > order[i - 1])), "after the device card: the Settings, Player and Advanced sections, in that order");
  ok(/<details id="sec-settings"[^>]*><summary[^>]*><h2>Settings<\/h2><\/summary>/.test(html) && /<details id="sec-player"[^>]* open><summary[^>]*><h2>Player<\/h2><\/summary>/.test(html) && /<details id="sec-advanced"[^>]*><summary[^>]*><h2>Advanced<\/h2><\/summary>/.test(html), "each section is a <details> with its h2 title; only the Player starts open");
  ok(!/id="face"[^>]*><details|<details[^>]*id="face"/.test(html), "the device card is not collapsible");
  const inAdv = html.slice(at('id="sec-advanced"'), at('</main>'));
  ok(['adv-boots', 'bd-num', 'al-on', 's-rssi', 'units', 's-version', 'bootloader'].every((id) => inAdv.includes('id="' + id + '"')), "Advanced holds the stats, the tuning, the beat display, Auto latency, Air and Link, Peers heard and Firmware");
  ok(!/id="more"|id="advanced-extra"/.test(html), "the old Details and Advanced extra wrappers are gone");
  const inSet = html.slice(at('id="sec-settings"'), at('id="sec-player"'));
  ok(inSet.includes('id="c-name"') && inSet.includes('id="net-apply"') && !/id="scan"|id="groups"|id="scan-result"|Scan the groups/.test(html), "Settings keeps the name and the Wi-Fi; the groups block is gone");
}
ok(/id="c-midi"[^>]*aria-pressed="false"/.test(html), "Connect: the MIDI port button, not pressed before a connection");
ok(/id="c-midi"[^>]*><span class="connect-text">Connect<\/span><svg class="eject"/.test(html) && !/MIDI port \(USB, iPhone app\)/.test(html), "the button says Connect, and shows an eject icon while connected");
ok(!/bluetooth|gatt|\bble\b|\bble[-_]/i.test(html), "no Bluetooth, BLE or GATT anywhere in the page");
ok(!/id="c-sim"|>Demo<|>Simulator<|Sim\.|const Sim|sim\.js|\?sim=/.test(html), "no Demo / simulator mode left");
ok(!/face-stick|face-sim|face-bar|face-tag|<h[23]>Device|<h[23]>Connect|link-state|id="c-off"/.test(html), "no Device or Connect heading, no tag, no status text, no Disconnect button");
ok(!/>Connected\.|say\("Connected\.|>not connected<|Details \(USB\)/.test(html), "no 'Connected.' message, no 'not connected' label, no 'Details (USB)'");
ok(!/<summary[^>]*>Details<\/summary>/.test(html), "the separate Details section is merged into Advanced");
ok(/id="live" class="hide"/.test(html), "the face and the three sections (inside it) start hidden");
ok(!/face-hint|The device has no screen: this is a virtual screen of its state/.test(html), "no key-hint line and no 'no screen' note around the device");
ok(!/class="lbl|class="brand|<text[^>]*class="lbl/.test(html) && !/max-width: 520px/.test(html), "the new drawing has no edge labels and no caption (no text but the keys' marks); the drawing is not capped at 520 px");
ok(/Box3\.build\(svg\)/.test(html) && !/setAttribute\("viewBox"/.test(html) && /card\.title = Stick\.KEY_HINT/.test(html) && /id="dev"/.test(html) && !/id="ledView"|id="pwr"|class="brand"/.test(html), "the box is drawn by Box3.build from its table (view box from the table), no LEDs, no power switch, the keys are a tooltip");
ok(/<g id="dpad"><\/g>/.test(html) && /<foreignObject id="oledFO"[^>]*><canvas id="oled" width="320" height="240">/.test(html), "the drawing shell: the D-pad group, the screen canvas of 320 x 240");
ok(!/pl-test|testStick|Test device output|createOscillator/.test(html), "no 'Test device output': no link, no handler, no tone generator");
ok(/id="pl-out-row" class="note pl-out hide"/.test(html), "the 'device output' line starts hidden: it shows only when something needs the user");
ok(/Allow on every visit/.test(html), "the ? help says what 'Allow on every visit' does");
const faceUi = html.match(/<script id="face-ui">([\s\S]*?)<\/script>/)[1];
ok(!/id="ledView"|id="seamGlow"/.test(html) && !/makeLeds|paintLed|Stick\.leds/.test(faceUi), "the control page shows no LED dots and draws none (the Mac simulator still does)");
ok(!/lastBeat|takeBeat|parseBeat/.test(html), "the page keeps no beat-report code any more: only the LEDs used it");
ok(/Stick\.phaseFeed/.test(faceUi) && /id="bd-num"/.test(html) && /id="bd-slider"/.test(html), "the beat boxes run on the smooth phase, with the display delay row under Advanced");
ok(!/id="home"|id="h-|id="t-(tempo|set|play|up|down)"|id="n-(beat|ms)/.test(html), "the HOME card is gone");
for (const id of ["c-group", "c-transport", "c-usbmidi", "c-lowconf", "c-threshold", "c-autobar", "c-source"]) ok(!new RegExp('id="' + id + '"').test(html), id + ": the device's menu has it, the page does not");
for (const id of ["c-name", "net-mode", "net-ssid", "net-pass", "net-apply", "c-beat", "c-follower", "c-skip", "c-graded", "c-rate", "c-lat_trim", "c-beat_latency", "c-filter", "c-peer", "c-quantum", "c-offset"])
  ok((html.match(new RegExp('id="' + id + '"', "g")) || []).length === 1, id + " on the page once");
ok(!/id="pl-file"|id="pl-reset"|dragover|type="file" accept/.test(html), "no single-file button, no drag and drop");
ok(/id="pl-folder"[^>]*aria-label="Music folder"[^>]*><svg/.test(html) && /id="pl-then"[^>]*><svg/.test(html) && /id="pl-help"/.test(html), "the music folder and the after-track mode are icon buttons, the help toggle is there");
ok(/id="pl-play"[^>]*><svg/.test(html) && /id="pl-next"[^>]*><svg/.test(html) && !/>Play<\/button>|>Next<\/button>/.test(html), "Play and Next are icon buttons, no text");
ok(!/id="pl-pitch-num"/.test(html) && !/<select id="pl-then"/.test(html), "no pitch number box, no 'then' select");
ok(html.indexOf('id="pl-folder"') > html.indexOf('id="pl-next"') && html.indexOf('id="pl-folder"') < html.indexOf('id="pl-crumbs"'), "the folder button sits in the folder row, before the path, below the toolbar");
const playerUi = html.match(/<script id="player-ui">([\s\S]*?)<\/script>/)[1];
ok(!/analyser|pl-meter|class="meter"[^>]*><i id="pl-meter"/i.test(playerUi) && !/id="pl-meter"/.test(html) && /id="pl-status"/.test(html), "the player card has no level meter (no analyser, no bar) and keeps its status line");
ok(/id="pl-prev"[^>]*aria-label="Previous track"[^>]*><svg/.test(html) && /id="pl-prev"[^>]*title="Plays the previous track you heard\. Pressed more than 3 seconds into a track, it restarts that track\."/.test(html), "the Previous button is an icon button with its label and tip");
ok(html.indexOf('id="pl-prev"') < html.indexOf('id="pl-play"') && html.indexOf('id="pl-play"') < html.indexOf('id="pl-next"') && html.indexOf('id="pl-next"') < html.indexOf('id="pl-then"'), "the buttons run Previous, Play, Next, after-track mode");
ok(html.indexOf('id="pl-pos"') > html.indexOf('id="pl-name"') && html.indexOf('id="pl-pos"') < html.indexOf('id="pl-seek"') && html.indexOf('id="pl-scope"') > html.indexOf('id="pl-folder"') && html.indexOf('id="pl-scope"') < html.indexOf('id="pl-crumbs"'), "the time sits right of the file name above the scrub bar; all folders sits next to the folder button");
ok(html.indexOf('id="pl-bpm"') > html.indexOf('id="pl-pitchval"') && html.indexOf('id="pl-bpm"') < html.indexOf('id="pl-pitch"'), "the tag BPM text sits in the Pitch row");
ok(!/files in scope/.test(html + playerUi) && /id="pl-rescan"/.test(html), "no 'files in scope' count text, the rescan link stays");
ok(/id="pl-bpm"/.test(html) && /Player\.expectedBpm\(/.test(html), "the player shows the tag BPM times the pitch through Player.expectedBpm");
ok(/id="pl-monitor"[^>]*aria-pressed="true"/.test(html) && /id="pl-monitor"[^>]*mutes only your speakers/.test(html), "the monitor is an icon toggle, on by default, its tip says it mutes only the speakers");
ok(/params\.get\("screen"\) === "oled" \? "2\.42" : "lcd2"/.test(html) && !/board === "lcd2"|params\.get\("scale"\)/.test(html), "the face shows the colour lcd2 screen by default for every board; only ?screen=oled gives the 2.42 in OLED; no ?scale= any more");
// the only web addresses in the page are the links (w0ts in the title bar, GitHub, the hosted page only on the Jank's drive, where the tabs are pointed at it); nothing is loaded from them
const withoutLinks = html.replace(/<a (id="home-hosted" class="hide" )?href="https:\/\/(github\.com\/w0ts\/jank|w0ts\.github\.io\/jank\/device\.html|w0ts\.github\.io\/)"/g, "");
ok(!/<nav id="home-links"[\s\S]*?target=/.test(html.split("</nav>")[0]), "the two links open in the same tab");
const withoutNs = withoutLinks.replace('a.href = "https://w0ts.github.io/jank/"', "").replace("xmlns='http://www.w3.org/2000/svg'", "");   // the favicon's SVG namespace is a name, not a load
ok(!/https?:\/\//.test(withoutNs), "the page loads nothing external");
ok(/<link rel="icon" type="image\/svg\+xml" href="data:image\/svg\+xml,/.test(html), "the page has its own favicon, inline");
ok(/<nav id="home-links" class="site" aria-label="Jank pages">/.test(html) && /<a id="home-hosted" class="hide"/.test(html) && /location\.protocol === "file:"\) \{[^}]*\$\("home-hosted"\)\.classList\.remove\("hide"\)/.test(html), "the GitHub link (with its icon) always shows; the hosted page link only when the page is opened from a file");
ok(/<a href="https:\/\/github\.com\/w0ts\/jank" aria-label="GitHub"[^>]*><svg class="gh"[^>]*><path[^>]*\/><\/svg><\/a>\s*<\/nav>/.test(html), "the GitHub link is the icon alone, last in the nav (as on the other w0ts sites)");
ok(/Audio works better on the GitHub hosted page/.test(html), "the hosted page link's tip says audio works better there");
ok(/navigator\.mediaSession\.setActionHandler\("previoustrack", \(\) => \{ if \(!el\("pl-prev"\)\.disabled\) previous\(\); \}\)/.test(html), "the media key previous track does what the Previous button does");
ok(/navigator\.mediaSession\.setActionHandler\("nexttrack", \(\) => \{ if \(!el\("pl-next"\)\.disabled\) skip\(\); \}\)/.test(html), "the media key next track does what the Next button does");
ok(!/<script[^>]+src=/.test(html) && !/<link(?![^>]*rel="icon"[^>]*href="data:)/.test(html), "no script or style from elsewhere (the favicon is inline data)");
ok(/<title>Jank · Device<\/title>/.test(html), "title");
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

// --- reports: 'J' 'B' <type> are not the control line's answers: the line layer drops them ---
const report = [0xf0, 0x7d, 0x4a, 0x42, 0x10, ..."b 1 12000 steady 1".split("").map((ch) => ch.charCodeAt(0)), 0xf7];
eq(box.sysexToText(report), null);

// --- the HOME line, as tests/test_home.c makes it ---
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
  // M2h: the latency field is the auto base, its note the AUDIO SHIFT on top (the nudges' results are on the device screen)
  eq(box.latField({ beat_latency: "-62", lat_auto: "-65" }), "-65");                       // the field is the auto base
  eq(box.latField({ beat_latency: "26" }), "26");
  eq(box.latNote("usb", { lat_trim: "3" }), "(the computer's) + audio shift +3 ms");
  eq(box.latNote("usb", { lat_trim: "0" }), "(the computer's)");
  eq(box.latNote("host:1935:000c", {}), "(host:1935:000c)");
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
  const frames = (text) => box.textToSysex(text).forEach((f) => line.receive(f));
  // an answer across frames, a report in the middle of it
  let p = line.send("get");
  await tick();
  eq(written, ["get\n"]);
  frames("group=3\nname=front of house\nfilter=li");
  line.receive(report);
  frames("nk\nok\n");
  const a = await p;
  eq(a.values, { group: "3", name: "front of house", filter: "link" });
  // two commands queue: the second goes out only after the first's answer
  const p1 = line.send("set filter all"), p2 = line.send("get");
  await tick();
  eq(written.slice(1), ["set filter all\n"]);
  frames("ok\n");
  await p1;
  await tick();
  eq(written.slice(1), ["set filter all\n", "get\n"]);
  frames("err bad setting\n");
  await assert.rejects(p2, /err bad setting/);
  checks++;
  // lines with nobody waiting are dropped; a timeout rejects and the next command still runs
  frames("stray=1\nok\n");
  p = line.send("status");
  await tick();
  timers.fire();
  await assert.rejects(p, /The Jank did not answer/);
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
  const usb = { id: "1", name: "jank", state: "connected" }, named = { id: "2", name: "jank 1A2B", state: "connected" };
  const other = { id: "3", name: "FM1-E040", state: "connected" }, gone = { id: "4", name: "jank", state: "disconnected" };
  const polyfill = { forEach(f) { [other, named, usb, gone].forEach((p, i) => f(p, String(i))); } };
  eq(box.portList(polyfill).map((p) => p.id), ["3", "2", "1"]);
  eq(box.portList(new Map([["a", usb], ["b", gone]])).map((p) => p.id), ["1"]);
  eq(box.portList(undefined), []);
  eq(box.jankPorts(box.portList(polyfill)).map((p) => p.name), ["jank", "jank 1A2B"]);   // USB first
  eq(["jank", "jank 1A2B", "jank (USB)", "jank1A2B", "My jank", "JANK"].map(box.jankPortName), [true, true, true, true, true, true]);
  eq(["Digitone Audio+MIDI Elektron Digitone II", "FM1-E040", "Jankowski", "jankins", "IAC Driver Bus 1", "", undefined].map(box.jankPortName), [false, false, false, false, false, false, false]);
  eq(box.jankPorts([other, { name: "Digitone" }]), []);
  eq(box.portNames([other, usb]) + "|" + box.portNames([]), "FM1-E040, jank|none");
  eq(box.outputFor(named, [usb, named]).id, "2");
  eq(box.outputFor({ name: "IAC Driver Bus 1" }, [other, usb]).id, "1");
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
eq(box.networkLine({ net: "jank", group: "2" }), "MODE JANK, group 2");
eq(box.networkLine({ net: "wifi", ssid: "Home" }), "Wi-Fi network Home");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "0" }), "Looking for Home");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "1", wifi_ip: "0.0.0.0" }), "On Home, waiting for an address");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "1", wifi_ip: "192.168.1.5", wifi_rssi: "-50", wifi_channel: "6" }), "On Home, 192.168.1.5, -50 dBm, channel 6");
// several saved networks (owner 2026-11-25): get lists them by name, status says the one joined
eq(box.savedNetworks({ nets: "0" }), []);
eq(box.savedNetworks({ ssid: "Home" }), []);                                   // a firmware without the list
eq(box.savedNetworks({ nets: "2", net_1: "Home", net_2: "Office Guests" }), ["Home", "Office Guests"]);
eq(box.networkLine({ net: "wifi", ssid: "Home", nets: "2", net_1: "Home", net_2: "Office Guests", wifi: "0" }), "Looking for a saved network");
eq(box.networkLine({ net: "wifi", ssid: "Home", nets: "1", net_1: "Home", wifi: "0" }), "Looking for Home");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi_ssid: "Office Guests", wifi: "1", wifi_ip: "10.0.0.7", wifi_rssi: "-61", wifi_channel: "11" }), "On Office Guests, 10.0.0.7, -61 dBm, channel 11");
eq(box.splitLines("", "ab"), { lines: [], rest: "ab" });
eq(box.splitLines("ab", "c\nd"), { lines: ["abc"], rest: "d" });
eq(box.splitLines("", "a\r\nb\n"), { lines: ["a", "b"], rest: "" });
{
  const a = box.parseAnswer(["group=3", "name=front of house", "peer=8c:aa:bb:cc:dd:ee,-44,100,5", "peer=8c:aa:bb:cc:dd:ef,-60,20,1", "not a pair"]);
  eq(a.values, { group: "3", name: "front of house" });
  eq(a.units[0], { mac: "8c:aa:bb:cc:dd:ee", rssi: -44, age: 100, frames: 5 });
  eq(box.parseAnswer(["peer=on"]).values.peer, "on");
}
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
eq(box.setCommand("ble", "auto"), null);              // gone: the control line has no such setting
eq(box.setCommand("pass", "secret12"), null);         // not through the settings form

// --- tooltips: every control the user can change says what it changes ---
{
  const VOID = new Set(["input", "br", "img", "meta", "link", "hr", "path", "rect", "circle", "line", "use", "stop", "text"]);
  const body = html.replace(/<!--[\s\S]*?-->/g, "").replace(/<(script|style)[\s\S]*?<\/\1>/g, "");
  const attrsOf = (s) => { const out = {}; s.replace(/([a-zA-Z-]+)(?:="([^"]*)")?/g, (m, k, v) => { out[k] = v === undefined ? "" : v; return m; }); return out; };
  const stack = [], bare = [], titles = [];
  for (const m of body.matchAll(/<(\/?)([a-zA-Z0-9]+)([^>]*)>/g)) {
    const [, close, name, rest] = m, tag = name.toLowerCase();
    if (close) { while (stack.length && stack.pop().tag !== tag); continue; }
    const a = attrsOf(rest), tip = (a.title || a["aria-label"] || "").trim();
    if (a.title !== undefined) titles.push([a.id || tag, a.title]);
    const interactive = ["button", "select", "textarea", "summary"].includes(tag) || (tag === "input" && a.type !== "hidden") || a.role === "button" || (a.tabindex !== undefined && a.tabindex !== "-1");
    if (interactive && !tip && !stack.some((s) => s.tip)) bare.push(a.id || tag);
    if (!VOID.has(tag) && !/\/\s*$/.test(rest)) stack.push({ tag, tip });
  }
  // exceptions: none. (A tabindex of -1, the player card's, is a programmatic focus target, not a control.)
  eq(bare, []);
  ok(titles.length > 40, "the page carries its tooltips (" + titles.length + ")");
  eq(titles.filter(([, v]) => !v.trim() || v.length > 240).map(([id, v]) => id + " (" + v.length + ")"), []);
  const tipOf = (id) => (html.match(new RegExp('<[a-z]+ [^>]*id="' + id + '"[^>]*>')) || [""])[0];
  for (const id of ["c-midi", "pl-play", "pl-vol", "bd-slider", "al-on", "net-apply", "bootloader"]) ok(/\stitle="[^"]+"/.test(tipOf(id)), id + " has a tooltip of its own");
  ok(/Pitch[^"]*tempo the device tracks/.test(tipOf("pl-pitch")) && /mutes only your speakers/.test(html) && /restarts it/.test(tipOf("net-apply")), "the tooltips carry the side effects: pitch moves the tracked tempo, monitor off mutes only the speakers, Save and restart restarts the device");
}

// the ABOUT screen's numbers: `about` is asked only from the draw loop while that screen is open (no timer of its own), at most every 1.5 s, one at a time
ok(/if \(Stick\.aboutOpen\(model\)\) pollAbout\(\);/.test(html) && !/setInterval\(pollAbout/.test(html), "about is polled only while the ABOUT screen is shown");
ok(/send\("about"\)/.test(html) && (html.match(/send\("about"\)/g) || []).length === 1 && /S\.aboutBusy \|\| S\.aboutOff \|\| now - S\.aboutTried < ABOUT_EVERY_MS/.test(html), "one about request at a time, spaced by ABOUT_EVERY_MS, off for a jank without it");
ok(/about: S\.lastAbout, aboutAt: S\.aboutAt/.test(html), "the face's state carries the about reply");

lineTests().then(() => console.log("page: " + checks + " checks passed"), (e) => { console.error(e); process.exit(1); });
