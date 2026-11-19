// The reference for the lcd2 layout (ctx.layout 'lcd2': its own 320 x 240 grid) and its ink plane: ui/oled.js's own draw(), run in node, for
// every screen the layout has. Each frame is the 1-bit frame in the page layout (byte (y >> 3) * 320 + x, bit y & 7) as runs of one byte, "hh:length,...", and the ink
// plane as runs of one id 0..5 per pixel, "id:length,..." (y * 320 + x, OLED.INK). test_oled.c replays each case in C (jo_press and
// jo_draw_lcd2) and must draw both exactly, and log the same setter calls. The QR encoder is the one the OLED parity uses (beat/live/player.html).
// Usage: node tests/oled_ref/gen_cases.js --ink > tests/oled_ref/cases_ink.txt
'use strict';
const fs = require('fs');
const path = require('path');
const { baseState, dumpState, hex, ABOUT_FULL, ABOUT_NONE } = require('./state.js');
const root = path.join(__dirname, '..', '..');
const src = fs.readFileSync(path.join(root, 'ui', 'bigfont.js'), 'utf8') + fs.readFileSync(path.join(root, 'ui', 'oled.js'), 'utf8');
const OLED = new Function(src + '; return OLED;')();
const player = fs.readFileSync(path.join(root, 'beat', 'live', 'player.html'), 'utf8');
const QR = new Function(player.match(/const QR = \(\(\) => \{[\s\S]*?\n\}\)\(\);\n/)[0] + '; return QR;')();
const { w: W, h: H } = OLED.size('lcd2');

const out = [];
const B = { up: 'U', down: 'D', no: 'N', yes: 'Y' };
let ncase = 0, nframes = 0, ninkpx = 0;

function extraState(s) {
  const num = v => (v === null || v === undefined ? 'null' : String(v));
  out.push('c layout 1');
  out.push('c rssi ' + num(s.rssi));
  out.push('c peakDb ' + num(s.peakDb));
  for (const k of ['airLost', 'airRetry', 'rssiMin', 'rssiMax']) out.push('c ' + k + ' ' + num(s[k]));
  for (const k of ['confKnown', 'silent', 'gated', 'confHold']) out.push('c ' + k + ' ' + (s[k] ? 1 : 0));
  if (s.confGraded) { out.push('c confGraded 1'); out.push('c confGate ' + s.confGate); }     // (only when set: the other cases keep their lines)
  out.push('c autoBar ' + OLED.autoBarIndex(s.autoBar));
  out.push('c fold ' + s.fold);
  out.push('c usbMidi ' + s.usbMidi);
}

// the runs of the plane: "id:length,id:length,..." in pixel order
function runs(ink) {
  const r = [];
  for (let i = 0; i < ink.length;) { let j = i; while (j < ink.length && ink[j] === ink[i]) j++; r.push(ink[i] + ':' + (j - i)); i = j; }
  return r.join(',');
}

// the frame as runs of one byte, "hh:length,..." in page order (the OLED cases keep plain hex: only this file's frames are runs, to keep it small)
function pageRuns(fb) {
  const pages = Buffer.from(hex(fb, W, H), 'hex'), r = [];
  for (let i = 0; i < pages.length;) { let j = i; while (j < pages.length && pages[j] === pages[i]) j++; r.push(pages[i].toString(16).padStart(2, '0') + ':' + (j - i)); i = j; }
  return r.join(',');
}

