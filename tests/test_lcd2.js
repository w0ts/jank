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
const GOLDEN = JSON.parse('["92586ffb36b70a7d","12a9465b16c68428","f4f15225710bfde3","175c54f575f5b515","9da040822b6d69fc","4df4155e0ba4841d","6d33155d4e1c772f","d7690151e5ad2b33"]');
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
const L = OLED.LCD2, NB = { rowIN: 90, rowCF: 98 }, WB = { rowIN: 86, rowCF: 94, rowBAR: 102 }, AW = 36, AH = 28, X0A = 2;      // the AUDIO rows without / with the BAR row, written out
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
const BANDS = [['top bar', 0, 12], ['tempo', L.tempoY, L.tempoY + 34], ['beat boxes', L.boxY, L.boxY + AH - 1], ['INPUT', WB.rowIN, WB.rowIN + 6], ['CONFIDENCE', WB.rowCF, WB.rowCF + 6], ['BAR', WB.rowBAR, WB.rowBAR + 6], ['state', L.stateY, L.stateY + 6]];
for (let i = 1; i < BANDS.length; i++) ok(BANDS[i][1] > BANDS[i - 1][2], BANDS[i][0] + ' starts below ' + BANDS[i - 1][0] + ' (' + BANDS[i][1] + ' > ' + BANDS[i - 1][2] + '): the rows do not overlap');
const gaps = bands => bands.slice(1).map((b, i) => b[1] - bands[i][2] - 1);
eq(gaps(BANDS), [3, 4, 3, 1, 1, 4], 'with the BAR row, the empty rows between the items: 3 (the top bar band ends at its margin row 12) above the tempo, 4 tempo to boxes, 3 boxes to INPUT, 1 between INPUT, CONFIDENCE and BAR, 4 BAR to the state line');
eq([L.audioBox, L.rowGap, L.boxY, L.stateY], [{ w: 36, h: 28, gap: 4 }, 1, 55, 113], 'the AUDIO box data: 36 x 28, gap 4, y 55; the info rows 1 apart; the state line at y 113');
eq([L.tempoY - 12, L.boxY - (L.tempoY + 35), WB.rowIN - (L.boxY + AH), WB.rowCF - (WB.rowIN + 7), WB.rowBAR - (WB.rowCF + 7), L.stateY - (WB.rowBAR + 7)], [4, 4, 3, 1, 1, 4], 'padding: 4 rows between the icons (y 1..11) and the tempo, 4 between the tempo and the boxes, 3 under the boxes, the info rows 1 apart, 4 above the state line');
eq([L.tempoY + 34, L.boxY + AH - 1, L.stateY + 6, X0A, X0A + 4 * AW + 3 * L.audioBox.gap], [50, 82, 119, 2, 158], 'tempo ends at y 50, the boxes at y 82 (36 x 28, x 2..157 like LINK\'s), the state line at y 119 (the last row of the grid)');
eq(Math.floor((160 - (4 * L.linkBoxSide + 3 * L.boxGap)) / 2), X0A, 'the AUDIO boxes are at the same x positions as LINK\'s (x 2, 42, 82, 122)');
{
  const nb = [['tempo', L.tempoY, L.tempoY + 34], ['beat boxes', L.boxY, L.boxY + AH - 1], ['INPUT', NB.rowIN, NB.rowIN + 6], ['CONFIDENCE', NB.rowCF, NB.rowCF + 6], ['state', L.stateY, L.stateY + 6]];
  eq(gaps(nb), [4, 7, 1, 8], 'without the BAR row: 4 tempo to boxes, 7 boxes to INPUT, 1 between INPUT and CONFIDENCE, 8 CONFIDENCE to the state line');
  // the info block is centred between the boxes (last row 82) and the state line (first row 113): the empty rows above and below differ by at most 1, the odd one is below
  const block = (rows, top, n) => [top - (L.boxY + AH), L.stateY - (top + rows)];
  eq([block(23, WB.rowIN), block(15, NB.rowIN)], [[3, 4], [7, 8]], 'the info block (with BAR 23 rows, without 15) is centred in the 30 rows between the boxes and the state line: 3 above and 4 below, 7 above and 8 below (an odd leftover goes below)');
}
const barOn = { bar: { state: 'locked', conf: 0.5 } };
const full = drawn(ctxOf({ ctx: barOn }));
{
  const rows = litRows(full), inBand = j => BANDS.some(([, a, b]) => j >= a && j <= b);
  ok([...rows].every(inBand), 'every lit pixel of a full HOME lies inside a row of the table (none in the gaps): rows ' + [...rows].filter(j => !inBand(j)).join());
  const tempoRows = litRows(full, 0, 160);
  ok([...tempoRows].filter(j => j >= BANDS[1][1] && j <= BANDS[1][2]).length === 35, 'the tempo fills its 35 rows');
  eq([isText(full, 'CONFIDENCE', L.labelX, WB.rowCF, null), isText(full, 'INPUT', L.labelX, WB.rowIN, null), isText(full, 'BAR', L.labelX, WB.rowBAR, null), isText(full, 'LOCKED', L.labelX + 58 - textW('LOCKED'), WB.rowBAR, null)], [true, true, true, true], 'the labels are the words CONFIDENCE, INPUT, BAR with AUTO BAR\'s state, in the 10 character label column (x 2..60)');
  ok(textW('CONFIDENCE') === 59 && L.barX >= 2 + 59 + 2, 'the longest label is 59 px (x 2..60) and the bars start at x ' + L.barX);
  eq(litIn(full, 126, WB.rowCF, 3, 7).length + litIn(full, 126, WB.rowIN, 3, 7).length, 0, 'between a bar (ends x 125) and its value (from x 129 at the widest) there is nothing');
  eq([litIn(full, 0, L.tempoY, 24, BigFont.NORMAL.ROWS).length, litIn(full, 136, L.tempoY, 24, BigFont.NORMAL.ROWS).length], [0, 0], 'no BPM label: "120.0" is centred, x 24..135, nothing right of it');
}

