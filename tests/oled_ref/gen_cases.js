// node tests/oled_ref/gen_cases.js PLAYER_HTML > tests/oled_ref/cases.txt
// node tests/oled_ref/gen_cases.js --ink > tests/oled_ref/cases_ink.txt   (the colour layout's ink plane: gen_ink.js, from ui/oled.js)
// The reference frames for the OLED UI's C port (core/jank_oled*.c, tests/test_oled.c): the browser emulator's own code
// (beat-listener live/player.html: the OLED module, the QR encoder and the host's button handling btnDown / btnUp,
// armHint, comboStep), run in node on a fake clock, for a broad set of states and button sequences. Each frame is written
// in the SSD1309's page layout (byte page * 128 + x, bit y % 8) as hex; test_oled.c replays the same steps and must
// draw the same bytes, and log the same setter calls.
'use strict';
if (process.argv[2] === '--ink') { require('./gen_ink.js'); return; }       // node gen_cases.js --ink > cases_ink.txt
const fs = require('fs');
const html = require('../load_html.js')(process.argv[2]);
const grab = re => { const m = html.match(re); if (!m) throw new Error('not found: ' + re); return m[0]; };
const OLED = new Function(grab(/const OLED = \(\(\) => \{[\s\S]*?\n\}\)\(\);\n/) + '; return OLED;')();
const QR = new Function(grab(/const QR = \(\(\) => \{[\s\S]*?\n\}\)\(\);\n/) + '; return QR;')();
// the host's buttons: from the HOME combos' comment to the end of btnUp
const HOST_SRC = grab(/\/\/ HOME combos[\s\S]*?\nfunction btnUp\(b\) \{[\s\S]*?\n\}\n/);
const W = OLED.W, H = OLED.H;
const { baseState, dumpState: dumpStateInto, hex: hexOf, ABOUT_FULL, ABOUT_NONE } = require('./state.js');

const FIELDS = ['netmode', 'inoff', 'out0', 'out1', 'out2', 'rate', 'threshold', 'gain', 'ltmode', 'bright', 'sleep', 'group',
                'lowconf', 'insrc', 'usbrole', 'autobar', 'ssid', 'pass', 'appass', 'apname'];

