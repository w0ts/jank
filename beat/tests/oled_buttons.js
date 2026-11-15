// node oled_buttons.js PLAYER_HTML: the one-button logic of NO (clicks, triple = Bluetooth pair window, combos), the screenless
// unit (single NO toggles the source, the LED language), the BLUETOOTH menu item and YES with the XIAO in AUDIO
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(process.argv[2], 'utf8');
const OLED = new Function(html.match(/const OLED = \(\(\) => \{[\s\S]*?\n\}\)\(\);\n/)[0] + '; return OLED;')();
const grab = name => {                                   // a function of the page, whole (one line, or up to its closing brace at the line start)
  const one = html.match(new RegExp('^function ' + name + '\\([^)]*\\) \\{[^\\n]*\\}\\n', 'm'));
  return (one || html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n')))[0];
};
const konst = name => html.match(new RegExp('const ' + name + ' = [^\n]*;[^\n;]*\n'))[0].replace(/\/\/.*$/m, '') + '\n';   // one line, its comment dropped
let checks = 0;
const ok = (c, msg) => { assert.ok(c, msg); checks++; };
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };

// ---- the screen: BLUETOOTH in the menu, the SOURCE choices, the status line, the pair box, YES with the XIAO ----
const W = OLED.W, lit = (fb, x, y) => fb[y * W + x] === 1;
const rowLit = (fb, y0, y1) => { let n = 0; for (let y = y0; y <= y1; y++) for (let x = 0; x < W; x++) n += fb[y * W + x]; return n; };
const calls = [];
function ctx(extra = {}) {
  return { link: false, lineIn: false, xiao: false, trackerLeads: false, bpm: 124, linkBpm: 124, conf: 0.9, th: 80, levelDb: -20, levelMin: -25, levelMax: -15,
           running: true, armed: false, beatN: 1, beatMs: 0, peers: 0, wifiQ: null, usb: false, fitted: false, batt: 100,
           ltMode: 2, rate: 2, usbRole: 0, inSrc: 0, net: { mode: 0, group: 1, ssid: '', apName: '' }, autoBar: true, bar: null,
           ble: { mode: 1, left: null, phone: false },
           setField: (f, v) => calls.push(['field', f, v]), tempoStep: d => { calls.push(['tempo', d]); return 120; }, setHold: on => calls.push(['hold', on]),
           linkStartStop: on => calls.push(['startstop', on]), ...extra };
}
const home = (c, now = 0) => { const fb = new Uint8Array(W * OLED.H); OLED.draw(fb, OLED.create(), c, now); return fb; };

// YES in AUDIO with the XIAO: play / stop by the icon (UP / DOWN still say TRACKER LEADS); without it the hold as before
let ui = OLED.create();
OLED.press(ui, 'yes', 0, ctx({ xiao: true, trackerLeads: true, running: true }));
OLED.press(ui, 'yes', 0, ctx({ xiao: true, trackerLeads: true, running: false, armed: true }));
OLED.press(ui, 'yes', 0, ctx({ xiao: true, trackerLeads: true, running: false, armed: false }));
eq(calls.splice(0), [['startstop', false], ['startstop', false], ['startstop', true]]);
OLED.press(ui, 'up', 0, ctx({ xiao: true, trackerLeads: true }));
eq([calls.splice(0), ui.overlay.title], [[], 'TRACKER LEADS']);
ui = OLED.create();
OLED.press(ui, 'yes', 0, ctx());
OLED.press(ui, 'up', 0, ctx());
eq(calls.splice(0), [['hold', true], ['tempo', 1]]);                                  // the local tracker: hold, x2

