// SPDX-License-Identifier: MIT
// The colour face of the lcd2 variant: drawn at the panel's own 320 x 240 pixels (ui/oled.js, ctx.layout 'lcd2'), painted 1:1
// (ui/lcd2.js). Tested here: the palette (WCAG AA), the sizes and the 1:1 paint, every element's box and the rows (no overlaps),
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
const P = Lcd2.PALETTE, AA = 4.5, INK = OLED.INK, OW = OLED.W, OH = OLED.H, LW = 320, LH = 240;

// ---- fixtures: the HOME line, `get`, `status` as the stick reports them ----
const home = { bpm: 120, net: { kind: 'jank', group: 1 }, transport: 'playing', bar: 100, quantum: 4, link: 2, air: 1, battery: 80, level: -20, conf: 90, confHold: false, levelMin: -25, levelMax: -15, audio: true, fold: '' };
const NOW = 1100;
function ctxOf(over = {}, { get = { threshold: '70', lowconf: 'stop' }, status = { rssi: '-55', peak_db: '-12' }, beat = null } = {}) {
  const c = Stick.ctx({ home: { ...home, ...(over.home || {}) }, homeAt: 1000, get, status, statusAt: 1000, beat }, NOW);
  return { ...c, beatN: 0, ...(over.ctx || {}) };
}
// draws on the layout's own grid; returns { fb, ink, w, h }
function drawn(c, { ink = true, layout = true, ui = null } = {}) {
  const size = layout ? { w: 320, h: 240 } : { w: 128, h: 64 }, fb = new Uint8Array(size.w * size.h), plane = new Uint8Array(size.w * size.h);
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
eq([OLED.W, OLED.H, OLED.size(), OLED.size('oled'), OLED.size('lcd2')], [128, 64, { w: 128, h: 64 }, { w: 128, h: 64 }, { w: 320, h: 240 }], 'sizes: OLED.W / OLED.H stay the OLED\'s; OLED.size(layout) says the grid of a layout');
{
  const c = ctxOf(), a = drawn(c, { ink: false, layout: false }), b = drawn(c, { ink: true, layout: false });
  eq([a.fb.length, Buffer.from(b.fb).equals(Buffer.from(a.fb)), b.ink.every(v => v === 0)], [128 * 64, true, true], 'OLED layout with an ink plane handed in: same bytes, and the plane stays empty');
  const d = drawn(c, { ink: false, layout: true }), e = drawn(c, { ink: true, layout: true });
  eq([d.fb.length, Buffer.from(d.fb).equals(Buffer.from(e.fb))], [320 * 240, true], 'the lcd2 layout is 320 x 240 and draws the same pixels with or without a plane');
  eq(sha([drawn(c, { ink: false, layout: false }).fb]), sha([a.fb]), 'drawing lcd2 does not leak into the next OLED draw (the grid is restored)');
  assert.throws(() => OLED.draw(new Uint8Array(128 * 64), OLED.create(), { ...c, layout: 'lcd2' }, NOW), /320 x 240/); checks++;
  assert.throws(() => OLED.draw(new Uint8Array(320 * 240), OLED.create(), { ...c, layout: 'lcd2' }, NOW, new Uint8Array(128 * 64)), /as big as/); checks++;
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

// ---- the 1:1 paint: 320 x 240 is covered once, no bands, no centring, no scaling ----
eq([Lcd2.W, Lcd2.H, Lcd2.SRC_W, Lcd2.SRC_H, Lcd2.SCALE], [320, 240, 320, 240, 1], 'the panel is 320 x 240, the frame 320 x 240, one frame pixel per panel pixel');
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
  eq([allLit, allDark, opaque], [true, true, true], 'every one of the 320 x 240 pixels is painted: a full frame leaves no gap and no band, an empty one is all background');
  for (const [sx, sy] of [[0, 0], [319, 0], [0, 239], [319, 239], [160, 120], [37, 201]]) {
    const fb = new Uint8Array(LW * LH), ink = new Uint8Array(LW * LH); fb[sy * LW + sx] = 1; ink[sy * LW + sx] = INK.GOOD;
    const c = canvas(); Lcd2.paint(c.ctx, c.img, fb, ink);
    let lit = [];
    for (let y = 0; y < 240; y++) for (let x = 0; x < 320; x++) if (px(c.img, x, y).join() === P.good.join()) lit.push([x, y]);
    eq(lit, [[sx, sy]], 'frame pixel (' + sx + ', ' + sy + ') is exactly the panel pixel (' + sx + ', ' + sy + '), in its ink colour');
  }
}

// ---- helpers that read the pixels: the 5 x 7 font is parsed from ui/oled.js's own table and doubled with Scale2x (BigFont.scale2x), so a text is checked pixel by pixel ----
const L = OLED.LCD2, NB = { rowIN: 181, rowCF: 197 }, WB = { rowIN: 173, rowCF: 189, rowBAR: 205 }, AW = 72, AH = 56, X0A = 4;      // the AUDIO rows without / with the BAR row, written out
const FONT = {};
for (const m of oledSrc.matchAll(/'(.)': '([.#|]{41})'/g)) FONT[m[1]] = m[2].split('|');
const adv = ch => ('.:,'.includes(ch) ? 6 : ch === ' ' ? 8 : 12);
const textW = s => Math.max(0, [...s].reduce((w, ch) => w + adv(ch), 0) - 1);
const textPoints = s => { const pts = []; let cx = 0; for (const ch of s) { const off = '.:,'.includes(ch) ? 2 : 0; BigFont.scale2x(FONT[ch]).forEach((row, r) => [...row].forEach((v, c) => { if (v === '#') pts.push([cx + 1 + c - off, r]); })); cx += adv(ch); } return pts; };
const litIn = (d, x, y, w, h) => { const out = []; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (d.fb[j * d.w + i]) out.push(d.ink[j * d.w + i]); return out; };
const idsIn = (d, x, y, w, h) => [...new Set(litIn(d, x, y, w, h))].sort((a, b) => a - b);
const litRows = (d, x0 = 0, x1 = d.w) => { const rows = new Set(); for (let j = 0; j < d.h; j++) for (let i = x0; i < x1; i++) if (d.fb[j * d.w + i]) rows.add(j); return rows; };
const litCols = (d, y, h) => { const cols = []; for (let i = 0; i < d.w; i++) for (let j = y; j < y + h; j++) if (d.fb[j * d.w + i]) { cols.push(i); break; } return cols; };
// the text s with its top left at (x, y): exactly its pixels are lit in its box (14 rows), and its ink is `ink` (null: not checked)
function isText(d, s, x, y, ink) {
  const want = new Set(textPoints(s).map(([i, j]) => (y + j) * d.w + x + i)), w = textW(s);
  for (let j = y; j < y + 14; j++) for (let i = x; i < x + w; i++) if ((d.fb[j * d.w + i] === 1) !== want.has(j * d.w + i)) return false;
  return ink === null || [...want].every(k => d.ink[k] === ink);
}
const rightText = (s, xr = 316) => xr - textW(s);

// ---- the labels, the rows: one table, every element's box, and nothing in the gaps ----
const BANDS = [['top bar', 0, 25], ['tempo', L.tempoY, L.tempoY + 69], ['beat boxes', L.boxY, L.boxY + AH - 1], ['INPUT', WB.rowIN, WB.rowIN + 13], ['CONFIDENCE', WB.rowCF, WB.rowCF + 13], ['BAR', WB.rowBAR, WB.rowBAR + 13], ['state', L.stateY, L.stateY + 13]];
for (let i = 1; i < BANDS.length; i++) ok(BANDS[i][1] > BANDS[i - 1][2], BANDS[i][0] + ' starts below ' + BANDS[i - 1][0] + ' (' + BANDS[i][1] + ' > ' + BANDS[i - 1][2] + '): the rows do not overlap');
const gaps = bands => bands.slice(1).map((b, i) => b[1] - bands[i][2] - 1);
eq(gaps(BANDS), [6, 8, 7, 2, 2, 7], 'with the BAR row, the empty rows between the items: 6 under the top bar band (y 0..25), 8 tempo to boxes, 7 boxes to INPUT, 2 between INPUT, CONFIDENCE and BAR, 7 BAR to the state line');
eq([L.audioBox, L.rowGap, L.boxY, L.stateY], [{ w: 72, h: 56, gap: 8 }, 2, 110, 226], 'the AUDIO box data: 72 x 56, gap 8, y 110; the info rows 2 apart; the state line at y 226');
eq([L.tempoY - 24, L.boxY - (L.tempoY + 70), WB.rowIN - (L.boxY + AH), WB.rowCF - (WB.rowIN + 14), WB.rowBAR - (WB.rowCF + 14), L.stateY - (WB.rowBAR + 14)], [8, 8, 7, 2, 2, 7], 'padding: 8 rows between the icons (y 2..23) and the tempo, 8 between the tempo and the boxes, 7 under the boxes, the info rows 2 apart, 7 above the state line');
eq([L.tempoY + 69, L.boxY + AH - 1, L.stateY + 13, X0A, X0A + 4 * AW + 3 * L.audioBox.gap], [101, 165, 239, 4, 316], 'tempo ends at y 101, the boxes at y 165 (72 x 56, x 4..315 like LINK\'s), the state line at y 239 (the last row of the frame)');
eq(Math.floor((320 - (4 * L.linkBoxSide + 3 * L.boxGap)) / 2), X0A, 'the AUDIO boxes are at the same x positions as LINK\'s (x 4, 84, 164, 244)');
{
  const nb = [['tempo', L.tempoY, L.tempoY + 69], ['beat boxes', L.boxY, L.boxY + AH - 1], ['INPUT', NB.rowIN, NB.rowIN + 13], ['CONFIDENCE', NB.rowCF, NB.rowCF + 13], ['state', L.stateY, L.stateY + 13]];
  eq(gaps(nb), [8, 15, 2, 15], 'without the BAR row: 8 tempo to boxes, 15 boxes to INPUT, 2 between INPUT and CONFIDENCE, 15 CONFIDENCE to the state line');
  // the info block is centred between the boxes (last row 165) and the state line (first row 226): the empty rows above and below differ by at most 1, the odd one is below
  const block = (rows, top) => [top - (L.boxY + AH), L.stateY - (top + rows)];
  eq([block(46, WB.rowIN), block(30, NB.rowIN)], [[7, 7], [15, 15]], 'the info block (with BAR 46 rows, without 30) is centred in the 60 rows between the boxes and the state line: 7 above and 7 below, 15 above and 15 below');
}
const barOn = { bar: { state: 'locked', conf: 0.5 } };
const full = drawn(ctxOf({ ctx: barOn }));
{
  const rows = litRows(full), inBand = j => BANDS.some(([, a, b]) => j >= a && j <= b);
  ok([...rows].every(inBand), 'every lit pixel of a full HOME lies inside a row of the table (none in the gaps): rows ' + [...rows].filter(j => !inBand(j)).join());
  const tempoRows = litRows(full, 0, 320);
  ok([...tempoRows].filter(j => j >= BANDS[1][1] && j <= BANDS[1][2]).length === 70, 'the tempo fills its 70 rows');
  eq([isText(full, 'CONFIDENCE', L.labelX, WB.rowCF, null), isText(full, 'INPUT', L.labelX, WB.rowIN, null), isText(full, 'BAR', L.labelX, WB.rowBAR, null), isText(full, 'LOCKED', L.labelX + 116 - textW('LOCKED'), WB.rowBAR, null)], [true, true, true, true], 'the labels are the words CONFIDENCE, INPUT, BAR with AUTO BAR\'s state, in the 10 character label column (x 4..122)');
  ok(textW('CONFIDENCE') === 119 && L.barX >= 4 + 119 + 2, 'the longest label is 119 px (x 4..122) and the bars start at x ' + L.barX);
  eq(litIn(full, 252, WB.rowCF, 5, 14).length + litIn(full, 252, WB.rowIN, 5, 14).length, 0, 'between a bar (ends x 251) and its value (from x 257 at the widest) there is nothing');
  eq([litIn(full, 0, L.tempoY, 48, BigFont.NORMAL.ROWS).length, litIn(full, 272, L.tempoY, 48, BigFont.NORMAL.ROWS).length], [0, 0], 'no BPM label: "120.0" is centred, x 48..271, nothing right of it');
}

// ---- the big tempo digits ----
{
  const GL = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '.'];
  eq(Object.keys(BigFont.NORMAL.GLYPHS).sort(), [...GL].sort(), 'the glyphs 0-9, "." and "-", complete');
  for (const ch of GL) {
    const g = BigFont.NORMAL.GLYPHS[ch], w = g[0].length;
    ok(g.length === BigFont.NORMAL.ROWS && g.every(r => r.length === w && /^[.#]+$/.test(r)) && g.some(r => r.includes('#')), '"' + ch + '": ' + BigFont.NORMAL.ROWS + ' rows of equal width, bits only, not empty');
    if (ch !== '.') eq(w, 52, '"' + ch + '": 52 columns wide, the same as every digit');
  }
  eq([BigFont.NORMAL.GLYPHS['.'][0].length, BigFont.NORMAL.GLYPHS['.'].findIndex(r => r.includes('#'))], [10, 60], 'the point is a 10 px dot on the bottom 10 rows');
  for (const ch of GL.slice(0, 10)) ok(BigFont.NORMAL.GLYPHS[ch].slice(0, 3).some(r => r.includes('#')) && BigFont.NORMAL.GLYPHS[ch].slice(-3).some(r => r.includes('#')), '"' + ch + '": uses the top and the bottom of the cell');
  for (const s of ['120.0', '146.1', '88.8', '999.9', '---']) ok(BigFont.NORMAL.width(s) <= 312, '"' + s + '" is ' + BigFont.NORMAL.width(s) + ' px wide, fits the 312 px between the margins');
  eq(BigFont.NORMAL.width('120.0'), 3 * 52 + 10 + 34 + 4 * 6, '"120.0" is 3 big digits + the point + 1 small digit + 4 gaps of 6: 224 px');
  const SM = BigFont.NORMAL.SMALL;
  eq(Object.keys(SM).sort(), GL.slice(0, 10).sort(), 'the small digits 0-9, complete');
  for (const ch of GL.slice(0, 10)) ok(SM[ch].length === 46 && SM[ch].every(r => r.length === 34 && /^[.#]+$/.test(r)) && SM[ch][0].includes('#') && SM[ch][45].includes('#'), 'small "' + ch + '": 46 rows of 34 bits, uses the top and the bottom of its cell');
  ok(Math.abs(BigFont.NORMAL.SMALL_ROWS / BigFont.NORMAL.ROWS - 2 / 3) < 0.02 && Math.abs(34 / 52 - 2 / 3) < 0.02, 'the small digits are about 2/3 of the big ones: 46 x 34 of 70 x 52');
  ok(SM['0'][10].startsWith('######.') && SM['0'][10].endsWith('.######') && SM['0'][0].startsWith('..'), 'small 0: 6 px strokes, rounded corners');
  const tempoWant = (s, x0) => { const w = new Set(); BigFont.NORMAL.draw((x, y) => w.add(y * 320 + x), s, x0, L.tempoY); return w; };
  for (const [s, bpm, left] of [['120.0', 120, 48], ['98.4', 98.4, 77], ['88.8', 88.8, 77], ['146.1', 146.1, 48], ['---', null, 76], ['999.9', 999.9, 48]]) {
    eq([BigFont.NORMAL.width(s), Math.floor((320 - BigFont.NORMAL.width(s)) / 2)], [{ 48: 224, 77: 166, 76: 168 }[left], left], '"' + s + '": ' + BigFont.NORMAL.width(s) + ' px wide, left = floor((320 - width) / 2) = ' + left);
    const d = drawn(ctxOf({ home: { bpm } }, {})), want = tempoWant(s, left);
    let same = true; for (let j = L.tempoY; j < L.tempoY + 70; j++) for (let i = 0; i < 320; i++) if ((d.fb[j * 320 + i] === 1) !== want.has(j * 320 + i)) same = false;
    ok(same, 'the tempo "' + s + '" is drawn exactly as the tables say, centred, AUDIO');
  }
  { const d = drawn(ctxOf({ home: { bpm: 88.8 } }), {}); let lo = 320, hi = -1; for (let j = L.tempoY; j < L.tempoY + 70; j++) for (let i = 0; i < 320; i++) if (d.fb[j * 320 + i]) { lo = Math.min(lo, i); hi = Math.max(hi, i); }
    eq([lo, hi, 319 - hi], [77, 242, 77], '88.8: lit from x 77 to 242, 77 columns free on each side (centred)'); }
  // the LARGE font (LINK): 94 x 70 big digits, the small digit 62 x 46, the point 14 x 14, gap 8
  const LG = BigFont.LARGE;
  eq([LG.ROWS, LG.SMALL_ROWS, LG.GAP], [94, 62, 8], 'LARGE: 94 rows, the small digit 62 rows, gap 8');
  eq(Object.keys(LG.GLYPHS).sort(), [...GL].sort(), 'LARGE: the glyphs 0-9, "." and "-", complete');
  for (const ch of GL) { const g = LG.GLYPHS[ch], w = g[0].length; ok(g.length === 94 && g.every(r => r.length === w && /^[.#]+$/.test(r)) && g.some(r => r.includes('#')), 'LARGE "' + ch + '": 94 rows of equal width, bits only'); if (ch !== '.') eq(w, 70, 'LARGE "' + ch + '": 70 columns'); }
  eq([LG.GLYPHS['.'][0].length, LG.GLYPHS['.'].findIndex(r => r.includes('#'))], [14, 80], 'LARGE: the point is a 14 x 14 dot on the bottom rows');
  eq(Object.keys(LG.SMALL).sort(), GL.slice(0, 10).sort(), 'LARGE: the small digits 0-9, complete');
  for (const ch of GL.slice(0, 10)) ok(LG.SMALL[ch].length === 62 && LG.SMALL[ch].every(r => r.length === 46 && /^[.#]+$/.test(r)) && LG.GLYPHS[ch].slice(0, 3).some(r => r.includes('#')) && LG.GLYPHS[ch].slice(-3).some(r => r.includes('#')), 'LARGE "' + ch + '": 62 x 46 small digit, the big one uses the top and bottom of its cell');
  ok(Math.abs(62 / 94 - 2 / 3) < 0.02 && Math.abs(46 / 70 - 2 / 3) < 0.02, 'the LARGE small digit keeps about the 2/3 ratio (62 of 94, 46 of 70)');
  ok(LG.GLYPHS['0'][20].startsWith('##############.') && LG.GLYPHS['0'][20].endsWith('.##############') && LG.SMALL['0'][20].startsWith('##########.'), 'LARGE 0: 14 px strokes; small 0: 10 px strokes');
  eq(['120.0', '98.4', '88.8', '146.1', '---', '999.9'].map(t => LG.width(t)), [302, 224, 224, 302, 226, 302], 'LARGE widths: 120.0 = 3 x 70 + 14 + 46 + 4 x 8 = 302, all fit 312');
  for (const [t, bpm] of [['120.0', 120], ['98.4', 98.4], ['88.8', 88.8], ['146.1', 146.1], ['---', null]]) {
    const left = Math.floor((320 - LG.width(t)) / 2), want = new Set(); LG.draw((x, y) => want.add(y * 320 + x), t, left, L.linkTempoY);
    const k = drawn(ctxOf({ home: { audio: false, link: 1, bpm } }, {}));
    let same = true; for (let j = L.linkTempoY; j < L.linkTempoY + 94; j++) for (let i = 0; i < 320; i++) if ((k.fb[j * 320 + i] === 1) !== want.has(j * 320 + i)) same = false;
    ok(same && left === Math.floor((320 - LG.width(t)) / 2), 'LINK: the tempo "' + t + '" is drawn with the LARGE tables, centred (left ' + left + ', width ' + LG.width(t) + ')');
  }
  { const d = drawn(ctxOf({ home: { bpm: 120 } }), {}), x0 = 48, smallX = x0 + 52 * 3 + 10 + 4 * 6;
    const lit = (x, y) => d.fb[y * 320 + x];
    let top = 999, bot = -1; for (let j = L.tempoY; j < L.tempoY + 70; j++) for (let i = smallX; i < smallX + 34; i++) if (lit(i, j)) { top = Math.min(top, j); bot = Math.max(bot, j); }
    eq([top, bot], [L.tempoY + 24, L.tempoY + 69], 'the digit after the dot is 46 rows, on the big digits\' baseline (rows 56..101), the dot unchanged'); }
}

// healthy: ink only inside the top bar's link / battery, the first beat box, the CONFIDENCE number and line and the VU bar
const R = { link: [L.linkX, 0, L.linkW, 26], batt: [L.battX, 2, L.battW, 22], beat1: [X0A, L.boxY, AW, AH],
            cfValue: [316 - 47, NB.rowCF, 47, 14], cfBar: [L.barX, NB.rowCF, L.barW, 14], inValue: [316 - 59, NB.rowIN, 59, 14], inBar: [L.barX, NB.rowIN, L.barW, 14] };
const insideAny = (boxes, x, y) => boxes.some(([bx, by, bw, bh]) => x >= bx && x < bx + bw && y >= by && y < by + bh);
const outsideInk = (d, boxes) => { let n = 0; for (let i = 0; i < d.ink.length; i++) if (d.ink[i] !== 0 && !insideAny(boxes, i % d.w, Math.floor(i / d.w))) n++; return n; };
const healthy = drawn(ctxOf());
ok(healthy.ink.every((v, i) => v === 0 || healthy.fb[i] === 1), 'ink only on lit pixels');
eq(outsideInk(healthy, [R.link, R.batt, R.beat1, R.cfValue, R.cfBar, R.inBar, [4, L.stateY, 120, 14]]), 0, 'a healthy HOME: ink only in the chain, battery, the lit first beat, the CONFIDENCE number and line, and the VU bar');
eq(idsIn(healthy, ...R.link), [INK.GOOD], 'joined (2 Link peers): the chain and its count are green');
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
  eq([box(on, 0), box(on, 1), box(off, 1), box(off, 0)], [AW * AH, outline(AW, AH), AW * AH, outline(AW, AH)], 'the lit box is solid (72 x 56), the others are 2 px outlines');
}

// the four boxes, every beat, both layouts: exact rectangles, solid when lit, 2 px outline when not, even gaps, amber only for a lit first box
for (const [mode, over, y, h, w] of [['AUDIO', {}, L.boxY, AH, AW], ['AUDIO (BAR row)', { ctx: barOn }, L.boxY, AH, AW], ['LINK', { home: { audio: false, link: 1 } }, L.linkBoxY, L.linkBoxSide, L.linkBoxSide]]) {
  const W0 = w, X0 = Math.floor((320 - (4 * W0 + 3 * L.boxGap)) / 2);
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
  eq([gapLit, litIn(d, 0, y, X0, h).length, litIn(d, X0 + 4 * W0 + 3 * L.boxGap, y, d.w - (X0 + 4 * W0 + 3 * L.boxGap), h).length], [0, 0, 0], mode + ': the three 8 px gaps and the side margins are dark; ' + W0 + ' x ' + h + '');
}

// ---- confidence: the value, the line and the notch all carry the id of the rule ----
const confCase = (conf, extra = {}, get) => drawn(ctxOf({ home: { conf }, ...extra }, get ? { get } : undefined));
const TICKI = Math.round((L.barW - 2) * 0.7), TICK = L.barX + TICKI;                // the stop threshold 70 %: the notch is 2 px wide (TICK, TICK + 1)
const inNotch = x => x === TICK || x === TICK + 1;
for (const [conf, id, why] of [[60, INK.BAD, 'below'], [69, INK.BAD, 'below'], [70, INK.WARN, 'at'], [75, INK.WARN, 'within 10 above'], [80, INK.WARN, '10 above'], [81, INK.GOOD, 'over 10 above'], [87, INK.GOOD, 'above'], [100, INK.GOOD, 'full']]) {
  const d = confCase(conf);
  const bar = idsIn(d, L.barX, NB.rowCF, L.barW, 14).filter(v => v !== 0), val = idsIn(d, ...R.cfValue);
  eq([val, bar], [[id], [id]], 'CONFIDENCE ' + conf + ' against 70 (' + why + '): the value and the line are the same colour id (' + id + ')');
}
eq(isText(confCase(87), '87%', rightText('87%'), NB.rowCF, INK.GOOD), true, 'the value "87%" is right-aligned to x 315');
eq(idsIn(confCase(75, {}, { threshold: '50', lowconf: 'stop' }), ...R.cfValue), [INK.GOOD], 'the threshold is the stick\'s: 75 against 50 is green');
eq(idsIn(confCase(75, {}, { threshold: '90', lowconf: 'run' }), ...R.cfValue), [INK.BAD], '... against 90 red, also with KEEP RUNNING');
eq(idsIn(confCase(30, { ctx: { silent: true } }), ...R.cfValue), [INK.DIM], 'silence: the number is grey, not red (nothing to track)');
eq(idsIn(drawn(ctxOf({ home: { conf: null } })), ...R.cfValue), [INK.DIM], 'no confidence reported: the dashes are grey');
eq(idsIn(drawn(ctxOf({}, { status: null })), ...R.cfValue), [INK.GOOD], 'no status line: the confidence comes from the HOME line and is still green');
// the line: ONE thin line (6 rows, y + 4..y + 9), its length the value on 124 px; counted on its middle row away from the notch
for (const [conf, n] of [[0, 0], [50, 62], [100, 124]]) {
  const d = confCase(conf), row = NB.rowCF + 6, cut = [TICKI, TICKI + 1].filter(i => i < n).length;
  let lit = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (!inNotch(x) && d.fb[row * d.w + x]) lit++;
  eq(lit, n - cut, 'CONFIDENCE ' + conf + ' %: the line is ' + n + ' px of 124' + (cut ? ' (' + cut + ' are the notch\'s columns)' : ''));
  eq([d.fb[row * d.w + L.barX], d.fb[row * d.w + L.barX + L.barW - 1], d.fb[row * d.w + L.barX - 1]], [n > 0 ? 1 : 0, n === 124 && !inNotch(L.barX + 123) ? 1 : 0, 0], '... it starts at x ' + L.barX + ' and ends at its value, nothing beyond');
}
{
  const d = confCase(50), lineRows = [...Array(14).keys()].map(k => { let c = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (!inNotch(x) && d.fb[(NB.rowCF + k) * d.w + x]) c++; return c; });
  eq(lineRows, [0, 0, 0, 0, 62, 62, 62, 62, 62, 62, 0, 0, 0, 0], 'CONFIDENCE 50 %: the line is exactly 6 rows (y + 4..y + 9): no outline, no track');
}
{
  const d = confCase(100), x = TICK, col = (d, xx) => [...Array(14).keys()].map(k => d.fb[(NB.rowCF + k) * d.w + xx]);
  eq([d.fb[(NB.rowCF + 2) * d.w + x], d.fb[(NB.rowCF + 11) * d.w + x], d.ink[(NB.rowCF + 2) * d.w + x], d.ink[(NB.rowCF + 11) * d.w + x]], [1, 1, INK.GOOD, INK.GOOD], 'the notch for 70 % is at x ' + x + ': 2 px wide, 10 px tall (y + 2..y + 11), a stub above and below the line, in the number\'s colour');
  eq([col(d, x), col(d, x + 1)], [[0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0], [0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0]], '... and cuts a gap through a line that passes it (both columns)');
  const e = confCase(40);
  eq([col(e, x), col(e, x + 1), idsIn(e, x, NB.rowCF, 2, 14)], [[0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0], [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0], [INK.BAD]], 'over the empty part the notch is a 10 px line, in red');
  const t2 = confCase(60, {}, { threshold: '30', lowconf: 'stop' }), x2 = L.barX + Math.round((L.barW - 2) * 0.3);
  ok(t2.fb[(NB.rowCF + 2) * t2.w + x2] === 1 && t2.ink[(NB.rowCF + 2) * t2.w + x2] === INK.GOOD, 'the notch follows the stick\'s threshold (30 %: x ' + x2 + ')');
}

// ---- the INPUT VU meter: the ramp, the fill, the colour of every column, the notch ----
const VUB = L.barX, VT = NB.rowIN + 2;                                              // the bar's x and first row (10 rows)
const IN_NOTCH = Math.round((L.barW - 2) * 40 / 90);                               // -50 dBFS on a -90..0 scale: column 54 of the bar (2 px wide)
const levelCase = (levelDb, peakDb, max) => drawn(ctxOf({ home: { level: levelDb, levelMax: max === undefined ? levelDb : max }, ctx: { peakDb } }));
const levelIds = d => [idsIn(d, ...R.inValue).filter(v => v !== 0), idsIn(d, L.barX, NB.rowIN + 2, L.barW, 10).filter(v => v < OLED.VU.ink0 && v !== 0)];
const colId = i => OLED.VU.ink0 + Math.floor(OLED.VU.steps * i / L.barW);
eq([OLED.VU, Lcd2.VU_RAMP.length], [{ ink0: 6, steps: 32 }, 32], 'the VU ramp: 32 ink ids from 6 (ids 6..37), after the 6 fixed ones');
for (const [i, rgb] of Lcd2.VU_RAMP.entries()) {
  ok(rgb.length === 3 && rgb.every(v => Number.isInteger(v) && v >= 0 && v <= 255), 'ramp step ' + i + ': exact 8-bit RGB ' + rgb);
  ok(Lcd2.contrast(rgb, P.bg) >= 6.5, 'ramp step ' + i + ' reads on the background (' + Lcd2.contrast(rgb, P.bg).toFixed(2) + ' : 1, bars need visibility, not AA text)');
  eq(Lcd2.inkColour(OLED.VU.ink0 + i), rgb, 'ink id ' + (OLED.VU.ink0 + i) + ' is ramp step ' + i);
  if (i > 0) ok(rgb.every((v, k) => Math.abs(v - Lcd2.VU_RAMP[i - 1][k]) <= (i >= 29 ? 27 : 16)), 'ramp step ' + i + ' differs from the one before by at most 16 per channel (no visible banding; the amber to red leg, steps 29..31, at most 27)');
}
eq([Lcd2.VU_RAMP[0], Lcd2.VU_RAMP[16], Lcd2.VU_RAMP[17], Lcd2.VU_RAMP[23], Lcd2.VU_RAMP[28], Lcd2.VU_RAMP[31]], [P.good, P.good, [87, 223, 119], [176, 230, 64], P.warn, [255, 96, 88]], 'the ramp: green up to step 16 (-42 dBFS), lime at 23 (-25), amber at 28 (-11), red at 31 (-3)');
eq(Lcd2.inkColour(OLED.VU.ink0 + 32), P.neutral, 'an id past the ramp is neutral');
eq(levelIds(levelCase(-20, -12)), [[], []], 'a good level: the number is neutral');
eq(levelIds(levelCase(-20, -0.5)), [[INK.BAD], []], 'the last second peaked at -0.5 dBFS: the number is red (clipping)');
eq(levelIds(levelCase(-20, -1)), [[INK.BAD], []], '... -1 dBFS is the edge');
eq(levelIds(levelCase(-20, -1.5)), [[], []], '... -1.5 is not clipping');
eq(levelIds(levelCase(-20, null, -2))[0], [INK.BAD], 'peak not reported: an RMS maximum over -3 dB counts');
eq(levelIds(levelCase(-20, null, -10))[0], [], '... under it, neutral');
eq(levelIds(levelCase(-55, -40))[0], [INK.WARN], 'under the tracker\'s -50 dBFS sound level: the number is amber (too quiet to track)');
eq(levelIds(levelCase(-50, -40))[0], [], '... -50 itself is enough');
eq(isText(levelCase(-20, -12), '-20dB', rightText('-20dB'), NB.rowIN, null), true, 'the number "-20dB" is right-aligned to x 315');
{
  const col = db => { const d = levelCase(db, null, db), row = VT + 4, cols = []; for (let x = 0; x < L.barW; x++) if (d.fb[row * d.w + VUB + x]) cols.push(x); return { d, cols }; };
  for (const db of [-90, -60, -45, -30, -12, 0]) {
    const n = Math.round(124 * (db + 90) / 90), { d, cols } = col(db);
    const want = Array.from({ length: L.barW }, (_, x) => x).filter(x => (x < n) !== (x === IN_NOTCH || x === IN_NOTCH + 1));
    eq(cols, want, db + ' dB: ' + n + ' continuous columns lit on the bar\'s middle row (no gaps), the 2 notch columns XOR-ed (x ' + (VUB + IN_NOTCH) + ')');
    eq(cols.filter(x => x !== IN_NOTCH && x !== IN_NOTCH + 1).every(x => d.ink[(VT + 4) * d.w + VUB + x] === colId(x)), true, db + ' dB: every lit column has the ramp id of its position on the dBFS scale');
  }
  const colAt = (d, x) => [...Array(14).keys()].map(k => d.fb[(NB.rowIN + k) * 320 + x]);
  eq(colAt(levelCase(-45, null, -45), VUB), [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0], 'the bar is 10 rows high (y + 2..y + 11), no outline, no track');
  eq(colAt(col(-90).d, VUB + IN_NOTCH), Array(14).fill(1), 'the -50 dBFS notch is a 14 px column, 2 px wide, at bar column ' + IN_NOTCH + ' (even over an empty bar)');
  eq(colAt(col(-20).d, VUB + IN_NOTCH), [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1], 'through a lit bar the notch cuts a gap and leaves a stub above and below');
  eq(idsIn(col(-90).d, VUB + IN_NOTCH, NB.rowIN, 2, 14), [0], 'the notch is neutral');
  eq(litIn(levelCase(-30, -12), VUB + 104, VT, 1, 10).length, 0, 'nothing is drawn beyond the bar\'s end (no peak tick)');
}

// ---- the BAR row: AUTO BAR's state and confidence, neutral, only with AUTO BAR on ----
{
  const rowLit = d => litIn(d, 0, WB.rowBAR, d.w, 14).length;
  const barLabel = d => isText(d, 'BAR', 4, WB.rowBAR, null);
  eq([barLabel(healthy), barLabel(drawn(ctxOf({ ctx: { bar: { state: 'off', conf: 0 } } }))), isText(healthy, 'CONFIDENCE', 4, NB.rowCF, null)], [false, false, true], 'no AUTO BAR (or OFF): no BAR row, the rows close up (CONFIDENCE takes the slot after INPUT, the boxes grow)');
  const lock = drawn(ctxOf({ ctx: { bar: { state: 'locked', conf: 0.5 } } })), lis = drawn(ctxOf({ ctx: { bar: { state: 'listening', conf: 1 } } })), man = drawn(ctxOf({ ctx: { bar: { state: 'manual', conf: 0.9 } } }));
  ok(rowLit(lock) > 200 && rowLit(lis) > rowLit(lock) && rowLit(man) > 0, 'LOCKED, LISTEN, MANUAL draw the row; the fill grows with the confidence');
  eq([lock, lis, man].map(d => idsIn(d, 0, WB.rowBAR, d.w, 14)), [[0], [0], [0]], 'the BAR row is neutral');
  eq([isText(lis, 'LISTEN', 4 + 116 - textW('LISTEN'), WB.rowBAR, null), isText(man, 'MANUAL', 4 + 116 - textW('MANUAL'), WB.rowBAR, null)], [true, true], 'the state words LISTEN and MANUAL end at x 119');
  let lit = 0; const row = WB.rowBAR + 6; for (let x = L.barX + 1; x < L.barX + L.barW - 1; x++) if (lock.fb[row * lock.w + x]) lit++;
  eq([lit, lock.fb[(WB.rowBAR + 2) * lock.w + L.barX + 80], lock.fb[(WB.rowBAR + 11) * lock.w + L.barX + 80]], [61, 1, 1], 'BAR 0.5: the BAR row keeps its 1 px outlined bar (a frame above and below, 124 x 10), 61 px of 122 filled');
}

// ---- the top bar: icons 22 high. Left: transport icon x 4, LINK TRANSPORT icon x 28, fold label x 62. Right: GRP n, chain + peers, the plug (charging), battery ----
const LT_W = [22, 26, 30];
const grpAt = text => L.nameXR - textW(text);
{
  const d = drawn(ctxOf({ home: { battery: 80 }, ctx: { usb: false } }));
  const cols = litCols(d, 0, 26);
  eq(cols[cols.length - 1], 315, 'the right group ends at x 315 (the battery\'s nub), four columns from the edge');
  eq([L.ltX, L.foldX, L.nameXR, L.linkX, L.linkW, L.peersX, L.plugX, L.battX, L.battW], [28, 62, 187, 195, 49, 220, 246, 268, 48], 'the top bar slots: LINK icon x 28, fold x 62, name right edge 187, chain x 195 (slot 49), peers x 220, plug x 246, battery x 268 (48 wide)');
  ok(L.ltX + 26 < L.foldX && L.foldX + 31 < L.nameXR - textW('GRP 1') && L.nameXR + 8 === L.linkX && L.linkX + L.linkW + 2 === L.plugX && L.plugX + 18 + 4 === L.battX && L.battX + L.battW === 316, 'no overlaps: the left group ends at x 93, GRP 1 spans x 128..186, then 8 px, the chain slot x 195..243, the plug x 246..263, 4 px, the battery x 268..315');
  const rows = [...litRows(d, L.linkX, 320)].filter(j => j < 26); eq([Math.min(...rows), Math.max(...rows)], [2, 23], 'the right group\'s icons are 22 rows high (y 2..23)');
  const pl = [...litRows(d, 4, 22)].filter(j => j < 26);
  eq([Math.min(...pl), Math.max(...pl)], [2, 23], 'the transport icon (play triangle) is 22 rows high');
  const ch = [...litRows(d, L.linkX, L.linkX + 19)].filter(j => j < 26);
  eq([Math.min(...ch), Math.max(...ch), litCols(d, 0, 26).filter(x => x >= L.linkX && x < L.linkX + L.linkW).length > 19], [6, 19, true], 'the chain is 14 rows high, centred in the bar (y 6..19), 19 wide (the Optimist\'s icon)');
  eq([isText(d, 'GRP 1', grpAt('GRP 1'), 6, null), litIn(d, 58, 0, grpAt('GRP 1') - 59, 26).length, litIn(d, L.nameXR, 0, L.linkX - L.nameXR, 26).length], [true, 0, 0], 'GRP n sits next to the chain (right-aligned to x 186, 8 px before it); the middle of the bar is free (x 58..' + (grpAt('GRP 1') - 1) + ')');
  eq(litIn(d, 22, 0, 6, 26).length, 0, 'nothing between the transport icon and the LINK icon (x 22..27)');
  const stop = drawn(ctxOf({ ctx: { running: false, armed: false } }));
  eq([litIn(stop, 4, 4, 18, 18).length, litIn(stop, 4, 2, 18, 2).length, litIn(stop, 4, 22, 18, 2).length], [324, 0, 0], 'stopped: an 18 x 18 square at x 4..21, y 4..21');
  const play = drawn(ctxOf({ ctx: { running: true } })); eq([litIn(play, 4, 2, 18, 22).length, litIn(play, 4, 12, 18, 1).length, litIn(play, 4, 2, 18, 1).length], [200, 18, 1], 'running: the play triangle, 18 wide at its middle rows, 1 at its tips');
  // the LINK TRANSPORT icon is in the bar in both modes, the same pixels; the fold label (AUDIO) sits next to it
  for (const mode of [0, 1, 2]) {
    const au = drawn(ctxOf({ ctx: { ltMode: mode } })), lk = drawn(ctxOf({ home: { audio: false, link: 1 }, ctx: { ltMode: mode } }));
    const same = [...Array(22 * 30).keys()].every(k => au.fb[(2 + Math.floor(k / 30)) * 320 + L.ltX + (k % 30)] === lk.fb[(2 + Math.floor(k / 30)) * 320 + L.ltX + (k % 30)]);
    ok(litIn(au, L.ltX, 2, LT_W[mode], 22).length > 80 && litIn(au, L.ltX + LT_W[mode], 2, 4, 22).length === 0 && same, 'LINK TRANSPORT icon ' + mode + ' (' + ['IGNORE', 'FOLLOW', 'BOTH'][mode] + '): ' + LT_W[mode] + ' x 22 at x ' + L.ltX + ', in AUDIO and in LINK mode');
  }
  eq([0, 1, 2].map(m => new Set([0, 1, 2].map(n => sha([drawn(ctxOf({ ctx: { ltMode: n } })).fb]))).size), [3, 3, 3], 'the three LINK TRANSPORT icons differ');
  // the three icons are the old 160 x 120 bitmaps painted as exact 2 x 2 blocks (no Scale2x: it rounds the corners and fills the 1 px gap between the two triangles of FOLLOW+LEAD)
  {
    const OLD = [
      ['...#####...', '..##...##..', '.##.....##.', '##.....#.##', '#.....#...#', '#....#....#', '#...#.....#', '##.#.....##', '.##.....##.', '..##...##..', '...#####...'],
      ['.....#.......', '....##.......', '...###.......', '..####.......', '.############', '#############', '.############', '..####.......', '...###.......', '....##.......', '.....#.......'],
      ['.....#.#.....', '....##.##....', '...###.###...', '..####.####..', '.###########.', '#############', '.###########.', '..####.####..', '...###.###...', '....##.##....', '.....#.#.....'],
    ];
    for (const mode of [0, 1]) {
      const d = drawn(ctxOf({ ctx: { ltMode: mode } })); let same = true;
      for (let j = 0; j < 22; j++) for (let i = 0; i < 2 * OLD[mode][0].length; i++) if (d.fb[(L.iconY + j) * 320 + L.ltX + i] !== (OLD[mode][j >> 1][i >> 1] === '#' ? 1 : 0)) same = false;
      ok(same, 'LINK TRANSPORT icon ' + mode + ' is its small bitmap in exact 2 x 2 blocks');
    }
    const d2 = drawn(ctxOf({ ctx: { ltMode: 2 } })); const gap = [], widths = [], expect = []; let split = true;
    for (let j = 0; j < 22; j++) {
      const lit = x => d2.fb[(L.iconY + j) * 320 + L.ltX + x] === 1;
      for (let x = 12; x <= 17; x++) gap.push(lit(x) ? 1 : 0);
      let l = 0, r = 0; for (let x = 0; x < 12; x++) l += lit(x) ? 1 : 0; for (let x = 18; x < 30; x++) r += lit(x) ? 1 : 0;
      for (let x = 12 - l; x < 12; x++) if (!lit(x)) split = false;
      for (let x = 18; x < 18 + r; x++) if (!lit(x)) split = false;
      widths.push([l, r]); const w = Math.max(1, Math.round(12 * (1 - Math.abs(j - 10.5) / 11))); expect.push([w, w]);
    }
    eq(gap.reduce((a, b) => a + b, 0), 0, 'FOLLOW+LEAD: the gap x 12..17 is unlit on all 22 rows');
    eq(widths, expect, 'FOLLOW+LEAD: per row both triangles are max(1, round(12 * (1 - |y - 10.5| / 11))) wide');
    ok(split, 'FOLLOW+LEAD: the left triangle ends at x 11 and the right starts at x 18, each a solid run');
  }
  const fold = drawn(ctxOf({ home: { fold: '2' }, ctx: { fold: 2 } }));
  ok(litIn(fold, L.foldX, 2, 31, 22).length > 400 && litIn(fold, L.foldX + 31, 2, 4, 22).length === 0 && litIn(fold, L.ltX, 2, 26, 22).length > 80, 'the fold label X2 is an inverted 31 x 22 box at x 62, next to the LINK icon');
  eq(litIn(drawn(ctxOf({ home: { audio: false, link: 1, fold: '2' }, ctx: { fold: 2 } })), L.foldX, 2, 31, 22).length, 0, 'no fold label in LINK mode');
  const wifi = drawn({ ...ctxOf(), net: { ...ctxOf().net, mode: 1, ssid: 'studio-wifi-5G' } });
  ok(isText(wifi, 'studio-', grpAt('studio-'), 6, null), 'the Wi-Fi name takes the place of GRP n: right-aligned to x 186, cut to 7 characters');
}
eq(idsIn(drawn(ctxOf({ home: { link: 0 } })), ...R.link), [INK.DIM], 'AUDIO mode, no Link peer: dim (a lone leader is normal)');
eq(idsIn(drawn(ctxOf({ home: { link: 0, audio: false } })), ...R.link), [INK.BAD], 'LINK mode, no session peer: red (nothing to follow)');
eq(idsIn(drawn(ctxOf({ home: { link: 1, audio: false } })), ...R.link), [INK.GOOD], 'LINK mode, one peer: green');
{
  const two = drawn(ctxOf({ home: { link: 2 } })), none = drawn(ctxOf({ home: { link: 0 } }));
  ok(isText(two, '2', L.peersX, 6, INK.GOOD) && litIn(none, L.peersX, 6, 24, 14).length === 0, 'the Link peer count is drawn beside the chain only when there are peers');
  ok(idsIn(two, grpAt('GRP 1'), 6, textW('GRP 1'), 14).every(v => v === 0), 'GRP n is neutral');
}
const battId = pct => idsIn(drawn(ctxOf({ home: { battery: pct }, ctx: { usb: false } })), ...R.batt);
eq([battId(100), battId(80), battId(30), battId(29), battId(15), battId(14), battId(0)], [[0], [0], [0], [INK.WARN], [INK.WARN], [INK.BAD], [INK.BAD]], 'battery: neutral from 30 %, amber under 30 (BATT_WARN_PCT), red under 15 (BATT_BAD_PCT)');
eq(idsIn(drawn(ctxOf({ home: { battery: null } })), ...R.batt), [], 'no battery fitted: no icon, no ink');
{
  const fillOf = pct => { const d = drawn(ctxOf({ home: { battery: pct }, ctx: { usb: false } })); let n = 0; for (let x = L.battX + 3; x < L.battX + 41; x++) if (d.fb[12 * d.w + x]) n++; return n; };
  eq([fillOf(0), fillOf(50), fillOf(100)], [0, 19, 38], 'the battery icon\'s fill: 0, 19 and 38 px of 38');
  const d = drawn(ctxOf({ home: { battery: 100 }, ctx: { usb: false } }));
  eq([d.fb[2 * d.w + L.battX], d.fb[2 * d.w + L.battX + 1], d.fb[12 * d.w + L.battX + 44], d.fb[8 * d.w + L.battX + 47], d.fb[7 * d.w + L.battX + 47]], [0, 1, 1, 1, 0], 'the battery: a 44 x 22 body with a 1 px outline and chamfered corners and a 4 x 10 nub');
  // charging (the BATTERY info screen's inference: on USB, a cell fitted, under 100 %): the plug (18 x 14, the 9 x 7 plug doubled) beside the battery, to its left; the battery itself stays a plain battery
  const PLUG = BigFont.scale2x(['..####...', '..#######', '#########', '######...', '#########', '..#######', '..####...']);
  for (const pct of [10, 50, 99]) {
    const on = drawn(ctxOf({ home: { battery: pct }, ctx: { usb: true } })), off = drawn(ctxOf({ home: { battery: pct }, ctx: { usb: false } }));
    let ok1 = true; for (let j = 0; j < 14; j++) for (let i = 0; i < 18; i++) if (on.fb[(6 + j) * 320 + L.plugX + i] !== (PLUG[j][i] === '#' ? 1 : 0)) ok1 = false;
    let rest = true; for (let k = 0; k < 320 * 26; k++) { const x = k % 320, y = Math.floor(k / 320), inPlug = x >= L.plugX && x < L.plugX + 18 && y >= 6 && y < 20; if (!inPlug && on.fb[k] !== off.fb[k]) rest = false; }
    ok(ok1 && rest && litIn(off, L.plugX, 6, 18, 14).length === 0, 'charging at ' + pct + ' %: only the 18 x 14 plug at x 246..263 is added, the battery is the same plain battery');
  }
  const full100 = drawn(ctxOf({ home: { battery: 100 }, ctx: { usb: true } }));
  eq(sha([full100.fb]), sha([d.fb]), 'USB plugged but full (100 %): no plug (the INFO screen says FULL)');
  eq(litIn(drawn(ctxOf({ home: { battery: null, link: 0 }, ctx: { usb: true } })), L.plugX, 0, 74, 26).length, 0, 'USB plugged and no cell: no plug, nothing is drawn right of the chain');
  eq([idsIn(drawn(ctxOf({ home: { battery: 50 }, ctx: { usb: true } })), ...R.batt), idsIn(drawn(ctxOf({ home: { battery: 50 }, ctx: { usb: true } })), L.plugX, 6, 18, 14)], [[0], [0]], 'charging at 50 %: the battery and the plug are neutral');
  const low = drawn(ctxOf({ home: { battery: 12 }, ctx: { usb: true } }));
  eq([idsIn(low, ...R.batt), idsIn(low, L.plugX, 6, 18, 14)], [[INK.BAD], [0]], 'charging at 12 %: the battery red, the plug stays neutral');
}

// ---- the state line (AUDIO): one word, centred, the first state that applies; empty while simply running; its row is always reserved ----
{
  const states = [['GATED', { ctx: { gated: true } }, INK.BAD], ['HOLD', { home: { confHold: true } }, INK.DIM], ['COAST', { home: { conf: 50 }, ctx: { running: true } }, INK.WARN],
                  ['STOPPED', { ctx: { running: false } }, INK.DIM], ['SILENT', { ctx: { running: true, silent: true } }, INK.DIM]];
  for (const [word, over, ink] of states) {
    const d = drawn(ctxOf(over)), x = Math.floor((320 - textW(word)) / 2);
    eq([isText(d, word, x, L.stateY, ink), litIn(d, 0, L.stateY, d.w, 14).length === textPoints(word).length], [true, true], word + ': the only thing on the state line, centred at x ' + x + ' = floor((320 - ' + textW(word) + ') / 2), in its colour');
    eq([x, Math.abs(x - (320 - x - textW(word))) <= 1], [x, true], word + ': the left and right margins differ by at most 1 px');
  }
  const first = (over, word) => isText(drawn(ctxOf(over)), word, Math.floor((320 - textW(word)) / 2), L.stateY, null);
  eq([first({ ctx: { gated: true, confHold: true, running: true, silent: true } }, 'GATED'), first({ home: { confHold: true }, ctx: { running: false } }, 'HOLD'), first({ home: { conf: 50 }, ctx: { running: true, silent: true } }, 'COAST'), first({ ctx: { running: false, silent: true } }, 'STOPPED'), first({ ctx: { running: true, silent: true } }, 'SILENT')], [true, true, true, true, true], 'the order: GATED, HOLD, COAST, STOPPED, SILENT');
  eq([litIn(healthy, 0, L.stateY, 320, 14).length, litIn(drawn(ctxOf({ ctx: barOn.bar ? { running: true, bar: barOn.bar } : {} })), 0, L.stateY, 320, 14).length], [0, 0], 'a plain running state has no word: the line is empty (no PLAYING), with and without the BAR row');
  eq(litIn(drawn(ctxOf({ ctx: { gated: true } })), 0, 224, 320, 2).length, 0, 'the state line keeps its place (y 226..239) whether a word is shown or not: the gap above it is empty');
}

// ---- the margins around the tempo, measured on the LIT pixels of drawn frames: above (last lit row of the top bar to the first lit row of the digits) == below (last lit row of the digits to the first lit row of the squares) ----
{
  const litRowsOf = (d, x0, x1, y0, y1) => { const r = []; for (let j = y0; j <= y1; j++) for (let i = x0; i < x1; i++) if (d.fb[j * d.w + i]) { r.push(j); break; } return r; };
  const margins = (d, tempoTop, boxTop) => {
    const bar = litRowsOf(d, 0, 320, 0, tempoTop - 1), digits = litRowsOf(d, 0, 320, tempoTop, boxTop - 1), box = litRowsOf(d, 0, 320, boxTop, boxTop);
    return { above: digits[0] - bar[bar.length - 1] - 1, below: boxTop - digits[digits.length - 1] - 1, first: digits[0], last: digits[digits.length - 1], box: box[0] };
  };
  for (const [t, bpm] of [['120.0', 120], ['98.4', 98.4], ['88.8', 88.8], ['146.1', 146.1], ['133.3', 133.3], ['999.9', 999.9]]) {
    const a = margins(drawn(ctxOf({ home: { bpm } })), L.tempoY - 6, L.boxY), k = margins(drawn(ctxOf({ home: { audio: false, link: 1, bpm } })), L.linkTempoY - 6, L.linkBoxY);
    eq([a.above, a.below, a.first, a.last], [8, 8, 32, 101], 'AUDIO "' + t + '": lit margins above (top bar y 23 to the digits y 32) and below (digits to y 101, squares y 110) are both 8 rows');
    eq([k.above, k.below, k.first, k.last], [14, 14, 38, 131], 'LINK "' + t + '": lit margins above (y 23 to y 38) and below (y 131 to the squares at y 146) are both 14 rows');
  }
  { const a = margins(drawn(ctxOf({ home: { bpm: null } })), L.tempoY - 6, L.boxY), k = margins(drawn(ctxOf({ home: { audio: false, link: 1, bpm: null } })), L.linkTempoY - 6, L.linkBoxY);
    eq([a.above, a.below, k.above, k.below], [37, 38, 53, 54], '"---" (only the dash rows are lit, in the middle of the digit cell): AUDIO 37 above and 38 below; LINK 53 above and 54 below (the dash rows cannot sit exactly in the middle of the cell)'); }
}

// ---- the LINK layout: the same top bar, the LARGE tempo, four SQUARE beat boxes, no audio rows, no state line ----
{
  const link = drawn(ctxOf({ home: { audio: false, link: 1, bpm: 98.4 } })), S = L.linkBoxSide, x0 = Math.floor((320 - (4 * S + 3 * L.boxGap)) / 2);
  eq([S, L.linkBoxY, x0, 4 * S + 3 * L.boxGap], [72, 146, 4, 312], 'LINK: four square boxes 72 x 72 with 8 px gaps, a row of 312 centred (x 4..315), at y 146..217');
  eq([L.linkTempoY - 24, L.linkBoxY - (L.linkTempoY + 94), 240 - (L.linkBoxY + S)], [14, 14, 22], 'LINK: the LARGE tempo (y 38..131) has 14 empty rows above (under the top bar icons) and 14 below (to the squares); the squares keep their place, 22 empty rows under them');
  for (let i = 0; i < 4; i++) {
    const x = x0 + i * (S + L.boxGap), on = drawn(ctxOf({ home: { audio: false, link: 1 }, ctx: { beatN: i } }));
    let solid = true, ring = true, inner = 0; for (let dy = 0; dy < S; dy++) for (let dx = 0; dx < S; dx++) { const v = on.fb[(L.linkBoxY + dy) * 320 + x + dx], edge = dx < 2 || dy < 2 || dx >= S - 2 || dy >= S - 2; if (!v) solid = false; if (edge && !v) ring = false; if (!edge && v) inner++; }
    const off = drawn(ctxOf({ home: { audio: false, link: 1 }, ctx: { beatN: (i + 1) % 4 } }));
    let offRing = true, offInner = 0; for (let dy = 0; dy < S; dy++) for (let dx = 0; dx < S; dx++) { const v = off.fb[(L.linkBoxY + dy) * 320 + x + dx], edge = dx < 2 || dy < 2 || dx >= S - 2 || dy >= S - 2; if (edge && !v) offRing = false; if (!edge && v) offInner++; }
    eq([solid, offRing && offInner === 0, idsIn(on, x, L.linkBoxY, S, S)], [true, true, i === 0 ? [INK.ONE] : [0]], 'LINK beat ' + (i + 1) + ': box ' + (i + 1) + ' is a solid 72 x 72 square (the first amber only while lit), unlit it is a 2 px outline; width == height');
  }
  eq([0, 1, 2].map(g => litIn(link, x0 + (g + 1) * S + g * L.boxGap, L.linkBoxY, L.boxGap, S).filter(() => true).length), [0, 0, 0], 'LINK: the three 8 px gaps between the boxes are dark');
  eq([litIn(link, 0, L.linkBoxY + S, link.w, 240 - L.linkBoxY - S).length, outsideInk(link, [R.link, R.batt, [x0, L.linkBoxY, 4 * S + 24, S]])], [0, 0], 'LINK HOME: nothing under the boxes (no CONFIDENCE, INPUT or BAR rows, no state line), no stray ink');
  ok(litRows(link, 0, 320).has(L.linkTempoY + 93) && !litRows(link, 0, 320).has(L.linkTempoY + 94) && !litRows(link, 0, 320).has(L.linkBoxY - 1), 'the LARGE tempo fills rows 38..131 and nothing lies between it and the boxes');
}

// ---- menus, edit, info, net, text, confirm, setup: neutral, the whole frame, no ink ----
const noInk = (d, what) => ok(d.fb.some(v => v === 1) && d.ink.every(v => v === 0), what + ': drawn, and no ink at all');
const qr = { size: 25, mod: Array.from({ length: 25 }, (_, y) => Array.from({ length: 25 }, (_, x) => ((x * y + x + y) % 3 === 0 ? 1 : 0))) };
{
  const menu = withUi(ctxOf(), ['no'], 'lcd2');
  let d = drawn(menu.c, { ui: menu.ui });
  noInk(d, 'MENU');
  eq([d.fb[0], d.fb[LW - 1], d.fb[21 * LW], d.fb[22 * LW]], [1, 1, 1, 0], 'the title bar is 22 rows, the full width');
  const rowBand = k => litIn(d, 0, 26 + 18 * k, d.w, 14).length;
  eq([rowBand(0), rowBand(1), rowBand(10)].map(n => n > 0), [true, true, true], 'the 11 menu items are rows of 18 px from y 26 (the last at y ' + (26 + 180) + ')');
  eq([d.fb[(26 - 2) * LW], d.fb[(26 - 2) * LW + LW - 1], d.fb[30 * LW + 2]], [1, 1, 1], 'the selected row (the first) is inverted over the full width');
  eq(litIn(d, 0, 26 + 18 - 2, 1, 18).length, 0, 'the next row is not');
  const lastLit = Math.max(...litRows(d));
  ok(lastLit >= 26 + 18 * 10 && lastLit < LH, 'the list reaches y ' + lastLit + ' of 239: the frame is used');
  const dn = withUi(ctxOf(), ['no', ...Array(10).fill('down')], 'lcd2');
  d = drawn(dn.c, { ui: dn.ui });
  noInk(d, 'MENU, last item selected');
  eq(d.fb[(26 + 180 - 2) * LW + 100], 1, 'the last item (ABOUT) is the selected, inverted row at the bottom');
  const sub = withUi(ctxOf(), ['no', 'yes'], 'lcd2');
  d = drawn(sub.c, { ui: sub.ui });
  noInk(d, 'a sub menu (' + sub.ui.screen + ')');
  const ed = withUi(ctxOf(), ['no', 'yes', 'yes'], 'lcd2');
  d = drawn(ed.c, { ui: ed.ui });
  noInk(d, 'an edit screen (' + ed.ui.screen + ')');
  if (ed.ui.screen === 'edit') {
    const box = [];
    for (let j = 40; j < 120; j++) { let lo = -1, hi = -1; for (let i = 0; i < LW; i++) if (d.fb[j * LW + i]) { if (lo < 0) lo = i; hi = i; } if (lo >= 0 && hi - lo > 60) box.push([lo, hi]); }
    ok(box.length > 0 && Math.abs(box[0][0] - (LW - 1 - box[0][1])) <= 1, 'the edit value box is centred on the 320 px width (x ' + (box[0] || []) + ')');
  }
  const batt = { ...ctxOf(), tempoStep: () => null, setField() {}, setHold() {}, linkStartStop() {} };
  const infoUi = { ...OLED.create(), screen: 'info', page: 'battery' };
  d = drawn(batt, { ui: infoUi }); noInk(d, 'the BATTERY info screen');
  ok(litIn(d, 8, 184, 304, 14).length > 200, '... with its level bar across the width');
  const aboutUi = { ...OLED.create(), screen: 'info', page: 'about' };
  const about = { tempC: 41, cpuPct: 37, cpu1Pct: 3, heapKb: 123, psramKb: 7936, flashKb: 8192, appUsedKb: 1260, appKb: 3072, uptimeS: 3725, heapMinKb: 98, reset: 'POWERON', version: '0.1.0', clkLateUs: 12, uiLoopUs: 4210, beatLoopUs: 8800 };
  d = drawn({ ...batt, ...about }, { ui: aboutUi }); noInk(d, 'the ABOUT info screen');
  const ab = (d, label, val, y) => isText(d, label, 8, y, null) && isText(d, val, 312 - textW(val), y, null);      // a label at the left, its value right aligned to x 312 (the list rows' margin)
  ok(ab(d, 'VERSION', '0.1.0', 36) && ab(d, 'REPO', 'github.com/w0ts/jank', 36 + 24) && !isText(d, 'NAME', 8, 36, null) && ab(d, 'TEMP', '41 C', 36 + 96) && ab(d, 'MEMORY', '123K PSRAM 7936K', 36 + 144), '... its rows from y 36, 24 px apart (8 of 12 show, no NAME row), the label at x 8 and the value right aligned to x 312');
  ok(8 + textW('MEMORY') + 12 < 312 - textW('123K PSRAM 7936K') && 8 + textW('REPO') + 12 < 312 - textW('github.com/w0ts/jank'), '... label and value never overlap (the widest pairs, MEMORY and REPO)');
  ok(ab(d, 'CPU', '37/3%', 36 + 120), '... CPU shows core 0 / core 1: 37/3%');
  d = drawn({ ...batt, ...about }, { ui: { ...aboutUi, ascroll: 8 } });
  ok(ab(d, 'FLASH', '8M 1.2/3.0M', 36) && ab(d, 'BEAT LOOP', '8800 US', 36 + 72) && !isText(d, 'TEMP', 8, 36 + 168, null) && litIn(d, 0, 36 + 96, 320, 100).length === 0, '... page 2 (rows 8 to 12): the last 4 rows from the top, no row of page 1 repeated, nothing below them');
  d = drawn({ ...batt, ...Object.fromEntries(Object.keys(about).map(k => [k, null])) }, { ui: aboutUi });
  ok(ab(d, 'VERSION', '-', 36) && ab(d, 'TEMP', '-', 36 + 96) && ab(d, 'CPU', '-/-%', 36 + 120), '... "-" for what is not known');
  const sc = (k, c) => { const u = { ...OLED.create(), screen: 'info', page: 'about' }; for (const b of k) OLED.press(u, b, 0, c); return u.ascroll; };
  eq([sc(['down'], { ...batt, layout: 'lcd2' }), sc(['down', 'down'], { ...batt, layout: 'lcd2' }), sc(['down', 'up'], { ...batt, layout: 'lcd2' }), sc(['down', 'down', 'down'], batt), sc(['down', 'down', 'down', 'up'], batt), sc(['up'], batt)], [8, 8, 0, 10, 5, 0], 'ABOUT: DOWN / UP move one page (8 rows on lcd2: pages start at 0 and 8; 5 on the OLED: 0, 5, 10), no row on two pages, stopped at the ends');
  for (const screen of ['netinfo', 'confirm']) { d = drawn(batt, { ui: { ...OLED.create(), screen, ncur: 0 } }); noInk(d, 'the ' + screen + ' screen'); }
  // the NETWORK screen: its rows (TYPE, GROUP, PEERS) and the read-only SIGNAL row under them, the only ink on the screen is the quality word
  const netD = rssi => drawn({ ...batt, rssi }, { ui: { ...OLED.create(), screen: 'net', ncur: 0 } });
  const SY = 26 + 3 * 18;                                            // the SIGNAL row follows the 3 rows of the JANK mode
  for (const [rssi, dbm, word, id] of [[-55, '-55dBm', 'GOOD', INK.GOOD], [-67, '-67dBm', 'GOOD', INK.GOOD], [-68, '-68dBm', 'FAIR', INK.WARN], [-80, '-80dBm', 'FAIR', INK.WARN], [-81, '-81dBm', 'POOR', INK.BAD], [0, null, 'NONE', INK.DIM]]) {
    const n = netD(rssi), wx = 312 - textW(word);
    eq([isText(n, 'SIGNAL', 8, SY, null), isText(n, word, wx, SY, id), dbm === null || isText(n, dbm, wx - 12 - textW(dbm), SY, 0)], [true, true, true], 'NETWORK, signal ' + rssi + ': SIGNAL ' + (dbm || '') + ' ' + word + ' (' + id + ')');
    eq(outsideInk(n, [[wx, SY, textW(word), 14]]), 0, '... the word is the only ink on the screen');
  }
  { const n = netD(null); eq([isText(n, 'SIGNAL', 8, SY, null), isText(n, '-', 312 - textW('-'), SY, 0), n.ink.every(v => v === 0)], [true, true, true], 'NETWORK, signal unknown: SIGNAL and "-", no ink'); }
  eq([litIn(netD(-55), 0, SY + 14, 320, 2).length, litIn(drawn({ ...batt, rssi: -55 }, { ui: { ...OLED.create(), screen: 'net', ncur: 1 } }), 0, SY - 2, 320, 1).length], [0, 0], 'the SIGNAL row is never selected (not inverted) and keeps its place');
  // the air rows under SIGNAL (JANK mode only): LOST, LEVEL, RX RETRY; the quality word and the percentages are the only ink
  const AY = SY + 18, airD = (air, over = {}) => drawn({ ...batt, rssi: -55, ...air, ...over }, { ui: { ...OLED.create(), screen: 'net', ncur: 0 } });
  const right = (s, y) => [312 - textW(s), y];
  const airRows = (n, lost, lostInk, level, retry, retryInk) => [isText(n, 'LOST', 8, AY, null), isText(n, lost, ...right(lost, AY), lostInk), isText(n, 'LEVEL', 8, AY + 18, null),
    isText(n, level, ...right(level, AY + 18), 0), isText(n, 'RX RETRY', 8, AY + 36, null), isText(n, retry, ...right(retry, AY + 36), retryInk)];
  for (const [lost, id] of [[0, INK.GOOD], [2, INK.GOOD], [3, INK.WARN], [9, INK.WARN], [10, INK.BAD], [100, INK.BAD]]) {
    const n = airD({ airLost: lost, airRetry: 1, rssiMin: -78, rssiMax: -52 });
    eq(airRows(n, lost + '%', id, '-78/-52 dBm', '1%', 0), Array(6).fill(true), 'NETWORK, LOST ' + lost + '%: the label, the value in ink ' + id + ', LEVEL -78/-52 dBm and RX RETRY 1% neutral');
    eq(outsideInk(n, [[312 - textW('GOOD'), SY, textW('GOOD'), 14], [312 - textW(lost + '%'), AY, textW(lost + '%'), 14]]), 0, '... the SIGNAL word and LOST are the only ink on the screen');
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
    eq([litIn(n, 0, AY + 18, 320, 40).length, n.ink.every(v => v === 0)], [0, true], 'NETWORK, mode ' + mode + ': no air rows, no ink');
  }
  d = drawn({ ...batt, net: { ...batt.net, scan: [{ name: 'home-wifi', bars: 3 }, { name: 'cafe', bars: 1 }, { name: 'x', bars: 2 }] } }, { ui: { ...OLED.create(), screen: 'scan', scur: 1, stop: 0 } }); noInk(d, 'the scan list');
  d = drawn(batt, { ui: { ...OLED.create(), screen: 'text', txt: { field: 'pass', text: 'secret', pos: 2, mask: true } } }); noInk(d, 'the text editor');
  const setupC = { ...batt, setup: true, qr, net: { ...batt.net, setupName: 'jank-setup-1234' } };
  d = drawn(setupC); noInk(d, 'SETUP with its QR code');
  const qrCols = litCols(d, 2, 236), qrRows = [...litRows(d, 0, 200)];
  ok(Math.max(...qrRows) < LH && qrCols[0] === 0 && qrCols.length > 200, 'the QR code (25 modules) is drawn big on the left, inside the frame (x ' + qrCols[0] + '..' + Math.max(...qrCols) + ', y ' + Math.min(...qrRows) + '..' + Math.max(...qrRows) + ')');
  let boxW = 0; while (d.fb[2 * LW + boxW]) boxW++;
  ok(boxW === 6 * 25 + 12 && litIn(d, boxW, 0, LW - boxW, LH).length > 200, 'module size 6: the lit box is ' + boxW + ' px, the text keeps the rest of the width');
  d = drawn({ ...setupC, qr: { size: 33, mod: Array.from({ length: 33 }, () => Array(33).fill(1)) } });
  ok(Math.max(...litRows(d)) < LH, 'a bigger QR code (33 modules) steps down to module size 4 and still fits');
}

// overlays and SAVED: centred on the new frame, nothing coloured under them
{
  const hov = { ...OLED.create(), overlay: { until: NOW + 1000, title: 'DOUBLE', value: '240.0' } };
  let d = drawn(ctxOf(), { ui: hov });
  const rowsLit = j => { let lo = -1, hi = -1; for (let i = 0; i < LW; i++) if (d.fb[j * LW + i]) { if (lo < 0) lo = i; hi = i; } return [lo, hi]; };
  eq(rowsLit(104), [30, 289], 'the overlay box is 260 px wide, centred (30 px each side)');
  eq(idsIn(d, 30, 60, 260, 120), [0], 'no ink under the overlay box');
  ok(litIn(d, 40, 100, 240, 60).length > 400, 'its value is big (scale 4 for 240.0)');
  d = drawn({ ...ctxOf(), saved: true });
  eq([idsIn(d, 70, 84, 180, 68), (() => { const r = [-1, -1]; for (let i = 0; i < LW; i++) if (d.fb[104 * LW + i]) { if (r[0] < 0) r[0] = i; r[1] = i; } return r; })()], [[0], [70, 249]], 'the SAVED box is centred (70 px each side) and carries no ink');
  const hint = { ...OLED.create(), hint: { title: 'PHASE', up: 'UP: +1/2', down: 'DOWN: -1/2' } };
  d = drawn(ctxOf(), { ui: hint });
  eq([rowsLit(104), idsIn(d, 30, 60, 260, 120)], [[30, 289], [0]], 'the NO / YES hint box is centred on the frame too');
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
  eq([lcd.w, lcd.h, lcd.fb.length, lcd.ink.length, lcd.ink.some(v => v !== 0)], [320, 240, 76800, 76800, true], 'Stick.screen(model, now, "lcd2"): the 320 x 240 frame buffer and its ink plane, made by the stick');
  eq([oled.w, oled.h, oled.ink, Buffer.from(oled.fb).equals(Buffer.from(Stick.framebuffer(model, NOW)))], [128, 64, null, true], 'Stick.screen without a layout, and Stick.framebuffer: the OLED\'s 128 x 64, no ink');
}

console.log('lcd2: ' + checks + ' checks passed');