// one case's world: the state, the logged setter calls, the ui, the fake clock and the host's button code
function world(state) {
  const log = [];
  let clock = 0, seq = 0, timers = [];
  const setField = (f, v) => {
    log.push('set ' + f + ' ' + v);
    const n = state.net;
    if (f === 'threshold') state.th = v; else if (f === 'lowconf') state.confStop = v === 1; else if (f === 'ltmode') state.ltMode = v;
    else if (f === 'gain') state.gain = v; else if (f === 'bright') state.bright = v; else if (f === 'sleep') state.sleep = v;
    else if (f === 'inoff') state.inOff = v; else if (f === 'out0') state.offs[0] = v; else if (f === 'out1') state.offs[1] = v;
    else if (f === 'out2') state.offs[2] = v; else if (f === 'rate') state.rate = v;
    else if (f === 'netmode') { if (v === 3) { if (n.mode !== 3) n.prev = n.mode; n.mode = 3; } else n.mode = v; }
    else if (f === 'group') n.group = v + 1; else if (f === 'insrc') state.inSrc = v; else if (f === 'usbrole') state.usbRole = v;
    else if (f === 'autobar') state.autoBar = v === 1; else if (f === 'ssid') n.ssid = v; else if (f === 'pass') n.pass = v;
    else if (f === 'appass') n.appass = v; else if (f === 'apname') n.apName = v;
    else throw new Error('field ' + f);
  };
  const tempoStep = d => {
    log.push('tempo ' + d);
    if (state.link) {
      if (state.linkBpm == null) return null;
      state.linkBpm = Math.max(90, Math.min(180, Math.round(state.linkBpm) + d));
      return state.linkBpm;
    }
    state.bpm = state.bpm ? (d > 0 ? state.bpm * 2 : state.bpm / 2) : 0;
    return state.bpm;
  };
  const setHold = on => { log.push('hold ' + (on ? 1 : 0)); state.running = !on; state.held = !!on; };
  const linkStartStop = on => { log.push('lss ' + (on ? 1 : 0)); state.running = on; state.held = !on; };
  const ctx = () => Object.assign({}, state, { setup: state.net.mode === 3, qr: QR.encode('WIFI:S:' + state.net.setupName + ';T:nopass;;'),
                                               net: state.net, tempoStep, setField, setHold, linkStartStop });
  const ui = OLED.create();
  ui.hint = null;
  const dev = { held: { up: false, down: false, no: false, yes: false }, combo: false, deferred: {}, repeat: {}, woke: {},
                lastInput: 0, boot: false, hintTimer: 0 };
  Object.defineProperty(dev, 'link', { get: () => state.link });
  Object.defineProperty(dev, 'inOff', { get: () => state.inOff });
  Object.defineProperty(dev, 'net', { get: () => state.net });
  const performance = { now: () => clock };
  const setTimeout = (fn, ms) => { const t = { due: clock + ms, fn, id: ++seq }; timers.push(t); return t.id; };
  const clearTimeout = id => { timers = timers.filter(t => t.id !== id); };
  const SLEEP_MS = [0, 30000, 60000, 300000];
  const asleep = now => SLEEP_MS[state.sleep] > 0 && state.net.mode !== 3 && now - dev.lastInput > SLEEP_MS[state.sleep];
  const api = (name, body) => { if (name === 'shift') log.push('shift ' + Math.round(body.beats * 2)); else throw new Error('api ' + name); };
  const S = { get xiao() { return state.xiao; } };
  const host = new Function('OLED', 'ui', 'dev', 'performance', 'setTimeout', 'clearTimeout', 'buildCtx', 'powered', 'asleep',
                            'api', 'setField', 'S', 'clampN', 'REPEAT',
                            HOST_SRC + '; return { btnDown, btnUp };')(
    OLED, ui, dev, performance, setTimeout, clearTimeout, () => ctx(), () => true, asleep, api, setField, S,
    (v, lo, hi) => Math.max(lo, Math.min(hi, v)), { firstMs: 400, everyMs: 120 });
  const advance = t => {
    if (t < clock) throw new Error('time goes back');
    for (;;) {
      const due = timers.filter(x => x.due <= t).sort((a, b) => a.due - b.due || a.id - b.id)[0];
      if (!due) break;
      timers = timers.filter(x => x !== due);
      clock = due.due;
      due.fn();
    }
    clock = t;
  };
  return { state, log, ui, ctx, host, advance, now: () => clock };
}

const out = [];
const B = { up: 'U', down: 'D', no: 'N', yes: 'Y' };
const dumpState = s => dumpStateInto(out, s);
const hex = fb => hexOf(fb, W, H);
let ncase = 0, nframes = 0;
// a case: a state, then steps: ['draw', t] | ['press', btn, t] | ['down', btn, t] | ['up', btn, t] | ['tick', t]
function scenario(name, edit, steps) {
  const s = baseState();
  if (edit) edit(s);
  const w = world(s);
  out.push('case ' + name);
  dumpState(s);
  for (const st of steps) {
    const [op, a, b] = st;
    if (op === 'draw') {
      w.advance(a);
      const fb = new Uint8Array(W * H);
      OLED.draw(fb, w.ui, w.ctx(), a);
      out.push('draw ' + a + ' ' + hex(fb));
      nframes++;
    } else if (op === 'press') {
      w.advance(b);
      OLED.press(w.ui, a, b, w.ctx());
      out.push('press ' + B[a] + ' ' + b);
    } else if (op === 'down' || op === 'up') {
      w.advance(b);
      (op === 'down' ? w.host.btnDown : w.host.btnUp)(a);
      out.push(op + ' ' + B[a] + ' ' + b);
    } else if (op === 'tick') {
      w.advance(a);
      out.push('tick ' + a);
    } else throw new Error(op);
  }
  for (const l of w.log) out.push('log ' + l);
  out.push('end');
  ncase++;
}
const draw = t => ['draw', t || 0];
const presses = (btns, t0 = 0) => btns.map((b, i) => ['press', b, t0 + i]);
// presses then one frame after each
const walk = (btns, t0 = 0) => btns.flatMap((b, i) => [['press', b, t0 + i * 10], draw(t0 + i * 10 + 5)]);

// ---- HOME, AUDIO layout ----
scenario('audio running locked', s => { s.bpm = 124.25; s.wifiQ = { rssi: -58, kbps: 1000, tx: 400, retried: 16, lost: 1 }; },
         [draw(0), draw(2000), draw(4000), draw(6000)]);