// the menu: BLUETOOTH right after USB; YES applies the choice (PAIR NOW also unchanged: it opens the window)
function bluetooth(mode, ...steps) {
  const u = OLED.create(), cx = ctx({ ble: { mode, left: null, phone: false } });
  OLED.press(u, 'no', 0, cx);
  for (let i = 0; i < 5; i++) OLED.press(u, 'down', 0, cx);           // AUDIO, LINK TRANSPORT, AUTO BAR, NETWORK, USB, BLUETOOTH
  OLED.press(u, 'yes', 0, cx);
  ok(u.screen === 'edit' && u.edit.field === 'ble' && u.edit.val === mode, 'BLUETOOTH edit opens on the current choice');
  for (const st of steps) OLED.press(u, st, 0, cx);
  eq(calls.length, 0);                                                // nothing before YES
  OLED.press(u, 'yes', 0, cx);
  ok(u.screen === 'menu', 'back to the menu');
  return calls.splice(0);
}
eq(bluetooth(0), [['field', 'ble', 0]]);                              // PAIR NOW, unchanged: picked again
eq(bluetooth(1), [['field', 'ble', 1]]);                              // ON START
eq(bluetooth(1, 'up'), [['field', 'ble', 2]]);                        // OFF
eq(bluetooth(1, 'down'), [['field', 'ble', 0]]);                      // PAIR NOW
eq(bluetooth(2, 'up'), [['field', 'ble', 0]]);                        // choices cycle round
eq(bluetooth(0, 'down'), [['field', 'ble', 2]]);
{
  const u = OLED.create(), cx = ctx();
  OLED.press(u, 'no', 0, cx);
  for (let i = 0; i < 5; i++) OLED.press(u, 'down', 0, cx);
  OLED.press(u, 'yes', 0, cx); OLED.press(u, 'up', 0, cx); OLED.press(u, 'no', 0, cx);       // NO: nothing applied
  eq([calls.splice(0), u.screen], [[], 'menu']);
  const m = new Uint8Array(W * OLED.H); OLED.draw(m, u, cx, 0);
  ok(rowLit(m, 0, 63) > 0, 'the menu draws with the new row');
  for (let i = 0; i < 4; i++) OLED.press(u, 'down', 0, cx);                                  // on to the bottom: ten rows scroll
  eq(u.cursor, 9);
}
// the SOURCE edit has a 4th choice, LINK
ui = OLED.create();
const cs = ctx({ inSrc: 0 });
OLED.press(ui, 'no', 0, cs); OLED.press(ui, 'yes', 0, cs); OLED.press(ui, 'yes', 0, cs);   // MENU, AUDIO, SOURCE (the first row)
OLED.press(ui, 'down', 0, cs);                                                             // AUTO -> LINK (choices cycle round)
eq([ui.edit.field, ui.edit.val, calls.splice(0)], ['insrc', 3, [['field', 'insrc', 3]]]);   // applied live

// HOME's status line: "BT PAIR m:ss" while the window is open, "PHONE" connected; nothing otherwise
const quiet = rowLit(home(ctx()), 56, 63);
eq(quiet, 0);
const w75 = home(ctx({ ble: { mode: 1, left: 75, phone: false } })), w74 = home(ctx({ ble: { mode: 1, left: 74, phone: false } }));
ok(rowLit(w75, 56, 63) > 0, 'BT PAIR 1:15 on the status line');
ok(w75.some((v, i) => v !== w74[i]), 'the count moves');
eq(home(ctx({ ble: { mode: 1, left: 75, phone: false } })), home(ctx({ ble: { mode: 1, left: 74.01, phone: false } })));      // seconds round up
const ph = home(ctx({ ble: { mode: 1, left: null, phone: true } }));
ok(rowLit(ph, 56, 63) > 0, 'PHONE on the status line');
const peers = home(ctx({ peers: 2 }));
ok(rowLit(peers, 56, 63) > 0, 'a peer item is still shown without Bluetooth');
eq([OLED.mmss(75), OLED.mmss(0.2), OLED.mmss(120), OLED.mmss(9.5), OLED.mmss(-1)], ['1:15', '0:01', '2:00', '0:10', '0:00']);
// the pair box (the triple NO): a lit frame with the countdown, for 3 s
ui = OLED.create();
OLED.pairOverlay(ui, 1000);
const box = new Uint8Array(W * OLED.H); OLED.draw(box, ui, ctx({ ble: { mode: 1, left: 119, phone: false } }), 1500);
const base = new Uint8Array(W * OLED.H); OLED.draw(base, OLED.create(), ctx({ ble: { mode: 1, left: 119, phone: false } }), 1500);
ok(lit(box, 14, 12) && lit(box, 60, 30) && !lit(base, 14, 12), 'the box is drawn');
const box2 = new Uint8Array(W * OLED.H); OLED.draw(box2, ui, ctx({ ble: { mode: 1, left: 100, phone: false } }), 1500);
ok(box.some((v, i) => v !== box2[i]), 'the box counts down');
const gone = new Uint8Array(W * OLED.H); OLED.draw(gone, ui, ctx({ ble: { mode: 1, left: 100, phone: false } }), 4001);
ok(!lit(gone, 14, 12), 'gone after 3 s');
// the AUDIO layout whatever the source (no line in needed): the IN and CF bars are drawn
ok(rowLit(home(ctx({ link: false, lineIn: false })), 45, 52) > 600, 'AUDIO layout without a line in: the CF bar');
ok(rowLit(home(ctx({ link: true, lineIn: true })), 45, 52) < 400, 'LINK layout: the beat boxes, no CF bar');