// ---- the big tempo digits ----
{
  const GL = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '.'];
  eq(Object.keys(BigFont.NORMAL.GLYPHS).sort(), [...GL].sort(), 'the glyphs 0-9, "." and "-", complete');
  for (const ch of GL) {
    const g = BigFont.NORMAL.GLYPHS[ch], w = g[0].length;
    ok(g.length === BigFont.NORMAL.ROWS && g.every(r => r.length === w && /^[.#]+$/.test(r)) && g.some(r => r.includes('#')), '"' + ch + '": ' + BigFont.NORMAL.ROWS + ' rows of equal width, bits only, not empty');
    if (ch !== '.') eq(w, 26, '"' + ch + '": 26 columns wide, the same as every digit');
  }
  eq([BigFont.NORMAL.GLYPHS['.'][0].length, BigFont.NORMAL.GLYPHS['.'].findIndex(r => r.includes('#'))], [5, 30], 'the point is a 5 px dot on the bottom 5 rows');
  for (const ch of GL.slice(0, 10)) ok(BigFont.NORMAL.GLYPHS[ch].slice(0, 3).some(r => r.includes('#')) && BigFont.NORMAL.GLYPHS[ch].slice(-3).some(r => r.includes('#')), '"' + ch + '": uses the top and the bottom of the cell');
  for (const s of ['120.0', '146.1', '88.8', '999.9', '---']) ok(BigFont.NORMAL.width(s) <= 156, '"' + s + '" is ' + BigFont.NORMAL.width(s) + ' px wide, fits the 156 px between the margins');
  eq(BigFont.NORMAL.width('120.0'), 3 * 26 + 5 + 17 + 4 * 3, '"120.0" is 3 big digits + the point + 1 small digit + 4 gaps of 3: 112 px');
  const SM = BigFont.NORMAL.SMALL;
  eq(Object.keys(SM).sort(), GL.slice(0, 10).sort(), 'the small digits 0-9, complete');
  for (const ch of GL.slice(0, 10)) ok(SM[ch].length === 23 && SM[ch].every(r => r.length === 17 && /^[.#]+$/.test(r)) && SM[ch][0].includes('#') && SM[ch][22].includes('#'), 'small "' + ch + '": 23 rows of 17 bits, uses the top and the bottom of its cell');
  eq([BigFont.NORMAL.SMALL_ROWS, 35 * 2 / 3, 26 * 2 / 3], [23, 23.333333333333332, 17.333333333333332], 'the small digits are 2/3 of the big ones: 23 x 17 of 35 x 26');
  eq(SM['8'].map(r => r.indexOf('#')).filter(i => i >= 0).length === 23 && SM['0'][3] === '###...........###' && SM['0'][0] === '.###############.', true, 'small 0: 3 px strokes, 1 px chamfers');
  const tempoWant = (s, x0) => { const w = new Set(); BigFont.NORMAL.draw((x, y) => w.add(y * 160 + x), s, x0, L.tempoY); return w; };
  for (const [s, bpm, left] of [['120.0', 120, 24], ['98.4', 98.4, 38], ['88.8', 88.8, 38], ['146.1', 146.1, 24], ['---', null, 38], ['999.9', 999.9, 24]]) {
    eq([BigFont.NORMAL.width(s), Math.floor((160 - BigFont.NORMAL.width(s)) / 2)], [{ 24: 112, 38: s === '---' ? 84 : 83 }[left], left], '"' + s + '": ' + BigFont.NORMAL.width(s) + ' px wide, left = floor((160 - width) / 2) = ' + left);
    const d = drawn(ctxOf({ home: { bpm } }, {})), want = tempoWant(s, left);
    let same = true; for (let j = L.tempoY; j < L.tempoY + 35; j++) for (let i = 0; i < 160; i++) if ((d.fb[j * 160 + i] === 1) !== want.has(j * 160 + i)) same = false;
    ok(same, 'the tempo "' + s + '" is drawn exactly as the tables say, centred, AUDIO');
  }
  { const d = drawn(ctxOf({ home: { bpm: 88.8 } }), {}); let lo = 160, hi = -1; for (let j = L.tempoY; j < L.tempoY + 35; j++) for (let i = 0; i < 160; i++) if (d.fb[j * 160 + i]) { lo = Math.min(lo, i); hi = Math.max(hi, i); }
    eq([lo, hi, 159 - hi], [38, 120, 39], '88.8: lit from x 38 to 120, 38 columns free on the left and 39 on the right (centred; an odd width leaves the spare column on the right)'); }
  // the LARGE font (LINK): 47 x 35 big digits, the small digit 31 x 23, the point 7 x 7, gap 4
  const LG = BigFont.LARGE;
  eq([LG.ROWS, LG.SMALL_ROWS, LG.GAP], [47, 31, 4], 'LARGE: 47 rows, the small digit 31 rows, gap 4');
  eq(Object.keys(LG.GLYPHS).sort(), [...GL].sort(), 'LARGE: the glyphs 0-9, "." and "-", complete');
  for (const ch of GL) { const g = LG.GLYPHS[ch], w = g[0].length; ok(g.length === 47 && g.every(r => r.length === w && /^[.#]+$/.test(r)) && g.some(r => r.includes('#')), 'LARGE "' + ch + '": 47 rows of equal width, bits only'); if (ch !== '.') eq(w, 35, 'LARGE "' + ch + '": 35 columns'); }
  eq([LG.GLYPHS['.'][0].length, LG.GLYPHS['.'].findIndex(r => r.includes('#'))], [7, 40], 'LARGE: the point is a 7 x 7 dot on the bottom rows');
  eq(Object.keys(LG.SMALL).sort(), GL.slice(0, 10).sort(), 'LARGE: the small digits 0-9, complete');
  for (const ch of GL.slice(0, 10)) ok(LG.SMALL[ch].length === 31 && LG.SMALL[ch].every(r => r.length === 23 && /^[.#]+$/.test(r)) && LG.GLYPHS[ch].slice(0, 3).some(r => r.includes('#')) && LG.GLYPHS[ch].slice(-3).some(r => r.includes('#')), 'LARGE "' + ch + '": 31 x 23 small digit, the big one uses the top and bottom of its cell');
  eq([47 * 2 / 3, 35 * 2 / 3].map(Math.round), [31, 23], 'the LARGE small digit keeps the 2/3 ratio (31 of 47, 23 of 35)');
  eq([LG.GLYPHS['0'][0], LG.GLYPHS['0'][10], LG.SMALL['0'][0]], ['...#############################...', '#######.....................#######', '..###################..'], 'LARGE 0: 7 px strokes, 3 px corner cuts; small 0: 5 px strokes, 2 px cuts');
  eq(['120.0', '98.4', '88.8', '146.1', '---', '999.9'].map(t => LG.width(t)), [151, 112, 112, 151, 113, 151], 'LARGE widths: 120.0 = 3 x 35 + 7 + 23 + 4 x 4 = 151, all fit 156');
  for (const [t, bpm] of [['120.0', 120], ['98.4', 98.4], ['88.8', 88.8], ['146.1', 146.1], ['---', null]]) {
    const left = Math.floor((160 - LG.width(t)) / 2), want = new Set(); LG.draw((x, y) => want.add(y * 160 + x), t, left, L.linkTempoY);
    const k = drawn(ctxOf({ home: { audio: false, link: 1, bpm } }, {}));
    let same = true; for (let j = L.linkTempoY; j < L.linkTempoY + 47; j++) for (let i = 0; i < 160; i++) if ((k.fb[j * 160 + i] === 1) !== want.has(j * 160 + i)) same = false;
    ok(same && left === Math.floor((160 - LG.width(t)) / 2), 'LINK: the tempo "' + t + '" is drawn with the LARGE tables, centred (left ' + left + ', width ' + LG.width(t) + ')');
  }
  { const d = drawn(ctxOf({ home: { bpm: 120 } }), {}), x0 = 24, smallX = x0 + 26 * 3 + 5 + 4 * 3 - 0;
    const lit = (x, y) => d.fb[y * 160 + x];
    let top = 99, bot = -1; for (let j = L.tempoY; j < L.tempoY + 35; j++) for (let i = smallX; i < smallX + 17; i++) if (lit(i, j)) { top = Math.min(top, j); bot = Math.max(bot, j); }
    eq([top, bot], [L.tempoY + 12, L.tempoY + 34], 'the digit after the dot is 23 rows, on the big digits\' baseline (rows 28..50), the dot unchanged'); }
}

// healthy: ink only inside the top bar's link / battery, the first beat box, the CONFIDENCE number and line and the VU bar
const R = { link: [L.linkX, 0, L.linkW, 13], batt: [L.battX, 1, L.battW, 11], beat1: [X0A, L.boxY, AW, AH],
            cfValue: [158 - 23, NB.rowCF, 23, 7], cfBar: [L.barX, NB.rowCF, L.barW, 7], inValue: [158 - 29, NB.rowIN, 29, 7], inBar: [L.barX, NB.rowIN, L.barW, 7] };
const insideAny = (boxes, x, y) => boxes.some(([bx, by, bw, bh]) => x >= bx && x < bx + bw && y >= by && y < by + bh);
const outsideInk = (d, boxes) => { let n = 0; for (let i = 0; i < d.ink.length; i++) if (d.ink[i] !== 0 && !insideAny(boxes, i % d.w, Math.floor(i / d.w))) n++; return n; };
const healthy = drawn(ctxOf());
ok(healthy.ink.every((v, i) => v === 0 || healthy.fb[i] === 1), 'ink only on lit pixels');
eq(outsideInk(healthy, [R.link, R.batt, R.beat1, R.cfValue, R.cfBar, R.inBar, [2, L.stateY, 60, 7]]), 0, 'a healthy HOME: ink only in the link glyph, battery, the lit first beat, the CONFIDENCE number and line, and the VU bar');
eq(idsIn(healthy, ...R.link), [INK.GOOD], 'joined (2 Link peers): the link glyph and its count are green');
eq(idsIn(healthy, ...R.batt), [0], '80 %: the battery is neutral (colour only when it informs)');
eq(idsIn(healthy, ...R.beat1), [INK.ONE], 'the first beat box, lit, carries the bar-1 id');
eq([1, 2, 3].map(i => idsIn(healthy, X0A + i * (AW + L.audioBox.gap), L.boxY, AW, AH)), [[0], [0], [0]], 'the other boxes are neutral');
eq(idsIn(healthy, ...R.cfValue), [INK.GOOD], 'CONFIDENCE 90 against 70: the value is green');
eq(idsIn(healthy, ...R.inValue), [0], 'an input at -20 dB: the number is neutral');

// the first beat is amber only while it is the lit one
{
  const on = drawn(ctxOf({ ctx: { beatN: 4 } })), off = drawn(ctxOf({ ctx: { beatN: 1 } })), none = drawn(ctxOf({ ctx: { beatN: null } }));
  eq([idsIn(on, ...R.beat1), idsIn(off, ...R.beat1), idsIn(none, ...R.beat1)], [[INK.ONE], [0], [0]], 'beat 4 is the bar\'s first again (amber); beat 2 and no beat: the first box is a neutral frame');
  const box = (d, i) => litIn(d, X0A + i * (AW + L.audioBox.gap), L.boxY, AW, AH).length;
  const outline = (w, h) => w * h - (w - 4) * (h - 4);
  eq([box(on, 0), box(on, 1), box(off, 1), box(off, 0)], [AW * AH, outline(AW, AH), AW * AH, outline(AW, AH)], 'the lit box is solid (36 x 28), the others are 2 px outlines');
}

// the four boxes, every beat, both layouts: exact rectangles, solid when lit, 2 px outline when not, even gaps, amber only for a lit first box
for (const [mode, over, y, h, w] of [['AUDIO', {}, L.boxY, AH, AW], ['AUDIO (BAR row)', { ctx: barOn }, L.boxY, AH, AW], ['LINK', { home: { audio: false, link: 1 } }, L.linkBoxY, L.linkBoxSide, L.linkBoxSide]]) {
  const W0 = w, X0 = Math.floor((160 - (4 * W0 + 3 * L.boxGap)) / 2);
  for (let beat = 0; beat < 4; beat++) {
    const d = drawn(ctxOf({ ...over, ctx: { ...(over.ctx || {}), beatN: beat } }));
    const ids = [], solid = [];
    for (let i = 0; i < 4; i++) {
      const x = X0 + i * (W0 + L.boxGap), px = (dx, dy) => d.fb[(y + dy) * d.w + x + dx];
      let ring = true, inner = 0, all = true;
      for (let dy = 0; dy < h; dy++) for (let dx = 0; dx < W0; dx++) {
        const edge = dx < 2 || dy < 2 || dx >= W0 - 2 || dy >= h - 2;
        if (edge && !px(dx, dy)) ring = false;
        if (!edge) inner += px(dx, dy) ? 1 : 0;
        if (!px(dx, dy)) all = false;
      }
      solid.push(i === beat ? all : !all && ring && inner === 0);
      ids.push(idsIn(d, x, y, W0, h));
    }
    eq(solid, [true, true, true, true], mode + ' beat ' + (beat + 1) + ': box ' + (beat + 1) + ' is solid, the other three are 2 px outlines with a dark inside');
    eq(ids, [0, 1, 2, 3].map(i => (i === 0 && beat === 0 ? [INK.ONE] : [0])), mode + ' beat ' + (beat + 1) + ': ink only on the first box, and only while it is lit');
  }
  const d = drawn(ctxOf({ ...over, ctx: { ...(over.ctx || {}), beatN: 1 } }));
  let gapLit = 0; for (let g = 0; g < 3; g++) for (let k = 0; k < L.boxGap; k++) gapLit += litIn(d, X0 + (g + 1) * W0 + g * L.boxGap + k, y, 1, h).length;
  eq([gapLit, litIn(d, 0, y, X0, h).length, litIn(d, X0 + 4 * W0 + 3 * L.boxGap, y, d.w - (X0 + 4 * W0 + 3 * L.boxGap), h).length], [0, 0, 0], mode + ': the three 4 px gaps and the side margins are dark; ' + W0 + ' x ' + h + '');
}

// ---- confidence: the value, the line and the notch all carry the id of the rule ----
const confCase = (conf, extra = {}, get) => drawn(ctxOf({ home: { conf }, ...extra }, get ? { get } : undefined));
const TICK = L.barX + Math.round((L.barW - 1) * 0.7);                             // the stop threshold 70 %
for (const [conf, id, why] of [[60, INK.BAD, 'below'], [69, INK.BAD, 'below'], [70, INK.WARN, 'at'], [75, INK.WARN, 'within 10 above'], [80, INK.WARN, '10 above'], [81, INK.GOOD, 'over 10 above'], [87, INK.GOOD, 'above'], [100, INK.GOOD, 'full']]) {
  const d = confCase(conf);
  const bar = idsIn(d, L.barX, NB.rowCF, L.barW, 7).filter(v => v !== 0), val = idsIn(d, ...R.cfValue);
  eq([val, bar], [[id], [id]], 'CONFIDENCE ' + conf + ' against 70 (' + why + '): the value and the line are the same colour id (' + id + ')');
}
eq(isText(confCase(87), '87%', rightText('87%'), NB.rowCF, INK.GOOD), true, 'the value "87%" is right-aligned to x 157');
eq(idsIn(confCase(75, {}, { threshold: '50', lowconf: 'stop' }), ...R.cfValue), [INK.GOOD], 'the threshold is the stick\'s: 75 against 50 is green');
eq(idsIn(confCase(75, {}, { threshold: '90', lowconf: 'run' }), ...R.cfValue), [INK.BAD], '... against 90 red, also with KEEP RUNNING');
eq(idsIn(confCase(30, { ctx: { silent: true } }), ...R.cfValue), [INK.DIM], 'silence: the number is grey, not red (nothing to track)');
eq(idsIn(drawn(ctxOf({ home: { conf: null } })), ...R.cfValue), [INK.DIM], 'no confidence reported: the dashes are grey');
eq(idsIn(drawn(ctxOf({}, { status: null })), ...R.cfValue), [INK.GOOD], 'no status line: the confidence comes from the HOME line and is still green');
// the line: ONE thin line (3 rows, y + 2..y + 4), its length the value on 62 px; counted on its middle row away from the notch
for (const [conf, n] of [[0, 0], [50, 31], [100, 62]]) {
  const d = confCase(conf), row = NB.rowCF + 3, inLine = n > TICK - L.barX;
  let lit = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (x !== TICK && d.fb[row * d.w + x]) lit++;
  eq(lit, n - (inLine ? 1 : 0), 'CONFIDENCE ' + conf + ' %: the line is ' + n + ' px of 62' + (inLine ? ' (one is the notch\'s column)' : ''));
  eq([d.fb[row * d.w + L.barX], d.fb[row * d.w + L.barX + L.barW - 1], d.fb[row * d.w + L.barX - 1]], [n > 0 ? 1 : 0, n === 62 ? 1 : 0, 0], '... it starts at x ' + L.barX + ' and ends at its value, nothing beyond');
}
{
  const d = confCase(50), lineRows = [0, 1, 2, 3, 4, 5, 6].map(k => { let c = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (x !== TICK && d.fb[(NB.rowCF + k) * d.w + x]) c++; return c; });
  eq(lineRows, [0, 0, 31, 31, 31, 0, 0], 'CONFIDENCE 50 %: the line is exactly 3 rows (y + 2..y + 4): no outline, no track');
}
{
  const d = confCase(100), x = TICK;
  eq([d.fb[(NB.rowCF + 1) * d.w + x], d.fb[(NB.rowCF + 5) * d.w + x], d.ink[(NB.rowCF + 1) * d.w + x], d.ink[(NB.rowCF + 5) * d.w + x]], [1, 1, INK.GOOD, INK.GOOD], 'the notch for 70 % is at x ' + x + ': 1 px wide, 5 px tall (y + 1..y + 5), a stub above and below the line, in the number\'s colour');
  eq([d.fb[(NB.rowCF + 2) * d.w + x], d.fb[(NB.rowCF + 3) * d.w + x], d.fb[(NB.rowCF + 4) * d.w + x], d.fb[NB.rowCF * d.w + x], d.fb[(NB.rowCF + 6) * d.w + x]], [0, 0, 0, 0, 0], '... and cuts a gap through a line that passes it');
  const e = confCase(40), tickLit = [0, 1, 2, 3, 4, 5, 6].map(k => e.fb[(NB.rowCF + k) * e.w + x]);
  eq([tickLit, idsIn(e, x, NB.rowCF, 1, 7)], [[0, 1, 1, 1, 1, 1, 0], [INK.BAD]], 'over the empty part the notch is a 5 px line, in red');
  const t2 = confCase(60, {}, { threshold: '30', lowconf: 'stop' }), x2 = L.barX + Math.round((L.barW - 1) * 0.3);
  ok(t2.fb[(NB.rowCF + 1) * t2.w + x2] === 1 && t2.ink[(NB.rowCF + 1) * t2.w + x2] === INK.GOOD, 'the notch follows the stick\'s threshold (30 %: x ' + x2 + ')');
}

// ---- the INPUT VU meter: the ramp, the fill, the colour of every column, the notch ----
const VUB = L.barX, VT = NB.rowIN + 1;                                              // the bar's x and first row (5 rows)
const IN_NOTCH = Math.round((L.barW - 1) * 40 / 90);                               // -50 dBFS on a -90..0 scale: column 27 of the bar
const levelCase = (levelDb, peakDb, max) => drawn(ctxOf({ home: { level: levelDb, levelMax: max === undefined ? levelDb : max }, ctx: { peakDb } }));
const levelIds = d => [idsIn(d, ...R.inValue).filter(v => v !== 0), idsIn(d, L.barX, NB.rowIN + 1, L.barW, 5).filter(v => v < OLED.VU.ink0 && v !== 0)];
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
eq(isText(levelCase(-20, -12), '-20dB', rightText('-20dB'), NB.rowIN, null), true, 'the number "-20dB" is right-aligned to x 157');
{
  const col = db => { const d = levelCase(db, null, db), row = VT + 2, cols = []; for (let x = 0; x < L.barW; x++) if (d.fb[row * d.w + VUB + x]) cols.push(x); return { d, cols }; };
  for (const db of [-90, -60, -45, -30, -12, 0]) {
    const n = Math.round(62 * (db + 90) / 90), { d, cols } = col(db);
    const want = Array.from({ length: n }, (_, i) => i).filter(x => x !== IN_NOTCH).concat(n > IN_NOTCH ? [] : [IN_NOTCH]).sort((a, b) => a - b);
    eq(cols, want, db + ' dB: ' + n + ' continuous columns lit on the bar\'s middle row (no gaps), the notch column XOR-ed (x ' + (VUB + IN_NOTCH) + ')');
    eq(cols.filter(x => x !== IN_NOTCH).every(x => d.ink[(VT + 2) * d.w + VUB + x] === colId(x)), true, db + ' dB: every lit column has the ramp id of its position on the dBFS scale');
  }
  eq([0, 1, 2, 3, 4, 5, 6].map(k => levelCase(-45, null, -45).fb[(NB.rowIN + k) * 160 + VUB]), [0, 1, 1, 1, 1, 1, 0], 'the bar is 5 rows high (y + 1..y + 5), no outline, no track');
  eq([0, 1, 2, 3, 4, 5, 6].map(k => col(-90).d.fb[(NB.rowIN + k) * 160 + VUB + IN_NOTCH]), [1, 1, 1, 1, 1, 1, 1], 'the -50 dBFS notch is a 7 px column at bar column ' + IN_NOTCH + ' (even over an empty bar)');
  eq([0, 1, 2, 3, 4, 5, 6].map(k => col(-20).d.fb[(NB.rowIN + k) * 160 + VUB + IN_NOTCH]), [1, 0, 0, 0, 0, 0, 1], 'through a lit bar the notch cuts a gap and leaves a stub above and below');
  eq(idsIn(col(-90).d, VUB + IN_NOTCH, NB.rowIN, 1, 7), [0], 'the notch is neutral');
  eq(litIn(levelCase(-30, -12), VUB + 52, VT, 1, 5).length, 0, 'nothing is drawn beyond the bar\'s end (no peak tick)');
}

// ---- the BAR row: AUTO BAR's state and confidence, neutral, only with AUTO BAR on ----
{
  const rowLit = d => litIn(d, 0, WB.rowBAR, d.w, 7).length;
  const barLabel = d => isText(d, 'BAR', 2, WB.rowBAR, null);
  eq([barLabel(healthy), barLabel(drawn(ctxOf({ ctx: { bar: { state: 'off', conf: 0 } } }))), isText(healthy, 'CONFIDENCE', 2, NB.rowCF, null)], [false, false, true], 'no AUTO BAR (or OFF): no BAR row, the rows close up (CONFIDENCE takes the slot after INPUT, the boxes grow)');
  const lock = drawn(ctxOf({ ctx: { bar: { state: 'locked', conf: 0.5 } } })), lis = drawn(ctxOf({ ctx: { bar: { state: 'listening', conf: 1 } } })), man = drawn(ctxOf({ ctx: { bar: { state: 'manual', conf: 0.9 } } }));
  ok(rowLit(lock) > 100 && rowLit(lis) > rowLit(lock) && rowLit(man) > 0, 'LOCKED, LISTEN, MANUAL draw the row; the fill grows with the confidence');
  eq([lock, lis, man].map(d => idsIn(d, 0, WB.rowBAR, d.w, 7)), [[0], [0], [0]], 'the BAR row is neutral');
  eq([isText(lis, 'LISTEN', 2 + 58 - textW('LISTEN'), WB.rowBAR, null), isText(man, 'MANUAL', 2 + 58 - textW('MANUAL'), WB.rowBAR, null)], [true, true], 'the state words LISTEN and MANUAL end at x 59');
  let lit = 0; const row = WB.rowBAR + 3; for (let x = L.barX + 1; x < L.barX + L.barW - 1; x++) if (lock.fb[row * lock.w + x]) lit++;
  eq([lit, lock.fb[(WB.rowBAR + 1) * lock.w + L.barX + 40], lock.fb[(WB.rowBAR + 5) * lock.w + L.barX + 40]], [30, 1, 1], 'BAR 0.5: the BAR row keeps its outlined bar (a frame above and below, 62 x 5), 30 px of 60 filled');
}

// ---- the top bar: icons 11 high. Left: transport icon x 2, LINK TRANSPORT icon x 14, fold label x 31. Right: GRP n, link glyph + peers, battery ----
const LT_W = [11, 13, 13];
const grpAt = text => L.nameXR - textW(text);
{
  const d = drawn(ctxOf({ home: { battery: 80 }, ctx: { usb: false } }));
  const cols = litCols(d, 0, 13);
  eq(cols[cols.length - 1], 157, 'the right group ends at x 157 (the battery\'s nub), two columns from the edge');
  eq([L.ltX, L.foldX, L.nameXR, L.linkX, L.linkW, L.peersX, L.battX, L.battW], [14, 31, 99, 103, 27, 118, 134, 24], 'the top bar slots: LINK icon x 14, fold x 31, name right edge 99, link glyph x 103 (slot 27), peers x 118, battery x 134 (24 wide)');
  ok(L.ltX + 13 < L.foldX && L.foldX + 15 < L.nameXR - textW('GRP 1') && L.nameXR + 4 === L.linkX && L.linkX + L.linkW + 4 === L.battX && L.battX + L.battW === 158, 'no overlaps: the left group ends at x 45, GRP 1 spans x 70..98, then 4 px, the link slot x 103..129, 4 px, the battery x 134..157');
  const rows = [...litRows(d, L.linkX, 160)].filter(j => j < 13); eq([Math.min(...rows), Math.max(...rows)], [1, 11], 'the right group\'s icons are 11 rows high (y 1..11)');
  const pl = [...litRows(d, 2, 11)].filter(j => j < 13);
  eq([Math.min(...pl), Math.max(...pl)], [1, 11], 'the transport icon (play triangle) is 11 rows high');
  eq([isText(d, 'GRP 1', grpAt('GRP 1'), 3, null), litIn(d, 46, 0, grpAt('GRP 1') - 47, 13).length, litIn(d, L.nameXR, 0, L.linkX - L.nameXR, 13).length], [true, 0, 0], 'GRP n sits next to the link glyph (right-aligned to x 98, 4 px before it); the middle of the bar is free (x 46..' + (grpAt('GRP 1') - 2) + ')');
  eq(litIn(d, 11, 0, 3, 13).length + litIn(d, 2, 0, 9, 1).length, 0, 'nothing at the old GRP place right after the transport icon but the LINK icon (x 14)');
  const stop = drawn(ctxOf({ ctx: { running: false, armed: false } }));
  eq([litIn(stop, 2, 2, 9, 9).length, litIn(stop, 2, 1, 9, 1).length, litIn(stop, 2, 11, 9, 1).length], [81, 0, 0], 'stopped: a 9 x 9 square at x 2..10, y 2..10');
  const play = drawn(ctxOf({ ctx: { running: true } })); eq([litIn(play, 2, 1, 9, 11).length, litIn(play, 2, 6, 9, 1).length, litIn(play, 2, 1, 9, 1).length], [2 + 3 + 5 + 6 + 8 + 9 + 8 + 6 + 5 + 3 + 2, 9, 2], 'running: the play triangle, 9 wide at its middle row, 2 at its tips');
  // the LINK TRANSPORT icon is in the bar in both modes, the same pixels; the fold label (AUDIO) sits next to it
  for (const mode of [0, 1, 2]) {
    const au = drawn(ctxOf({ ctx: { ltMode: mode } })), lk = drawn(ctxOf({ home: { audio: false, link: 1 }, ctx: { ltMode: mode } }));
    const same = [...Array(11 * 15).keys()].every(k => au.fb[(1 + Math.floor(k / 15)) * 160 + L.ltX + (k % 15)] === lk.fb[(1 + Math.floor(k / 15)) * 160 + L.ltX + (k % 15)]);
    ok(litIn(au, L.ltX, 1, LT_W[mode], 11).length > 20 && litIn(au, L.ltX + LT_W[mode], 1, 4, 11).length === 0 && same, 'LINK TRANSPORT icon ' + mode + ' (' + ['IGNORE', 'FOLLOW', 'BOTH'][mode] + '): ' + LT_W[mode] + ' x 11 at x ' + L.ltX + ', in AUDIO and in LINK mode');
  }
  eq([0, 1, 2].map(m => new Set([0, 1, 2].map(n => sha([drawn(ctxOf({ ctx: { ltMode: n } })).fb]))).size), [3, 3, 3], 'the three LINK TRANSPORT icons differ');
  const fold = drawn(ctxOf({ home: { fold: '2' }, ctx: { fold: 2 } }));
  ok(litIn(fold, L.foldX, 1, 15, 11).length > 100 && litIn(fold, L.foldX + 15, 1, 4, 11).length === 0 && litIn(fold, L.ltX, 1, 13, 11).length > 20, 'the fold label X2 is an inverted 15 x 11 box at x 31, next to the LINK icon');
  eq(litIn(drawn(ctxOf({ home: { audio: false, link: 1, fold: '2' }, ctx: { fold: 2 } })), L.foldX, 1, 15, 11).length, 0, 'no fold label in LINK mode');
  const wifi = drawn({ ...ctxOf(), net: { ...ctxOf().net, mode: 1, ssid: 'studio-wifi-5G' } });
  ok(isText(wifi, 'studio-w', grpAt('studio-w'), 3, null), 'the Wi-Fi name takes the place of GRP n: right-aligned to x 98, cut to 8 characters');
}
eq(idsIn(drawn(ctxOf({ home: { link: 0 } })), ...R.link), [INK.DIM], 'AUDIO mode, no Link peer: dim (a lone leader is normal)');
eq(idsIn(drawn(ctxOf({ home: { link: 0, audio: false } })), ...R.link), [INK.BAD], 'LINK mode, no session peer: red (nothing to follow)');
eq(idsIn(drawn(ctxOf({ home: { link: 1, audio: false } })), ...R.link), [INK.GOOD], 'LINK mode, one peer: green');
{
  const two = drawn(ctxOf({ home: { link: 2 } })), none = drawn(ctxOf({ home: { link: 0 } }));
  ok(isText(two, '2', L.peersX, 3, INK.GOOD) && litIn(none, L.peersX, 3, 12, 7).length === 0, 'the Link peer count is drawn beside the glyph only when there are peers');
  ok(idsIn(two, grpAt('GRP 1'), 3, textW('GRP 1'), 7).every(v => v === 0), 'GRP n is neutral');
}
const noPlug = ctx => ctxOf({ ctx: { usb: false, ...ctx } });
const battId = pct => idsIn(drawn(ctxOf({ home: { battery: pct }, ctx: { usb: false } })), ...R.batt);
eq([battId(100), battId(80), battId(30), battId(29), battId(15), battId(14), battId(0)], [[0], [0], [0], [INK.WARN], [INK.WARN], [INK.BAD], [INK.BAD]], 'battery: neutral from 30 %, amber under 30 (BATT_WARN_PCT), red under 15 (BATT_BAD_PCT)');
eq(idsIn(drawn(ctxOf({ home: { battery: null } })), ...R.batt), [], 'no battery fitted: no icon, no ink');
{
  const fillOf = pct => { const d = drawn(ctxOf({ home: { battery: pct }, ctx: { usb: false } })); let n = 0; for (let x = L.battX + 2; x < L.battX + 20; x++) if (d.fb[6 * d.w + x]) n++; return n; };
  eq([fillOf(0), fillOf(50), fillOf(100)], [0, 9, 18], 'the battery icon\'s fill: 0, 9 and 18 px of 18');
  const d = drawn(ctxOf({ home: { battery: 100 }, ctx: { usb: false } }));
  eq([d.fb[1 * d.w + L.battX], d.fb[1 * d.w + L.battX + 1], d.fb[6 * d.w + L.battX + 22], d.fb[4 * d.w + L.battX + 23], d.fb[3 * d.w + L.battX + 23]], [0, 1, 1, 1, 0], 'the battery: a 22 x 11 body with chamfered corners and a 2 x 5 nub');
  // charging (the BATTERY info screen's inference: on USB, a cell fitted, under 100 %): the plug (9 x 7) inside the battery, XOR-ed over the fill; no separate plug icon
  const PLUG = ['..####...', '..#######', '#########', '######...', '#########', '..#######', '..####...'];
  for (const pct of [10, 50, 99]) {
    const on = drawn(ctxOf({ home: { battery: pct }, ctx: { usb: true } })), off = drawn(ctxOf({ home: { battery: pct }, ctx: { usb: false } }));
    let ok1 = true; for (let j = 0; j < 7; j++) for (let i = 0; i < 9; i++) { const k = (3 + j) * 160 + L.battX + 6 + i; if (on.fb[k] !== (off.fb[k] ^ (PLUG[j][i] === '#' ? 1 : 0))) ok1 = false; }
    let rest = true; for (let k = 0; k < 160 * 13; k++) { const x = k % 160, y = Math.floor(k / 160), inPlug = x >= L.battX + 6 && x < L.battX + 15 && y >= 3 && y < 10; if (!inPlug && on.fb[k] !== off.fb[k]) rest = false; }
    ok(ok1 && rest, 'charging at ' + pct + ' %: only the 9 x 7 plug inside the battery changes (dark over the fill, lit over the empty part)');
  }
  const full100 = drawn(ctxOf({ home: { battery: 100 }, ctx: { usb: true } }));
  eq(sha([full100.fb]), sha([d.fb]), 'USB plugged but full (100 %): no plug (the INFO screen says FULL)');
  eq(litIn(drawn(ctxOf({ home: { battery: null, link: 0 }, ctx: { usb: true } })), 118, 0, 40, 13).length, 0, 'no separate USB plug icon: with USB plugged and no cell nothing is drawn right of the link glyph');
  eq(idsIn(drawn(ctxOf({ home: { battery: 50 }, ctx: { usb: true } })), ...R.batt), [0], 'charging at 50 %: the plug carries the battery\'s ink (neutral)');
  eq(idsIn(drawn(ctxOf({ home: { battery: 12 }, ctx: { usb: true } })), ...R.batt), [INK.BAD], 'charging at 12 %: battery and plug red');
}

// ---- the state line (AUDIO): one word, centred, the first state that applies; empty while simply running; its row is always reserved ----
{
  const states = [['GATED', { ctx: { gated: true } }, INK.BAD], ['HOLD', { home: { confHold: true } }, INK.DIM], ['COAST', { home: { conf: 50 }, ctx: { running: true } }, INK.WARN],
                  ['STOPPED', { ctx: { running: false } }, INK.DIM], ['SILENT', { ctx: { running: true, silent: true } }, INK.DIM]];
  for (const [word, over, ink] of states) {
    const d = drawn(ctxOf(over)), x = Math.floor((160 - textW(word)) / 2);
    eq([isText(d, word, x, L.stateY, ink), litIn(d, 0, L.stateY, d.w, 7).length === textPoints(word).length], [true, true], word + ': the only thing on the state line, centred at x ' + x + ' = floor((160 - ' + textW(word) + ') / 2), in its colour');
    eq([x, 159 - (x + textW(word) - 1) - (x - 0) >= -1], [x, true], word + ': the left and right margins differ by at most 1 px');
  }
  const first = (over, word) => isText(drawn(ctxOf(over)), word, Math.floor((160 - textW(word)) / 2), L.stateY, null);
  eq([first({ ctx: { gated: true, confHold: true, running: true, silent: true } }, 'GATED'), first({ home: { confHold: true }, ctx: { running: false } }, 'HOLD'), first({ home: { conf: 50 }, ctx: { running: true, silent: true } }, 'COAST'), first({ ctx: { running: false, silent: true } }, 'STOPPED'), first({ ctx: { running: true, silent: true } }, 'SILENT')], [true, true, true, true, true], 'the order: GATED, HOLD, COAST, STOPPED, SILENT');
  eq([litIn(healthy, 0, L.stateY, 160, 7).length, litIn(drawn(ctxOf({ ctx: barOn.bar ? { running: true, bar: barOn.bar } : {} })), 0, L.stateY, 160, 7).length], [0, 0], 'a plain running state has no word: the line is empty (no PLAYING), with and without the BAR row');
  eq(litIn(drawn(ctxOf({ ctx: { gated: true } })), 0, 111, 160, 2).length, 0, 'the state line keeps its place (y 113..119) whether a word is shown or not: the gap above it is empty');
}

// ---- the margins around the tempo, measured on the LIT pixels of drawn frames: above (last lit row of the top bar to the first lit row of the digits) == below (last lit row of the digits to the first lit row of the squares) ----
{
  const litRowsOf = (d, x0, x1, y0, y1) => { const r = []; for (let j = y0; j <= y1; j++) for (let i = x0; i < x1; i++) if (d.fb[j * d.w + i]) { r.push(j); break; } return r; };
  const margins = (d, tempoTop, boxTop) => {
    const bar = litRowsOf(d, 0, 160, 0, tempoTop - 1), digits = litRowsOf(d, 0, 160, tempoTop, boxTop - 1), box = litRowsOf(d, 0, 160, boxTop, boxTop);
    return { above: digits[0] - bar[bar.length - 1] - 1, below: boxTop - digits[digits.length - 1] - 1, first: digits[0], last: digits[digits.length - 1], box: box[0] };
  };
  for (const [t, bpm] of [['120.0', 120], ['98.4', 98.4], ['88.8', 88.8], ['146.1', 146.1], ['133.3', 133.3], ['999.9', 999.9]]) {
    const a = margins(drawn(ctxOf({ home: { bpm } })), L.tempoY - 3, L.boxY), k = margins(drawn(ctxOf({ home: { audio: false, link: 1, bpm } })), L.linkTempoY - 3, L.linkBoxY);
    eq([a.above, a.below, a.first, a.last], [4, 4, 16, 50], 'AUDIO "' + t + '": lit margins above (top bar y 11 to the digits y 16) and below (digits to y 50, squares y 55) are both 4 rows');
    eq([k.above, k.below, k.first, k.last], [7, 7, 19, 65], 'LINK "' + t + '": lit margins above (y 11 to y 19) and below (y 65 to the squares at y 73) are both 7 rows');
  }
  { const a = margins(drawn(ctxOf({ home: { bpm: null } })), L.tempoY - 3, L.boxY), k = margins(drawn(ctxOf({ home: { audio: false, link: 1, bpm: null } })), L.linkTempoY - 3, L.linkBoxY);
    eq([a.above, a.below, k.above, k.below], [19, 19, 26, 27], '"---" (only the dash rows are lit, in the middle of the digit cell): AUDIO 19 above and 19 below; LINK 26 above and 27 below (the 8 dash rows cannot sit exactly in the middle of 47)'); }
}

// ---- the LINK layout: the same top bar, the LARGE tempo, four SQUARE beat boxes, no audio rows, no state line ----
{
  const link = drawn(ctxOf({ home: { audio: false, link: 1, bpm: 98.4 } })), S = L.linkBoxSide, x0 = Math.floor((160 - (4 * S + 3 * L.boxGap)) / 2);
  eq([S, L.linkBoxY, x0, 4 * S + 3 * L.boxGap], [36, 73, 2, 156], 'LINK: four square boxes 36 x 36 with 4 px gaps, a row of 156 centred (x 2..157), at y 73..108');
  eq([L.linkTempoY - 12, L.linkBoxY - (L.linkTempoY + 47), 120 - (L.linkBoxY + S)], [7, 7, 11], 'LINK: the LARGE tempo (y 19..65) has 7 empty rows above (under the top bar icons) and 7 below (to the squares); the squares keep their place, 11 empty rows under them');
  for (let i = 0; i < 4; i++) {
    const x = x0 + i * (S + L.boxGap), on = drawn(ctxOf({ home: { audio: false, link: 1 }, ctx: { beatN: i } }));
    let solid = true, ring = true, inner = 0; for (let dy = 0; dy < S; dy++) for (let dx = 0; dx < S; dx++) { const v = on.fb[(L.linkBoxY + dy) * 160 + x + dx], edge = dx < 2 || dy < 2 || dx >= S - 2 || dy >= S - 2; if (!v) solid = false; if (edge && !v) ring = false; if (!edge && v) inner++; }
    const off = drawn(ctxOf({ home: { audio: false, link: 1 }, ctx: { beatN: (i + 1) % 4 } }));
    let offRing = true, offInner = 0; for (let dy = 0; dy < S; dy++) for (let dx = 0; dx < S; dx++) { const v = off.fb[(L.linkBoxY + dy) * 160 + x + dx], edge = dx < 2 || dy < 2 || dx >= S - 2 || dy >= S - 2; if (edge && !v) offRing = false; if (!edge && v) offInner++; }
    eq([solid, offRing && offInner === 0, idsIn(on, x, L.linkBoxY, S, S)], [true, true, i === 0 ? [INK.ONE] : [0]], 'LINK beat ' + (i + 1) + ': box ' + (i + 1) + ' is a solid 36 x 36 square (the first amber only while lit), unlit it is a 2 px outline; width == height');
  }
  eq([0, 1, 2].map(g => litIn(link, x0 + (g + 1) * S + g * L.boxGap, L.linkBoxY, L.boxGap, S).filter(() => true).length), [0, 0, 0], 'LINK: the three 4 px gaps between the boxes are dark');
  eq([litIn(link, 0, L.linkBoxY + S, link.w, 120 - L.linkBoxY - S).length, outsideInk(link, [R.link, R.batt, [x0, L.linkBoxY, 4 * S + 12, S]])], [0, 0], 'LINK HOME: nothing under the boxes (no CONFIDENCE, INPUT or BAR rows, no state line), no stray ink');
  ok(litRows(link, 0, 160).has(L.linkTempoY + 46) && !litRows(link, 0, 160).has(L.linkTempoY + 47) && !litRows(link, 0, 160).has(L.linkBoxY - 1), 'the LARGE tempo fills rows 19..65 and nothing lies between it and the boxes');
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
  eq([rowBand(0), rowBand(1), rowBand(10)].map(n => n > 0), [true, true, true], 'the 11 menu items are rows of 9 px from y 13 (the last at y ' + (13 + 90) + ')');
  eq([d.fb[(13 - 1) * LW], d.fb[(13 - 1) * LW + LW - 1], d.fb[20 * LW + 2]], [1, 1, 1], 'the selected row (the first) is inverted over the full width');
  eq(litIn(d, 0, 13 + 9 - 1, 1, 9).length, 0, 'the next row is not');
  const lastLit = Math.max(...litRows(d));
  ok(lastLit >= 13 + 9 * 10 && lastLit < LH, 'the list reaches y ' + lastLit + ' of 119: the grid is used');
  const dn = withUi(ctxOf(), ['no', ...Array(10).fill('down')], 'lcd2');
  d = drawn(dn.c, { ui: dn.ui });
  noInk(d, 'MENU, last item selected');
  eq(d.fb[(13 + 90 - 1) * LW + 100], 1, 'the last item (ABOUT) is the selected, inverted row at the bottom');
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
  const aboutUi = { ...OLED.create(), screen: 'info', page: 'about' };
  const about = { tempC: 41, cpuPct: 37, heapKb: 123, psramKb: 7936, flashKb: 8192, appUsedKb: 1260, appKb: 3072, uptimeS: 3725, heapMinKb: 98, reset: 'POWERON', version: '0.1.0', clkLateUs: 12, uiLoopUs: 4210, beatLoopUs: 8800 };
  d = drawn({ ...batt, ...about }, { ui: aboutUi }); noInk(d, 'the ABOUT info screen');
  ok(isText(d, 'NAME jank', 4, 18, null) && isText(d, 'REPO github.com/w0ts/jank', 4, 18 + 24, null) && isText(d, 'TEMP 41 C', 4, 18 + 60, null) && isText(d, 'MEMORY 123K PSRAM 7936K', 4, 18 + 84, null), '... its rows from y 18, 12 px apart (8 of 13 show)');
  d = drawn({ ...batt, ...about }, { ui: { ...aboutUi, ascroll: 5 } });
  ok(isText(d, 'FLASH 8M 1.2/3.0M', 4, 18 + 48, null) && isText(d, 'BEAT LOOP 8800 US', 4, 18 + 84, null), '... scrolled by 5: the last row at the bottom');
  d = drawn({ ...batt, ...Object.fromEntries(Object.keys(about).map(k => [k, null])) }, { ui: aboutUi });
  ok(isText(d, 'VERSION -', 4, 30, null) && isText(d, 'TEMP -', 4, 18 + 60, null), '... "-" for what is not known');
  const sc = (k, c) => { const u = { ...OLED.create(), screen: 'info', page: 'about' }; for (const b of k) OLED.press(u, b, 0, c); return u.ascroll; };
  eq([sc(['down', 'down', 'down', 'down', 'down', 'down', 'up'], { ...batt, layout: 'lcd2' }), sc(['down', 'down', 'down', 'down', 'down', 'down', 'down', 'down', 'up'], batt), sc(['up'], batt)], [4, 7, 0], 'ABOUT: DOWN / UP scroll, stopped at the ends (5 on lcd2, 8 on the OLED)');
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
  eq([litIn(netD(-55), 0, SY + 7, 160, 2).length, litIn(drawn({ ...batt, rssi: -55 }, { ui: { ...OLED.create(), screen: 'net', ncur: 1 } }), 0, SY - 1, 160, 1).length], [0, 0], 'the SIGNAL row is never selected (not inverted) and keeps its place');
  // the air rows under SIGNAL (JANK mode only): LOST, LEVEL, RX RETRY; the quality word and the percentages are the only ink
  const AY = SY + 9, airD = (air, over = {}) => drawn({ ...batt, rssi: -55, ...air, ...over }, { ui: { ...OLED.create(), screen: 'net', ncur: 0 } });
  const right = (s, y) => [156 - textW(s), y];
  const airRows = (n, lost, lostInk, level, retry, retryInk) => [isText(n, 'LOST', 4, AY, null), isText(n, lost, ...right(lost, AY), lostInk), isText(n, 'LEVEL', 4, AY + 9, null),
    isText(n, level, ...right(level, AY + 9), 0), isText(n, 'RX RETRY', 4, AY + 18, null), isText(n, retry, ...right(retry, AY + 18), retryInk)];
  for (const [lost, id] of [[0, INK.GOOD], [2, INK.GOOD], [3, INK.WARN], [9, INK.WARN], [10, INK.BAD], [100, INK.BAD]]) {
    const n = airD({ airLost: lost, airRetry: 1, rssiMin: -78, rssiMax: -52 });
    eq(airRows(n, lost + '%', id, '-78/-52 dBm', '1%', 0), Array(6).fill(true), 'NETWORK, LOST ' + lost + '%: the label, the value in ink ' + id + ', LEVEL -78/-52 dBm and RX RETRY 1% neutral');
    eq(outsideInk(n, [[156 - textW('GOOD'), SY, textW('GOOD'), 7], [156 - textW(lost + '%'), AY, textW(lost + '%'), 7]]), 0, '... the SIGNAL word and LOST are the only ink on the screen');
  }
  for (const [retry, id] of [[0, 0], [29, 0], [30, INK.BAD], [100, INK.BAD]]) {
    const n = airD({ airLost: 0, airRetry: retry, rssiMin: -60, rssiMax: -60 });
    eq(airRows(n, '0%', INK.GOOD, '-60/-60 dBm', retry + '%', id), Array(6).fill(true), 'NETWORK, RX RETRY ' + retry + '%: ink ' + id);
  }
  { const n = airD({ airLost: null, airRetry: null, rssiMin: 0, rssiMax: 0 }, { rssi: null });
    eq(airRows(n, '-', 0, '-', '-', 0), Array(6).fill(true), 'NETWORK, the air not known: LOST, LEVEL and RX RETRY read "-"');
    eq(n.ink.every(v => v === 0), true, '... and there is no ink'); }
  for (const mode of [1, 2]) {
    const n = airD({ airLost: 20, airRetry: 40, rssiMin: -80, rssiMax: -50 }, { rssi: null, net: { ...batt.net, mode } });
    eq([litIn(n, 0, AY + 9, 160, 20).length, n.ink.every(v => v === 0)], [0, true], 'NETWORK, mode ' + mode + ': no air rows, no ink');
  }
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
  eq(rowsLit(52), [15, 144], 'the overlay box is 130 px wide, centred (15 px each side)');
  eq(idsIn(d, 15, 30, 130, 60), [0], 'no ink under the overlay box');
  ok(litIn(d, 20, 50, 120, 30).length > 100, 'its value is big (scale 4 for 240.0)');
  d = drawn({ ...ctxOf(), saved: true });
  eq([idsIn(d, 35, 42, 90, 34), (() => { const r = [-1, -1]; for (let i = 0; i < LW; i++) if (d.fb[52 * LW + i]) { if (r[0] < 0) r[0] = i; r[1] = i; } return r; })()], [[0], [35, 124]], 'the SAVED box is centred (35 px each side) and carries no ink');
  const hint = { ...OLED.create(), hint: { title: 'PHASE', up: 'UP: +1/2', down: 'DOWN: -1/2' } };
  d = drawn(ctxOf(), { ui: hint });
  eq([rowsLit(52), idsIn(d, 15, 30, 130, 60)], [[15, 144], [0]], 'the NO / YES hint box is centred on the grid too');
}

// ---- BEAT FLASH is gone: the menu is one list of 11 items on both layouts, the control line refuses it, ui/lcd2.js has no flash members ----
{
  const keys = n => ['no', ...Array(n).fill('down')];
  const oledM = withUi(ctxOf(), keys(15)), lcdM = withUi(ctxOf(), keys(15), 'lcd2');
  eq([oledM.ui.cursor, lcdM.ui.cursor], [10, 10], 'the OLED and the lcd2 menu have 11 items (the cursor stops at 10)');
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