// the setters, as test_oled.c's world has them: the state they change, and the log of calls
function setters(s, log) {
  const n = s.net;
  const setField = (f, v) => {
    log.push('set ' + f + ' ' + v);
    if (f === 'threshold') s.th = v; else if (f === 'lowconf') s.confStop = v === 1; else if (f === 'ltmode') s.ltMode = v;
    else if (f === 'gain') s.gain = v; else if (f === 'bright') s.bright = v; else if (f === 'sleep') s.sleep = v;
    else if (f === 'inoff') s.inOff = v; else if (f === 'out0') s.offs[0] = v; else if (f === 'out1') s.offs[1] = v;
    else if (f === 'out2') s.offs[2] = v; else if (f === 'rate') s.rate = v;
    else if (f === 'netmode') { if (v === 3) { if (n.mode !== 3) n.prev = n.mode; n.mode = 3; } else n.mode = v; }
    else if (f === 'group') n.group = v + 1; else if (f === 'insrc') s.inSrc = v; else if (f === 'usbrole') s.usbRole = v;
    else if (f === 'autobar') s.autoBar = v; else if (f === 'usbmidi') s.usbMidi = v;
    else if (f === 'ssid') n.ssid = v; else if (f === 'pass') n.pass = v;
    else if (f === 'appass') n.appass = v; else if (f === 'apname') n.apName = v;
    else throw new Error('field ' + f);
  };
  const tempoStep = d => {
    log.push('tempo ' + d);
    if (s.link) { if (s.linkBpm == null) return null; s.linkBpm = Math.max(90, Math.min(180, Math.round(s.linkBpm) + d)); return s.linkBpm; }
    s.bpm = s.bpm ? (d > 0 ? s.bpm * 2 : s.bpm / 2) : 0;
    return s.bpm;
  };
  return { setField, tempoStep, setHold: on => { log.push('hold ' + (on ? 1 : 0)); s.running = !on; s.held = !!on; }, linkStartStop: on => { log.push('lss ' + (on ? 1 : 0)); s.running = on; s.held = !on; } };
}

// one case: a state, then steps ['draw', t] | ['press', btn, t]
function scenario(name, edit, steps) {
  const s = Object.assign(baseState(), { rssi: -55, airLost: null, airRetry: null, rssiMin: 0, rssiMax: 0, peakDb: -12, confKnown: true, silent: false, gated: false, confHold: false, fold: 1,
                                         usbMidi: 0, beatN: 0 });
  if (edit) edit(s);
  const log = [], set = setters(s, log);
  const ctx = () => Object.assign({}, s, { layout: 'lcd2', setup: s.net.mode === 3, qr: QR.encode('WIFI:S:' + s.net.setupName + ';T:nopass;;') }, set);
  const ui = OLED.create();
  ui.hint = null;
  out.push('case ' + name);
  dumpState(out, s);
  extraState(s);
  for (const [op, a, b] of steps) {
    if (op === 'draw') {
      const fb = new Uint8Array(W * H), ink = new Uint8Array(W * H);
      OLED.draw(fb, ui, ctx(), a, ink);
      out.push('draw ' + a + ' ' + pageRuns(fb) + ' ' + runs(ink));
      nframes++;
      ninkpx += ink.reduce((k, v) => k + (v ? 1 : 0), 0);
    } else if (op === 'press') {
      OLED.press(ui, a, b, ctx());
      out.push('press ' + B[a] + ' ' + b);
    } else throw new Error(op);
  }
  for (const l of log) out.push('log ' + l);
  out.push('end');
  ncase++;
}
const draw = t => ['draw', t || 0];
const presses = (btns, t0 = 0) => btns.map((b, i) => ['press', b, t0 + i]);
const walk = (btns, t0 = 0) => btns.flatMap((b, i) => [['press', b, t0 + i * 10], draw(t0 + i * 10 + 5)]);
const audio = (extra) => s => { Object.assign(s, extra); };
const link = (extra) => s => { Object.assign(s, { link: true, lineIn: false, linkBpm: 120 }, extra); };