// ---- NO's clicks: the firmware's jb_step on events ----
const kNames = ['clicker', 'noEvent', 'noClick', 'pairNow', 'btnDown', 'btnUp', 'armHint', 'dropHint', 'sourceToggle'];
function rig(over = {}) {
  let clock = 0;
  const log = [];
  const dev = { sw: true, boot: false, held: { up: false, down: false, no: false, yes: false }, link: false, combo: false, deferred: {}, repeat: {}, woke: {},
                lastInput: 0, net: { mode: 0 }, inSrc: 0, bleMode: 1, bleEnd: 0, srcFlash: null };
  const ui = { screen: 'home', hint: null, overlay: null };
  const env = {
    dev, ui, view: { scr: '1.54' }, S: { xiao: false }, XIAO: { status: null, statusAt: 0, pairAt: 0 }, XIAO_STATUS_FRESH_MS: 5000, HINTS: {}, HINT_MS: 400, REPEAT: { firstMs: 400, everyMs: 120 },
    BLE_WINDOW_S: 120, SRC_FLASH: null, linePlugged: () => env.plugged, plugged: false,
    performance: { now: () => clock },
    OLED: { press: (u, b) => log.push(['press', b, clock]), pairOverlay: () => log.push(['overlay', clock]) },
    buildCtx: () => ({}), powered: () => true, asleep: () => false, onHome: () => ui.screen === 'home' && dev.net.mode !== 3,
    comboStep: (mod, d) => log.push(['combo', mod, d]), setField: (f, v) => { log.push(['field', f, v]); if (f === 'insrc') dev.inSrc = v; },
    xiaoSend: l => log.push(['send', l]), askStatus: () => {}, setTimeout: () => 1, clearTimeout: () => {},
    clickNo: { st: 'idle', tDown: 0, tEdge: 0 }, CLICK: null, ...over,
  };
  env.CLICK = new Function('return ' + html.match(/const CLICK = (\{[^}]*\});/)[1])();
  env.SRC_FLASH = new Function('return ' + html.match(/const SRC_FLASH = (\{[^}]*\});/)[1])();
  const keys = Object.keys(env);
  const fns = new Function(...keys, kNames.map(grab).join('\n') + '; return { ' + kNames.join(', ') + ' };')(...keys.map(k => env[k]));
  const tick = () => { const e = fns.clicker(env.clickNo, 'tick', clock); if (e) fns.noEvent(e, clock); };   // frame()'s line
  return { env, dev, ui, log, fns, tick, at: t => { clock = t; tick(); }, down: b => fns.btnDown(b), up: b => fns.btnUp(b) };
}
ok(html.includes("clicker(clickNo, 'tick', performance.now()); if (e) noEvent(e, performance.now())"), 'frame() ticks the clicker');
const presses = r => r.log.filter(e => e[0] === 'press');

