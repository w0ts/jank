// SPDX-License-Identifier: MIT
// The colour face of the lcd2 variant (ui/lcd2.js + the ink plane of ui/oled.js): a small palette that reads (WCAG AA), the ink plane
// marking exactly the pixels that carry information (confidence number, link glyph, signal bars, battery, input level number, first beat while lit, GATED),
// no ink and unchanged bytes on the OLED layout and in the menus, the ctx fields the stick feeds, and the x2 / x2.5 scalings.
// LCD2_OLED=<path to an older ui/oled.js> prints the OLED fixtures' hashes made by that file (how GOLDEN below was made).
'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const crypto = require('crypto');

const read = f => fs.readFileSync(path.join(__dirname, '..', f), 'utf8');
const oledSrc = process.env.LCD2_OLED ? fs.readFileSync(process.env.LCD2_OLED, 'utf8') : read('ui/oled.js');
const { OLED, Lcd2, Stick } = new Function(oledSrc + read('ui/lcd2.js') + read('ui/stick.js') + '; return { OLED, Lcd2, Stick };')();
let checks = 0;
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };
const ok = (c, what) => { checks++; assert.ok(c, what); };
const P = Lcd2.PALETTE, AA = 4.5, INK = OLED.INK, W = OLED.W, H = OLED.H;

// ---- fixtures: the HOME line, `get`, `status` as the stick reports them ----
const home = { bpm: 120, net: { kind: 'jank', group: 1 }, transport: 'playing', bar: 100, quantum: 4, link: 2, air: 1, battery: 80, level: -20, conf: 90, confHold: false, levelMin: -25, levelMax: -15, audio: true, fold: '' };
const NOW = 1100;
function ctxOf(over = {}, { get = { threshold: '70', lowconf: 'stop' }, status = { rssi: '-55', peak_db: '-12' }, beat = null } = {}) {
  const c = Stick.ctx({ home: { ...home, ...(over.home || {}) }, homeAt: 1000, get, status, statusAt: 1000, beat }, NOW);
  return { ...c, beatN: 0, ...(over.ctx || {}) };
}
function drawn(c, { ink = true, layout = true, ui = null } = {}) {
  const fb = new Uint8Array(W * H), plane = new Uint8Array(W * H);
  OLED.draw(fb, ui || OLED.create(), layout ? { ...c, layout: 'lcd2' } : c, NOW, ink ? plane : undefined);
  return { fb, ink: plane };
}
const sha = fbs => crypto.createHash('sha256').update(Buffer.concat(fbs.map(f => Buffer.from(f)))).digest('hex').slice(0, 16);

// ---- the OLED stays bit-identical: golden hashes made by the unmodified ui/oled.js on these fixtures (all screens an OLED draws) ----
function oledFixtures() {
  const out = [];
  const homes = [ctxOf(), ctxOf({ home: { transport: 'armed', conf: 40, battery: 10, link: 0 } }), ctxOf({ home: { audio: false, bpm: 98.4, link: 0 } }),
                 ctxOf({ home: { conf: 55, confHold: true, level: -3, levelMax: 0 } }, { status: { gated: '1', rssi: '-90' } }),
                 ctxOf({ home: { net: { kind: 'wifi', up: false }, battery: null } }, { status: null }), ctxOf({ home: { level: null, conf: null } }, { status: {} })];
  for (const c of homes) out.push(drawn({ ...c, wifiQ: null }, { ink: false, layout: false }).fb);
  const menu = { ...ctxOf(), tempoStep: () => null, setField() {}, setHold() {}, linkStartStop() {} };
  const model = Stick.create(() => menu, () => {});
  Stick.down(model, 'no', 0); Stick.up(model, 'no', 50);
  out.push(drawn(menu, { ink: false, layout: false, ui: model.ui }).fb);
  Stick.down(model, 'yes', 60); Stick.up(model, 'yes', 90);
  out.push(drawn(menu, { ink: false, layout: false, ui: model.ui }).fb);
  return out;
}
if (process.env.LCD2_OLED) { console.log(JSON.stringify(oledFixtures().map(f => sha([f])))); process.exit(0); }
const GOLDEN = JSON.parse('["92586ffb36b70a7d","12a9465b16c68428","f4f15225710bfde3","175c54f575f5b515","9da040822b6d69fc","4df4155e0ba4841d","2fc384e7b7fbbf3b","d7690151e5ad2b33"]');
eq(oledFixtures().map(f => sha([f])), GOLDEN, 'the OLED layout draws the bytes the unmodified OLED module drew (8 screens)');
{
  const c = ctxOf(), a = drawn(c, { ink: false, layout: false }), b = drawn(c, { ink: true, layout: false });
  eq([Buffer.from(b.fb).equals(Buffer.from(a.fb)), b.ink.every(v => v === 0)], [true, true], 'OLED layout with an ink plane handed in: same bytes, and the plane stays empty');
  const d = drawn(c, { ink: false, layout: true }), e = drawn(c, { ink: true, layout: true });
  ok(Buffer.from(d.fb).equals(Buffer.from(e.fb)), 'the lcd2 layout draws the same pixels with or without a plane');
  ok(!Buffer.from(d.fb).equals(Buffer.from(a.fb)), 'the lcd2 layout is a different picture (the CF number for the bar)');
}