// ---- HOME (AUDIO): the confidence line against the stop threshold (70), value, line and notch ----
for (const v of [60, 69, 70, 75, 80, 81, 87, 100]) scenario('cf ' + v + ' th 70', audio({ conf: v / 100, th: 70 }), [draw(0)]);
for (const v of [0, 0.5, 1]) scenario('cf line ' + v, audio({ conf: v, th: 70 }), [draw(0)]);
scenario('cf 75 th 50', audio({ conf: 0.75, th: 50 }), [draw(0)]);
scenario('cf 75 th 90 keep running', audio({ conf: 0.75, th: 90, confStop: false }), [draw(0)]);
scenario('cf th 0', audio({ conf: 0.01, th: 0 }), [draw(0)]);
scenario('cf th 100', audio({ conf: 0.99, th: 100 }), [draw(0)]);
scenario('cf rounding 69.5', audio({ conf: 0.695, th: 70 }), [draw(0)]);
scenario('cf unknown', audio({ confKnown: false, conf: 0 }), [draw(0)]);
scenario('cf silent', audio({ silent: true, conf: 0.3 }), [draw(0)]);
scenario('cf gated', audio({ gated: true, conf: 0.4 }), [draw(0)]);
scenario('cf hold', audio({ confHold: true, conf: 0.55 }), [draw(0)]);
scenario('cf coast', audio({ conf: 0.5, running: true }), [draw(0)]);
// the graded value is shown (confGraded): no notch; the colour, COAST and the state word judge the gate's own value (confGate)
scenario('cf graded 40 gate 90', audio({ conf: 0.4, confGraded: true, confGate: 0.9, th: 70, running: true }), [draw(0)]);
scenario('cf graded 95 gate 50', audio({ conf: 0.95, confGraded: true, confGate: 0.5, th: 70, running: true }), [draw(0)]);
scenario('cf graded 75 gate 75 amber', audio({ conf: 0.75, confGraded: true, confGate: 0.75, th: 70, running: true }), [draw(0)]);
scenario('cf graded silent', audio({ conf: 0.3, confGraded: true, confGate: 0.9, silent: true }), [draw(0)]);
scenario('cf low but stopped', audio({ conf: 0.5, running: false, armed: false, beatN: null }), [draw(0)]);
scenario('cf gated beats hold', audio({ gated: true, confHold: true, conf: 0.5 }), [draw(0)]);
scenario('cf coast unknown conf', audio({ confKnown: false, conf: 0.2, running: true }), [draw(0)]);

// ---- the input level line: the notch at -50 dBFS, clipping, too quiet ----
for (const db of [-90, -75, -60, -50, -45, -30, -20, -12, -3, 0]) scenario('in ' + db, audio({ levelDb: db, levelMin: db - 5, levelMax: db + 2, peakDb: -20 }), [draw(0)]);
scenario('in above 0', audio({ levelDb: 6, levelMax: 6, peakDb: -20 }), [draw(0)]);
scenario('in below -90', audio({ levelDb: -120, levelMin: -120, levelMax: -120, peakDb: -99 }), [draw(0)]);
for (const pk of [-0.5, -1, -1.5]) scenario('peak ' + pk, audio({ peakDb: pk }), [draw(0)]);
scenario('peak unknown rms max -2', audio({ peakDb: null, levelMax: -2 }), [draw(0)]);
scenario('peak unknown rms max -10', audio({ peakDb: null, levelMax: -10 }), [draw(0)]);
scenario('too quiet -55', audio({ levelDb: -55, levelMin: -60, levelMax: -52, peakDb: -40 }), [draw(0)]);
scenario('quiet edge -50', audio({ levelDb: -50, levelMin: -60, levelMax: -45, peakDb: -40 }), [draw(0)]);
scenario('clip beats quiet', audio({ levelDb: -60, peakDb: 0 }), [draw(0)]);
scenario('level -0.4 rounds to 0', audio({ levelDb: -0.4, peakDb: -5 }), [draw(0)]);

// ---- the beat boxes: each of the four lit, both layouts; the first is amber only while lit ----
for (const n of [0, 1, 2, 3, 4, 7, -1, -4, null]) scenario('beat ' + n, audio({ beatN: n }), [draw(0)]);
for (const n of [0, 1, 2, 3, null]) scenario('link beat ' + n, link({ beatN: n }), [draw(0)]);