let r = rig();                                                         // a single click: said 350 ms after the release
r.at(0); r.down('no'); r.at(100); r.up('no');
r.at(300); eq(presses(r), []);
r.at(449); eq(presses(r), []);
r.at(450); eq(presses(r), [['press', 'no', 450]]);
r.at(2000); eq(presses(r).length, 1);

r = rig();                                                             // a double: nothing
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(200); r.down('no'); r.at(300); r.up('no'); r.at(1000);
eq(r.log, []);

r = rig();                                                             // a triple: on the third press itself, once
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(200); r.down('no'); r.at(300); r.up('no'); r.at(400); r.down('no');
eq(r.log, [['overlay', 400]]);
eq([r.dev.bleEnd, r.dev.combo], [400 + 120000, true]);
r.at(5000); r.up('no'); r.at(9000);
eq(r.log, [['overlay', 400]]);                                         // held: no hint, no click after
eq(r.env.clickNo.st, 'idle');

r = rig({ S: { xiao: true } });                                        // --xiao: `ble pair` goes out
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(200); r.down('no'); r.at(300); r.up('no'); r.at(400); r.down('no');
eq(r.log, [['send', 'ble pair'], ['overlay', 400]]);
eq(r.env.XIAO.pairAt, 400);

r = rig(); r.dev.bleMode = 2;                                          // local, Bluetooth OFF: no window
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(200); r.down('no'); r.at(300); r.up('no'); r.at(400); r.down('no');
eq([r.log, r.dev.bleEnd], [[], 0]);

r = rig();                                                             // a third press that came too late is a new click
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(200); r.down('no'); r.at(300); r.up('no'); r.at(700); eq(r.log, []);
r.down('no'); r.at(750); r.up('no'); r.at(1200);
eq(presses(r).length, 1);                                              // (the double was nothing; this one press is a click)

r = rig();                                                             // the second press after the 350 ms: two clicks
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(450);
r.at(500); r.down('no'); r.at(600); r.up('no'); r.at(950);
eq(presses(r).map(e => e[2]), [450, 950]);

r = rig();                                                             // a late tick: the down says the click, and starts a new press
r.at(0); r.down('no'); r.at(100); r.up('no');
r.env.performance.now = () => 600; r.down('no');
eq(presses(r).length, 1);
eq(r.env.clickNo.st, 'down1');

r = rig();                                                             // NO held + UP: a combo, no click, no double count
r.at(0); r.down('no'); r.at(50); r.down('up'); r.at(100); r.up('up'); r.at(200); r.up('no'); r.at(2000);
eq(r.log.filter(e => e[0] !== 'press' || e[1] !== 'up'), [['combo', 'no', 1], ['press', 'up', 50]].filter(e => e[0] === 'combo'));
eq(presses(r).filter(e => e[1] === 'no'), []);
r.at(2100); r.down('no'); r.at(2200); r.up('no'); r.at(2600);          // the next NO is a normal click again
eq(presses(r).filter(e => e[1] === 'no').length, 1);

r = rig();                                                             // a click, then at once NO + UP: the combo cancels the sequence
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(200); r.down('no'); r.at(250); r.down('down'); r.at(300); r.up('down'); r.at(350); r.up('no'); r.at(2000);
eq(presses(r).filter(e => e[1] === 'no'), []);
eq(r.log.filter(e => e[0] === 'combo'), [['combo', 'no', -1]]);
eq(r.log.filter(e => e[0] === 'overlay'), []);

r = rig();                                                             // a long press (1.5 s) is no click
r.at(0); r.down('no'); r.at(1600); r.up('no'); r.at(3000);
eq(presses(r).filter(e => e[1] === 'no'), []);
r = rig();                                                             // a hold of 0.6 s (the hint's time) is still a click
r.at(0); r.down('no'); r.at(600); r.up('no'); r.at(1000);
eq(presses(r).filter(e => e[1] === 'no').length, 1);