// ---- the palette: small, complete, readable ----
eq(Object.keys(P).sort(), ['bad', 'bg', 'dim', 'good', 'neutral', 'warn'], 'the palette: background, neutral, good, warn, bad, dim');
for (const [name, rgb] of Object.entries(P)) ok(rgb.length === 3 && rgb.every(v => Number.isInteger(v) && v >= 0 && v <= 255), name + ': exact 8-bit RGB');
ok(Math.max(...P.bg) <= 20, 'the background is near black');
ok(Math.min(...P.neutral) >= 220, 'the neutral is near white');
for (const name of ['neutral', 'good', 'warn', 'bad', 'dim']) {
  ok(Lcd2.contrast(P[name], P.bg) >= AA, name + ' reads on the background (' + Lcd2.contrast(P[name], P.bg).toFixed(2) + ' >= 4.5)');
  ok(Lcd2.contrast(P.bg, P[name]) >= AA, name + ': the background reads on it (an inverted label)');
}
eq(new Set(['neutral', 'good', 'warn', 'bad', 'dim'].map(k => P[k].join(','))).size, 5, 'the five colours are all different');
eq(Lcd2.contrast([0, 0, 0], [255, 255, 255]), 21, 'the contrast formula: black on white is 21');
eq(Object.values(INK).sort(), [0, 1, 2, 3, 4, 5], 'the ink ids are the numbers 0..5 (meanings, not colours)');
eq(Object.values(INK).map(id => Lcd2.INK_PALETTE[id]), ['neutral', 'good', 'warn', 'bad', 'dim', 'warn'], 'each ink id maps to a palette colour (ONE, the first beat, is amber)');
ok(Lcd2.INK_PALETTE.every(k => k in P), 'every mapped colour is in the palette');
eq(Lcd2.inkColour(99), P.neutral, 'an unknown id is neutral');

// ---- the ink plane: where it is allowed, and what it says ----
const litIn = (d, x, y, w, h) => { const px = []; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (d.fb[j * W + i]) px.push(d.ink[j * W + i]); return px; };
const idsIn = (d, x, y, w, h) => [...new Set(litIn(d, x, y, w, h))].sort();
const DIGITS_X = 3 + 30 + 2;                         // textW('CF ', 2) = 30: the digits start at 35
const R = { link: [54, 0, 24, 8], bars: [79, 0, 8, 8], batt: [107, 0, 17, 7], beat1: [87, 9, 9, 9], inNum: [19, 33, 30, 7], conf: [DIGITS_X, 44, 40, 14], word: [78, 43, 48, 7] };
const inside = (x, y) => Object.values(R).some(([bx, by, bw, bh]) => x >= bx && x < bx + bw && y >= by && y < by + bh);
const outsideInk = d => { let n = 0; for (let i = 0; i < d.ink.length; i++) if (d.ink[i] !== 0 && !inside(i % W, Math.floor(i / W))) n++; return n; };

