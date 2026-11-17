// The reference for the colour layout (ctx.layout 'lcd2') and its ink plane: ui/oled.js's own draw(), run in node, for the HOME
// states the firmware shows in colour and for screens that must stay without ink. Each frame is the 1-bit frame in the SSD1309's
// page layout (hex, as cases.txt) and the ink plane, as runs of one id 0..5 per pixel, "id:length,..." (y * 128 + x, OLED.INK). test_oled.c replays
// each case in C (jo_draw_ink) and must draw both exactly. A frame written as '-' is not compared: the screen is one where ui/oled.js
// and the C port (a port of player.html's OLED) differ for reasons that are not the colour layout (the menu's items), and only the
// ink is: it must be empty. Usage: node tests/oled_ref/gen_cases.js --ink > tests/oled_ref/cases_ink.txt
'use strict';
const fs = require('fs');
const path = require('path');
const { baseState, dumpState, hex } = require('./state.js');
const src = fs.readFileSync(path.join(__dirname, '..', '..', 'ui', 'oled.js'), 'utf8');
const OLED = new Function(src + '; return OLED;')();
const W = OLED.W, H = OLED.H;

const out = [];
const B = { up: 'U', down: 'D', no: 'N', yes: 'Y' };
let ncase = 0, nframes = 0, ninkpx = 0;

function inkState(s) {
  const num = v => (v === null || v === undefined ? 'null' : String(v));
  out.push('c layout 1');
  out.push('c rssi ' + num(s.rssi));
  out.push('c peakDb ' + num(s.peakDb));
  for (const k of ['confKnown', 'silent', 'gated', 'confHold']) out.push('c ' + k + ' ' + (s[k] ? 1 : 0));
}

// the plane as runs: "id:length,id:length,..." in pixel order
function runs(ink) {
  const r = [];
  for (let i = 0; i < ink.length;) { let j = i; while (j < ink.length && ink[j] === ink[i]) j++; r.push(ink[i] + ':' + (j - i)); i = j; }
  return r.join(',');
}