// ---- the BAR row ----
for (const state of ['off', 'listening', 'locked', 'manual'])
  for (const conf of [0, 0.5, 1]) scenario('bar ' + state + ' ' + conf, audio({ bar: { state, conf } }), [draw(0)]);
scenario('bar none', audio({ bar: null }), [draw(0)]);
scenario('bar conf above 1', audio({ bar: { state: 'locked', conf: 1.4 } }), [draw(0)]);

// ---- the tempo: the big digits, centred, the small digit after the dot; both layouts ----
for (const b of [120, 98.4, 88.8, 146.1, 133.3, 99.95, 999.9, 5, 0.04, 87.5, 100, 111.1]) {
  scenario('bpm ' + b, audio({ bpm: b }), [draw(0)]);
  scenario('link bpm ' + b, link({ linkBpm: b }), [draw(0)]);
}
scenario('bpm none', audio({ bpm: null }), [draw(0)]);
scenario('bpm zero', audio({ bpm: 0 }), [draw(0)]);
scenario('link bpm none', link({ linkBpm: null }), [draw(0)]);
scenario('link bpm 174.96', link({ linkBpm: 174.96 }), [draw(0)]);

// ---- the state word (AUDIO): each word, and the order they win in ----
scenario('state running', audio({ conf: 0.9 }), [draw(0)]);
scenario('state gated', audio({ gated: true, conf: 0.4 }), [draw(0)]);
scenario('state hold', audio({ confHold: true, conf: 0.55 }), [draw(0)]);
scenario('state coast', audio({ conf: 0.5, running: true }), [draw(0)]);
scenario('state stopped', audio({ running: false, beatN: null }), [draw(0)]);
scenario('state silent', audio({ silent: true, conf: 0.9 }), [draw(0)]);
scenario('state gated beats hold', audio({ gated: true, confHold: true, conf: 0.5 }), [draw(0)]);
scenario('state hold beats coast', audio({ confHold: true, conf: 0.2 }), [draw(0)]);
scenario('state coast beats silent', audio({ silent: true, conf: 0.2 }), [draw(0)]);
scenario('state stopped beats silent', audio({ silent: true, running: false, conf: 0.9 }), [draw(0)]);
scenario('state coast unknown conf', audio({ confKnown: false, conf: 0.2, running: true }), [draw(0)]);
scenario('state armed', audio({ running: false, armed: true, beatN: null }), [draw(0), draw(400)]);

// ---- the top bar: transport, fold, link glyph and peers, signal, plug, battery ----
scenario('transport running', audio({}), [draw(0)]);
scenario('transport armed', audio({ running: false, armed: true, beatN: null }), [draw(0), draw(400)]);
scenario('transport paused (started, not running)', audio({ running: false, beatN: null }), [draw(0)]);
scenario('transport stopped (user hold)', audio({ running: false, held: true, beatN: null }), [draw(0)]);
scenario('link transport paused', link({ running: false, beatN: null }), [draw(0)]);
scenario('link transport stopped (user hold)', link({ running: false, held: true, beatN: null }), [draw(0)]);
scenario('armed blink', audio({ running: false, armed: true, beatN: null }), [draw(0), draw(400), draw(799), draw(1200)]);
for (const f of [2, 4, 0.5, 0.25]) scenario('fold ' + f, audio({ fold: f }), [draw(0)]);
for (const p of [0, 1, 2, 12]) {
  scenario('audio peers ' + p, audio({ peers: p }), [draw(0)]);
  scenario('link peers ' + p, link({ peers: p }), [draw(0)]);
}
for (const b of [0, 12, 14, 15, 20, 29, 30, 50, 80, 99, 100]) {
  scenario('battery ' + b + ' charging', audio({ batt: b }), [draw(0)]);
  scenario('battery ' + b + ' on cell', audio({ batt: b, usb: false }), [draw(0)]);
}
scenario('no battery no usb', audio({ fitted: false, usb: false }), [draw(0)]);
for (const lt of [0, 1, 2]) scenario('audio lt ' + lt, audio({ ltMode: lt, fold: 2 }), [draw(0)]);
for (const lt of [0, 1, 2]) scenario('link lt ' + lt, link({ ltMode: lt, net: Object.assign(baseState().net, { mode: 1, ssid: 'Studio WiFi' }) }), [draw(0)]);
scenario('client ssid', audio({ net: Object.assign(baseState().net, { mode: 1, ssid: 'HomeNet' }) }), [draw(0)]);
scenario('client long ssid', audio({ net: Object.assign(baseState().net, { mode: 1, ssid: 'A Very Long Network Name Indeed' }) }), [draw(0)]);
scenario('client long ssid with fold', audio({ fold: 2, net: Object.assign(baseState().net, { mode: 1, ssid: 'A Very Long Network Name Indeed' }) }), [draw(0)]);
scenario('client no ssid', audio({ net: Object.assign(baseState().net, { mode: 1, ssid: '' }) }), [draw(0)]);
scenario('ap name', audio({ net: Object.assign(baseState().net, { mode: 2, apName: 'jank-a3f9' }) }), [draw(0)]);
scenario('group 9', audio({ net: Object.assign(baseState().net, { group: 9 }) }), [draw(0)]);