const healthy = drawn(ctxOf());
ok(healthy.ink.every((v, i) => v === 0 || healthy.fb[i] === 1), 'ink only on lit pixels');
eq(outsideInk(healthy), 0, 'a healthy HOME: ink only inside the link glyph, signal bars, battery, input number, first beat, CF number and its word');
eq(idsIn(healthy, ...R.link), [INK.GOOD], 'joined (2 Link peers): the link glyph is green');
eq(idsIn(healthy, ...R.bars), [INK.GOOD], '-55 dBm: the signal bars are green');
eq(idsIn(healthy, ...R.batt), [INK.GOOD], '80 %: the battery is green');
eq(idsIn(healthy, ...R.beat1), [INK.ONE], 'the first beat box carries the bar-1 id while it is the lit beat (beat 1)');
for (const beatN of [1, 2, 3, null]) eq(idsIn(drawn(ctxOf({ ctx: { beatN } })), ...R.beat1), [0], 'beat ' + beatN + ' is lit, not the first: the first box is neutral');
eq(idsIn(drawn(ctxOf({ ctx: { beatN: 4 } })), ...R.beat1), [INK.ONE], 'beat 5 is the next bar\'s first: amber again');
eq(idsIn(healthy, ...R.conf), [INK.GOOD], 'CF 90 against 70: green');
eq(idsIn(healthy, ...R.inNum), [0], 'an input at -20 dB: its number is neutral (nothing coloured)');
eq(litIn(healthy, ...R.inNum).length > 10, true, '... and the number is there');
eq(idsIn(healthy, 3, 33, 14, 7), [0], "the 'IN' label is neutral");
{
  eq(idsIn(drawn(ctxOf({ home: { link: 2 } })), 87 + 10, 9, 9, 9), [0], 'the second beat box is neutral');
  eq([87 + 20, 87 + 30].map(x => idsIn(healthy, x, 9, 9, 9)), [[0], [0]], 'and the third and fourth');
}

// the label 'CF' is neutral, the number is coloured, and the number is exactly the digits
{
  const d = drawn(ctxOf({ home: { conf: 87 } }));
  eq(idsIn(d, 3, 44, 24, 14), [0], "the 'CF' label is neutral");
  const digits = litIn(d, DIGITS_X, 44, 24, 14);
  ok(digits.length > 20 && digits.every(v => v === INK.GOOD), 'the digits of 87 carry the id (' + digits.length + ' lit pixels)');
}