r = rig(); r.ui.screen = 'menu';                                       // off HOME: NO acts at once (back / cancel), the sequence is not used
r.at(0); r.down('no');
eq(presses(r), [['press', 'no', 0]]);
r.at(100); r.up('no'); r.at(2000);
eq(presses(r).length, 1);
r = rig(); r.dev.net.mode = 3;                                         // SETUP: NO acts at once (LEAVE SETUP?)
r.at(0); r.down('no'); eq(presses(r).length, 1);

r = rig(); r.dev.link = true;                                          // LINK mode: NO + UP is still a combo (nothing to nudge), still no click
r.at(0); r.down('no'); r.at(50); r.down('up'); r.at(100); r.up('up'); r.at(150); r.up('no'); r.at(1000);
eq(presses(r).filter(e => e[1] === 'no'), []);

// ---- the screenless unit ----
const NS = { view: { scr: 'none' } };
r = rig(NS);                                                           // single NO: AUTO -> LINK (purple), again: LINK -> AUTO (cyan)
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(450);
eq(r.log, [['field', 'insrc', 3]]);
eq(r.dev.srcFlash, { until: 450 + 400, rgb: [20, 0, 40] });
r.at(1000); r.down('no'); r.at(1100); r.up('no'); r.at(1450);
eq(r.log.slice(1), [['field', 'insrc', 0]]);
eq(r.dev.srcFlash, { until: 1450 + 400, rgb: [0, 30, 30] });
eq(presses(r), []);                                                    // never the menu
r = rig(NS); r.dev.inSrc = 1;                                          // LINE IN only -> LINK; USB AUDIO only -> LINK
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(450);
eq(r.log, [['field', 'insrc', 3]]);
r = rig(NS); r.env.plugged = true;                                     // the line is plugged: mechanical, NO does nothing
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(450);
eq([r.log, r.dev.srcFlash], [[], null]);
r = rig({ ...NS, S: { xiao: true } }); r.dev.inSrc = 3;                // --xiao: the field goes out as `set source` by setField
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(450);
eq(r.log, [['field', 'insrc', 0]]);
r = rig(NS);                                                           // triple still pairs; the combos still nudge
r.at(0); r.down('no'); r.at(100); r.up('no'); r.at(200); r.down('no'); r.at(300); r.up('no'); r.at(400); r.down('no');
eq(r.log, [['overlay', 400]]);
r = rig(NS);
r.at(0); r.down('no'); r.at(50); r.down('up'); r.at(100); r.up('up'); r.at(150); r.up('no'); r.at(1000);
eq(r.log.filter(e => e[0] === 'combo'), [['combo', 'no', 1]]);
eq(r.log.filter(e => e[0] === 'field'), []);

// the toggle's rule, as a function
const T = new Function(konst('SRC_FLASH') + grab('sourceToggle') + '; return sourceToggle;')();
eq(T(0, true, 0), null);
eq(T(3, true, 0), null);
eq(T(0, false, 10), { src: 3, flash: { until: 410, rgb: [20, 0, 40] } });
eq(T(3, false, 10), { src: 0, flash: { until: 410, rgb: [0, 30, 30] } });
eq(T(2, false, 10).src, 3);