// ---- overlays, boxes, boot, splash ----
scenario('overlay double', audio({ bpm: 87.5 }), [['press', 'up', 0], draw(1), draw(999), draw(1200)]);
scenario('overlay half', audio({ bpm: 87.5 }), [['press', 'down', 0], draw(1)]);
scenario('overlay link tempo', link({ linkBpm: 120.4, peers: 2 }), [['press', 'up', 0], draw(10), ['press', 'down', 20], draw(30)]);
scenario('overlay link tempo none', link({ linkBpm: null }), [['press', 'up', 0], draw(1)]);
scenario('overlay long value', audio({ bpm: 8888.8 }), [['press', 'up', 0], draw(1)]);
scenario('saved box', audio({ saved: true }), [draw(0)]);
scenario('boot', audio({ boot: true }), [draw(0)]);
scenario('splash', audio({ splash: true }), [draw(0)]);

// ---- MENU: the 11 items, every row, every value ----
const downAll = Array(10).fill('down');
scenario('menu', null, [['press', 'no', 0], draw(1)]);
scenario('menu walk', null, [['press', 'no', 0], draw(1), ...walk(downAll, 10), ...walk(['down', 'up', 'up', 'up', 'up', 'up', 'up', 'up', 'up', 'up', 'up', 'up'], 200)]);
scenario('menu values', audio({ ltMode: 0, autoBar: 0, net: Object.assign(baseState().net, { mode: 1 }), usbRole: 1, usbMidi: 1, rate: 4, fitted: false }),
         [['press', 'no', 0], draw(1)]);
scenario('menu values 2', audio({ ltMode: 1, autoBar: 2, net: Object.assign(baseState().net, { mode: 2 }), rate: 0, batt: 7 }),
         [['press', 'no', 0], draw(1)]);
scenario('menu back home', null, [['press', 'no', 0], ['press', 'no', 1], draw(2)]);