// ---- confidence number: colour against the stop threshold (70) at 3 values around it, plus the borders ----
const confId = (conf, extra = {}, get) => idsIn(drawn(ctxOf({ home: { conf }, ...extra }, get ? { get } : undefined)), ...R.conf);
eq([confId(60), confId(69)], [[INK.BAD], [INK.BAD]], 'below the threshold (60, 69 against 70): red');
eq([confId(70), confId(75), confId(80)], [[INK.WARN], [INK.WARN], [INK.WARN]], 'at the threshold up to 10 points above (70, 75, 80): amber');
eq([confId(81), confId(87), confId(100)], [[INK.GOOD], [INK.GOOD], [INK.GOOD]], 'more than 10 above (81, 87, 100): green');
eq(confId(75, {}, { threshold: '50', lowconf: 'stop' }), [INK.GOOD], 'the threshold is the stick\'s: 75 against 50 is green');
eq(confId(75, {}, { threshold: '90', lowconf: 'run' }), [INK.BAD], '... against 90 red, also with KEEP RUNNING');
eq(idsIn(drawn(ctxOf({ home: { conf: null } })), ...R.conf), [INK.DIM], 'no confidence reported: the dashes are grey');
eq(confId(30, { ctx: { silent: true } }), [INK.DIM], 'silence: the number is grey, not red (nothing to track)');
eq(idsIn(drawn(ctxOf({}, { status: null })), ...R.conf), [INK.GOOD], 'no status line: the confidence comes from the HOME line and is still green');
{
  const noHome = Stick.ctx({ home: null, homeAt: 0, get: {}, status: null, statusAt: 0 }, NOW);
  eq([noHome.confKnown, noHome.rssi, noHome.peakDb], [false, null, null], 'no HOME line: confidence, signal and peak are "not reported"');
  eq(idsIn(drawn({ ...noHome, link: false }), ...R.conf), [INK.DIM], 'a stick that says nothing (AUDIO layout forced): grey dashes');
}
// no bars on the colour HOME: the IN number is small text, the free rows around it are empty, nothing slider-like is left
{
  const d = drawn(ctxOf({ home: { level: -20, levelMin: -40, levelMax: -5 } })), a = drawn(ctxOf({ home: { level: -20, levelMin: -40, levelMax: -5 } }), { layout: false });
  eq(litIn(d, 0, 30, W, 14).length, litIn(d, 3, 33, 44, 7).length, 'rows 30..43: lit pixels only inside the IN number box (no bar, ticks or lines)');
  ok(litIn(a, 0, 31, W, 11).length > 400, 'the OLED layout still has its IN bar there');
  ok(JSON.stringify(Array.from(drawn(ctxOf({ home: { level: -20 } })).fb.slice(33 * W, 40 * W))) !== JSON.stringify(Array.from(drawn(ctxOf({ home: { level: -35 } })).fb.slice(33 * W, 40 * W))), 'the number follows the level');
  const listening = drawn(ctxOf({ ctx: { bar: { state: 'listening', conf: 0.6 }, beatN: 1 } })), plain = drawn(ctxOf({ ctx: { bar: { state: 'listening', conf: 0.6 }, beatN: 1 } }), { layout: false });
  eq([litIn(listening, 87, 22, 36, 1).length, litIn(plain, 87, 22, 36, 1).length > 0], [0, true], 'the downbeat estimator\'s growing line is gone on the colour layout, not on the OLED');
  const locked = drawn(ctxOf({ ctx: { bar: { state: 'locked', conf: 0.6 }, beatN: 1 } }));
  eq([litIn(locked, 87, 19, 9, 2).length, litIn(locked, 87, 22, 36, 1).length], [18, 0], 'a locked bar still lights the 1\'s box underline (2 rows), without the line');
  eq(litIn(drawn(ctxOf({ ctx: { bar: { state: 'manual', conf: 0 } } })), 87, 21, 8, 8).length > 5, true, 'a manual bar still writes its M');
}
// the number replaced the bar: the 11 px CF bar is gone, the digits are drawn
{
  const d = drawn(ctxOf({ home: { conf: 87 } })), a = drawn(ctxOf({ home: { conf: 87 } }), { layout: false });
  ok(a.fb[49 * W + 100] === 1 && d.fb[49 * W + 100] === 0, 'the OLED layout keeps its CF bar; the lcd2 layout does not');
}
// the word next to the number
eq(idsIn(drawn(ctxOf({ ctx: { gated: true } })), ...R.word), [INK.BAD], 'GATED (outputs stopped for low confidence): red');
eq(idsIn(drawn(ctxOf({ home: { confHold: true } })), ...R.word), [INK.DIM], 'HOLD: grey');
eq(idsIn(drawn(ctxOf({ home: { conf: 50 }, ctx: { running: true } })), ...R.word), [INK.WARN], 'COAST (running under the threshold): amber');
eq(idsIn(drawn(ctxOf()), ...R.word), [], 'a healthy number has no word');