scenario('audio halves', s => { s.bpm = 0.15; s.levelDb = -0.5; s.levelMin = -60.5; s.levelMax = 2.5; s.conf = 0.805; s.th = 85; }, [draw(0)]);
scenario('audio stopped listening', s => { s.running = false; s.beatN = null; s.bar = { state: 'listening', conf: 1 / 3 }; s.peers = 1; }, [draw(0)]);
scenario('audio armed blink', s => { s.running = false; s.armed = true; s.beatN = null; }, [draw(0), draw(400), draw(799), draw(1200)]);
scenario('audio manual no bpm', s => { s.bpm = null; s.bar = { state: 'manual', conf: 1 }; s.levelDb = -120; s.levelMin = -120; s.levelMax = -120;
                                       s.conf = 0; s.peers = 0; }, [draw(0)]);
scenario('audio bar off low conf', s => { s.bar = { state: 'off', conf: 1 }; s.conf = 0.3; s.th = 50; s.beatN = -3; s.wifi = 'weak';
                                          s.wifiQ = { rssi: -85, kbps: 1000, tx: 400, retried: 160, lost: 48 }; },
         [draw(0), draw(2000), draw(4000), draw(6000)]);
scenario('audio no estimator', s => { s.bar = null; s.beatN = 6; s.peers = 12; s.wifiQ = { rssi: 0, kbps: 0, tx: 0, retried: 0, lost: 0 }; },
         [draw(0), draw(2000), draw(4000), draw(6000)]);
for (const mode of [0, 1, 2])
  for (const wifi of ['good', 'weak', 'none'])
    scenario('icons mode ' + mode + ' wifi ' + wifi, s => { s.net.mode = mode; s.wifi = wifi; s.usb = mode !== 1; s.batt = 33 * mode; },
             [draw(0)]);
scenario('client long ssid', s => { s.net.mode = 1; s.net.ssid = 'A Very Long Network Name Indeed'; }, [draw(0)]);
scenario('client no ssid', s => { s.net.mode = 1; s.net.ssid = ''; }, [draw(0)]);
scenario('group 9 no battery', s => { s.net.group = 9; s.fitted = false; s.usb = false; }, [draw(0)]);
for (const b of [0, 4, 5, 50, 95, 100]) scenario('battery ' + b, s => { s.batt = b; }, [draw(0)]);

// ---- HOME, LINK layout ----
for (const lt of [0, 1, 2])
  scenario('link lt ' + lt, s => { s.link = true; s.lineIn = false; s.ltMode = lt; s.linkBpm = 120; s.beatN = lt; }, [draw(0)]);
scenario('link no tempo stopped', s => { s.link = true; s.lineIn = false; s.linkBpm = null; s.running = false; s.beatN = null; s.peers = 0; }, [draw(0)]);
scenario('link fast ap', s => { s.link = true; s.lineIn = false; s.linkBpm = 174.96; s.net.mode = 2; s.beatN = 3; }, [draw(0)]);
scenario('link client armed', s => { s.link = true; s.lineIn = false; s.net.mode = 1; s.running = false; s.armed = true; s.ltMode = 1; }, [draw(0), draw(450)]);

// ---- overlays from HOME presses ----
scenario('link tempo up down', s => { s.link = true; s.lineIn = false; s.linkBpm = 120.4; },
         [['press', 'up', 0], draw(10), ['press', 'down', 20], ['press', 'down', 30], draw(40), draw(1030), draw(1031)]);
scenario('link tempo none', s => { s.link = true; s.lineIn = false; s.linkBpm = null; }, [['press', 'up', 0], draw(1)]);
scenario('link tempo clamp', s => { s.link = true; s.lineIn = false; s.linkBpm = 180; }, [['press', 'up', 0], draw(1)]);
scenario('audio double half', s => { s.bpm = 87.5; }, [['press', 'up', 0], draw(1), ['press', 'down', 2], ['press', 'down', 3], draw(4)]);
scenario('audio zero bpm double', s => { s.bpm = 0; }, [['press', 'up', 0], draw(1)]);
scenario('tracker leads', s => { s.trackerLeads = true; s.xiao = true; }, [['press', 'yes', 0], draw(1), ['press', 'up', 2000], draw(2001)]);
scenario('audio yes stop start', null, [['press', 'yes', 0], draw(1), ['press', 'yes', 2], draw(3)]);
scenario('link yes start stop', s => { s.link = true; s.lineIn = false; s.running = false; },
         [['press', 'yes', 0], draw(1), ['press', 'yes', 2], draw(3)]);
