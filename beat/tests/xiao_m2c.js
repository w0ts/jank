// node xiao_m2c.js PLAYER_HTML: the page against the firmware M2c (fw/integ-2): the fold (UP / DOWN call `fold`, the label, the
// `home` line's mode token, `status` fold= / fold_manual=), AUTO BAR's three settings (OFF / AT START / ALWAYS, `set autobar`,
// read back from `get`), and the Bluetooth rune in HOME's top bar (no overlaps)
const fs = require('fs');
const assert = require('assert');
const html = require('../../tests/load_html.js')(process.argv[2]);
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
const konst = name => html.match(new RegExp('const ' + name + ' = ([0-9.-]+);'))[0];
const consts = ['XIAO_STATUS_FRESH_MS', 'XIAO_PLAY_WAIT_MS', 'XIAO_HOME_FRESH_MS', 'XIAO_REPORT_FRESH_MS'].map(konst).join('\n');
const OLED = new Function(html.match(/const OLED = \(\(\) => \{[\s\S]*?\n\}\)\(\);\n/)[0] + '; return OLED;')();
let checks = 0;
const ok = (c, msg) => { assert.ok(c, msg); checks++; };
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };
const fns = ['homeMode', 'parseHome', 'foldNow', 'foldNext', 'autoBarFrom'];
const P = new Function('const AUTOBAR_WORDS = ["off", "start", "always"];\n' + consts + '\n' + fns.map(grab).join('\n') + '; return { ' + fns.join(', ') + ' };')();

// ---- the home line's mode token ----
const H = tok => 'home=h 12800 0 1 2 4 1 0 87 -23 64 -30 -12' + (tok === undefined ? '' : ' ' + tok);
eq([P.parseHome(H('0')).mode, P.parseHome(H('0')).fold], ['link', null]);
eq([P.parseHome(H('1')).mode, P.parseHome(H('1')).fold], ['audio', 1]);
for (const [t, k] of [['x2', 2], ['x4', 4], ['/2', 0.5], ['/4', 0.25]]) eq([P.parseHome(H(t)).mode, P.parseHome(H(t)).fold], ['audio', k]);
eq([P.parseHome(H('zz')).mode, P.parseHome(H('zz')).fold], [null, null]);
eq(P.parseHome(H()).fold, null);                                                   // an older firmware
eq(P.parseHome(H('x2')).max, -12);                                                  // the numbers before it unchanged

// ---- the fold in force ----
const st = (o = {}) => Object.assign({ link_peers: 1, fold: 1, fold_manual: 0 }, o);
const homeAt = (tok, at = 1000) => [P.parseHome(H(tok)), at];
eq(P.foldNow(false, null, 0, null, 0, null, 5, 2, true), { k: 2, auto: true });    // local: the page's k and the player's fold_auto
eq(P.foldNow(false, null, 0, null, 0, null, 5, 1, true), { k: 1, auto: false });
eq(P.foldNow(true, st({ fold: 2 }), 1000, null, 0, null, 1500, 1, false), { k: 2, auto: true });          // status: AUTO RANGE
eq(P.foldNow(true, st({ fold: 0.5, fold_manual: 1 }), 1000, null, 0, null, 1500, 1, false), { k: 0.5, auto: false });   // the user's
eq(P.foldNow(true, st({ fold: 1 }), 1000, null, 0, null, 1500, 1, false), { k: 1, auto: false });
eq(P.foldNow(true, st({ fold: 2 }), 1000, null, 0, null, 7000, 1, false), { k: 1, auto: false });          // a stale status: none
eq(P.foldNow(true, st({ fold: 1 }), 1000, ...homeAt('x4'), null, 1500, 1, false), { k: 4, auto: false });  // the home line wins (fresh)
eq(P.foldNow(true, st({ fold: 4 }), 1000, ...homeAt('x4'), null, 1500, 1, false), { k: 4, auto: true });
eq(P.foldNow(true, st({ fold: 2 }), 1000, ...homeAt('/2', 100), null, 1700, 1, false), { k: 2, auto: true });   // home too old: the status
eq(P.foldNow(true, st({ fold: 2 }), 1000, ...homeAt('0'), null, 1500, 1, false).k, 2);                    // LINK token: no fold of its own
eq(P.foldNow(true, st({ fold: 1 }), 1000, null, 0, { k: 2, at: 1200 }, 1500, 1, false), { k: 2, auto: false });   // `fold x2` sent, no answer yet
eq(P.foldNow(true, st({ fold: 1 }), 2000, null, 0, { k: 2, at: 1200 }, 2100, 1, false), { k: 1, auto: false });   // a newer status answered
eq(P.foldNow(true, st({ fold: 1 }), 1000, null, 0, { k: 2, at: 1200 }, 1200 + 2600, 1, false), { k: 1, auto: false });   // never stuck
eq([P.foldNext(1, 1), P.foldNext(1, -1), P.foldNext(2, 1), P.foldNext(4, 1), P.foldNext(0.25, -1), P.foldNext(0.5, 1)], [2, 0.5, 4, 4, 0.25, 1]);