// one case: a state, then steps ['draw', t] | ['press', btn, t]
function scenario(name, edit, steps) {
  const s = Object.assign(baseState(), { rssi: -55, peakDb: -12, confKnown: true, silent: false, gated: false, confHold: false });
  if (edit) edit(s);
  const log = [];
  const tempoStep = d => {
    log.push('tempo ' + d);
    if (s.link) { if (s.linkBpm == null) return null; s.linkBpm = Math.max(90, Math.min(180, Math.round(s.linkBpm) + d)); return s.linkBpm; }
    s.bpm = s.bpm ? (d > 0 ? s.bpm * 2 : s.bpm / 2) : 0;
    return s.bpm;
  };
  const ctx = () => Object.assign({}, s, { layout: 'lcd2', ble: { mode: 2, left: null, phone: false }, usbMidi: 0, setup: false, qr: null, tempoStep,
                                          setField() {}, setHold() {}, linkStartStop() {} });
  const ui = OLED.create();
  ui.hint = null;
  out.push('case ' + name);
  dumpState(out, s);
  inkState(s);
  for (const [op, a, b] of steps) {
    if (op === 'draw') {
      const fb = new Uint8Array(W * H), ink = new Uint8Array(W * H);
      OLED.draw(fb, ui, ctx(), a, ink);
      out.push('draw ' + a + ' ' + (ui.screen === 'home' ? hex(fb, W, H) : '-') + ' ' + runs(ink));
      nframes++;
      ninkpx += ink.reduce((n, v) => n + (v ? 1 : 0), 0);
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

// ---- the confidence number against the stop threshold (70): below red, up to 10 above amber, above green ----
for (const v of [60, 69, 70, 75, 80, 81, 87, 100]) scenario('cf ' + v + ' th 70', s => { s.conf = v / 100; s.th = 70; }, [draw(0)]);
scenario('cf 75 th 50', s => { s.conf = 0.75; s.th = 50; }, [draw(0)]);
scenario('cf 75 th 90 keep running', s => { s.conf = 0.75; s.th = 90; s.confStop = false; }, [draw(0)]);
scenario('cf 0 and 100 edges', s => { s.conf = 0; s.th = 0; }, [draw(0)]);
scenario('cf rounding 69.5', s => { s.conf = 0.695; s.th = 70; }, [draw(0)]);
scenario('cf unknown', s => { s.confKnown = false; s.conf = 0; }, [draw(0)]);
scenario('cf silent', s => { s.silent = true; s.conf = 0.3; }, [draw(0)]);
scenario('cf gated', s => { s.gated = true; s.conf = 0.4; }, [draw(0)]);
scenario('cf hold', s => { s.confHold = true; s.conf = 0.55; }, [draw(0)]);
scenario('cf coast', s => { s.conf = 0.5; s.running = true; }, [draw(0)]);
scenario('cf low but stopped', s => { s.conf = 0.5; s.running = false; s.armed = false; s.beatN = null; }, [draw(0)]);
scenario('cf gated beats hold', s => { s.gated = true; s.confHold = true; s.conf = 0.5; }, [draw(0)]);

// ---- link glyph and peer count ----
scenario('audio alone', s => { s.peers = 0; }, [draw(0)]);
scenario('link alone', s => { s.link = true; s.lineIn = false; s.peers = 0; s.linkBpm = 120; }, [draw(0)]);
scenario('link one peer', s => { s.link = true; s.lineIn = false; s.peers = 1; s.linkBpm = 120; beat(s, 2); }, [draw(0)]);
scenario('audio joined 2', s => { s.peers = 2; }, [draw(0)]);
scenario('audio joined 12', s => { s.peers = 12; }, [draw(0)]);
scenario('link no tempo', s => { s.link = true; s.lineIn = false; s.linkBpm = null; s.peers = 3; s.running = false; s.beatN = null; }, [draw(0)]);
function beat(s, n) { s.beatN = n; }

// ---- signal ----
for (const r of [null, 0, -55, -67, -68, -80, -81, -95]) scenario('signal ' + r, s => { s.rssi = r; }, [draw(0)]);

// ---- battery ----
for (const b of [0, 12, 14, 15, 20, 29, 30, 80, 100]) scenario('battery ' + b, s => { s.batt = b; }, [draw(0)]);
scenario('no battery no usb', s => { s.fitted = false; s.usb = false; }, [draw(0)]);

// ---- input level: clipping and too quiet ----
scenario('level good', s => { s.levelDb = -20; s.peakDb = -12; }, [draw(0)]);
for (const pk of [-0.5, -1, -1.5]) scenario('peak ' + pk, s => { s.peakDb = pk; }, [draw(0)]);
scenario('peak unknown rms max -2', s => { s.peakDb = null; s.levelMax = -2; }, [draw(0)]);
scenario('peak unknown rms max -10', s => { s.peakDb = null; s.levelMax = -10; }, [draw(0)]);
scenario('too quiet -55', s => { s.levelDb = -55; s.levelMin = -60; s.levelMax = -52; s.peakDb = -40; }, [draw(0)]);
scenario('quiet edge -50', s => { s.levelDb = -50; s.levelMin = -60; s.levelMax = -45; s.peakDb = -40; }, [draw(0)]);
scenario('clip beats quiet', s => { s.levelDb = -60; s.peakDb = 0; }, [draw(0)]);
scenario('level silent -120', s => { s.levelDb = -120; s.levelMin = -120; s.levelMax = -120; s.peakDb = -99; }, [draw(0)]);
scenario('level -100 three digits', s => { s.levelDb = -100; s.levelMin = -110; s.levelMax = -90; s.peakDb = -95; }, [draw(0)]);
scenario('level 0', s => { s.levelDb = 0; s.levelMin = -3; s.levelMax = 0; s.peakDb = -2; }, [draw(0)]);
scenario('level -0.4 rounds to 0', s => { s.levelDb = -0.4; s.peakDb = -5; }, [draw(0)]);
scenario('level full', s => { s.levelDb = 0; s.levelMin = -3; s.levelMax = 0; s.peakDb = 0; }, [draw(0)]);

// ---- the first beat's box: amber only while it is the lit one ----
for (const n of [0, 1, 2, 3, 4, -4, null]) scenario('beat ' + n, s => { s.beatN = n; }, [draw(0)]);
for (const n of [0, 2]) scenario('link beat ' + n, s => { s.link = true; s.lineIn = false; s.linkBpm = 100; s.beatN = n; }, [draw(0)]);
scenario('bar listening no line', s => { s.bar = { state: 'listening', conf: 0.6 }; }, [draw(0)]);
scenario('bar manual', s => { s.bar = { state: 'manual', conf: 0 }; }, [draw(0)]);
scenario('bar off', s => { s.bar = { state: 'off', conf: 1 }; }, [draw(0)]);
scenario('bar locked marks beat 0', s => { s.beatN = 0; s.bar = { state: 'locked', conf: 0.5 }; }, [draw(0)]);

// ---- the status line: the Wi-Fi quality in one line ----
scenario('wifiq good', s => { s.wifiQ = { rssi: -58, kbps: 24000, tx: 400, retried: 12, lost: 0 }; }, [draw(0)]);
scenario('wifiq bad all', s => { s.wifiQ = { rssi: -85, kbps: 1000, tx: 400, retried: 160, lost: 48 }; }, [draw(0)]);
scenario('wifiq no signal', s => { s.wifiQ = { rssi: 0, kbps: 0, tx: 0, retried: 0, lost: 0 }; }, [draw(0)]);
scenario('wifiq no counts', s => { s.wifiQ = { rssi: -60, kbps: 1000, tx: 0, retried: 0, lost: 0 }; }, [draw(0)]);
scenario('wifiq edges', s => { s.wifiQ = { rssi: -80, kbps: 54000, tx: 100, retried: 30, lost: 9 }; }, [draw(0)]);
scenario('wifiq widest', s => { s.wifiQ = { rssi: -100, kbps: 72000, tx: 100, retried: 99, lost: 99 }; }, [draw(0)]);
scenario('link wifiq', s => { s.link = true; s.lineIn = false; s.linkBpm = 110; s.wifiQ = { rssi: -72, kbps: 1000, tx: 100, retried: 5, lost: 1 }; }, [draw(0)]);

// ---- the top bar: names, icons, transport ----
scenario('client ssid', s => { s.net.mode = 1; s.net.ssid = 'HomeNet'; }, [draw(0)]);
scenario('client long ssid', s => { s.net.mode = 1; s.net.ssid = 'A Very Long Network Name Indeed'; }, [draw(0)]);
scenario('ap name', s => { s.net.mode = 2; s.net.apName = 'jank-a3f9'; }, [draw(0)]);
scenario('group 9', s => { s.net.group = 9; }, [draw(0)]);
scenario('link lt icons', s => { s.link = true; s.lineIn = false; s.ltMode = 1; s.linkBpm = 120; s.net.mode = 1; s.net.ssid = 'Studio WiFi'; }, [draw(0)]);
scenario('stopped no usb', s => { s.running = false; s.beatN = null; s.usb = false; }, [draw(0)]);
scenario('armed blink', s => { s.running = false; s.armed = true; s.beatN = null; }, [draw(0), draw(400)]);

// ---- overlays and boxes: no ink under them ----
scenario('overlay double', s => { s.bpm = 87.5; }, [['press', 'up', 0], draw(1), draw(1200)]);
scenario('overlay link tempo', s => { s.link = true; s.lineIn = false; s.linkBpm = 120.4; s.peers = 2; }, [['press', 'up', 0], draw(10)]);
scenario('saved box', s => { s.saved = true; }, [draw(0)]);
scenario('boot', s => { s.boot = true; }, [draw(0)]);
scenario('splash', s => { s.splash = true; }, [draw(0)]);

// ---- screens without ink (the frame is not compared: see the header) ----
scenario('menu', null, [['press', 'no', 0], draw(1)]);
scenario('menu down', null, [['press', 'no', 0], ['press', 'down', 1], ['press', 'down', 2], draw(3)]);
scenario('sub audio', null, [['press', 'no', 0], ['press', 'yes', 1], draw(2)]);
scenario('net screen', s => { s.net.mode = 1; }, [['press', 'no', 0], ['press', 'down', 1], ['press', 'down', 2], ['press', 'down', 3], ['press', 'yes', 4], draw(5)]);

process.stdout.write('# generated by tests/oled_ref/gen_ink.js from ui/oled.js sha1 ' + require('crypto').createHash('sha1').update(src).digest('hex') +
                     ': ' + ncase + ' cases, ' + nframes + ' frames, ' + ninkpx + ' ink pixels\n' + out.join('\n') + '\n');
process.stderr.write(ncase + ' cases, ' + nframes + ' frames, ' + ninkpx + ' ink pixels\n');