// ---- the sub menus and every edit screen ----
const toMenu = ['no'];
const menuItem = i => [...toMenu, ...Array(i).fill('down')];
scenario('sub audio', audio({ gain: -7, th: 35 }), [...presses(['no', 'yes']), draw(5), ...walk(['down', 'down', 'down', 'down', 'up'], 10)]);
scenario('sub audio keep running', audio({ confStop: false, inSrc: 3, gain: 12 }), [...presses(['no', 'yes']), draw(5), ...walk(['down', 'down'], 10)]);
scenario('sub offsets', audio({ inOff: -100, offs: [50, -1, 0] }), [...presses([...menuItem(6), 'yes']), draw(10), ...walk(['down', 'down', 'down'], 20)]);
scenario('sub display', audio({ bright: 10, sleep: 3 }), [...presses([...menuItem(8), 'yes']), draw(20), ...walk(['down'], 30)]);
scenario('edit usbmidi', null, [...presses([...menuItem(5), 'yes']), draw(10), ...walk(['up', 'up', 'down'], 20), ['press', 'no', 60], draw(61)]);
scenario('edit autobar', null, [...presses([...menuItem(2), 'yes']), draw(10), ...walk(['up', 'up', 'up', 'down', 'down'], 20), ['press', 'yes', 80], draw(81)]);
scenario('edit autobar repick', audio({ autoBar: 2 }), [...presses([...menuItem(2), 'yes', 'yes']), draw(10)]);
scenario('edit insrc', null, [...presses(['no', 'yes', 'yes']), draw(10), ...walk(['up', 'up', 'up', 'up', 'down'], 20), ['press', 'no', 60], draw(61)]);
scenario('edit gain live', audio({ gain: 11 }), [...presses(['no', 'yes', 'down', 'yes']), draw(10), ...walk(['up', 'up', 'down', 'down', 'down'], 20), ['press', 'no', 100], draw(101)]);
scenario('edit threshold', audio({ th: 95 }), [...presses(['no', 'yes', 'down', 'down', 'down', 'yes']), draw(10), ...walk(['up', 'up', 'down'], 20)]);
scenario('edit threshold live green', audio({ th: 50, conf: 0.72 }), [...presses(['no', 'yes', 'down', 'down', 'down', 'yes']), draw(10), ...walk(['up', 'up', 'down'], 20)]);
scenario('edit threshold live red', audio({ th: 90, conf: 0.72 }), [...presses(['no', 'yes', 'down', 'down', 'down', 'yes']), draw(10), ...walk(['down', 'down'], 20)]);
scenario('edit threshold live unknown', audio({ th: 70, confKnown: false }), [...presses(['no', 'yes', 'down', 'down', 'down', 'yes']), draw(10), ...walk(['up'], 20)]);
scenario('edit lowconf wide', null, [...presses(['no', 'yes', 'down', 'down', 'yes']), draw(10), ...walk(['up', 'up'], 20), ['press', 'yes', 50], draw(51)]);
scenario('edit ltmode wide', null, [...presses([...menuItem(1), 'yes']), draw(10), ...walk(['up', 'up', 'up', 'down'], 20), ['press', 'yes', 60], draw(61)]);
scenario('edit usb', null, [...presses([...menuItem(4), 'yes']), draw(10), ...walk(['down', 'down'], 20), ['press', 'yes', 50], draw(51)]);
scenario('edit rate', audio({ rate: 3 }), [...presses([...menuItem(7), 'yes']), draw(10), ...walk(['up', 'up', 'up', 'down'], 20), ['press', 'no', 60], draw(61)]);
scenario('edit offsets', audio({ offs: [49, -49, 0], inOff: 99 }), [...presses([...menuItem(6), 'yes', 'yes']), draw(10), ...walk(['up', 'up', 'down'], 20), ['press', 'yes', 50], draw(51)]);
scenario('edit display', audio({ bright: 90 }), [...presses([...menuItem(8), 'yes', 'yes']), draw(20), ...walk(['up', 'up', 'down', 'down'], 30), ['press', 'no', 80], draw(81)]);
scenario('battery info', null, [...presses([...menuItem(9), 'yes']), draw(20), ['press', 'up', 21], draw(22), ['press', 'yes', 23], draw(24)]);
for (const [b, usb, fitted] of [[100, true, true], [37, false, true], [0, true, true], [64, false, false], [64, true, false], [99, true, true]])
  scenario('battery info ' + b + ' ' + usb + ' ' + fitted, audio({ batt: b, usb, fitted }), [...presses([...menuItem(9), 'yes']), draw(20)]);