// UP / DOWN with --xiao in AUDIO: `fold x2` / `fold /2`, and the BPM the overlay shows is the folded one
{
  const sent = [], timers = [];
  const XIAO = { status: st({ fold: 1 }), statusAt: 900, home: null, homeAt: 0, foldSent: null, rep: { bpm: 100 }, repAt: 950 };
  const env = { S: { xiao: true }, dev: { link: false, foldK: 1 }, XIAO, performance: { now: () => 1000 }, XIAO_REPORT_FRESH_MS: 3000, foldNow: P.foldNow, foldNext: P.foldNext,
                xiaoSend: l => sent.push(l), setTimeout: f => timers.push(f), askStatus: () => {}, askHome: () => {}, XIAO_STATUS_FRESH_MS: 5000, XIAO_HOME_FRESH_MS: 1500,
                XIAO_PLAY_WAIT_MS: 2500, hold: () => {}, api: () => { throw new Error('the local fold API'); }, clampN: (v, a, b) => Math.min(b, Math.max(a, v)), TEMPO_RANGE: [20, 300] };
  const body = grab('tempoStep').replace(/^function tempoStep\(d\) \{/, 'function tempoStep(d) {');
  const tempoStep = new Function(...Object.keys(env), body + '; return tempoStep;')(...Object.values(env));
  eq(tempoStep(1), 200);
  eq(tempoStep(-1), 50);
  eq(sent, ['fold x2', 'fold /2']);
  eq(XIAO.foldSent.k, 1);                       // x2 then /2: back to 1
}

// ---- AUTO BAR: the setting read back ----
eq(P.autoBarFrom(true, { autobar: 'always' }, null, {}), 2);
eq(P.autoBarFrom(true, { autobar: 'start' }, { bar: 'off' }, {}), 1);              // get wins over the status
eq(P.autoBarFrom(true, null, { bar: 'off' }, {}), 0);
eq(P.autoBarFrom(true, null, { bar: 'locked' }, {}), null);
eq(P.autoBarFrom(false, null, null, { autobar: 'off' }), 0);
eq(P.autoBarFrom(false, null, null, {}), null);

// ---- AUTO BAR on the screen: three choices, 2 lines of help each, cycled, the old true / false still read ----
const W = OLED.W, H_ = OLED.H;
const calls = [];
const ctx = (extra = {}) => ({ link: false, lineIn: true, bpm: 124, linkBpm: 124, conf: 0.9, th: 80, levelDb: -20, levelMin: -25, levelMax: -15, running: true, armed: false,
  beatN: 1, beatMs: 0, peers: 0, wifiQ: null, usb: false, fitted: false, batt: 100, ltMode: 2, rate: 2, usbRole: 0, inSrc: 0, net: { mode: 0, group: 1, ssid: '', apName: '' },
  autoBar: 1, bar: null, ble: { mode: 1, left: null, phone: false }, fold: 1, setField: (f, v) => calls.push([f, v]), tempoStep: () => 0, setHold: () => {}, linkStartStop: () => {}, ...extra });
const draw = (c, now = 0, ui = OLED.create()) => { const fb = new Uint8Array(W * H_); OLED.draw(fb, ui, c, now); return fb; };
eq(OLED.AUTOBAR_NAMES, ['OFF', 'AT START', 'ALWAYS']);
eq([OLED.autoBarIndex(true), OLED.autoBarIndex(false), OLED.autoBarIndex(2), OLED.autoBarIndex(undefined)], [1, 0, 2, 0]);
{
  const ui = OLED.create(), c = ctx({ autoBar: 1 });
  OLED.press(ui, 'no', 0, c); OLED.press(ui, 'down', 0, c); OLED.press(ui, 'down', 0, c); OLED.press(ui, 'yes', 0, c);
  eq([ui.screen, ui.edit.field, ui.edit.val], ['edit', 'autobar', 1]);
  const seen = [];
  for (let i = 0; i < 3; i++) {                                                      // UP cycles: AT START -> ALWAYS -> OFF -> AT START
    OLED.press(ui, 'up', 0, c);
    seen.push(ui.edit.val);
  }
  eq(seen, [2, 0, 1]);
  OLED.press(ui, 'up', 0, c);                                                       // ALWAYS: its help is drawn, two lines of at most 21 characters
  const fb = draw(c, 0, ui);
  ok(fb.some(v => v === 1), 'the edit screen draws');
  OLED.press(ui, 'yes', 0, c);
  eq(calls.splice(0), [['autobar', 2]]);
  // the menu row shows the setting
  const m = OLED.create(); OLED.press(m, 'no', 0, ctx());
  for (const [v, t] of [[0, 'OFF'], [1, 'START'], [2, 'ALWAYS'], [true, 'START'], [false, 'OFF']]) {
    const f1 = draw(ctx({ autoBar: v }), 0, m);
    ok(f1.some(x => x === 1), 'menu ' + t);
  }
}
{
  const src = html.match(/autobar: \{ title: 'AUTO BAR'[\s\S]*?\]\] \},/)[0];
  const lines = [...src.matchAll(/'([^']*)'/g)].map(m => m[1]).slice(1).filter(t => !/^(OFF|AT START|ALWAYS)$/.test(t));
  ok(lines.length === 6 && lines.every(t => t.length <= 21), 'three choices, two help lines each, 21 characters at most: ' + lines.join(' | '));
}