// ---- link glyph: joined green; alone dim in AUDIO, red in LINK ----
eq(idsIn(drawn(ctxOf({ home: { link: 0 } })), ...R.link), [INK.DIM], 'AUDIO mode, no Link peer: dim (a lone leader is normal)');
eq(idsIn(drawn(ctxOf({ home: { link: 0, audio: false } })), ...R.link), [INK.BAD], 'LINK mode, no session peer: red (nothing to follow)');
eq(idsIn(drawn(ctxOf({ home: { link: 1, audio: false } })), ...R.link), [INK.GOOD], 'LINK mode, one peer: green');

// the Link peer count sits beside the glyph (not in the status line, which stays empty on HOME)
{
  const two = drawn(ctxOf({ home: { link: 2 } })), none = drawn(ctxOf({ home: { link: 0 } }));
  ok(litIn(two, 66, 0, 12, 7).length > 0 && litIn(none, 66, 0, 12, 7).length === 0, 'the peer count is drawn beside the glyph only when there are peers');
  eq(litIn(two, 0, 58, 128, 6).length, 0, 'the lcd2 status line no longer lists the peers');
}
// ---- signal bars: the bars filled by quality, coloured by it; none and not reported grey ----
const barsOf = (rssi, over = {}) => { const d = drawn(ctxOf({ ctx: { rssi }, ...over })); return [idsIn(d, ...R.bars), litIn(d, ...R.bars).length]; };
eq(barsOf(-55), [[INK.GOOD], 24], '-55 dBm: three bars, green');
eq(barsOf(-67), [[INK.GOOD], 24], '-67 dBm: the good edge');
eq(barsOf(-68), [[INK.WARN], 14], '-68 dBm: two bars and a stub, amber (fair)');
eq(barsOf(-80), [[INK.WARN], 14], '-80 dBm: the fair edge');
eq(barsOf(-81), [[INK.BAD], 8], '-81 dBm: one bar and two stubs, red (poor)');
eq(barsOf(0), [[INK.DIM], 6], 'none heard (0): three stubs, grey');
eq(barsOf(null), [[INK.DIM], 6], 'not reported (null): the same grey stubs');

// ---- battery ----
const battId = pct => idsIn(drawn(ctxOf({ home: { battery: pct } })), ...R.batt);
eq([battId(100), battId(30), battId(29), battId(15), battId(14), battId(0)], [[INK.GOOD], [INK.GOOD], [INK.WARN], [INK.WARN], [INK.BAD], [INK.BAD]], 'battery: green from 30 %, amber 15..29, red under 15');
eq(idsIn(drawn(ctxOf({ home: { battery: null } })), ...R.batt), [], 'no battery fitted: no icon, no ink');

// ---- input level ----
const levelId = (levelDb, peakDb, max) => idsIn(drawn(ctxOf({ home: { level: levelDb, levelMax: max === undefined ? levelDb : max }, ctx: { peakDb } })), ...R.inNum).filter(v => v !== 0);
eq(levelId(-20, -12), [], 'a good level: the number is neutral');
eq(levelId(-20, -0.5), [INK.BAD], 'the last second peaked at -0.5 dBFS: red (clipping)');
eq(levelId(-20, -1), [INK.BAD], '... the -1 dBFS edge');
eq(levelId(-20, -1.5), [], '... -1.5 is not clipping');
eq(levelId(-20, null, -2), [INK.BAD], 'peak not reported: the RMS maximum over -3 dB counts');
eq(levelId(-20, null, -10), [], '... under it, neutral');
eq(levelId(-55, -40), [INK.WARN], 'under the tracker\'s -50 dBFS sound level: amber (too quiet to track)');
eq(levelId(-50, -40), [], '... -50 itself is enough');