scenario('saved box', s => { s.saved = true; }, [draw(0)]);
scenario('boot', s => { s.boot = true; }, [draw(0)]);
scenario('splash', s => { s.splash = true; }, [draw(0)]);

// ---- MENU: every row, the scrolling, every value ----
const down8 = Array(8).fill('down');
scenario('menu walk', null, [['press', 'no', 0], draw(1), ...walk(down8, 10), ...walk(['down', 'up', 'up', 'up', 'up', 'up', 'up', 'up', 'up', 'up'], 200)]);
scenario('menu values', s => { s.ltMode = 0; s.autoBar = false; s.net.mode = 1; s.usbRole = 1; s.rate = 4; s.fitted = false; },
         [['press', 'no', 0], draw(1), ...walk(down8, 10)]);
scenario('menu values 2', s => { s.ltMode = 1; s.net.mode = 2; s.rate = 0; s.batt = 7; }, [['press', 'no', 0], draw(1), ...walk(down8, 10)]);
scenario('menu back home', null, [['press', 'no', 0], ['press', 'no', 1], draw(2)]);

// ---- SUBs and EDITs ----
scenario('sub audio', s => { s.gain = -7; s.th = 35; }, [...presses(['no', 'yes']), draw(5), ...walk(['down', 'down', 'down', 'down', 'up'], 10)]);
scenario('sub audio keep running', s => { s.confStop = false; s.inSrc = 2; s.gain = 12; }, [...presses(['no', 'yes']), draw(5), ...walk(['down', 'down'], 10)]);
scenario('sub offsets', s => { s.inOff = -100; s.offs = [50, -1, 0]; },
         [...presses(['no', 'down', 'down', 'down', 'down', 'down', 'yes']), draw(10), ...walk(['down', 'down', 'down'], 20)]);
scenario('sub display', s => { s.bright = 10; s.sleep = 3; }, [...presses(['no', ...Array(7).fill('down'), 'yes']), draw(20), ...walk(['down'], 30)]);
scenario('edit gain live', s => { s.gain = 11; },
         [...presses(['no', 'yes', 'down', 'yes']), draw(10), ...walk(['up', 'up', 'down', 'down', 'down'], 20), ['press', 'no', 100], draw(101)]);
scenario('edit gain keep', s => { s.gain = -12; }, [...presses(['no', 'yes', 'down', 'yes', 'down', 'up', 'up', 'yes']), draw(20)]);
scenario('edit threshold', s => { s.th = 95; }, [...presses(['no', 'yes', 'down', 'down', 'down', 'yes']), draw(10), ...walk(['up', 'up', 'down'], 20)]);
scenario('edit lowconf', null, [...presses(['no', 'yes', 'down', 'down', 'yes']), draw(10), ...walk(['up', 'up'], 20), ['press', 'yes', 50], draw(51)]);
scenario('edit source', null, [...presses(['no', 'yes', 'yes']), draw(10), ...walk(['up', 'up', 'up'], 20), ['press', 'no', 50], draw(51)]);
scenario('edit ltmode', null, [...presses(['no', 'down', 'yes']), draw(10), ...walk(['up', 'up', 'up', 'down'], 20), ['press', 'yes', 60], draw(61)]);
scenario('edit autobar repick', s => { s.bar = { state: 'manual', conf: 0 }; },
         [...presses(['no', 'down', 'down', 'yes']), draw(10), ['press', 'yes', 11], draw(12), ['press', 'yes', 13], ['press', 'up', 14], draw(15),
          ['press', 'yes', 16], ['press', 'yes', 17], ['press', 'up', 18], ['press', 'no', 19], draw(20)]);
scenario('edit usb', null, [...presses(['no', 'down', 'down', 'down', 'down', 'yes']), draw(10), ...walk(['down', 'down'], 20), ['press', 'yes', 50], draw(51)]);
scenario('edit rate', s => { s.rate = 3; },
         [...presses(['no', ...Array(6).fill('down'), 'yes']), draw(10), ...walk(['up', 'up', 'up', 'down'], 20), ['press', 'no', 60], draw(61)]);
