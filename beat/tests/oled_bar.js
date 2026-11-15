// node oled_bar.js PLAYER_HTML: the device screen's AUTO BAR (player.html's OLED module): the marks under the beat boxes
// on HOME (AUDIO layout) and the menu item, picked again with YES even unchanged (MANUAL -> listening)
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(process.argv[2], 'utf8');
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
           ltMode: 2, rate: 2, usbRole: 0, net: { mode: 0, group: 1, ssid: '', apName: '' }, autoBar: true, bar,
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
OLED.press(ui, 'up', 0, c);                       // OFF (choices cycle)
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