// ---- the LED language (core/jank_led.c) ----
const led = new Function('const clampN = (v, lo, hi) => Math.max(lo, Math.min(hi, v));' + konst('BATT') + konst('lowPeriodMs') + html.match(/const LED = \{[\s\S]*?\};\n/)[0] + konst('SRC_FLASH') + grab('ledColor') + konst('ledPaint') + '; return { ledColor, ledPaint, LED };')();
const S0 = { now: 100000, flash: null, pair: false, battPct: -1, onUsb: true, joined: true, running: true, armed: false, lowConf: false, barLocked: false, barOne: false, beatMs: 10, bpm: 120 };
const col = o => led.ledColor({ ...S0, ...o });
eq(col({}), [0, 30, 0]);                                               // a beat: green
eq(col({ beatMs: 79 }), [0, 30, 0]);
eq(col({ beatMs: 80 }), [0, 0, 0]);                                    // 80 ms, then dark
eq(col({ barOne: true, beatMs: 149 }), [0, 30, 0]);                    // the 1: longer
eq(col({ barOne: true, beatMs: 150 }), [0, 0, 0]);
eq(col({ barOne: true, barLocked: true }), [40, 18, 0]);              // AUTO BAR locked: amber on the 1
eq(col({ barOne: false, barLocked: true }), [0, 30, 0]);
eq(col({ lowConf: true }), [40, 0, 0]);                                // low confidence: red
eq(col({ bpm: 300, beatMs: 66 }), [0, 30, 0]);                         // never longer than a third of a beat (200 ms)
eq(col({ bpm: 300, beatMs: 67 }), [0, 0, 0]);
eq(col({ running: false, armed: true }), [30, 20, 0]);                 // armed: yellow, steady
eq(col({ running: false, armed: true, now: 100250 }), [30, 20, 0]);
eq(col({ running: false, now: 100000 }), [0, 0, 0]);                   // stopped in a session: breathing (0 at the 4 s mark)
eq(col({ running: false, now: 102000 }), [0, 12, 0]);
eq(col({ running: false, now: 101000 }), [0, 6, 0]);
eq(col({ running: false, joined: false }), [0, 0, 0]);                 // nobody: dark
eq(col({ running: true, joined: false }), [0, 0, 0]);
eq(col({ pair: true, now: 100000 }), [0, 0, 40]);                      // the pair window: blue, 4 a second, over everything
eq(col({ pair: true, now: 100124 }), [0, 0, 40]);
eq(col({ pair: true, now: 100125 }), [0, 0, 0]);
eq(col({ pair: true, now: 100250 }), [0, 0, 40]);
eq(col({ onUsb: false, battPct: 20, now: 100000 - (100000 % 1920) }), [40, 0, 0]);   // low cell: a red flash (period at 20 % = 1.92 s)
eq(col({ onUsb: false, battPct: 20, now: 100000 - (100000 % 1920) + 100 }), [0, 30, 0]);   // 100 ms later it is over
eq(col({ onUsb: true, battPct: 20, now: 100000 - (100000 % 1920) }), [0, 30, 0]);   // on USB: no warning
eq(col({ onUsb: false, battPct: 80, now: 100000 }), [0, 30, 0]);
// the source flash: cyan = USB AUDIO, purple = LINK, 400 ms, over everything but the next state afterwards
const flash = { until: 100400, rgb: [0, 30, 30] };
eq(col({ flash, now: 100399, pair: true }), [0, 30, 30]);
eq(col({ flash, now: 100400, beatMs: 10 }), [0, 30, 0]);
const cy = led.ledPaint([0, 30, 30]), pu = led.ledPaint([20, 0, 40]), gr = led.ledPaint([0, 30, 0]), am = led.ledPaint([30, 20, 0]), re = led.ledPaint([40, 0, 0]), amb = led.ledPaint([40, 18, 0]);
ok(cy[1] > 150 && cy[2] > 150 && cy[0] === 0, 'cyan reads as cyan');
ok(pu[0] > 100 && pu[2] > 200 && pu[1] === 0, 'purple reads as purple');
ok(gr[1] > 150 && gr[0] === 0 && gr[2] === 0, 'green');
ok(am[0] > 150 && am[1] > 100 && am[2] === 0 && amb[0] > am[0], 'yellow and amber');
ok(re[0] > 200 && re[1] === 0, 'red');
ok(led.ledPaint([0, 12, 0])[1] > 60, 'the breath top is visible');
eq(led.ledPaint([0, 0, 0]), [0, 0, 0]);
console.log('oled buttons: ' + checks + ' checks passed');