scenario('edit offsets', s => { s.offs = [49, -49, 0]; s.inOff = 99; },
         [...presses(['no', ...Array(5).fill('down'), 'yes', 'yes']), draw(10), ...walk(['up', 'up', 'down'], 20), ['press', 'yes', 50],
          ['press', 'down', 51], ['press', 'yes', 52], draw(53), ...walk(['up', 'up', 'down'], 60), ['press', 'yes', 100],
          ['press', 'down', 101], ['press', 'yes', 102], ...walk(['down', 'down'], 110), ['press', 'no', 150], ['press', 'down', 151],
          ['press', 'yes', 152], ...walk(['down', 'up'], 160), ['press', 'yes', 190], draw(191)]);
scenario('edit display', s => { s.bright = 90; },
         [...presses(['no', ...Array(7).fill('down'), 'yes', 'yes']), draw(20), ...walk(['up', 'up', 'down', 'down'], 30), ['press', 'yes', 80],
          ['press', 'down', 81], ['press', 'yes', 82], draw(83), ...walk(['up', 'up', 'up', 'up'], 90), ['press', 'no', 140], draw(141)]);
scenario('battery info', null, [...presses(['no', ...down8, 'yes']), draw(20), ['press', 'up', 21], draw(22), ['press', 'yes', 23], draw(24)]);
for (const [b, usb, fitted] of [[100, true, true], [37, false, true], [0, true, true], [64, false, false], [64, true, false], [99, true, true]])
  scenario('battery info ' + b + ' ' + usb + ' ' + fitted, s => { s.batt = b; s.usb = usb; s.fitted = fitted; },
           [...presses(['no', ...down8, 'yes']), draw(20)]);

// ---- ABOUT: the system rows, 5 of 13 on the OLED (UP / DOWN scroll), "-" for what is not known ----
const down9 = Array(9).fill('down');
scenario('about', s => Object.assign(s, ABOUT_FULL), [...presses(['no', ...down9, 'yes']), draw(20), ...walk(['down', 'down', 'down', 'down', 'down', 'down', 'down', 'down', 'down', 'down'], 30), ['press', 'up', 400], draw(401), ['press', 'yes', 402], draw(403)]);
scenario('about unknown', s => Object.assign(s, ABOUT_NONE), [...presses(['no', ...down9, 'yes']), draw(20), ...walk(['down', 'down', 'down', 'down', 'down', 'down', 'down', 'down'], 30)]);
scenario('about long uptime', s => Object.assign(s, ABOUT_FULL, { uptimeS: 200000, flashKb: 4096, appUsedKb: null, reset: 'PANIC' }), [...presses(['no', ...down9, 'yes']), draw(20), ...walk(['down', 'down', 'down', 'down'], 30)]);

// ---- NETWORK ----
const toNet = ['no', 'down', 'down', 'down', 'yes'];
scenario('net jank', null, [...presses(toNet), draw(10), ...walk(['down', 'down', 'down', 'up'], 20)]);
scenario('net jank group', null, [...presses(toNet), ['press', 'down', 10], ['press', 'yes', 11], draw(12), ...walk(['up', 'up', 'down', 'down', 'down'], 20),
                                  ['press', 'yes', 80], draw(81), ['press', 'yes', 82], ['press', 'up', 83], ['press', 'no', 84], draw(85)]);
scenario('net client', s => { s.net.mode = 1; }, [...presses(toNet), draw(10), ...walk(['down', 'down', 'down', 'down'], 20)]);
scenario('net client connecting', s => { s.net.mode = 1; s.net.status = 'CONNECTING'; s.net.pass = 'abc'; s.net.ssid = 'A Very Long Network Name'; },
         [...presses(toNet), draw(10), ...walk(['down', 'down', 'down', 'yes'], 20), ['press', 'no', 70], draw(71)]);
scenario('net ap', s => { s.net.mode = 2; s.net.apClients = 0; }, [...presses(toNet), draw(10), ...walk(['down', 'down', 'down', 'yes'], 20)]);
scenario('net type edit', null, [...presses(toNet), ['press', 'yes', 10], draw(11), ...walk(['up', 'up', 'up', 'up', 'down'], 20),
                                 ['press', 'yes', 80], draw(81), ['press', 'no', 82], draw(83)]);