// ---- ABOUT: the system rows, 8 of 13 on the colour layout (UP / DOWN scroll), "-" for what is not known ----
scenario('about', audio(ABOUT_FULL), [...presses([...menuItem(10), 'yes']), draw(20), ...walk(Array(7).fill('down'), 30), ['press', 'up', 400], draw(401), ['press', 'yes', 402], draw(403)]);
scenario('about unknown', audio(ABOUT_NONE), [...presses([...menuItem(10), 'yes']), draw(20), ...walk(['down', 'down', 'down', 'down', 'down', 'down'], 30)]);
scenario('about long uptime', audio({ ...ABOUT_FULL, uptimeS: 200000, flashKb: 4096, appUsedKb: null, reset: 'PANIC' }), [...presses([...menuItem(10), 'yes']), draw(20), ...walk(['down', 'down', 'down', 'down'], 30)]);

// ---- NETWORK, the scan list, the character editor, SETUP with its QR code ----
const toNet = [...menuItem(3), 'yes'];
scenario('net jank', null, [...presses(toNet), draw(10), ...walk(['down', 'down', 'down', 'up'], 20)]);
scenario('net jank group', null, [...presses(toNet), ['press', 'down', 10], ['press', 'yes', 11], draw(12), ...walk(['up', 'up', 'down', 'down', 'down'], 20), ['press', 'yes', 80], draw(81)]);
scenario('net client', audio({ net: Object.assign(baseState().net, { mode: 1 }) }), [...presses(toNet), draw(10), ...walk(['down', 'down', 'down', 'down'], 20)]);
scenario('net client connecting', audio({ net: Object.assign(baseState().net, { mode: 1, status: 'CONNECTING', pass: 'abc', ssid: 'A Very Long Network Name' }) }),
         [...presses(toNet), draw(10), ...walk(['down', 'down', 'down', 'yes'], 20), ['press', 'no', 70], draw(71)]);
scenario('net ap', audio({ net: Object.assign(baseState().net, { mode: 2, apClients: 0 }) }), [...presses(toNet), draw(10), ...walk(['down', 'down', 'down', 'yes'], 20)]);
scenario('net type edit', null, [...presses([...toNet, 'yes']), draw(11), ...walk(['up', 'up', 'up', 'up', 'down'], 20), ['press', 'no', 82], draw(83)]);
scenario('net setup confirm', null, [...presses([...toNet, 'yes', 'up']), draw(12), ['press', 'yes', 13], draw(14), ['press', 'yes', 15], draw(16), ['press', 'up', 17], draw(18),
                                     ['press', 'no', 19], draw(20)]);
for (const name of ['jank-setup-a3f9', 'jk', 'jank-setup-0123456789ab'])
  scenario('setup ' + name, audio({ net: Object.assign(baseState().net, { mode: 3, prev: 0, setupName: name }) }), [draw(0)]);
scenario('scan', audio({ net: Object.assign(baseState().net, { mode: 1, scan: [{ name: 'HomeNet', bars: 3 }, { name: 'jank-studio', bars: 2 }, { name: 'Cafe Guest', bars: 1 }, { name: 'FritzBox 7590', bars: 2 },
    { name: 'Neighbour 5G', bars: 0 }, { name: 'xx', bars: 3 }, { name: 'Long Name Number Seven Here', bars: 1 }, { name: 'a', bars: 1 }, { name: 'b', bars: 2 }, { name: 'c', bars: 3 }, { name: 'd', bars: 0 },
    { name: 'e', bars: 1 }, { name: 'f', bars: 2 }, { name: 'g', bars: 3 }] }) }),
         [...presses([...toNet, 'down', 'yes']), draw(12), ...walk(Array(13).fill('down'), 20), ...walk(Array(13).fill('up'), 200), ['press', 'down', 400], ['press', 'yes', 401], draw(402)]);
scenario('scan empty', audio({ net: Object.assign(baseState().net, { mode: 1, scan: [] }) }), [...presses([...toNet, 'down', 'yes']), draw(12), ['press', 'no', 13], draw(14)]);
scenario('text pass', audio({ net: Object.assign(baseState().net, { mode: 1 }) }),
         [...presses([...toNet, 'down', 'down', 'yes']), draw(13), ...walk(['up', 'yes', 'down', 'down', 'yes'], 20), ...walk(Array(8).fill('yes'), 80), ['press', 'no', 200], draw(201)]);
