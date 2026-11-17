// node oled_bar.js PLAYER_HTML: the device screen's AUTO BAR (player.html's OLED module): the marks under the beat boxes
// on HOME (AUDIO layout) and the menu item, picked again with YES even unchanged (MANUAL -> listening)
const fs = require('fs');
const assert = require('assert');
const html = require('../../tests/load_html.js')(process.argv[2]);
const src = html.match(/const OLED = \(\(\) => \{[\s\S]*?\n\}\)\(\);\n/)[0];
const OLED = new Function(src + '; return OLED;')();
let checks = 0;
const ok = (c, msg) => { assert.ok(c, msg); checks++; };

const W = OLED.W;
const lit = (fb, x, y) => fb[y * W + x] === 1;
const fields = [];
function ctx(bar, extra = {}) {
  return { link: false, lineIn: true, bpm: 124, linkBpm: 124, conf: 0.9, th: 80, levelDb: -20, levelMin: -25, levelMax: -15,
           running: true, armed: false, beatN: 1, beatMs: 0, peers: 0, wifiQ: null, usb: false, fitted: false, batt: 100,
           ltMode: 2, rate: 2, usbRole: 0, net: { mode: 0, group: 1, ssid: '', apName: '' }, autoBar: true, bar, ble: { mode: 1, left: null, phone: false },
           setField: (f, v) => fields.push([f, v]), tempoStep: () => 0, setHold: () => {}, linkStartStop: () => {}, ...extra };
}
function home(bar) {
  const fb = new Uint8Array(W * OLED.H);
  OLED.draw(fb, OLED.create(), ctx(bar), 0);
  return fb;
}
const underOne = fb => [...Array(9).keys()].every(i => lit(fb, 87 + i, 19) && lit(fb, 87 + i, 20));
const tick = fb => [...Array(39).keys()].filter(i => lit(fb, 87 + i, 22)).length;   // the confidence line under the boxes
const mRows = fb => lit(fb, 87, 24) && lit(fb, 89, 24) && lit(fb, 91, 24);              // the M glyph's middle row, #.#.#

let fb = home({ state: 'locked', conf: 1 });
ok(underOne(fb), 'locked: the first box is lit underneath');
ok(tick(fb) === 39, 'full confidence: the line under all four boxes');
ok(!mRows(fb), 'no M when locked');
fb = home({ state: 'listening', conf: 1 / 3 });
ok(!underOne(fb) && tick(fb) === 13, 'listening: no mark, a third of the line');
fb = home({ state: 'manual', conf: 1 });
ok(mRows(fb) && !underOne(fb) && [...Array(30).keys()].every(i => !lit(fb, 96 + i, 22)), 'manual: an M, no line');
fb = home({ state: 'off', conf: 1 });
ok(!mRows(fb) && !underOne(fb) && tick(fb) === 0, 'off: nothing');
fb = home(null);
ok(tick(fb) === 0, 'no estimator: nothing');

// the CF bar (y44): solid with its value; while the XIAO's confidence holds (jank M2k), hatched (a checkerboard) with HOLD
{
  const draw = extra => { const f = new Uint8Array(W * OLED.H); OLED.draw(f, OLED.create(), ctx(null, extra), 0); return f; };
  const solid = draw({ conf: 0.9 }), held = draw({ conf: 0.9, confHold: true });
  const at = (f, x, y) => f[y * W + x];
  // far right inside the fill (x 104..111, rows 46..53), clear of the text and the threshold tick (x 100)
  const cells = [];
  for (let y = 46; y < 54; y++) for (let x = 104; x < 112; x++) cells.push([x, y]);
  ok(cells.every(([x, y]) => at(solid, x, y) === 1), 'measured: the CF bar is filled');
  ok(cells.every(([x, y]) => at(held, x, y) === ((x + y) % 2 === 0 ? 1 : 0)), 'HOLD: the CF bar is a checkerboard');
  const textCols = f => { let n = 0; for (let x = 60; x < 96; x++) for (let y = 46; y < 53; y++) n += at(f, x, y) !== at(solid, x, y) ? 1 : 0; return n; };
  ok(textCols(held) > 0, 'HOLD: the label differs (HOLD written after the value)');
  ok([...Array(122).keys()].every(i => at(draw({ conf: 0.9, confHold: false }), 3 + i, 47) === at(solid, 3 + i, 47)), 'no hold: as before');
  const empty = draw({ conf: 0, confHold: true });
  ok(cells.every(([x, y]) => at(empty, x, y) === 0), 'HOLD at 0: no fill');
}

