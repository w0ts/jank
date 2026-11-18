// SPDX-License-Identifier: MIT
// The colour face of the lcd2 variant: its own 160 x 120 logical grid (ui/oled.js, ctx.layout 'lcd2'), painted exactly x2 on 320 x 240
// (ui/lcd2.js). Tested here: the palette (WCAG AA), the sizes and the exact x2 tiling, every element's box and the rows (no overlaps),
// the ink plane marking exactly the pixels that carry information (confidence, input level, the bars' fills and ticks, link, signal,
// battery, first beat, GATED words), no ink in menus / edit / info / setup / overlays, the OLED layout unchanged, and the ctx fields.
// LCD2_OLED=<path to an older ui/oled.js> prints the OLED fixtures' hashes made by that file (how GOLDEN below was made).
'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const crypto = require('crypto');

const read = f => fs.readFileSync(path.join(__dirname, '..', f), 'utf8');
const oledSrc = process.env.LCD2_OLED ? fs.readFileSync(process.env.LCD2_OLED, 'utf8') : read('ui/oled.js');
const { OLED, Lcd2, Stick, BigFont } = new Function(read('ui/bigfont.js') + oledSrc + read('ui/lcd2.js') + read('ui/stick.js') + '; return { OLED, Lcd2, Stick, BigFont };')();
let checks = 0;
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };
const ok = (c, what) => { checks++; assert.ok(c, what); };
const P = Lcd2.PALETTE, AA = 4.5, INK = OLED.INK, OW = OLED.W, OH = OLED.H, LW = 160, LH = 120;

// ---- fixtures: the HOME line, `get`, `status` as the stick reports them ----
const home = { bpm: 120, net: { kind: 'jank', group: 1 }, transport: 'playing', bar: 100, quantum: 4, link: 2, air: 1, battery: 80, level: -20, conf: 90, confHold: false, levelMin: -25, levelMax: -15, audio: true, fold: '' };
const NOW = 1100;
function ctxOf(over = {}, { get = { threshold: '70', lowconf: 'stop' }, status = { rssi: '-55', peak_db: '-12' }, beat = null } = {}) {
  const c = Stick.ctx({ home: { ...home, ...(over.home || {}) }, homeAt: 1000, get, status, statusAt: 1000, beat }, NOW);
  return { ...c, beatN: 0, ...(over.ctx || {}) };
}
// draws on the layout's own grid; returns { fb, ink, w, h }
function drawn(c, { ink = true, layout = true, ui = null } = {}) {
  const size = layout ? { w: 160, h: 120 } : { w: 128, h: 64 }, fb = new Uint8Array(size.w * size.h), plane = new Uint8Array(size.w * size.h);
  OLED.draw(fb, ui || OLED.create(), layout ? { ...c, layout: 'lcd2' } : c, NOW, ink ? plane : undefined);
  return { fb, ink: plane, w: size.w, h: size.h };
}
const sha = fbs => crypto.createHash('sha256').update(Buffer.concat(fbs.map(f => Buffer.from(f)))).digest('hex').slice(0, 16);
const withUi = (c, keys, layout) => {                                // a menu state: the ui after these button presses (layout: the ctx the buttons see)
  const cc = { ...c, ...(layout ? { layout } : {}), tempoStep: () => null, setField() {}, setHold() {}, linkStartStop() {} }, m = Stick.create(() => cc, () => {});
  let t = 0;
  for (const k of keys) { Stick.down(m, k, t); t += 30; Stick.up(m, k, t); t += 30; }
  return { c: cc, ui: m.ui };
};

// ---- the OLED stays bit-identical: golden hashes made by the OLED module before the lcd2 layout existed, on these fixtures ----
function oledFixtures() {
  const out = [];
  const homes = [ctxOf(), ctxOf({ home: { transport: 'armed', conf: 40, battery: 10, link: 0 } }), ctxOf({ home: { audio: false, bpm: 98.4, link: 0 } }),
                 ctxOf({ home: { conf: 55, confHold: true, level: -3, levelMax: 0 } }, { status: { gated: '1', rssi: '-90' } }),
                 ctxOf({ home: { net: { kind: 'wifi', up: false }, battery: null } }, { status: null }), ctxOf({ home: { level: null, conf: null } }, { status: {} })];
  for (const c of homes) out.push(drawn({ ...c, wifiQ: null }, { ink: false, layout: false }).fb);
  for (const keys of [['no'], ['no', 'yes']]) { const m = withUi(ctxOf(), keys); out.push(drawn(m.c, { ink: false, layout: false, ui: m.ui }).fb); }
  return out;
}
if (process.env.LCD2_OLED) { console.log(JSON.stringify(oledFixtures().map(f => sha([f])))); process.exit(0); }
const GOLDEN = JSON.parse('["92586ffb36b70a7d","12a9465b16c68428","f4f15225710bfde3","175c54f575f5b515","9da040822b6d69fc","4df4155e0ba4841d","7fd977781a04b3ad","d7690151e5ad2b33"]');
eq(oledFixtures().map(f => sha([f])), GOLDEN, 'the OLED layout draws the bytes the OLED module drew before the lcd2 layout (8 screens)');
eq([OLED.W, OLED.H, OLED.size(), OLED.size('oled'), OLED.size('lcd2')], [128, 64, { w: 128, h: 64 }, { w: 128, h: 64 }, { w: 160, h: 120 }], 'sizes: OLED.W / OLED.H stay the OLED\'s; OLED.size(layout) says the grid of a layout');
{
  const c = ctxOf(), a = drawn(c, { ink: false, layout: false }), b = drawn(c, { ink: true, layout: false });
  eq([a.fb.length, Buffer.from(b.fb).equals(Buffer.from(a.fb)), b.ink.every(v => v === 0)], [128 * 64, true, true], 'OLED layout with an ink plane handed in: same bytes, and the plane stays empty');
  const d = drawn(c, { ink: false, layout: true }), e = drawn(c, { ink: true, layout: true });
  eq([d.fb.length, Buffer.from(d.fb).equals(Buffer.from(e.fb))], [160 * 120, true], 'the lcd2 layout is 160 x 120 and draws the same pixels with or without a plane');
  eq(sha([drawn(c, { ink: false, layout: false }).fb]), sha([a.fb]), 'drawing lcd2 does not leak into the next OLED draw (the grid is restored)');
  assert.throws(() => OLED.draw(new Uint8Array(128 * 64), OLED.create(), { ...c, layout: 'lcd2' }, NOW), /160 x 120/); checks++;
  assert.throws(() => OLED.draw(new Uint8Array(160 * 120), OLED.create(), { ...c, layout: 'lcd2' }, NOW, new Uint8Array(128 * 64)), /as big as/); checks++;
}

// ---- the palette: small, complete, readable ----
eq(Object.keys(P).sort(), ['bad', 'bg', 'dim', 'good', 'neutral', 'warn'], 'the palette: background, neutral, good, warn, bad, dim (the firmware\'s table is tested against exactly this)');
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