scenario('net type to client', null, [...presses(toNet), ['press', 'yes', 10], ['press', 'down', 11], ['press', 'yes', 12], draw(13)]);
scenario('net setup confirm', null, [...presses(toNet), ['press', 'yes', 10], ['press', 'up', 11], draw(12), ['press', 'yes', 13], draw(14),
                                     ['press', 'yes', 15], draw(16), ['press', 'up', 17], draw(18), ['press', 'no', 19], draw(20),
                                     ['press', 'up', 21], draw(22), ['press', 'no', 23], ['press', 'yes', 24], draw(25)]);
scenario('net setup cancel', null, [...presses(toNet), ['press', 'yes', 10], ['press', 'up', 11], ['press', 'yes', 12], ['press', 'no', 13], draw(14)]);
scenario('setup long name', s => { s.net.mode = 3; s.net.prev = 1; s.net.setupName = 'jank-setup-0123456789ab'; }, [draw(0)]);
scenario('setup short name', s => { s.net.mode = 3; s.net.setupName = 'jk'; }, [draw(0)]);
scenario('scan', s => { s.net.mode = 1; s.net.scan.push({ name: 'Neighbour 5G', bars: 0 }, { name: 'xx', bars: 3 }, { name: 'Long Name Number Seven Here', bars: 1 }); },
         [...presses(toNet), ['press', 'down', 10], ['press', 'yes', 11], draw(12), ...walk(['down', 'down', 'down', 'down', 'down', 'down', 'down', 'up', 'up', 'up', 'up', 'up', 'up'], 20),
          ['press', 'down', 200], ['press', 'down', 201], ['press', 'yes', 202], draw(203), ['press', 'yes', 204], ['press', 'no', 205], draw(206)]);
scenario('scan empty', s => { s.net.mode = 1; s.net.scan = []; }, [...presses(toNet), ['press', 'down', 10], ['press', 'yes', 11], draw(12),
                                                                     ['press', 'down', 13], ['press', 'yes', 14], draw(15), ['press', 'no', 16], draw(17)]);

// ---- the character editor ----
scenario('text pass', s => { s.net.mode = 1; },
         [...presses(toNet), ['press', 'down', 10], ['press', 'down', 11], ['press', 'yes', 12], draw(13), ...walk(['up', 'yes', 'down', 'down', 'yes'], 20),
          ...walk(Array(8).fill('yes'), 80), ['press', 'up', 200], draw(201), ['press', 'no', 202], draw(203), ['press', 'down', 204], ['press', 'yes', 205],
          draw(206), ['press', 'yes', 207], draw(208)]);
scenario('text apname long', s => { s.net.mode = 2; s.net.apName = 'abcdefghijklmnopqrstuvwxyz0123456789'; },
         [...presses(toNet), ['press', 'down', 10], ['press', 'yes', 11], draw(12), ...walk(Array(22).fill('yes'), 20), ...walk(['up', 'up', 'down'], 300),
          ...walk(Array(16).fill('yes'), 400), ['press', 'down', 600], draw(601), ['press', 'yes', 602], ['press', 'yes', 603], draw(604)]);
scenario('text appass delete all', s => { s.net.mode = 2; s.net.appass = 'ab'; },
         [...presses(toNet), ['press', 'down', 10], ['press', 'down', 11], ['press', 'yes', 12], draw(13), ...walk(['no', 'no'], 20), ['press', 'no', 50], draw(51)]);
scenario('text odd chars', s => { s.net.mode = 1; s.net.pass = 'a&b?#'; },
         [...presses(toNet), ['press', 'down', 10], ['press', 'down', 11], ['press', 'yes', 12], ...walk(['yes', 'up', 'yes', 'yes', 'down', 'yes', 'up'], 20)]);

// ---- the buttons: holds, hints, combos, repeats, sleep ----
scenario('btn no release menu', null, [['down', 'no', 0], draw(100), ['up', 'no', 200], draw(201), ['down', 'no', 300], ['up', 'no', 310], draw(311)]);
scenario('btn no hint', null, [['down', 'no', 0], draw(399), draw(400), draw(900), ['up', 'no', 1000], draw(1001)]);
scenario('btn yes hint then release', null, [['down', 'yes', 0], draw(500), ['up', 'yes', 600], draw(601)]);
scenario('btn nudge beat', null, [['down', 'no', 0], ['down', 'up', 100], draw(101), ['tick', 1000], draw(1000), ['up', 'up', 1100], ['down', 'down', 1200],
                                  draw(1201), ['up', 'down', 1300], ['up', 'no', 1400], draw(1401), draw(2300)]);