scenario('text apname long', audio({ net: Object.assign(baseState().net, { mode: 2, apName: 'abcdefghijklmnopqrstuvwxyz0123456789' }) }),
         [...presses([...toNet, 'down', 'yes']), draw(12), ...walk(Array(30).fill('yes'), 20), ...walk(['up', 'up', 'down'], 400), ['press', 'down', 600], draw(601)]);
scenario('text appass delete all', audio({ net: Object.assign(baseState().net, { mode: 2, appass: 'ab' }) }),
         [...presses([...toNet, 'down', 'down', 'yes']), draw(13), ...walk(['no', 'no'], 20), ['press', 'no', 50], draw(51)]);
// ---- the NETWORK screen's SIGNAL row: the jank's peer in JANK mode, the router in Wi-Fi mode; known, unknown and none ----
for (const r of [null, 0, -55, -67, -68, -80, -81, -95, 5]) {
  scenario('net signal jank ' + r, audio({ rssi: r }), [...presses(toNet), draw(10)]);
  scenario('net signal client ' + r, audio({ rssi: r, net: Object.assign(baseState().net, { mode: 1 }) }), [...presses(toNet), draw(10), ...walk(['down', 'down'], 20)]);
}
// ---- the air rows under SIGNAL (MODE JANK only): LOST, LEVEL, RX RETRY; known, unknown, the colour thresholds, and no rows in the other modes ----
const air = (lost, retry, lo, hi) => ({ airLost: lost, airRetry: retry, rssiMin: lo, rssiMax: hi });
scenario('net air unknown', audio(air(null, null, 0, 0)), [...presses(toNet), draw(10)]);
scenario('net air known', audio(air(3, 0, -78, -52)), [...presses(toNet), draw(10), ...walk(['down', 'down'], 20)]);
for (const p of [0, 2, 3, 9, 10, 50, 100]) scenario('net air lost ' + p, audio(air(p, 1, -90, -40)), [...presses(toNet), draw(10)]);
for (const p of [0, 1, 29, 30, 100]) scenario('net air retry ' + p, audio(air(1, p, -90, -40)), [...presses(toNet), draw(10)]);
scenario('net air only lost known', audio(air(4, null, 0, 0)), [...presses(toNet), draw(10)]);
scenario('net air only retry known', audio(air(null, 31, -66, -66)), [...presses(toNet), draw(10)]);
scenario('net air widest level', audio(air(12, 35, -100, -1)), [...presses(toNet), draw(10)]);
scenario('net air client none', audio(Object.assign(air(20, 40, -80, -50), { net: Object.assign(baseState().net, { mode: 1 }) })), [...presses(toNet), draw(10)]);
scenario('net air ap none', audio(Object.assign(air(20, 40, -80, -50), { net: Object.assign(baseState().net, { mode: 2 }) })), [...presses(toNet), draw(10)]);
scenario('net signal ap', audio({ rssi: -70, net: Object.assign(baseState().net, { mode: 2 }) }), [...presses(toNet), draw(10)]);
scenario('netinfo client', audio({ net: Object.assign(baseState().net, { mode: 1 }) }), [...presses([...toNet, 'down', 'down', 'down', 'yes']), draw(20)]);
scenario('netinfo ap', audio({ net: Object.assign(baseState().net, { mode: 2 }) }), [...presses([...toNet, 'down', 'down', 'down', 'yes']), draw(20)]);

process.stdout.write('# generated by tests/oled_ref/gen_ink.js from ui/bigfont.js + ui/oled.js sha1 ' + require('crypto').createHash('sha1').update(src).digest('hex') +
                     ': ' + ncase + ' cases, ' + nframes + ' frames, ' + ninkpx + ' ink pixels\n' + out.join('\n') + '\n');
process.stderr.write(ncase + ' cases, ' + nframes + ' frames, ' + ninkpx + ' ink pixels\n');