// ---- the stick feeds the ctx: nothing made up ----
const fed = (homeOver, status, get = {}) => Stick.ctx({ home: { ...home, ...homeOver }, homeAt: 1000, get, status, statusAt: 1000 }, NOW);
eq([fed({}, { rssi: '-58' }).rssi, fed({}, { rssi: '0' }).rssi, fed({}, null).rssi, fed({}, {}).rssi], [-58, 0, null, null], 'jank mode: the best peer\'s rssi (status rssi), 0 = none, no status = not reported');
eq([fed({ net: { kind: 'wifi', up: true } }, { rssi: '-30', wifi_rssi: '-72' }).rssi, fed({ net: { kind: 'wifi', up: false } }, { wifi_rssi: '-72' }).rssi], [-72, 0], 'Wi-Fi client: the router\'s rssi (status wifi_rssi), 0 while not joined');
eq([fed({}, { peak_db: '-3' }).peakDb, fed({}, {}).peakDb, fed({ conf: 80 }, null).confKnown, fed({ conf: null }, null).confKnown], [-3, null, true, false], 'peak_db and the confidence come as reported, else null / unknown');
eq(fed({ link: 3 }, null).peers, 3, 'the Link peer count is the HOME line\'s');
{
  const c = fed({}, { rssi: '-58' }), withInk = Stick.create(() => ({ ...c, tempoStep: () => null, setField() {}, setHold() {}, linkStartStop() {} }), () => {});
  const plane = new Uint8Array(W * H), fb = Stick.framebuffer(withInk, NOW, plane);
  ok(plane.some(v => v !== 0) && fb.some(v => v === 1), 'Stick.framebuffer with a plane: the lcd2 layout and its ink');
  const plain = Stick.framebuffer(withInk, NOW);
  eq(Buffer.from(plain).equals(Buffer.from(drawn(c, { ink: false, layout: false }).fb)), true, 'Stick.framebuffer without a plane: the OLED layout');
}

// ---- LINK layout, menus, edit and overlay screens ----
{
  const link = drawn(ctxOf({ home: { audio: false, link: 1 } }));
  eq(idsIn(link, 2, 41, 30, 12), [INK.ONE], 'LINK HOME: the first of the four wide boxes is the bar 1');
  eq(idsIn(link, 34, 41, 30, 12), [0], '... the others are neutral');
  eq(idsIn(drawn(ctxOf({ home: { audio: false, link: 1 }, ctx: { beatN: 2 } })), 2, 41, 30, 12), [0], 'LINK HOME: not the lit one: neutral');
}
{
  const c = { ...ctxOf(), tempoStep: () => null, setField() {}, setHold() {}, linkStartStop() {} };
  const m = Stick.create(() => c, () => {});
  Stick.down(m, 'no', 0); Stick.up(m, 'no', 50);
  eq(m.ui.screen, 'menu', 'the menu is open');
  let d = drawn(c, { ui: m.ui });
  eq([d.fb.some(v => v === 1), d.ink.every(v => v === 0)], [true, true], 'the menu draws no ink: neutral, the selected row is the white inverted bar');
  Stick.down(m, 'yes', 60); Stick.up(m, 'yes', 90);
  d = drawn(c, { ui: m.ui });
  ok(d.fb.some(v => v === 1) && d.ink.every(v => v === 0), 'a sub menu or edit screen: no ink (' + m.ui.screen + ')');
  // an overlay drawn over HOME removes the ink under its box
  const h = OLED.create(); h.overlay = { until: NOW + 1000, title: 'DOUBLE', value: '240.0' };
  d = drawn(ctxOf(), { ui: h });
  eq(idsIn(d, 14, 12, 100, 40), [0], 'an overlay box over HOME carries no ink');
  const s = drawn({ ...ctxOf(), saved: true });
  eq(idsIn(s, 30, 22, 68, 22), [0], 'the SAVED box carries no ink');
}