// ---- the exact x2: 320 x 240 is covered once, no bands, no centring ----
eq([Lcd2.W, Lcd2.H, Lcd2.SRC_W, Lcd2.SRC_H, Lcd2.SCALE], [320, 240, 160, 120, 2], 'the panel is 320 x 240, the grid 160 x 120, the scale 2');
function canvas() { const img = { data: new Uint8ClampedArray(320 * 240 * 4) }; return { img, ctx: { putImageData() {} } }; }
const px = (img, x, y) => [...img.data.slice(4 * (y * 320 + x), 4 * (y * 320 + x) + 3)];
{
  const full = new Uint8Array(LW * LH).fill(1), none = new Uint8Array(LW * LH), a = canvas(), b = canvas();
  Lcd2.paint(a.ctx, a.img, full, null);
  Lcd2.paint(b.ctx, b.img, none, null);
  let allLit = true, allDark = true, opaque = true;
  for (let i = 0; i < 320 * 240; i++) {
    if (a.img.data[4 * i] !== P.neutral[0] || a.img.data[4 * i + 1] !== P.neutral[1] || a.img.data[4 * i + 2] !== P.neutral[2]) allLit = false;
    if (b.img.data[4 * i] !== P.bg[0] || b.img.data[4 * i + 1] !== P.bg[1] || b.img.data[4 * i + 2] !== P.bg[2]) allDark = false;
    if (a.img.data[4 * i + 3] !== 255 || b.img.data[4 * i + 3] !== 255) opaque = false;
  }
  eq([allLit, allDark, opaque], [true, true, true], 'every one of the 320 x 240 pixels belongs to a source pixel: a full frame leaves no gap and no band, an empty one is all background');
  for (const [sx, sy] of [[0, 0], [159, 0], [0, 119], [159, 119], [80, 60], [37, 101]]) {
    const fb = new Uint8Array(LW * LH), ink = new Uint8Array(LW * LH); fb[sy * LW + sx] = 1; ink[sy * LW + sx] = INK.GOOD;
    const c = canvas(); Lcd2.paint(c.ctx, c.img, fb, ink);
    let lit = [];
    for (let y = 0; y < 240; y++) for (let x = 0; x < 320; x++) if (px(c.img, x, y).join() === P.good.join()) lit.push([x, y]);
    eq(lit, [[2 * sx, 2 * sy], [2 * sx + 1, 2 * sy], [2 * sx, 2 * sy + 1], [2 * sx + 1, 2 * sy + 1]], 'source pixel (' + sx + ', ' + sy + ') is exactly the 2 x 2 block at (' + 2 * sx + ', ' + 2 * sy + '), square, in its ink colour');
  }
}

// ---- helpers that read the pixels: the 5 x 7 font is parsed from ui/oled.js's own table, so a text is checked pixel by pixel ----
const L = OLED.LCD2;
const FONT = {};
for (const m of oledSrc.matchAll(/'(.)': '([.#|]{41})'/g)) FONT[m[1]] = m[2].split('|');
const adv = ch => ('.:,'.includes(ch) ? 3 : ch === ' ' ? 4 : 6);
const textW = s => Math.max(0, [...s].reduce((w, ch) => w + adv(ch), 0) - 1);
const textPoints = s => { const pts = []; let cx = 0; for (const ch of s) { const off = '.:,'.includes(ch) ? 1 : 0; FONT[ch].forEach((row, r) => [...row].forEach((v, c) => { if (v === '#') pts.push([cx + c - off, r]); })); cx += adv(ch); } return pts; };
const litIn = (d, x, y, w, h) => { const out = []; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (d.fb[j * d.w + i]) out.push(d.ink[j * d.w + i]); return out; };
const idsIn = (d, x, y, w, h) => [...new Set(litIn(d, x, y, w, h))].sort((a, b) => a - b);
const litRows = (d, x0 = 0, x1 = d.w) => { const rows = new Set(); for (let j = 0; j < d.h; j++) for (let i = x0; i < x1; i++) if (d.fb[j * d.w + i]) rows.add(j); return rows; };
const litCols = (d, y, h) => { const cols = []; for (let i = 0; i < d.w; i++) for (let j = y; j < y + h; j++) if (d.fb[j * d.w + i]) { cols.push(i); break; } return cols; };
// the text s with its top left at (x, y): exactly its pixels are lit in its box (7 rows), and its ink is `ink` (null: not checked)
function isText(d, s, x, y, ink) {
  const want = new Set(textPoints(s).map(([i, j]) => (y + j) * d.w + x + i)), w = textW(s);
  for (let j = y; j < y + 7; j++) for (let i = x; i < x + w; i++) if ((d.fb[j * d.w + i] === 1) !== want.has(j * d.w + i)) return false;
  return ink === null || [...want].every(k => d.ink[k] === ink);
}
const rightText = (s, xr = 158) => xr - textW(s);

// ---- the labels, the rows: one table, every element's box, and nothing in the gaps ----
const BANDS = [['top bar', 0, 12], ['tempo', L.tempoY, L.tempoY + BigFont.ROWS - 1], ['beat boxes', L.boxY, L.boxY + L.boxH - 1], ['CONFIDENCE', L.rowCF, L.rowCF + 6], ['INPUT', L.rowIN, L.rowIN + 6], ['BAR', L.rowBAR, L.rowBAR + 6], ['state', L.stateY, L.stateY + 6]];
for (let i = 1; i < BANDS.length; i++) ok(BANDS[i][1] > BANDS[i - 1][2], BANDS[i][0] + ' starts below ' + BANDS[i - 1][0] + ' (' + BANDS[i][1] + ' > ' + BANDS[i - 1][2] + '): the rows do not overlap');
eq([L.tempoY + BigFont.ROWS - 1, L.boxY + L.boxH - 1, L.linkBoxY + L.linkBoxH - 1, BANDS[BANDS.length - 1][2], 2 + 4 * L.boxW + 3 * L.boxGap], [49, 82, 118, 118, 158], 'tempo ends at y 49, the boxes at y 82 (LINK: 118), the state line at y 118 (only row 119 is empty), the boxes at x 157');
ok(L.boxY - (L.tempoY + BigFont.ROWS) >= 1 && L.rowCF - (L.boxY + L.boxH) >= 2 && L.linkBoxY - (L.tempoY + BigFont.ROWS) >= 1 && L.rowIN - (L.rowCF + 7) >= 2 && L.rowBAR - (L.rowIN + 7) >= 2 && L.stateY - (L.rowBAR + 7) >= 2, 'the boxes keep a gap to the tempo above and to the CONFIDENCE row below; the three rows and the state line are 2 px apart');
const barOn = { bar: { state: 'locked', conf: 0.5 } };
const full = drawn(ctxOf({ ctx: barOn }));
{
  const rows = litRows(full), inBand = j => BANDS.some(([, a, b]) => j >= a && j <= b);
  ok([...rows].every(inBand), 'every lit pixel of a full HOME lies inside a row of the table (none in the gaps): rows ' + [...rows].filter(j => !inBand(j)).join());
  const tempoRows = litRows(full, 0, 160);
  ok([...tempoRows].filter(j => j >= BANDS[1][1] && j <= BANDS[1][2]).length === 35, 'the tempo fills its 35 rows');
  eq([isText(full, 'CONFIDENCE', L.labelX, L.rowCF, null), isText(full, 'INPUT', L.labelX, L.rowIN, null), isText(full, 'BAR', L.labelX, L.rowBAR, null), isText(full, 'LOCKED', L.labelX + 58 - textW('LOCKED'), L.rowBAR, null)], [true, true, true, true], 'the labels are the words CONFIDENCE, INPUT, BAR with AUTO BAR\'s state, in the 10 character label column (x 2..60)');
  ok(textW('CONFIDENCE') === 59 && L.barX >= 2 + 59 + 2, 'the longest label is 59 px (x 2..60) and the bars start at x ' + L.barX);
  eq(litIn(full, 126, L.rowCF, 3, 7).length + litIn(full, 126, L.rowIN, 3, 7).length, 0, 'between a bar (ends x 125) and its value (from x 129 at the widest) there is nothing');
  eq([litIn(full, 0, L.tempoY, 24, BigFont.ROWS).length, litIn(full, 136, L.tempoY, 24, BigFont.ROWS).length], [0, 0], 'no BPM label: "120.0" is centred, x 24..135, nothing right of it');
}