// COAST: the outputs run while the confidence is under the threshold (the robust follower coasts): the CF bar is hatched and says COAST
{
  const draw = extra => { const f = new Uint8Array(W * OLED.H); OLED.draw(f, OLED.create(), ctx(null, extra), 0); return f; };
  const at = (f, x, y) => f[y * W + x];
  const coast = draw({ conf: 0.9, th: 99, running: true }), stopped = draw({ conf: 0.9, th: 99, running: false });
  const above = draw({ conf: 0.9, th: 80, running: true }), held = draw({ conf: 0.9, th: 99, running: true, confHold: true });
  const cells = [];
  for (let y = 46; y < 54; y++) for (let x = 104; x < 112; x++) cells.push([x, y]);     // inside the 90 % fill, clear of the text and the tick
  ok(cells.every(([x, y]) => at(stopped, x, y) === 1), 'stopped under the threshold: the solid fill, as before');
  ok(cells.every(([x, y]) => at(coast, x, y) === ((x + y) % 2 === 0 ? 1 : 0)), 'COAST: the CF bar is a checkerboard');
  const bar = f => f.slice(44 * W, 56 * W);               // the CF bar's rows (the transport icon above differs with running)
  ok(bar(above).every((v, i) => v === bar(draw({ conf: 0.9, th: 80, running: false }))[i]), 'running above the threshold: as before');
  const textDiff = (a, b) => { let n = 0; for (let x = 30; x < 100; x++) for (let y = 46; y < 53; y++) n += at(a, x, y) !== at(b, x, y) ? 1 : 0; return n; };
  ok(textDiff(coast, held) > 0, 'COAST and HOLD are written differently');
  // the label stays readable on the hatched bar (a checkerboard under XOR-ed text was garbage): a cleared box, so far fewer lit pixels than a checkerboard's half
  const litShare = f => { let n = 0, t = 0; for (let y = 46; y < 53; y++) for (let x = 5; x < 60; x++) { t++; n += at(f, x, y) ? 1 : 0; } return n / t; };
  ok(litShare(coast) < 0.4 && litShare(coast) > 0.1, 'COAST: the label sits on a cleared box');
  ok(litShare(held) < 0.4 && litShare(held) > 0.1, 'HOLD: the same');
  // the IN bar's min / max lines do not run through its label
  const withLines = draw({ conf: 0.9, th: 80, running: false, levelDb: -8, levelMin: -50, levelMax: -3 });
  const noLines = draw({ conf: 0.9, th: 80, running: false, levelDb: -8, levelMin: -90, levelMax: -90 });
  const labelBox = f => { let n = 0; for (let y = 34; y < 40; y++) for (let x = 5; x < 45; x++) n += at(f, x, y); return n; };
  ok(labelBox(withLines) === labelBox(noLines), 'IN: a min line in the label leaves the letters alone');
}

// the menu: AUTO BAR right after LINK TRANSPORT, YES on ON (unchanged) picks it again
const ui = OLED.create();
const c = ctx({ state: 'manual', conf: 0 });
OLED.press(ui, 'no', 0, c);                       // HOME -> MENU
ok(ui.screen === 'menu', 'menu');
OLED.press(ui, 'down', 0, c);                     // LINK TRANSPORT
OLED.press(ui, 'down', 0, c);                     // AUTO BAR
OLED.press(ui, 'yes', 0, c);                      // EDIT
ok(ui.screen === 'edit' && ui.edit.field === 'autobar' && ui.edit.val === 1, 'AUTO BAR edit, ON');
OLED.press(ui, 'yes', 0, c);                      // YES, unchanged: picked again
ok(fields.length === 1 && fields[0][0] === 'autobar' && fields[0][1] === 1, 'picked again: setField autobar 1');
OLED.press(ui, 'yes', 0, c);
OLED.press(ui, 'down', 0, c);                     // OFF (choices cycle: OFF, AT START, ALWAYS)
ok(fields.length === 1, 'not applied before YES');
OLED.press(ui, 'yes', 0, c);
ok(fields.length === 2 && fields[1][1] === 0, 'OFF on YES');
OLED.press(ui, 'yes', 0, c);
OLED.press(ui, 'up', 0, c);
OLED.press(ui, 'no', 0, c);                       // NO: nothing applied
ok(fields.length === 2 && ui.screen === 'menu', 'NO restores');
const mfb = new Uint8Array(W * OLED.H);
OLED.draw(mfb, ui, c, 0);
ok(mfb.some(v => v), 'the menu draws');
console.log(checks + ' checks passed');