// ---- the scalings ----
const coverage = (list, size) => { const seen = new Array(size).fill(0); for (const sp of list) for (let i = sp.start; i < sp.start + sp.width; i++) seen[i]++; return seen; };
for (const [key, w, h, ox, oy, top] of [['2', 256, 128, 32, 56, 56], ['2.5', 320, 160, 0, 40, 40]]) {
  const g = Lcd2.geometry(key), cols = coverage(g.cols, 320), rows = coverage(g.rows, 240);
  eq([g.cols.length, g.rows.length], [128, 64], key + ': one span for each source column and row');
  eq([g.cols[0].start, g.rows[0].start], [ox, oy], key + ': centred: the offset (' + ox + ', ' + oy + ')');
  eq([cols.filter(n => n === 1).length, cols.filter(n => n > 1).length], [w, 0], key + ': ' + w + ' destination columns, none twice');
  eq([rows.filter(n => n === 1).length, rows.filter(n => n > 1).length], [h, 0], key + ': ' + h + ' destination rows, none twice');
  eq([g.bands.top, g.bands.bottom], [{ y: 0, h: top }, { y: 240 - top, h: top }], key + ': the bands above and below are ' + top + ' px');
  for (let i = 1; i < 128; i++) assert.strictEqual(g.cols[i].start, g.cols[i - 1].start + g.cols[i - 1].width); checks++;
}
eq(new Set(Lcd2.geometry('2').cols.map(c => c.width)), new Set([2]), 'x2: every source pixel is 2 px wide');
eq(Lcd2.geometry('2.5').cols.slice(0, 6).map(c => c.width), [2, 3, 2, 3, 2, 3], 'x2.5: alternately 2 and 3');
assert.throws(() => Lcd2.geometry('3'), /unknown scaling/); checks++;

// ---- the painting ----
function canvas() { const img = { data: new Uint8ClampedArray(320 * 240 * 4) }, texts = []; return { img, texts, ctx: { putImageData() {}, fillText: (t, x, y) => texts.push([t, x, y]) } }; }
const px = (img, x, y) => [...img.data.slice(4 * (y * 320 + x), 4 * (y * 320 + x) + 3)];
{
  const fb = new Uint8Array(W * H), ink = new Uint8Array(W * H);
  fb[0] = 1; fb[1] = 1; fb[W * 63 + 127] = 1; ink[1] = INK.BAD; ink[W * 63 + 127] = INK.GOOD;
  const c = canvas();
  Lcd2.paint(c.ctx, c.img, fb, ink, '2');
  eq([px(c.img, 32, 56), px(c.img, 34, 57), px(c.img, 287, 183), px(c.img, 36, 56), px(c.img, 0, 0), px(c.img, 160, 10), px(c.img, 160, 230)],
     [P.neutral, P.bad, P.good, P.bg, P.bg, P.bg, P.bg], 'x2: lit pixels take their ink colour (neutral at 0), unlit and both bands are black');
  eq(c.texts, [], 'the bands carry no text');
  const g = canvas();
  Lcd2.paint(g.ctx, g.img, fb, null, '2.5');
  eq([px(g.img, 0, 40), px(g.img, 4, 40), px(g.img, 5, 40)], [P.neutral, P.neutral, P.bg], 'no ink plane at all: everything lit is neutral (x2.5: 2 + 3 px)');
}
// a whole healthy screen: mostly neutral, a few accents
{
  const d = drawn(ctxOf()), rgb = Lcd2.pixels(d.fb, d.ink), count = col => { let n = 0; for (let i = 0; i < d.fb.length; i++) if (d.fb[i] && rgb[3 * i] === col[0] && rgb[3 * i + 1] === col[1] && rgb[3 * i + 2] === col[2]) n++; return n; };
  const lit = d.fb.reduce((a, v) => a + v, 0);
  ok(count(P.neutral) > lit / 2, 'a healthy HOME is mostly neutral (' + count(P.neutral) + ' of ' + lit + ' lit pixels)');
  ok(count(P.good) > 0 && count(P.bad) === 0 && count(P.warn) > 0, 'a few green accents and the amber first beat while lit, no red');
  const off = drawn(ctxOf({ ctx: { beatN: 2 } })), offRgb = Lcd2.pixels(off.fb, off.ink);
  let amber = 0; for (let i = 0; i < off.fb.length; i++) if (off.fb[i] && offRgb[3 * i] === P.warn[0] && offRgb[3 * i + 1] === P.warn[1]) amber++;
  eq(amber, 0, 'a healthy HOME with the third beat lit has no amber at all');
}

console.log('lcd2: ' + checks + ' checks passed');