// ---- the big tempo digits ----
{
  const GL = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '.'];
  eq(Object.keys(BigFont.GLYPHS).sort(), [...GL].sort(), 'the glyphs 0-9, "." and "-", complete');
  for (const ch of GL) {
    const g = BigFont.GLYPHS[ch], w = g[0].length;
    ok(g.length === BigFont.ROWS && g.every(r => r.length === w && /^[.#]+$/.test(r)) && g.some(r => r.includes('#')), '"' + ch + '": ' + BigFont.ROWS + ' rows of equal width, bits only, not empty');
    if (ch !== '.') eq(w, 26, '"' + ch + '": 26 columns wide, the same as every digit');
  }
  eq([BigFont.GLYPHS['.'][0].length, BigFont.GLYPHS['.'].findIndex(r => r.includes('#'))], [5, 30], 'the point is a 5 px dot on the bottom 5 rows');
  for (const ch of GL.slice(0, 10)) ok(BigFont.GLYPHS[ch].slice(0, 3).some(r => r.includes('#')) && BigFont.GLYPHS[ch].slice(-3).some(r => r.includes('#')), '"' + ch + '": uses the top and the bottom of the cell');
  for (const s of ['120.0', '146.1', '88.8', '999.9', '---']) ok(BigFont.width(s) <= 156, '"' + s + '" is ' + BigFont.width(s) + ' px wide, fits the 156 px between the margins');
  eq(BigFont.width('120.0'), 3 * 26 + 5 + 17 + 4 * 3, '"120.0" is 3 big digits + the point + 1 small digit + 4 gaps of 3: 112 px');
  const SM = BigFont.SMALL;
  eq(Object.keys(SM).sort(), GL.slice(0, 10).sort(), 'the small digits 0-9, complete');
  for (const ch of GL.slice(0, 10)) ok(SM[ch].length === 23 && SM[ch].every(r => r.length === 17 && /^[.#]+$/.test(r)) && SM[ch][0].includes('#') && SM[ch][22].includes('#'), 'small "' + ch + '": 23 rows of 17 bits, uses the top and the bottom of its cell');
  eq([BigFont.SMALL_ROWS, 35 * 2 / 3, 26 * 2 / 3], [23, 23.333333333333332, 17.333333333333332], 'the small digits are 2/3 of the big ones: 23 x 17 of 35 x 26');
  eq(SM['8'].map(r => r.indexOf('#')).filter(i => i >= 0).length === 23 && SM['0'][3] === '###...........###' && SM['0'][0] === '.###############.', true, 'small 0: 3 px strokes, 1 px chamfers');
  const tempoWant = (s, x0) => { const w = new Set(); BigFont.draw((x, y) => w.add(y * 160 + x), s, x0, L.tempoY); return w; };
  for (const [s, bpm, left] of [['120.0', 120, 24], ['98.4', 98.4, 38], ['88.8', 88.8, 38], ['146.1', 146.1, 24], ['---', null, 38], ['999.9', 999.9, 24]]) {
    eq([BigFont.width(s), Math.floor((160 - BigFont.width(s)) / 2)], [{ 24: 112, 38: s === '---' ? 84 : 83 }[left], left], '"' + s + '": ' + BigFont.width(s) + ' px wide, left = floor((160 - width) / 2) = ' + left);
    const d = drawn(ctxOf({ home: { bpm } }, {})), want = tempoWant(s, left);
    let same = true; for (let j = L.tempoY; j < L.tempoY + 35; j++) for (let i = 0; i < 160; i++) if ((d.fb[j * 160 + i] === 1) !== want.has(j * 160 + i)) same = false;
    ok(same, 'the tempo "' + s + '" is drawn exactly as the tables say, centred, AUDIO');
    const k = drawn(ctxOf({ home: { audio: false, link: 1, bpm } }, {}));
    let samek = true; for (let j = L.tempoY; j < L.tempoY + 35; j++) for (let i = 0; i < 160; i++) if ((k.fb[j * 160 + i] === 1) !== want.has(j * 160 + i)) samek = false;
    ok(samek, 'the tempo "' + s + '" is the same in the LINK layout');
  }
  { const d = drawn(ctxOf({ home: { bpm: 88.8 } }), {}); let lo = 160, hi = -1; for (let j = L.tempoY; j < L.tempoY + 35; j++) for (let i = 0; i < 160; i++) if (d.fb[j * 160 + i]) { lo = Math.min(lo, i); hi = Math.max(hi, i); }
    eq([lo, hi, 159 - hi], [38, 120, 39], '88.8: lit from x 38 to 120, 38 columns free on the left and 39 on the right (centred; an odd width leaves the spare column on the right)'); }
  { const d = drawn(ctxOf({ home: { bpm: 120 } }), {}), x0 = 24, smallX = x0 + 26 * 3 + 5 + 4 * 3 - 0;
    const lit = (x, y) => d.fb[y * 160 + x];
    let top = 99, bot = -1; for (let j = L.tempoY; j < L.tempoY + 35; j++) for (let i = smallX; i < smallX + 17; i++) if (lit(i, j)) { top = Math.min(top, j); bot = Math.max(bot, j); }
    eq([top, bot], [L.tempoY + 12, L.tempoY + 34], 'the digit after the dot is 23 rows, on the big digits\' baseline (rows 27..49), the dot unchanged'); }
}

// healthy: ink only inside the top bar's link / battery, the first beat box, the CONFIDENCE number and line and the VU bar
const R = { link: [L.linkX, 0, L.linkW, 13], batt: [L.battX, 1, L.battW, 11], beat1: [L.boxX, L.boxY, L.boxW, L.boxH],
            cfValue: [158 - 23, L.rowCF, 23, 7], cfBar: [L.barX, L.rowCF, L.barW, 7], inValue: [158 - 29, L.rowIN, 29, 7], inBar: [L.barX, L.rowIN, L.barW, 7] };
const insideAny = (boxes, x, y) => boxes.some(([bx, by, bw, bh]) => x >= bx && x < bx + bw && y >= by && y < by + bh);
const outsideInk = (d, boxes) => { let n = 0; for (let i = 0; i < d.ink.length; i++) if (d.ink[i] !== 0 && !insideAny(boxes, i % d.w, Math.floor(i / d.w))) n++; return n; };
const healthy = drawn(ctxOf());
ok(healthy.ink.every((v, i) => v === 0 || healthy.fb[i] === 1), 'ink only on lit pixels');
eq(outsideInk(healthy, [R.link, R.batt, R.beat1, R.cfValue, R.cfBar, R.inBar, [2, L.stateY, 60, 7]]), 0, 'a healthy HOME: ink only in the link glyph, battery, the lit first beat, the CONFIDENCE number and line, and the VU bar');
eq(idsIn(healthy, ...R.link), [INK.GOOD], 'joined (2 Link peers): the link glyph and its count are green');
eq(idsIn(healthy, ...R.batt), [INK.GOOD], '80 %: the battery is green');
eq(idsIn(healthy, ...R.beat1), [INK.ONE], 'the first beat box, lit, carries the bar-1 id');
eq([1, 2, 3].map(i => idsIn(healthy, L.boxX + i * (L.boxW + L.boxGap), L.boxY, L.boxW, L.boxH)), [[0], [0], [0]], 'the other boxes are neutral');
eq(idsIn(healthy, ...R.cfValue), [INK.GOOD], 'CONFIDENCE 90 against 70: the value is green');
eq(idsIn(healthy, ...R.inValue), [0], 'an input at -20 dB: the number is neutral');

// the first beat is amber only while it is the lit one
{
  const on = drawn(ctxOf({ ctx: { beatN: 4 } })), off = drawn(ctxOf({ ctx: { beatN: 1 } })), none = drawn(ctxOf({ ctx: { beatN: null } }));
  eq([idsIn(on, ...R.beat1), idsIn(off, ...R.beat1), idsIn(none, ...R.beat1)], [[INK.ONE], [0], [0]], 'beat 4 is the bar\'s first again (amber); beat 2 and no beat: the first box is a neutral frame');
  const box = (d, i) => litIn(d, L.boxX + i * (L.boxW + L.boxGap), L.boxY, L.boxW, L.boxH).length;
  const outline = (w, h) => w * h - (w - 4) * (h - 4);
  eq([box(on, 0), box(on, 1), box(off, 1), box(off, 0)], [L.boxW * L.boxH, outline(L.boxW, L.boxH), L.boxW * L.boxH, outline(L.boxW, L.boxH)], 'the lit box is solid (36 x 31), the others are 2 px outlines');
}

// the four boxes, every beat, both layouts: exact rectangles, solid when lit, 2 px outline when not, even gaps, amber only for a lit first box
for (const [mode, over, y, h] of [['AUDIO', {}, L.boxY, L.boxH], ['LINK', { home: { audio: false, link: 1 } }, L.linkBoxY, L.linkBoxH]]) {
  for (let beat = 0; beat < 4; beat++) {
    const d = drawn(ctxOf({ ...over, ctx: { beatN: beat } }));
    const ids = [], solid = [];
    for (let i = 0; i < 4; i++) {
      const x = L.boxX + i * (L.boxW + L.boxGap), px = (dx, dy) => d.fb[(y + dy) * d.w + x + dx];
      let ring = true, inner = 0, all = true;
      for (let dy = 0; dy < h; dy++) for (let dx = 0; dx < L.boxW; dx++) {
        const edge = dx < 2 || dy < 2 || dx >= L.boxW - 2 || dy >= h - 2;
        if (edge && !px(dx, dy)) ring = false;
        if (!edge) inner += px(dx, dy) ? 1 : 0;
        if (!px(dx, dy)) all = false;
      }
      solid.push(i === beat ? all : !all && ring && inner === 0);
      ids.push(idsIn(d, x, y, L.boxW, h));
    }
    eq(solid, [true, true, true, true], mode + ' beat ' + (beat + 1) + ': box ' + (beat + 1) + ' is solid, the other three are 2 px outlines with a dark inside');
    eq(ids, [0, 1, 2, 3].map(i => (i === 0 && beat === 0 ? [INK.ONE] : [0])), mode + ' beat ' + (beat + 1) + ': ink only on the first box, and only while it is lit');
  }
  const d = drawn(ctxOf({ ...over, ctx: { beatN: 1 } }));
  let gapLit = 0; for (let g = 0; g < 3; g++) for (let k = 0; k < L.boxGap; k++) gapLit += litIn(d, L.boxX + (g + 1) * L.boxW + g * L.boxGap + k, y, 1, h).length;
  eq([gapLit, litIn(d, 0, y, L.boxX, h).length, litIn(d, L.boxX + 4 * L.boxW + 3 * L.boxGap, y, d.w - (L.boxX + 4 * L.boxW + 3 * L.boxGap), h).length], [0, 0, 0], mode + ': the three 4 px gaps and the 2 px margins are dark');
}

// ---- confidence: the value, the line and the notch all carry the id of the rule ----
const confCase = (conf, extra = {}, get) => drawn(ctxOf({ home: { conf }, ...extra }, get ? { get } : undefined));
const TICK = L.barX + Math.round((L.barW - 1) * 0.7);                             // the stop threshold 70 %
for (const [conf, id, why] of [[60, INK.BAD, 'below'], [69, INK.BAD, 'below'], [70, INK.WARN, 'at'], [75, INK.WARN, 'within 10 above'], [80, INK.WARN, '10 above'], [81, INK.GOOD, 'over 10 above'], [87, INK.GOOD, 'above'], [100, INK.GOOD, 'full']]) {
  const d = confCase(conf);
  const bar = idsIn(d, L.barX, L.rowCF, L.barW, 7).filter(v => v !== 0), val = idsIn(d, ...R.cfValue);
  eq([val, bar], [[id], [id]], 'CONFIDENCE ' + conf + ' against 70 (' + why + '): the value and the line are the same colour id (' + id + ')');
}
eq(isText(confCase(87), '87%', rightText('87%'), L.rowCF, INK.GOOD), true, 'the value "87%" is right-aligned to x 157');
eq(idsIn(confCase(75, {}, { threshold: '50', lowconf: 'stop' }), ...R.cfValue), [INK.GOOD], 'the threshold is the stick\'s: 75 against 50 is green');
eq(idsIn(confCase(75, {}, { threshold: '90', lowconf: 'run' }), ...R.cfValue), [INK.BAD], '... against 90 red, also with KEEP RUNNING');
eq(idsIn(confCase(30, { ctx: { silent: true } }), ...R.cfValue), [INK.DIM], 'silence: the number is grey, not red (nothing to track)');
eq(idsIn(drawn(ctxOf({ home: { conf: null } })), ...R.cfValue), [INK.DIM], 'no confidence reported: the dashes are grey');
eq(idsIn(drawn(ctxOf({}, { status: null })), ...R.cfValue), [INK.GOOD], 'no status line: the confidence comes from the HOME line and is still green');
// the line: ONE thin line (3 rows, y + 2..y + 4), its length the value on 62 px; counted on its middle row away from the notch
for (const [conf, n] of [[0, 0], [50, 31], [100, 62]]) {
  const d = confCase(conf), row = L.rowCF + 3, inLine = n > TICK - L.barX;
  let lit = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (x !== TICK && d.fb[row * d.w + x]) lit++;
  eq(lit, n - (inLine ? 1 : 0), 'CONFIDENCE ' + conf + ' %: the line is ' + n + ' px of 62' + (inLine ? ' (one is the notch\'s column)' : ''));
  eq([d.fb[row * d.w + L.barX], d.fb[row * d.w + L.barX + L.barW - 1], d.fb[row * d.w + L.barX - 1]], [n > 0 ? 1 : 0, n === 62 ? 1 : 0, 0], '... it starts at x ' + L.barX + ' and ends at its value, nothing beyond');
}
{
  const d = confCase(50), lineRows = [0, 1, 2, 3, 4, 5, 6].map(k => { let c = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (x !== TICK && d.fb[(L.rowCF + k) * d.w + x]) c++; return c; });
  eq(lineRows, [0, 0, 31, 31, 31, 0, 0], 'CONFIDENCE 50 %: the line is exactly 3 rows (y + 2..y + 4): no outline, no track');
}
{
  const d = confCase(100), x = TICK;
  eq([d.fb[(L.rowCF + 1) * d.w + x], d.fb[(L.rowCF + 5) * d.w + x], d.ink[(L.rowCF + 1) * d.w + x], d.ink[(L.rowCF + 5) * d.w + x]], [1, 1, INK.GOOD, INK.GOOD], 'the notch for 70 % is at x ' + x + ': 1 px wide, 5 px tall (y + 1..y + 5), a stub above and below the line, in the number\'s colour');
  eq([d.fb[(L.rowCF + 2) * d.w + x], d.fb[(L.rowCF + 3) * d.w + x], d.fb[(L.rowCF + 4) * d.w + x], d.fb[L.rowCF * d.w + x], d.fb[(L.rowCF + 6) * d.w + x]], [0, 0, 0, 0, 0], '... and cuts a gap through a line that passes it');
  const e = confCase(40), tickLit = [0, 1, 2, 3, 4, 5, 6].map(k => e.fb[(L.rowCF + k) * e.w + x]);
  eq([tickLit, idsIn(e, x, L.rowCF, 1, 7)], [[0, 1, 1, 1, 1, 1, 0], [INK.BAD]], 'over the empty part the notch is a 5 px line, in red');
  const t2 = confCase(60, {}, { threshold: '30', lowconf: 'stop' }), x2 = L.barX + Math.round((L.barW - 1) * 0.3);
  ok(t2.fb[(L.rowCF + 1) * t2.w + x2] === 1 && t2.ink[(L.rowCF + 1) * t2.w + x2] === INK.GOOD, 'the notch follows the stick\'s threshold (30 %: x ' + x2 + ')');
}

// ---- the INPUT VU meter: the ramp, the fill, the colour of every column, the notch ----
const VUB = L.barX, VT = L.rowIN + 1;                                              // the bar's x and first row (5 rows)
const IN_NOTCH = Math.round((L.barW - 1) * 40 / 90);                               // -50 dBFS on a -90..0 scale: column 27 of the bar
const levelCase = (levelDb, peakDb, max) => drawn(ctxOf({ home: { level: levelDb, levelMax: max === undefined ? levelDb : max }, ctx: { peakDb } }));
const levelIds = d => [idsIn(d, ...R.inValue).filter(v => v !== 0), idsIn(d, L.barX, L.rowIN + 1, L.barW, 5).filter(v => v < OLED.VU.ink0 && v !== 0)];
const colId = i => OLED.VU.ink0 + Math.floor(OLED.VU.steps * i / L.barW);
eq([OLED.VU, Lcd2.VU_RAMP.length], [{ ink0: 6, steps: 32 }, 32], 'the VU ramp: 32 ink ids from 6 (ids 6..37), after the 6 fixed ones');
for (const [i, rgb] of Lcd2.VU_RAMP.entries()) {
  ok(rgb.length === 3 && rgb.every(v => Number.isInteger(v) && v >= 0 && v <= 255), 'ramp step ' + i + ': exact 8-bit RGB ' + rgb);
  ok(Lcd2.contrast(rgb, P.bg) >= 6.5, 'ramp step ' + i + ' reads on the background (' + Lcd2.contrast(rgb, P.bg).toFixed(2) + ' : 1, bars need visibility, not AA text)');
  eq(Lcd2.inkColour(OLED.VU.ink0 + i), rgb, 'ink id ' + (OLED.VU.ink0 + i) + ' is ramp step ' + i);
  if (i > 0) ok(rgb.every((v, k) => Math.abs(v - Lcd2.VU_RAMP[i - 1][k]) <= 16), 'ramp step ' + i + ' differs from the one before by at most 16 per channel (no visible banding)');
}
eq([Lcd2.VU_RAMP[0], Lcd2.VU_RAMP[10], Lcd2.VU_RAMP[20], Lcd2.VU_RAMP[26], Lcd2.VU_RAMP[31]], [P.good, P.good, [176, 230, 64], P.warn, [255, 96, 88]], 'the ramp: green up to step 10, lime at 20, amber at 26, red at 31');
eq(Lcd2.inkColour(OLED.VU.ink0 + 32), P.neutral, 'an id past the ramp is neutral');
eq(levelIds(levelCase(-20, -12)), [[], []], 'a good level: the number is neutral');
eq(levelIds(levelCase(-20, -0.5)), [[INK.BAD], []], 'the last second peaked at -0.5 dBFS: the number is red (clipping)');
eq(levelIds(levelCase(-20, -1)), [[INK.BAD], []], '... -1 dBFS is the edge');
eq(levelIds(levelCase(-20, -1.5)), [[], []], '... -1.5 is not clipping');
eq(levelIds(levelCase(-20, null, -2))[0], [INK.BAD], 'peak not reported: an RMS maximum over -3 dB counts');
eq(levelIds(levelCase(-20, null, -10))[0], [], '... under it, neutral');
eq(levelIds(levelCase(-55, -40))[0], [INK.WARN], 'under the tracker\'s -50 dBFS sound level: the number is amber (too quiet to track)');
eq(levelIds(levelCase(-50, -40))[0], [], '... -50 itself is enough');
eq(isText(levelCase(-20, -12), '-20dB', rightText('-20dB'), L.rowIN, null), true, 'the number "-20dB" is right-aligned to x 157');
{
  const col = db => { const d = levelCase(db, null, db), row = VT + 2, cols = []; for (let x = 0; x < L.barW; x++) if (d.fb[row * d.w + VUB + x]) cols.push(x); return { d, cols }; };
  for (const db of [-90, -60, -45, -30, -12, 0]) {
    const n = Math.round(62 * (db + 90) / 90), { d, cols } = col(db);
    const want = Array.from({ length: n }, (_, i) => i).filter(x => x !== IN_NOTCH).concat(n > IN_NOTCH ? [] : [IN_NOTCH]).sort((a, b) => a - b);
    eq(cols, want, db + ' dB: ' + n + ' continuous columns lit on the bar\'s middle row (no gaps), the notch column XOR-ed (x ' + (VUB + IN_NOTCH) + ')');
    eq(cols.filter(x => x !== IN_NOTCH).every(x => d.ink[(VT + 2) * d.w + VUB + x] === colId(x)), true, db + ' dB: every lit column has the ramp id of its position on the dBFS scale');
  }
  eq([0, 1, 2, 3, 4, 5, 6].map(k => levelCase(-45, null, -45).fb[(L.rowIN + k) * 160 + VUB]), [0, 1, 1, 1, 1, 1, 0], 'the bar is 5 rows high (y + 1..y + 5), no outline, no track');
  eq([0, 1, 2, 3, 4, 5, 6].map(k => col(-90).d.fb[(L.rowIN + k) * 160 + VUB + IN_NOTCH]), [1, 1, 1, 1, 1, 1, 1], 'the -50 dBFS notch is a 7 px column at bar column ' + IN_NOTCH + ' (even over an empty bar)');
  eq([0, 1, 2, 3, 4, 5, 6].map(k => col(-20).d.fb[(L.rowIN + k) * 160 + VUB + IN_NOTCH]), [1, 0, 0, 0, 0, 0, 1], 'through a lit bar the notch cuts a gap and leaves a stub above and below');
  eq(idsIn(col(-90).d, VUB + IN_NOTCH, L.rowIN, 1, 7), [0], 'the notch is neutral');
  eq(litIn(levelCase(-30, -12), VUB + 52, VT, 1, 5).length, 0, 'nothing is drawn beyond the bar\'s end (no peak tick)');
}

// ---- the BAR row: AUTO BAR's state and confidence, neutral, only with AUTO BAR on ----
{
  const rowLit = d => litIn(d, 0, L.rowBAR, d.w, 7).length;
  eq([rowLit(healthy), rowLit(drawn(ctxOf({ ctx: { bar: { state: 'off', conf: 0 } } })))], [0, 0], 'no AUTO BAR (or OFF): no BAR row');
  const lock = drawn(ctxOf({ ctx: { bar: { state: 'locked', conf: 0.5 } } })), lis = drawn(ctxOf({ ctx: { bar: { state: 'listening', conf: 1 } } })), man = drawn(ctxOf({ ctx: { bar: { state: 'manual', conf: 0.9 } } }));
  ok(rowLit(lock) > 100 && rowLit(lis) > rowLit(lock) && rowLit(man) > 0, 'LOCKED, LISTEN, MANUAL draw the row; the fill grows with the confidence');
  eq([lock, lis, man].map(d => idsIn(d, 0, L.rowBAR, d.w, 7)), [[0], [0], [0]], 'the BAR row is neutral');
  eq([isText(lis, 'LISTEN', 2 + 58 - textW('LISTEN'), L.rowBAR, null), isText(man, 'MANUAL', 2 + 58 - textW('MANUAL'), L.rowBAR, null)], [true, true], 'the state words LISTEN and MANUAL end at x 59');
  let lit = 0; const row = L.rowBAR + 3; for (let x = L.barX + 1; x < L.barX + L.barW - 1; x++) if (lock.fb[row * lock.w + x]) lit++;
  eq([lit, lock.fb[(L.rowBAR + 1) * lock.w + L.barX + 40], lock.fb[(L.rowBAR + 5) * lock.w + L.barX + 40]], [30, 1, 1], 'BAR 0.5: the BAR row keeps its outlined bar (a frame above and below, 62 x 5), 30 px of 60 filled');
}

// ---- the top bar: icons 11 high, the status icons right-aligned ----
{
  const cols = d => litCols(d, 0, 13), xs = d => (c => [c[0], c[c.length - 1]])(cols(d).filter(x => x >= L.linkX));
  const d = drawn(ctxOf({ home: { battery: 80 } }));
  eq(xs(d)[1], 157, 'the right group ends at x 157 (the battery\'s nub), two columns from the edge');
  eq([L.linkX + L.linkW <= L.plugX, L.plugX + 13 <= L.battX, L.battX + L.battW], [true, true, 158], 'the right group: link + peers, USB plug, battery follow each other from the right edge without overlap (no signal bars)');
  const rows = [...litRows(d, L.linkX, 160)].filter(j => j < 13); eq([Math.min(...rows), Math.max(...rows)], [1, 11], 'the right group\'s icons are 11 rows high (y 1..11)');
  const pl = [...litRows(d, 2, 11)].filter(j => j < 13);
  eq([Math.min(...pl), Math.max(...pl)], [1, 11], 'the transport icon (play triangle) is 11 rows high');
  ok(isText(d, 'GRP 1', L.optX, 3, null), 'GRP n sits on the left, after the transport icon (no fold, no LINK option in AUDIO at fold 1)');
  const stop = drawn(ctxOf({ ctx: { running: false, armed: false } }));
  eq([litIn(stop, 2, 2, 9, 9).length, litIn(stop, 2, 1, 9, 1).length, litIn(stop, 2, 11, 9, 1).length], [81, 0, 0], 'stopped: a 9 x 9 square at x 2..10, y 2..10');
  const play = drawn(ctxOf({ ctx: { running: true } })); eq([litIn(play, 2, 1, 9, 11).length, litIn(play, 2, 6, 9, 1).length, litIn(play, 2, 1, 9, 1).length], [2 + 3 + 5 + 6 + 8 + 9 + 8 + 6 + 5 + 3 + 2, 9, 2], 'running: the play triangle, 9 wide at its middle row, 2 at its tips');
  const fold = drawn(ctxOf({ home: { fold: '2' }, ctx: { fold: 2 } })); ok(litIn(fold, L.optX, 1, 15, 11).length > 100 && isText(fold, 'GRP 1', L.optX + 15 + 4, 3, null), 'the fold label X2 is an inverted 15 x 11 box, GRP n after it');
  for (const [mode, w] of [[0, 11], [1, 13], [2, 13]]) { const k = drawn(ctxOf({ home: { audio: false, link: 1 }, ctx: { ltMode: mode } })); ok(litIn(k, L.optX, 1, w, 11).length > 20 && litIn(k, L.optX + w, 1, 4, 11).length === 0, 'LINK option icon ' + mode + ' is ' + w + ' x 11 at x ' + L.optX); }
}
eq(idsIn(drawn(ctxOf({ home: { link: 0 } })), ...R.link), [INK.DIM], 'AUDIO mode, no Link peer: dim (a lone leader is normal)');
eq(idsIn(drawn(ctxOf({ home: { link: 0, audio: false } })), ...R.link), [INK.BAD], 'LINK mode, no session peer: red (nothing to follow)');
eq(idsIn(drawn(ctxOf({ home: { link: 1, audio: false } })), ...R.link), [INK.GOOD], 'LINK mode, one peer: green');
{
  const two = drawn(ctxOf({ home: { link: 2 } })), none = drawn(ctxOf({ home: { link: 0 } }));
  ok(isText(two, '2', L.peersX, 3, INK.GOOD) && litIn(none, L.peersX, 3, 12, 7).length === 0, 'the Link peer count is drawn beside the glyph only when there are peers');
}
eq(litCols(healthy, 0, 13).filter(x => x >= 68).every(x => (x >= L.linkX && x < L.linkX + L.linkW) || (x >= L.plugX && x < L.plugX + 13) || (x >= L.battX && x < L.battX + L.battW)), true, 'no signal bars in the top bar: right of the name only the link group, the USB plug and the battery are lit');
const battId = pct => idsIn(drawn(ctxOf({ home: { battery: pct } })), ...R.batt);
eq([battId(100), battId(30), battId(29), battId(15), battId(14), battId(0)], [[INK.GOOD], [INK.GOOD], [INK.WARN], [INK.WARN], [INK.BAD], [INK.BAD]], 'battery: green from 30 %, amber 15..29, red under 15');
eq(idsIn(drawn(ctxOf({ home: { battery: null } })), ...R.batt), [], 'no battery fitted: no icon, no ink');
{
  const fillOf = pct => { const d = drawn(ctxOf({ home: { battery: pct } })); let n = 0; for (let x = L.battX + 2; x < L.battX + 20; x++) if (d.fb[6 * d.w + x]) n++; return n; };
  eq([fillOf(0), fillOf(50), fillOf(100)], [0, 9, 18], 'the battery icon\'s fill: 0, 9 and 18 px of 18');
  const d = drawn(ctxOf({ home: { battery: 100 } }));
  eq([d.fb[1 * d.w + L.battX], d.fb[1 * d.w + L.battX + 1], d.fb[6 * d.w + L.battX + 22], d.fb[4 * d.w + L.battX + 23], d.fb[3 * d.w + L.battX + 23]], [0, 1, 1, 1, 0], 'the battery: a 22 x 11 body with chamfered corners and a 2 x 5 nub');
}

// ---- the state line (AUDIO): always one word, the first state that applies ----
{
  const states = [['GATED', { ctx: { gated: true } }, INK.BAD], ['HOLD', { home: { confHold: true } }, INK.DIM], ['COAST', { home: { conf: 50 }, ctx: { running: true } }, INK.WARN],
                  ['STOPPED', { ctx: { running: false } }, INK.DIM], ['SILENT', { ctx: { running: true, silent: true } }, INK.DIM], ['PLAYING', { ctx: { running: true } }, 0]];
  for (const [word, over, ink] of states) {
    const d = drawn(ctxOf(over));
    eq([isText(d, word, 2, L.stateY, ink), litIn(d, 0, L.stateY, d.w, 7).length === textPoints(word).length, litIn(d, 0, 119, d.w, 1).length], [true, true, 0], word + ': the only thing on the state line, at x 2, in its colour; row 119 empty');
  }
  const first = (over, word) => isText(drawn(ctxOf(over)), word, 2, L.stateY, null);
  eq([first({ ctx: { gated: true, confHold: true, running: true, silent: true } }, 'GATED'), first({ home: { confHold: true }, ctx: { running: false } }, 'HOLD'), first({ home: { conf: 50 }, ctx: { running: true, silent: true } }, 'COAST'), first({ ctx: { running: false, silent: true } }, 'STOPPED'), first({ ctx: { running: true, silent: true } }, 'SILENT')], [true, true, true, true, true], 'the order: GATED, HOLD, COAST, STOPPED, SILENT, PLAYING');
  eq(isText(healthy, 'PLAYING', 2, L.stateY, null), true, 'a healthy HOME (transport playing) says PLAYING');
}

// ---- the LINK layout: the same top bar and tempo, big boxes, no audio rows, no word ----
{
  const link = drawn(ctxOf({ home: { audio: false, link: 1, bpm: 98.4 } }));
  eq([litIn(link, 0, L.linkBoxY + L.linkBoxH, link.w, link.h - L.linkBoxY - L.linkBoxH).length, outsideInk(link, [R.link, R.batt, [L.boxX, L.linkBoxY, L.boxW, L.linkBoxH]])], [0, 0], 'LINK HOME: nothing under the boxes (no CONFIDENCE, INPUT or BAR rows, no state line), no stray ink');
  eq(idsIn(link, L.boxX, L.linkBoxY, L.boxW, L.linkBoxH), [INK.ONE], 'LINK HOME: the first box (36 x 65), lit, is the bar 1');
  eq(litIn(link, L.boxX + 2 * (L.boxW + L.boxGap), L.linkBoxY, L.boxW, L.linkBoxH).length, L.boxW * L.linkBoxH - (L.boxW - 4) * (L.linkBoxH - 4), 'the others are 2 px outlines of the same size');
  eq(litIn(link, L.boxX + 3 * (L.boxW + L.boxGap) + L.boxW - 1, L.linkBoxY, 1, L.linkBoxH).length, L.linkBoxH, 'the fourth box ends at x 157');
  ok(litRows(link, 2, 160).has(L.tempoY) && litRows(link, 2, 160).has(L.tempoY + 34), 'the tempo is the same size as in AUDIO');
}

// ---- menus, edit, info, net, text, confirm, setup: neutral, the whole grid, no ink ----
const noInk = (d, what) => ok(d.fb.some(v => v === 1) && d.ink.every(v => v === 0), what + ': drawn, and no ink at all');
const qr = { size: 25, mod: Array.from({ length: 25 }, (_, y) => Array.from({ length: 25 }, (_, x) => ((x * y + x + y) % 3 === 0 ? 1 : 0))) };
{
  const menu = withUi(ctxOf(), ['no'], 'lcd2');
  let d = drawn(menu.c, { ui: menu.ui });
  noInk(d, 'MENU');
  eq([d.fb[0], d.fb[LW - 1], d.fb[10 * LW], d.fb[11 * LW]], [1, 1, 1, 0], 'the title bar is 11 rows, the full width');
  const rowBand = k => litIn(d, 0, 13 + 9 * k, d.w, 7).length;
  eq([rowBand(0), rowBand(1), rowBand(9)].map(n => n > 0), [true, true, true], 'the 10 menu items are rows of 9 px from y 13 (the last at y ' + (13 + 81) + ')');
  eq([d.fb[(13 - 1) * LW], d.fb[(13 - 1) * LW + LW - 1], d.fb[20 * LW + 2]], [1, 1, 1], 'the selected row (the first) is inverted over the full width');
  eq(litIn(d, 0, 13 + 9 - 1, 1, 9).length, 0, 'the next row is not');
  const lastLit = Math.max(...litRows(d));
  ok(lastLit >= 13 + 9 * 9 && lastLit < LH, 'the list reaches y ' + lastLit + ' of 119: the grid is used');
  const dn = withUi(ctxOf(), ['no', ...Array(9).fill('down')], 'lcd2');
  d = drawn(dn.c, { ui: dn.ui });
  noInk(d, 'MENU, last item selected');
  eq(d.fb[(13 + 81 - 1) * LW + 100], 1, 'the last item (BATTERY) is the selected, inverted row at the bottom');
  const sub = withUi(ctxOf(), ['no', 'yes'], 'lcd2');
  d = drawn(sub.c, { ui: sub.ui });
  noInk(d, 'a sub menu (' + sub.ui.screen + ')');
  const ed = withUi(ctxOf(), ['no', 'yes', 'yes'], 'lcd2');
  d = drawn(ed.c, { ui: ed.ui });
  noInk(d, 'an edit screen (' + ed.ui.screen + ')');
  if (ed.ui.screen === 'edit') {
    const box = [];
    for (let j = 20; j < 60; j++) { let lo = -1, hi = -1; for (let i = 0; i < LW; i++) if (d.fb[j * LW + i]) { if (lo < 0) lo = i; hi = i; } if (lo >= 0 && hi - lo > 30) box.push([lo, hi]); }
    ok(box.length > 0 && Math.abs(box[0][0] - (LW - 1 - box[0][1])) <= 1, 'the edit value box is centred on the 160 px width (x ' + (box[0] || []) + ')');
  }
  const batt = { ...ctxOf(), tempoStep: () => null, setField() {}, setHold() {}, linkStartStop() {} };
  const infoUi = { ...OLED.create(), screen: 'info', page: 'battery' };
  d = drawn(batt, { ui: infoUi }); noInk(d, 'the BATTERY info screen');
  ok(litIn(d, 4, 92, 152, 7).length > 100, '... with its level bar across the width');
  for (const screen of ['netinfo', 'confirm']) { d = drawn(batt, { ui: { ...OLED.create(), screen, ncur: 0 } }); noInk(d, 'the ' + screen + ' screen'); }
  // the NETWORK screen: its rows (TYPE, GROUP, PEERS) and the read-only SIGNAL row under them, the only ink on the screen is the quality word
  const netD = rssi => drawn({ ...batt, rssi }, { ui: { ...OLED.create(), screen: 'net', ncur: 0 } });
  const SY = 13 + 3 * 9;                                             // the SIGNAL row follows the 3 rows of the JANK mode
  for (const [rssi, dbm, word, id] of [[-55, '-55dBm', 'GOOD', INK.GOOD], [-67, '-67dBm', 'GOOD', INK.GOOD], [-68, '-68dBm', 'FAIR', INK.WARN], [-80, '-80dBm', 'FAIR', INK.WARN], [-81, '-81dBm', 'POOR', INK.BAD], [0, null, 'NONE', INK.DIM]]) {
    const n = netD(rssi), wx = 156 - textW(word);
    eq([isText(n, 'SIGNAL', 4, SY, null), isText(n, word, wx, SY, id), dbm === null || isText(n, dbm, wx - 6 - textW(dbm), SY, 0)], [true, true, true], 'NETWORK, signal ' + rssi + ': SIGNAL ' + (dbm || '') + ' ' + word + ' (' + id + ')');
    eq(outsideInk(n, [[wx, SY, textW(word), 7]]), 0, '... the word is the only ink on the screen');
  }
  { const n = netD(null); eq([isText(n, 'SIGNAL', 4, SY, null), isText(n, '-', 156 - textW('-'), SY, 0), n.ink.every(v => v === 0)], [true, true, true], 'NETWORK, signal unknown: SIGNAL and "-", no ink'); }
  eq([litIn(netD(-55), 0, SY + 8, 160, 8).length, litIn(drawn({ ...batt, rssi: -55 }, { ui: { ...OLED.create(), screen: 'net', ncur: 1 } }), 0, SY - 1, 160, 1).length], [0, 0], 'the SIGNAL row is never selected (not inverted) and keeps its place');
  d = drawn({ ...batt, net: { ...batt.net, scan: [{ name: 'home-wifi', bars: 3 }, { name: 'cafe', bars: 1 }, { name: 'x', bars: 2 }] } }, { ui: { ...OLED.create(), screen: 'scan', scur: 1, stop: 0 } }); noInk(d, 'the scan list');
  d = drawn(batt, { ui: { ...OLED.create(), screen: 'text', txt: { field: 'pass', text: 'secret', pos: 2, mask: true } } }); noInk(d, 'the text editor');
  const setupC = { ...batt, setup: true, qr, net: { ...batt.net, setupName: 'jank-setup-1234' } };
  d = drawn(setupC); noInk(d, 'SETUP with its QR code');
  const qrCols = litCols(d, 1, 118), qrRows = [...litRows(d, 0, 100)];
  ok(Math.max(...qrRows) < LH && qrCols[0] === 0 && qrCols.length > 100, 'the QR code (25 modules) is drawn big on the left, inside the grid (x ' + qrCols[0] + '..' + Math.max(...qrCols) + ', y ' + Math.min(...qrRows) + '..' + Math.max(...qrRows) + ')');
  let boxW = 0; while (d.fb[2 * LW + boxW]) boxW++;
  ok(boxW === 3 * 25 + 6 && litIn(d, boxW, 0, LW - boxW, LH).length > 100, 'module size 3: the lit box is ' + boxW + ' px, the text keeps the rest of the width');
  d = drawn({ ...setupC, qr: { size: 33, mod: Array.from({ length: 33 }, () => Array(33).fill(1)) } });
  ok(Math.max(...litRows(d)) < LH, 'a bigger QR code (33 modules) steps down to module size 2 and still fits');
}

// overlays and SAVED: centred on the new grid, nothing coloured under them
{
  const hov = { ...OLED.create(), overlay: { until: NOW + 1000, title: 'DOUBLE', value: '240.0' } };
  let d = drawn(ctxOf(), { ui: hov });
  const rowsLit = j => { let lo = -1, hi = -1; for (let i = 0; i < LW; i++) if (d.fb[j * LW + i]) { if (lo < 0) lo = i; hi = i; } return [lo, hi]; };
  eq(rowsLit(83), [15, 144], 'the overlay box is 130 px wide, centred (15 px each side)');
  eq(idsIn(d, 15, 30, 130, 60), [0], 'no ink under the overlay box');
  ok(litIn(d, 20, 50, 120, 30).length > 100, 'its value is big (scale 4 for 240.0)');
  d = drawn({ ...ctxOf(), saved: true });
  eq([idsIn(d, 35, 42, 90, 34), (() => { const r = [-1, -1]; for (let i = 0; i < LW; i++) if (d.fb[50 * LW + i]) { if (r[0] < 0) r[0] = i; r[1] = i; } return r; })()], [[0], [35, 124]], 'the SAVED box is centred (35 px each side) and carries no ink');
  const hint = { ...OLED.create(), hint: { title: 'PHASE', up: 'UP: +1/2', down: 'DOWN: -1/2' } };
  d = drawn(ctxOf(), { ui: hint });
  eq([rowsLit(83), idsIn(d, 15, 30, 130, 60)], [[15, 144], [0]], 'the NO / YES hint box is centred on the grid too');
}

// ---- BEAT FLASH is gone: the menu is one list of 10 items on both layouts, the control line refuses it, ui/lcd2.js has no flash members ----
{
  const keys = n => ['no', ...Array(n).fill('down')];
  const oledM = withUi(ctxOf(), keys(15)), lcdM = withUi(ctxOf(), keys(15), 'lcd2');
  eq([oledM.ui.cursor, lcdM.ui.cursor], [9, 9], 'the OLED and the lcd2 menu have 10 items (the cursor stops at 9)');
  eq([Stick.fieldCommands('beatflash', 1), Object.keys(Lcd2).filter(k => /flash|ledlit/i.test(k))], [{ refuse: 'the stick does not take beatflash from here' }, []], 'BEAT FLASH is gone: the control line refuses it, ui/lcd2.js has no flash members');
}

// ---- the stick feeds the ctx: nothing made up ----
const fed = (homeOver, status, get = {}) => Stick.ctx({ home: { ...home, ...homeOver }, homeAt: 1000, get, status, statusAt: 1000 }, NOW);
eq([fed({}, { rssi: '-58' }).rssi, fed({}, { rssi: '0' }).rssi, fed({}, null).rssi, fed({}, {}).rssi], [-58, 0, null, null], 'jank mode: the best peer\'s rssi (status rssi), 0 = none, no status = not reported');
eq([fed({ net: { kind: 'wifi', up: true } }, { rssi: '-30', wifi_rssi: '-72' }).rssi, fed({ net: { kind: 'wifi', up: false } }, { wifi_rssi: '-72' }).rssi], [-72, 0], 'Wi-Fi client: the router\'s rssi (status wifi_rssi), 0 while not joined');
eq([fed({}, { peak_db: '-3' }).peakDb, fed({}, {}).peakDb, fed({ conf: 80 }, null).confKnown, fed({ conf: null }, null).confKnown], [-3, null, true, false], 'peak_db and the confidence come as reported, else null / unknown');
eq(fed({ link: 3 }, null).peers, 3, 'the Link peer count is the HOME line\'s');
{
  const c = fed({}, { rssi: '-58' }), model = Stick.create(() => ({ ...c, tempoStep: () => null, setField() {}, setHold() {}, linkStartStop() {} }), () => {});
  const lcd = Stick.screen(model, NOW, 'lcd2'), oled = Stick.screen(model, NOW);
  eq([lcd.w, lcd.h, lcd.fb.length, lcd.ink.length, lcd.ink.some(v => v !== 0)], [160, 120, 19200, 19200, true], 'Stick.screen(model, now, "lcd2"): the 160 x 120 frame buffer and its ink plane, made by the stick');
  eq([oled.w, oled.h, oled.ink, Buffer.from(oled.fb).equals(Buffer.from(Stick.framebuffer(model, NOW)))], [128, 64, null, true], 'Stick.screen without a layout, and Stick.framebuffer: the OLED\'s 128 x 64, no ink');
}

console.log('lcd2: ' + checks + ' checks passed');