// ---- the fold's label on HOME (AUDIO) ----
eq([OLED.foldLabel(1), OLED.foldLabel(2), OLED.foldLabel(4), OLED.foldLabel(0.5), OLED.foldLabel(0.25)], ['', 'X2', 'X4', '/2', '/4']);
const lit = (fb, x, y) => fb[y * W + x] === 1;
const noName = { mode: 1, group: 1, ssid: '', apName: '' };
const lab = fb => { let n = 0; for (let y = 0; y < 7; y++) for (let x = 12; x < 24; x++) n += fb[y * W + x]; return n; };
ok(lab(draw(ctx({ fold: 1, net: noName }))) === 0, 'no label at 1');
for (const k of [2, 4, 0.5, 0.25]) ok(lab(draw(ctx({ fold: k, net: noName }))) > 6, 'label at ' + k);
{
  const a = draw(ctx({ fold: 2, link: true })), b = draw(ctx({ fold: 1, link: true }));
  ok(a.every((v, i) => v === b[i]), 'LINK mode shows no fold');
}

// ---- the Bluetooth rune: the top bar's boxes never overlap ----
const nets = [{ mode: 0, group: 9, ssid: '', apName: '' }, { mode: 1, group: 1, ssid: 'A-VERY-LONG-WIFI-NETWORK-NAME-HERE', apName: '' },
              { mode: 2, group: 1, ssid: '', apName: 'JANK-AP-WITH-A-LONG-NAME' }];
const bles = { off: { mode: 2, left: null, phone: false }, blinking: { mode: 0, left: 90, phone: false }, solid: { mode: 0, left: null, phone: true } };
const boxes = t => ['tri', 'left', 'nameBox', 'net', 'ble', 'usb', 'batt'].filter(k => t[k]).map(k => [k, t[k].l, t[k].r]);
let shown = 0;
for (const net of nets) for (const [bn, ble] of Object.entries(bles)) for (const usb of [false, true]) for (const fitted of [false, true]) for (const link of [false, true]) for (const fold of [1, 2, 0.25]) {
  const c = ctx({ net, ble, usb, fitted, link, fold });
  const t = OLED.topBar(c), bx = boxes(t), tag = [net.mode, bn, usb, fitted, link, fold].join('/');
  for (let i = 0; i < bx.length; i++) {
    ok(bx[i][1] >= 0 && bx[i][2] < W && bx[i][1] <= bx[i][2], 'inside the screen: ' + bx[i][0] + ' ' + tag);
    for (let j = i + 1; j < bx.length; j++) ok(bx[i][2] < bx[j][1] || bx[j][2] < bx[i][1], bx[i][0] + ' overlaps ' + bx[j][0] + ' ' + tag);
  }
  ok(!!t.ble === (bn !== 'off'), 'the rune is there unless BLE is idle: ' + tag);
  // pixels: the name's ink stays inside its box; the rune's columns hold only the rune (or nothing)
  for (const now of [0, 400]) {
    const fb = draw(c, now);
    const free = [];
    for (let x = (t.left ? t.left.r : t.tri.r) + 1; x < t.net.l; x++) for (let y = 0; y < 7; y++) if (lit(fb, x, y)) free.push(x);
    ok(free.every(x => t.nameBox && x >= t.nameBox.l && x <= t.nameBox.r), 'the name stays in its box: ' + tag + ' ' + free.join(','));
    if (t.ble) {
      const on = bn === 'solid' || Math.floor(now / 400) % 2 === 0;
      let n = 0;
      for (let y = 0; y < 7; y++) for (let x = t.ble.l; x <= t.ble.r; x++) n += fb[y * W + x];
      ok(n === (on ? 15 : 0), 'the rune ' + (on ? 'lit' : 'dark') + ' ' + tag + ' ' + n);
      shown++;
    }
  }
}
ok(shown > 0, 'rune states drawn');
{
  const base = OLED.topBar(ctx({ ble: bles.off }));
  const wide = OLED.topBar(ctx({ ble: bles.solid }));
  ok(base.net.r === 86 && wide.net.r < base.net.r && wide.ble.l > wide.net.r, 'the network icon moves left for the rune');
  const g9 = OLED.topBar(ctx({ ble: bles.solid, net: nets[0] }));
  eq(g9.name, 'GRP 9');                                                             // the longest group name still fits in AUDIO
  const idle = OLED.topBar(ctx({ ble: bles.off, net: nets[0] }));
  ok(idle.nameBox.r === 86 - 10 - 3 && g9.nameBox.r === g9.net.l - 3, 'the name stays against the network icon');
}
console.log('xiao M2c page helpers: ' + checks + ' checks passed');