scenario('btn nudge ms repeat', s => { s.inOff = 97; },
         [['down', 'yes', 0], draw(450), ['down', 'up', 500], draw(501), draw(900), draw(1020), draw(1140), draw(1500), ['up', 'up', 1510],
          ['down', 'down', 1600], draw(1601), ['up', 'down', 1700], ['up', 'yes', 1710], draw(1711)]);
scenario('btn nudge ms min', s => { s.inOff = -99; }, [['down', 'yes', 0], ['down', 'down', 10], ['tick', 2000], draw(2000), ['up', 'down', 2001], ['up', 'yes', 2002], draw(2003)]);
scenario('btn link no combos', s => { s.link = true; s.lineIn = false; },
         [['down', 'no', 0], draw(500), ['down', 'up', 600], draw(601), ['up', 'up', 700], ['up', 'no', 800], draw(801)]);
scenario('btn tempo repeat', s => { s.link = true; s.lineIn = false; s.linkBpm = 100; },
         [['down', 'up', 0], draw(1), draw(400), draw(520), draw(1000), ['up', 'up', 1001], draw(1002), ['down', 'down', 1100], ['up', 'down', 1150], draw(1151)]);
scenario('btn menu repeat', null, [['down', 'no', 0], ['up', 'no', 10], ['down', 'down', 20], draw(21), draw(420), draw(540), draw(900), ['up', 'down', 901],
                                   ['down', 'yes', 950], ['up', 'yes', 960], draw(961)]);
scenario('btn edit repeat', s => { s.th = 50; }, [['down', 'no', 0], ['up', 'no', 10], ['down', 'yes', 20], ['up', 'yes', 30], ['down', 'down', 40],
                                                  ['up', 'down', 50], ['down', 'down', 60], ['up', 'down', 70], ['down', 'down', 80], ['up', 'down', 90],
                                                  ['down', 'yes', 100], ['up', 'yes', 110], ['down', 'up', 120], draw(121), draw(1000), ['up', 'up', 1001],
                                                  ['down', 'yes', 1100], ['up', 'yes', 1110], draw(1111)]);
scenario('btn xiao combos', s => { s.trackerLeads = true; s.xiao = true; },
         [['down', 'no', 0], ['down', 'up', 100], draw(101), ['up', 'up', 200], ['up', 'no', 300], ['down', 'yes', 400], ['down', 'down', 500], draw(501),
          ['tick', 1100], ['up', 'down', 1100], ['up', 'yes', 1200], draw(1201), ['down', 'yes', 2000], ['up', 'yes', 2100], draw(2101)]);
scenario('btn sleep wake', s => { s.sleep = 1; }, [draw(0), ['down', 'no', 31000], ['up', 'no', 31100], draw(31101), ['down', 'no', 31200], ['up', 'no', 31300], draw(31301)]);
scenario('btn yes then no', null, [['down', 'yes', 0], ['down', 'up', 50], ['up', 'up', 60], ['down', 'no', 70], draw(500), ['up', 'no', 600], draw(601),
                                   ['up', 'yes', 700], draw(701)]);
scenario('btn two ups', s => { s.link = true; s.lineIn = false; s.linkBpm = 130; },
         [['down', 'up', 0], ['down', 'down', 100], ['tick', 1000], draw(1000), ['up', 'up', 1001], ['up', 'down', 1002], draw(1003)]);
scenario('btn setup leave', s => { s.net.mode = 3; s.net.prev = 2; },
         [['down', 'no', 0], ['up', 'no', 10], draw(11), ['down', 'up', 20], ['up', 'up', 30], draw(31), ['down', 'yes', 40], ['up', 'yes', 50], draw(51)]);

process.stdout.write('# generated by tests/oled_ref/gen_cases.js from ' + require('crypto').createHash('sha1').update(html).digest('hex') +
                     ' (player.html): ' + ncase + ' cases, ' + nframes + ' frames\n' + out.join('\n') + '\n');
process.stderr.write(ncase + ' cases, ' + nframes + ' frames\n');
