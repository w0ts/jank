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
const { OLED, Lcd2, Stick } = new Function(oledSrc + read('ui/lcd2.js') + read('ui/stick.js') + '; return { OLED, Lcd2, Stick };')();
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
const withUi = (c, keys) => {                                      // a menu state: the ui after these button presses
  const cc = { ...c, tempoStep: () => null, setField() {}, setHold() {}, linkStartStop() {} }, m = Stick.create(() => cc, () => {});
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
const GOLDEN = JSON.parse('["92586ffb36b70a7d","12a9465b16c68428","f4f15225710bfde3","175c54f575f5b515","9da040822b6d69fc","4df4155e0ba4841d","2fc384e7b7fbbf3b","d7690151e5ad2b33"]');
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

// ---- the HOME rows (AUDIO), one table: every element's box, and nothing in the gaps ----
const L = OLED.LCD2;
const litIn = (d, x, y, w, h) => { const out = []; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (d.fb[j * d.w + i]) out.push(d.ink[j * d.w + i]); return out; };
const idsIn = (d, x, y, w, h) => [...new Set(litIn(d, x, y, w, h))].sort();
const litRows = (d, x0 = 0, x1 = d.w) => { const rows = new Set(); for (let j = 0; j < d.h; j++) for (let i = x0; i < x1; i++) if (d.fb[j * d.w + i]) rows.add(j); return rows; };
const litCols = (d, y, h) => { const cols = []; for (let i = 0; i < d.w; i++) for (let j = y; j < y + h; j++) if (d.fb[j * d.w + i]) { cols.push(i); break; } return cols; };
const BANDS = [['top bar', 0, 12], ['tempo', L.tempoY, L.tempoY + 7 * L.tempoScale - 1], ['beat boxes', L.boxY, L.boxY + L.boxH - 1], ['CF', L.rowCF, L.rowCF + 6], ['IN', L.rowIN, L.rowIN + 6], ['BAR', L.rowBAR, L.rowBAR + 6], ['status', L.statusY, L.statusY + 6]];
for (let i = 1; i < BANDS.length; i++) ok(BANDS[i][1] > BANDS[i - 1][2], BANDS[i][0] + ' starts below ' + BANDS[i - 1][0] + ' (' + BANDS[i][1] + ' > ' + BANDS[i - 1][2] + '): the rows do not overlap');
ok(BANDS[BANDS.length - 1][2] < LH, 'the last row is on the grid');
eq([L.tempoY + 7 * L.tempoScale - 1, L.boxY + L.boxH - 1, 2 + 4 * L.boxW + 3 * L.boxGap], [49, 77, 158], 'tempo ends at y 49, the boxes at y 77 and x 157 (4 x 36 + 3 x 4 from x 2)');
ok(L.boxY - (L.tempoY + 7 * L.tempoScale) >= 1 && L.rowCF - (L.boxY + L.boxH) >= 2 && L.linkBoxY - (L.tempoY + 7 * L.tempoScale) >= 1 && L.statusY - (L.linkBoxY + L.linkBoxH) >= 2, 'the boxes keep a gap to the tempo above and to the CF row (AUDIO) or status line (LINK) below');
const barOn = { bar: { state: 'locked', conf: 0.5 } };
const full = drawn(ctxOf({ ctx: barOn }));
{
  const rows = litRows(full), inBand = j => BANDS.some(([, a, b]) => j >= a && j <= b);
  ok([...rows].every(inBand), 'every lit pixel of a full HOME lies inside a row of the table (none in the gaps): rows ' + [...rows].filter(j => !inBand(j)).join());
  const tempoRows = litRows(full, 2, 133);
  ok([...tempoRows].filter(j => j >= BANDS[1][1] && j <= BANDS[1][2]).length === 35, 'the tempo (scale 5) fills its 35 rows');
  const widthOf = (d, y0, y1) => { let lo = d.w, hi = -1; for (let j = y0; j <= y1; j++) for (let i = 0; i < L.bpmX - 2; i++) if (d.fb[j * d.w + i]) { lo = Math.min(lo, i); hi = Math.max(hi, i); } return [lo, hi]; };
  const [lo, hi] = widthOf(drawn(ctxOf({ home: { bpm: 123.4 } })), 15, 49);
  ok(lo >= 2 && hi <= 131 && hi - lo >= 100, 'the tempo 123.4 spans x ' + lo + '..' + hi + ' of 2..131');
  eq(widthOf(drawn(ctxOf({ home: { bpm: 999.9 } })), 15, 49)[1] <= 131, true, 'the widest tempo (999.9) still fits left of the BPM label');
  ok(litIn(full, L.bpmX, L.bpmY, 17, 7).length > 20, "the 'BPM' label sits right of the tempo");
}

// healthy: ink only inside the top bar's link / signal / battery, the first beat box, the CF number, bar and tick (no IN, BAR, status ink)
const R = { link: [L.linkX, 0, 24, 13], signal: [L.signalX, 0, 11, 13], batt: [L.battX, 2, L.battW + 2, 9], beat1: [L.boxX, L.boxY, L.boxW, L.boxH],
            cfValue: [L.valueXR - 23, L.rowCF, 23, 7], cfBar: [L.barX, L.rowCF, L.barW, 7], inValue: [L.valueXR - 29, L.rowIN, 29, 7], inBar: [L.barX, L.rowIN, L.barW, 7] };
const insideAny = (boxes, x, y) => boxes.some(([bx, by, bw, bh]) => x >= bx && x < bx + bw && y >= by && y < by + bh);
const outsideInk = (d, boxes) => { let n = 0; for (let i = 0; i < d.ink.length; i++) if (d.ink[i] !== 0 && !insideAny(boxes, i % d.w, Math.floor(i / d.w))) n++; return n; };
const healthy = drawn(ctxOf());
ok(healthy.ink.every((v, i) => v === 0 || healthy.fb[i] === 1), 'ink only on lit pixels');
eq(outsideInk(healthy, [R.link, R.signal, R.batt, R.beat1, R.cfValue, R.cfBar]), 0, 'a healthy HOME: ink only in the link glyph, signal bars, battery, the lit first beat, and the CF value, bar and tick');
eq(idsIn(healthy, ...R.link), [INK.GOOD], 'joined (2 Link peers): the link glyph and its count are green');
eq(idsIn(healthy, ...R.signal), [INK.GOOD], '-55 dBm: the signal bars are green');
eq(idsIn(healthy, ...R.batt), [INK.GOOD], '80 %: the battery is green');
eq(idsIn(healthy, ...R.beat1), [INK.ONE], 'the first beat box, lit, carries the bar-1 id');
eq([1, 2, 3].map(i => idsIn(healthy, L.boxX + i * (L.boxW + L.boxGap), L.boxY, L.boxW, L.boxH)), [[0], [0], [0]], 'the other boxes are neutral');
eq(idsIn(healthy, ...R.cfValue), [INK.GOOD], 'CF 90 against 70: the value is green');
eq(idsIn(healthy, ...R.inValue), [0], 'an input at -20 dB: the number is neutral');
eq(idsIn(healthy, ...R.inBar), [0], '... and its bar, fill and tick');

// the first beat is amber only while it is the lit one
{
  const on = drawn(ctxOf({ ctx: { beatN: 4 } })), off = drawn(ctxOf({ ctx: { beatN: 1 } })), none = drawn(ctxOf({ ctx: { beatN: null } }));
  eq([idsIn(on, ...R.beat1), idsIn(off, ...R.beat1), idsIn(none, ...R.beat1)], [[INK.ONE], [0], [0]], 'beat 4 is the bar\'s first again (amber); beat 2 and no beat: the first box is a neutral frame');
  const box = (d, i) => litIn(d, L.boxX + i * (L.boxW + L.boxGap), L.boxY, L.boxW, L.boxH).length;
  const outline = (w, h) => w * h - (w - 4) * (h - 4);
  eq([box(on, 0), box(on, 1), box(off, 1), box(off, 0)], [L.boxW * L.boxH, outline(L.boxW, L.boxH), L.boxW * L.boxH, outline(L.boxW, L.boxH)], 'the lit box is solid (36 x 26), the others are 2 px outlines');
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

// ---- confidence: the value, the bar's fill and the tick all carry the id of the rule ----
const confCase = (conf, extra = {}, get) => drawn(ctxOf({ home: { conf }, ...extra }, get ? { get } : undefined));
const TICK = L.barX + Math.round((L.barW - 1) * 0.7);                             // the stop threshold 70 %
for (const [conf, id, why] of [[60, INK.BAD, 'below'], [69, INK.BAD, 'below'], [70, INK.WARN, 'at'], [75, INK.WARN, 'within 10 above'], [80, INK.WARN, '10 above'], [81, INK.GOOD, 'over 10 above'], [87, INK.GOOD, 'above'], [100, INK.GOOD, 'full']]) {
  const d = confCase(conf);
  const bar = idsIn(d, L.barX, L.rowCF, L.barW, 7).filter(v => v !== 0), val = idsIn(d, ...R.cfValue);
  eq([val, bar], [[id], [id]], 'CF ' + conf + ' against 70 (' + why + '): the value and the bar are the same colour id (' + id + ')');
}
eq(idsIn(confCase(75, {}, { threshold: '50', lowconf: 'stop' }), ...R.cfValue), [INK.GOOD], 'the threshold is the stick\'s: 75 against 50 is green');
eq(idsIn(confCase(75, {}, { threshold: '90', lowconf: 'run' }), ...R.cfValue), [INK.BAD], '... against 90 red, also with KEEP RUNNING');
eq(idsIn(confCase(30, { ctx: { silent: true } }), ...R.cfValue), [INK.DIM], 'silence: the CF number is grey, not red (nothing to track)');
eq(idsIn(drawn(ctxOf({ home: { conf: null } })), ...R.cfValue), [INK.DIM], 'no confidence reported: the dashes are grey');
eq(idsIn(drawn(ctxOf({}, { status: null })), ...R.cfValue), [INK.GOOD], 'no status line: the confidence comes from the HOME line and is still green');
// the CF line: ONE thin line (3 rows, y + 2..y + 4), its length the value on 94 px; fill length at 0, 50 and 100 %, counted on its middle row away from the notch
for (const [conf, n] of [[0, 0], [50, 47], [100, 94]]) {
  const d = confCase(conf), row = L.rowCF + 3, inLine = n > TICK - L.barX;
  let lit = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (x !== TICK && d.fb[row * d.w + x]) lit++;
  eq(lit, n - (inLine ? 1 : 0), 'CF ' + conf + ' %: the line is ' + n + ' px of 94' + (inLine ? ' (one is the notch\'s column)' : ''));
  eq([d.fb[row * d.w + L.barX], d.fb[row * d.w + L.barX + L.barW - 1], d.fb[row * d.w + L.barX - 1]], [n > 0 ? 1 : 0, n === 94 ? 1 : 0, 0], '... it starts at x ' + L.barX + ' and ends at its value, nothing beyond');
}
{
  const d = confCase(50), lineRows = [0, 1, 2, 3, 4, 5, 6].map(k => { let c = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (x !== TICK && d.fb[(L.rowCF + k) * d.w + x]) c++; return c; });
  eq(lineRows, [0, 0, 47, 47, 47, 0, 0], 'CF 50 %: the line is exactly 3 rows (y + 2..y + 4): no outline above or below, no track, nothing in the rest of the row');
  eq([d.fb[(L.rowCF + 3) * d.w + L.barX + 80], d.fb[(L.rowCF + 1) * d.w + L.barX + 80], d.fb[(L.rowCF + 5) * d.w + L.barX + 93]], [0, 0, 0], '... the unfilled remainder is plain background (no baseline)');
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

// ---- input level: number and bar; red clipping, amber too quiet, nothing else coloured ----
const levelCase = (levelDb, peakDb, max) => drawn(ctxOf({ home: { level: levelDb, levelMax: max === undefined ? levelDb : max }, ctx: { peakDb } }));
const levelIds = d => [idsIn(d, ...R.inValue).filter(v => v !== 0), idsIn(d, L.barX, L.rowIN + 2, L.barW, 3).filter(v => v !== 0)];
eq(levelIds(levelCase(-20, -12)), [[], []], 'a good level: number, fill and tick are neutral');
eq(levelIds(levelCase(-20, -0.5)), [[INK.BAD], [INK.BAD]], 'the last second peaked at -0.5 dBFS: number and fill red (clipping)');
eq(levelIds(levelCase(-20, -1)), [[INK.BAD], [INK.BAD]], '... -1 dBFS is the edge');
eq(levelIds(levelCase(-20, -1.5)), [[], []], '... -1.5 is not clipping');
eq(levelIds(levelCase(-20, null, -2))[0], [INK.BAD], 'peak not reported: an RMS maximum over -3 dB counts');
eq(levelIds(levelCase(-20, null, -10))[0], [], '... under it, neutral');
eq(levelIds(levelCase(-55, -40)), [[INK.WARN], [INK.WARN]], 'under the tracker\'s -50 dBFS sound level: amber (too quiet to track)');
eq(levelIds(levelCase(-50, -40)), [[], []], '... -50 itself is enough');
{
  const IN_TICK = L.barX + Math.round((L.barW - 1) * (40 / 90));                  // -50 dBFS on a -90..0 line
  for (const [db, n] of [[-90, 0], [-45, 47], [0, 94]]) {
    const d = levelCase(db, -12, db), row = L.rowIN + 3, inLine = n > IN_TICK - L.barX;
    let lit = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (x !== IN_TICK && d.fb[row * d.w + x]) lit++;
    eq(lit, n - (inLine ? 1 : 0), 'IN ' + db + ' dB: the line is ' + n + ' px of 94 (-90 dB is empty, 0 dB full)');
  }
  const d = levelCase(-20, -12);
  eq([d.fb[(L.rowIN + 1) * d.w + IN_TICK], d.fb[(L.rowIN + 5) * d.w + IN_TICK], d.ink[(L.rowIN + 1) * d.w + IN_TICK], d.fb[L.rowIN * d.w + IN_TICK]], [1, 1, 0, 0], 'the IN line has a neutral 5 px notch at -50 dBFS (x ' + IN_TICK + '), the tracker\'s sound level');
  const c = levelCase(-55, -40), rows = [0, 1, 2, 3, 4, 5, 6].map(k => { let n = 0; for (let x = L.barX; x < L.barX + L.barW; x++) if (x !== IN_TICK && c.fb[(L.rowIN + k) * c.w + x]) n++; return n; });
  eq(rows, [0, 0, 37, 37, 37, 0, 0], 'IN -55 dB: a 3 row line of round(94 x 35 / 90) = 37 px, nothing else in the row');
}

// ---- the BAR row: AUTO BAR's state and confidence, neutral, only with AUTO BAR on ----
{
  const rowLit = d => litIn(d, 0, L.rowBAR, d.w, 7).length;
  eq([rowLit(healthy), rowLit(drawn(ctxOf({ ctx: { bar: { state: 'off', conf: 0 } } })))], [0, 0], 'no AUTO BAR (or OFF): no BAR row');
  const lock = drawn(ctxOf({ ctx: { bar: { state: 'locked', conf: 0.5 } } })), lis = drawn(ctxOf({ ctx: { bar: { state: 'listening', conf: 1 } } })), man = drawn(ctxOf({ ctx: { bar: { state: 'manual', conf: 0.9 } } }));
  ok(rowLit(lock) > 100 && rowLit(lis) > rowLit(lock) && rowLit(man) > 0, 'LOCKED, LISTEN, MANUAL draw the row; the fill grows with the confidence');
  eq([lock, lis, man].map(d => idsIn(d, 0, L.rowBAR, d.w, 7)), [[0], [0], [0]], 'the BAR row is neutral');
  let lit = 0; const row = L.rowBAR + 3; for (let x = L.barX + 1; x < L.barX + L.barW - 1; x++) if (lock.fb[row * lock.w + x]) lit++;
  eq([lit, lock.fb[(L.rowBAR + 1) * lock.w + L.barX + 40], lock.fb[(L.rowBAR + 5) * lock.w + L.barX + 40]], [46, 1, 1], 'BAR 0.5: the BAR row keeps its outlined bar (a frame above and below), 46 px of 92 filled');
}

// ---- the top bar: link, signal, battery (bigger, same rules) ----
eq(idsIn(drawn(ctxOf({ home: { link: 0 } })), ...R.link), [INK.DIM], 'AUDIO mode, no Link peer: dim (a lone leader is normal)');
eq(idsIn(drawn(ctxOf({ home: { link: 0, audio: false } })), ...R.link), [INK.BAD], 'LINK mode, no session peer: red (nothing to follow)');
eq(idsIn(drawn(ctxOf({ home: { link: 1, audio: false } })), ...R.link), [INK.GOOD], 'LINK mode, one peer: green');
{
  const two = drawn(ctxOf({ home: { link: 2 } })), none = drawn(ctxOf({ home: { link: 0 } }));
  ok(litIn(two, L.linkX + 12, 3, 12, 7).length > 0 && litIn(none, L.linkX + 12, 3, 12, 7).length === 0, 'the Link peer count is drawn beside the glyph only when there are peers');
}
const barsOf = (rssi, over = {}) => { const d = drawn(ctxOf({ ctx: { rssi }, ...over })); return [idsIn(d, ...R.signal), litIn(d, ...R.signal).length]; };
eq(barsOf(-55), [[INK.GOOD], 3 * 3 + 6 * 3 + 9 * 3 - 0], '-55 dBm: three bars (3 x 3, 6 x 3, 9 x 3 px), green');
eq(barsOf(-67), [[INK.GOOD], 54], '-67 dBm: the good edge');
eq(barsOf(-68), [[INK.WARN], 3 * 3 + 6 * 3 + 3], '-68 dBm: two bars and a stub, amber (fair)');
eq(barsOf(-80), [[INK.WARN], 30], '-80 dBm: the fair edge');
eq(barsOf(-81), [[INK.BAD], 9 + 3 + 3], '-81 dBm: one bar and two stubs, red (poor)');
eq(barsOf(0), [[INK.DIM], 9], 'none heard (0): three stubs, grey');
eq(barsOf(null), [[INK.DIM], 9], 'not reported (null): the same grey stubs');
const battId = pct => idsIn(drawn(ctxOf({ home: { battery: pct } })), ...R.batt);
eq([battId(100), battId(30), battId(29), battId(15), battId(14), battId(0)], [[INK.GOOD], [INK.GOOD], [INK.WARN], [INK.WARN], [INK.BAD], [INK.BAD]], 'battery: green from 30 %, amber 15..29, red under 15');
eq(idsIn(drawn(ctxOf({ home: { battery: null } })), ...R.batt), [], 'no battery fitted: no icon, no ink');
{
  const fillOf = pct => { const d = drawn(ctxOf({ home: { battery: pct } })); let n = 0; for (let x = L.battX + 2; x < L.battX + L.battW - 2; x++) if (d.fb[6 * d.w + x]) n++; return n; };
  eq([fillOf(0), fillOf(50), fillOf(100)], [0, 9, 17], 'the battery icon\'s fill: 0, 9 and 17 px of 17');
}

// ---- the status line: words on the left, the threshold on the right, no overlap ----
{
  const cols = d => litCols(d, L.statusY, 7), words = { gated: { ctx: { gated: true } }, hold: { home: { confHold: true } }, coast: { home: { conf: 50 }, ctx: { running: true } } };
  eq(idsIn(drawn(ctxOf(words.gated)), 2, L.statusY, 40, 7), [INK.BAD], 'GATED (outputs stopped for low confidence): red');
  eq(idsIn(drawn(ctxOf(words.hold)), 2, L.statusY, 40, 7), [INK.DIM], 'HOLD: grey');
  eq(idsIn(drawn(ctxOf(words.coast)), 2, L.statusY, 40, 7), [INK.WARN], 'COAST (running under the threshold): amber');
  for (const [name, o] of Object.entries(words)) {
    const d = drawn(ctxOf(o)), left = cols(d).filter(x => x < 80), right = cols(d).filter(x => x >= 80);
    ok(left.length > 10 && right.length > 20 && Math.max(...left) + 4 < Math.min(...right), name + ': the word (x ' + left[0] + '..' + Math.max(...left) + ') and the threshold text (x ' + right[0] + '..' + Math.max(...right) + ') do not touch');
    eq(idsIn(d, 80, L.statusY, 80, 7), [0], name + ': the threshold text is neutral');
  }
  eq(litIn(healthy, 0, L.statusY, 80, 7).length, 0, 'a healthy HOME: only the threshold, right (STOP 70)');
  const run = drawn(ctxOf({}, { get: { threshold: '70', lowconf: 'run' } })), stop = drawn(ctxOf());
  ok(litCols(run, L.statusY, 7).length < litCols(stop, L.statusY, 7).length + 8 && litCols(run, L.statusY, 7)[0] > 100, "'MIN 70' with KEEP RUNNING, 'STOP 70' with STOP, both right-aligned");
  const bt = drawn(ctxOf({ ctx: { ble: { mode: 0, left: 95, phone: false } } }));
  ok(litCols(bt, L.statusY, 7).some(x => x < 20), 'the Bluetooth pair window counts down at the left, on its own row');
  const wf = drawn(ctxOf({ ctx: { wifiQ: { rssi: -85, kbps: 24000, tx: 100, retried: 40, lost: 20 } } }));
  eq(idsIn(wf, 0, L.statusY - 1, 120, 9).includes(INK.BAD), true, 'a Wi-Fi line, when the stick reports one, shows its BAD parts inverted in red');
}

// ---- the LINK layout: the same top bar, tempo, big boxes, no audio bars ----
{
  const link = drawn(ctxOf({ home: { audio: false, link: 1, bpm: 98.4 } }));
  eq([litIn(link, 0, L.linkBoxY + L.linkBoxH, link.w, L.statusY - L.linkBoxY - L.linkBoxH).length, outsideInk(link, [R.link, R.signal, R.batt, [L.boxX, L.linkBoxY, L.boxW, L.linkBoxH]])], [0, 0], 'LINK HOME: nothing between the boxes and the status line (no CF, IN or BAR rows), no stray ink');
  eq(idsIn(link, L.boxX, L.linkBoxY, L.boxW, L.linkBoxH), [INK.ONE], 'LINK HOME: the first box (36 x 52), lit, is the bar 1');
  eq(litIn(link, L.boxX + 2 * (L.boxW + L.boxGap), L.linkBoxY, L.boxW, L.linkBoxH).length, L.boxW * L.linkBoxH - (L.boxW - 4) * (L.linkBoxH - 4), 'the others are 2 px outlines of the same size');
  eq(litIn(link, L.boxX + 3 * (L.boxW + L.boxGap) + L.boxW - 1, L.linkBoxY, 1, L.linkBoxH).length, L.linkBoxH, 'the fourth box ends at x 157');
  ok(litRows(link, 2, 133).has(L.tempoY) && litRows(link, 2, 133).has(L.tempoY + 34), 'the tempo is the same size as in AUDIO');
  eq(litIn(link, 0, L.statusY, link.w, 7).length, 0, 'no STOP / MIN text in LINK mode');
}

// ---- menus, edit, info, net, text, confirm, setup: neutral, the whole grid, no ink ----
const noInk = (d, what) => ok(d.fb.some(v => v === 1) && d.ink.every(v => v === 0), what + ': drawn, and no ink at all');
const qr = { size: 25, mod: Array.from({ length: 25 }, (_, y) => Array.from({ length: 25 }, (_, x) => ((x * y + x + y) % 3 === 0 ? 1 : 0))) };
{
  const menu = withUi(ctxOf(), ['no']);
  let d = drawn(menu.c, { ui: menu.ui });
  noInk(d, 'MENU');
  eq([d.fb[0], d.fb[LW - 1], d.fb[10 * LW], d.fb[11 * LW]], [1, 1, 1, 0], 'the title bar is 11 rows, the full width');
  const rowBand = k => litIn(d, 0, 13 + 9 * k, d.w, 7).length;
  eq([rowBand(0), rowBand(1), rowBand(10)].map(n => n > 0), [true, true, true], 'the 11 menu items are rows of 9 px from y 13 (the last at y ' + (13 + 90) + ')');
  eq([d.fb[(13 - 1) * LW], d.fb[(13 - 1) * LW + LW - 1], d.fb[20 * LW + 2]], [1, 1, 1], 'the selected row (the first) is inverted over the full width');
  eq(litIn(d, 0, 13 + 9 - 1, 1, 9).length, 0, 'the next row is not');
  const lastLit = Math.max(...litRows(d));
  ok(lastLit >= 13 + 9 * 10 && lastLit < LH, 'the list reaches y ' + lastLit + ' of 119: the grid is used');
  const dn = withUi(ctxOf(), ['no', ...Array(10).fill('down')]);
  d = drawn(dn.c, { ui: dn.ui });
  noInk(d, 'MENU, last item selected');
  eq(d.fb[(13 + 90 - 1) * LW + 100], 1, 'the last item (BATTERY) is the selected, inverted row at the bottom');
  const sub = withUi(ctxOf(), ['no', 'yes']);
  d = drawn(sub.c, { ui: sub.ui });
  noInk(d, 'a sub menu (' + sub.ui.screen + ')');
  const ed = withUi(ctxOf(), ['no', 'yes', 'yes']);
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
  for (const screen of ['net', 'netinfo', 'confirm']) { d = drawn(batt, { ui: { ...OLED.create(), screen, ncur: 0 } }); noInk(d, 'the ' + screen + ' screen'); }
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
  eq(rowsLit(79), [15, 144], 'the overlay box is 130 px wide, centred (15 px each side)');
  eq(idsIn(d, 15, 30, 130, 60), [0], 'no ink under the overlay box');
  ok(litIn(d, 20, 50, 120, 30).length > 100, 'its value is big (scale 4 for 240.0)');
  d = drawn({ ...ctxOf(), saved: true });
  eq([idsIn(d, 35, 42, 90, 34), (() => { const r = [-1, -1]; for (let i = 0; i < LW; i++) if (d.fb[50 * LW + i]) { if (r[0] < 0) r[0] = i; r[1] = i; } return r; })()], [[0], [35, 124]], 'the SAVED box is centred (35 px each side) and carries no ink');
  const hint = { ...OLED.create(), hint: { title: 'PHASE', up: 'UP: +1/2', down: 'DOWN: -1/2' } };
  d = drawn(ctxOf(), { ui: hint });
  eq([rowsLit(79), idsIn(d, 15, 30, 130, 60)], [[15, 144], [0]], 'the NO / YES hint box is centred on the grid too');
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
